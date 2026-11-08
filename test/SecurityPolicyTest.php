<?php

/**
 * Dispatch-level security policy regression (Application::run choke point).
 *
 * Covers:
 *  - restGuard internal gate behind trustProxies: a trusted proxy peer is
 *    judged by the resolved client, not by its own (internal) address
 *  - requestId trust: incoming id only believed from a trusted peer once
 *    trustProxies is configured
 *  - Application::webscan direct scanner path + fail_closed argument
 */

use Gene\Application;
use Gene\Request;
use Gene\Router;

echo "=== Security Policy Dispatch Test ===\n\n";

$fail = 0;
function check($name, $cond) {
    global $fail;
    if ($cond) {
        echo "✓ $name\n";
    } else {
        $fail++;
        echo "✗ $name\n";
    }
}

class PolicyTrace { public static $values = []; }
class PolicyRest { public function ping() { PolicyTrace::$values[] = 'rest'; } }
class PolicyWeb {
    public function run() { PolicyTrace::$values[] = 'web:' . \Gene\Context::get('request_id'); }
    public function save() { PolicyTrace::$values[] = 'save'; }
}

class PolicyAdmin {
    public function hit() { PolicyTrace::$values[] = 'admin'; }
}

$router = new Router('policy-test');
$router->clear()
    ->lang('zh,en')
    ->get('/', 'PolicyWeb@run')
    ->post('/save', 'PolicyWeb@save')
    ->post('/login', 'PolicyWeb@save')
    ->post('/hook/notify', 'PolicyWeb@save')
    ->post('/admin/save', 'PolicyWeb@save')
    ->get('/admin/:c/:a', 'PolicyAdmin@hit')
    ->get('/rest/ping', 'PolicyRest@ping');
$app = new Application('policy-test');
$app->trustProxies(['proxies' => '172.18.0.0/16']);

$GLOBALS['policyPre'] = null;
function dispatch($app, $method, $uri, array $server = [], array $header = [], array $post = []) {
    PolicyTrace::$values = [];
    $server += ['REQUEST_METHOD' => $method, 'REQUEST_URI' => $uri, 'QUERY_STRING' => ''];
    Request::init([], $post, [], $server, null, [], null, $header, '');
    if ($GLOBALS['policyPre']) {
        ($GLOBALS['policyPre'])();
    }
    ob_start();
    @$app->run($method, $uri);
    $out = ob_get_clean();
    Application::cleanup();
    return [PolicyTrace::$values, $out];
}

/* ---------- restGuard internal gate + trusted proxy ---------- */
$app->restGuard(['internal' => true]);
[$v] = dispatch($app, 'GET', '/rest/ping', ['REMOTE_ADDR' => '10.1.2.3']);
check("restGuard: direct internal peer admitted", $v === ['rest']);
[$v] = dispatch($app, 'GET', '/rest/ping', ['REMOTE_ADDR' => '8.8.8.8']);
check("restGuard: direct public peer denied", $v === []);
[$v] = dispatch($app, 'GET', '/rest/ping', ['REMOTE_ADDR' => '172.18.0.1'], ['x-forwarded-for' => '203.0.113.7']);
check("restGuard: public client via trusted proxy denied", $v === []);
[$v] = dispatch($app, 'GET', '/rest/ping', ['REMOTE_ADDR' => '172.18.0.1'], ['x-forwarded-for' => '10.9.9.9']);
check("restGuard: internal client via trusted proxy admitted", $v === ['rest']);
[$v] = dispatch($app, 'GET', '/rest/ping', ['REMOTE_ADDR' => '8.8.8.8'], ['x-forwarded-for' => '10.9.9.9']);
check("restGuard: XFF from untrusted peer ignored", $v === []);
Request::init([], [], [], ['REMOTE_ADDR' => '172.18.0.1'], null, [], null, ['x-forwarded-for' => '203.0.113.7'], '');
check("Request::isInternal behind proxy judges client", Request::isInternal() === false);
$app->restGuard(null);

