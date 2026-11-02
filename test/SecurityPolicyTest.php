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

$router = new Router('policy-test');
$router->clear()
    ->get('/', 'PolicyWeb@run')
    ->post('/save', 'PolicyWeb@save')
    ->get('/rest/ping', 'PolicyRest@ping');
$app = new Application('policy-test');
$app->trustProxies(['proxies' => '172.18.0.0/16']);

function dispatch($app, $method, $uri, array $server = [], array $header = [], array $post = []) {
    PolicyTrace::$values = [];
    $server += ['REQUEST_METHOD' => $method, 'REQUEST_URI' => $uri, 'QUERY_STRING' => ''];
    Request::init([], $post, [], $server, null, [], null, $header, '');
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

echo "\n=== Results: " . ($fail ? "FAILED $fail" : "ALL PASS") . " ===\n";
exit($fail ? 1 : 0);