/* ---------- requestId trust gated by trusted peer ---------- */
$app->requestId(['header' => 'X-Request-Id', 'trust' => true]);
[$v] = dispatch($app, 'GET', '/', ['REMOTE_ADDR' => '172.18.0.1'], ['x-request-id' => 'proxy-id']);
check("requestId: id from trusted proxy reused", $v === ['web:proxy-id']);
[$v] = dispatch($app, 'GET', '/', ['REMOTE_ADDR' => '8.8.8.8'], ['x-request-id' => 'client-id']);
check("requestId: id from direct client replaced",
    count($v) === 1 && $v[0] !== 'web:client-id' && strlen($v[0]) === strlen('web:') + 16);
$app->requestId(null);

/* ---------- webscan direct path ---------- */
$app->webscan(1, '', function () { return 'blocked'; });
[$v, $out] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], [], ['c' => '1 union select password from users']);
check("webscan: attack blocked via app path", $v === [] && $out === 'blocked');
[$v] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], [], ['c' => 'hello world']);
check("webscan: clean request passes", $v === ['save']);
$app->webscan(1, 'save');
[$v] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], [], ['c' => '1 union select password from users']);
check("webscan: white dir honoured via app path", $v === ['save']);

$old = ini_get('pcre.backtrack_limit');
$oldJit = ini_get('pcre.jit');
ini_set('pcre.jit', '0');
ini_set('pcre.backtrack_limit', '10');
$pad = ['c' => '<' . str_repeat('a', 5000) . ' union select'];
$app->webscan(1, '', null, [], 1, 1, 1, 1);
[$v] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], [], $pad);
check("webscan: PCRE error fail-closed by default", $v === []);
$app->webscan(1, '', null, [], 1, 1, 1, 1, 0);
[$v] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], [], $pad);
check("webscan: fail_closed=0 lets PCRE error pass", $v === ['save']);
ini_set('pcre.backtrack_limit', $old);
ini_set('pcre.jit', $oldJit);
$app->webscan(0);

/* ---------- language-prefix awareness ---------- */
$app->restGuard(['internal' => true]);
[$v] = dispatch($app, 'GET', '/zh/rest/ping', ['REMOTE_ADDR' => '8.8.8.8']);
check("lang: default /rest/ guard covers /zh/rest/*", $v === []);
[$v] = dispatch($app, 'GET', '/zh/rest/ping', ['REMOTE_ADDR' => '10.1.2.3']);
check("lang: /zh/rest/* from internal peer admitted", $v === ['rest']);
[$v] = dispatch($app, 'GET', '/zhx/rest/ping', ['REMOTE_ADDR' => '8.8.8.8']);
check("lang: unregistered segment is not stripped", $v === []);
$app->restGuard(null);

/* ---------- csrf: except + scopes are language-aware ---------- */
$cross = ['sec-fetch-site' => 'cross-site'];
$app->csrf(['except' => ['/login']]);
[$v] = dispatch($app, 'POST', '/zh/login', ['REMOTE_ADDR' => '8.8.8.8'], $cross);
check("csrf: except '/login' covers /zh/login", $v === ['save']);
[$v] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], $cross);
check("csrf: cross-site POST still denied", $v === []);
$app->csrf([
    'allow_same_site' => true,
    'scopes' => [
        '/admin/*' => ['allow_same_site' => false, 'require_origin' => true],
        '/hook/*'  => false,
    ],
]);
[$v] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], ['sec-fetch-site' => 'same-site']);
check("csrf scope: base allows same-site", $v === ['save']);
[$v] = dispatch($app, 'POST', '/en/admin/save', ['REMOTE_ADDR' => '8.8.8.8'], ['sec-fetch-site' => 'same-site']);
check("csrf scope: /admin/* override denies same-site (lang path)", $v === []);
[$v] = dispatch($app, 'POST', '/admin/save', ['REMOTE_ADDR' => '8.8.8.8', 'HTTP_HOST' => 'a.test'], ['host' => 'a.test']);
check("csrf scope: /admin/* require_origin denies missing Origin", $v === []);
[$v] = dispatch($app, 'POST', '/admin/save', ['REMOTE_ADDR' => '8.8.8.8'], ['host' => 'a.test', 'origin' => 'http://a.test']);
check("csrf scope: /admin/* same Origin admitted", $v === ['save']);
[$v] = dispatch($app, 'POST', '/hook/notify', ['REMOTE_ADDR' => '8.8.8.8'], $cross);
check("csrf scope: false disables csrf for subtree", $v === ['save']);
$app->csrf(null);

/* ---------- readonly: globs, true, language prefix ---------- */
$app->readonly(['/admin' => ['user' => ['list*', 'edit'], 'log' => true, '*' => []]]);
[$v] = dispatch($app, 'GET', '/admin/user/listAll', ['REMOTE_ADDR' => '8.8.8.8']);
check("readonly: glob action allowed", $v === ['admin']);
[$v] = dispatch($app, 'GET', '/admin/user/del', ['REMOTE_ADDR' => '8.8.8.8']);
check("readonly: unlisted action denied", $v === []);
[$v] = dispatch($app, 'GET', '/zh/admin/user/del', ['REMOTE_ADDR' => '8.8.8.8']);
check("readonly: /zh/admin covered by /admin entry", $v === []);
[$v] = dispatch($app, 'GET', '/admin/log/anything', ['REMOTE_ADDR' => '8.8.8.8']);
check("readonly: controller => true allows all", $v === ['admin']);
[$v] = dispatch($app, 'GET', '/admin/other/lists', ['REMOTE_ADDR' => '8.8.8.8']);
check("readonly: '*' => [] denies unknown controller", $v === []);
$app->readonly(null);

/* ---------- throttle ---------- */
$GLOBALS['policyPre'] = function () { \Gene\Di::set('thrstore', new \Gene\Memory('policy-thr')); };
$app->throttle([
    'store' => 'thrstore',
    'prefix' => 'thr:' . uniqid() . ':',
    'rules' => ['/login' => ['limit' => 2, 'window' => 60, 'methods' => ['POST']]],
]);
$hits = [];
for ($i = 0; $i < 3; $i++) {
    [$v, $out] = dispatch($app, 'POST', ($i === 1 ? '/zh/login' : '/login'), ['REMOTE_ADDR' => '8.8.4.4']);
    $hits[] = $v;
}
check("throttle: first two admitted (lang shares bucket)", $hits[0] === ['save'] && $hits[1] === ['save']);
check("throttle: third denied", $hits[2] === [] && strpos($out, '4000') !== false);
[$v] = dispatch($app, 'POST', '/login', ['REMOTE_ADDR' => '8.8.8.9']);
check("throttle: other client has own bucket", $v === ['save']);
[$v] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.4.4']);
check("throttle: unmatched path untouched", $v === ['save']);
$app->throttle(['store' => 'missing', 'rules' => ['/login' => ['limit' => 1, 'window' => 60]]]);
[$v] = @dispatch($app, 'POST', '/login', ['REMOTE_ADDR' => '8.8.4.4']);
check("throttle: missing store fails open", $v === ['save']);
$app->throttle(null);
$GLOBALS['policyPre'] = null;

/* ---------- webscan array config ---------- */
$attack = ['c' => '1 union select password from users'];
$app->webscan(['white_dir' => ['admin']]);
[$v, $out] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], ['x-requested-with' => 'XMLHttpRequest'], $attack);
check("webscan[]: blocked with default JSON for XHR", $v === [] && $out === '{"code":4000,"msg":"Illegal access"}');
[$v] = dispatch($app, 'POST', '/zh/admin/save', ['REMOTE_ADDR' => '8.8.8.8'], [], $attack);
check("webscan[]: white_dir covers /zh/admin", $v === ['save']);
$app->webscan(['mode' => 'log']);
[$v] = @dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], [], $attack);
check("webscan[]: mode=log never blocks", $v === ['save']);
$app->webscan(['on_fail' => function () { return 'custom'; }]);
[$v, $out] = dispatch($app, 'POST', '/save', ['REMOTE_ADDR' => '8.8.8.8'], [], $attack);
check("webscan[]: on_fail callback used", $v === [] && $out === 'custom');
$app->webscan(0);

/* ---------- Csrf helpers follow scopes ---------- */
$app->csrf(['session' => 'nosuch', 'scopes' => ['/admin/*' => ['session' => 'nosuch2']]]);
Request::init([], [], [], ['REQUEST_METHOD' => 'GET', 'REQUEST_URI' => '/'], null, [], null, [], '');
check("Csrf::check without session is false", \Gene\Csrf::check('x') === false);
$app->csrf(null);

echo "\n=== Results: " . ($fail ? "FAILED $fail" : "ALL PASS") . " ===\n";
exit($fail ? 1 : 0);
