<?php

/**
 * Gene Framework Swoole Entry Adapter Test
 *
 * Covers Request::initSwoole() and Application::handleSwoole() with
 * duck-typed Swoole\Http\Request/Response doubles — ext-swoole is NOT
 * required (runtime_type=2 falls back to the resident ctx in CLI).
 */

use Gene\Application;
use Gene\Request;
use Gene\Response;

class FakeSwooleRequest
{
    public $get;
    public $post;
    public $cookie;
    public $server;
    public $files;
    public $header;
    private $raw;
    public $rawCalls = 0;

    public function __construct($get = [], $post = [], $cookie = [], $server = [], $files = [], $header = [], $raw = '')
    {
        $this->get = $get;
        $this->post = $post;
        $this->cookie = $cookie;
        $this->server = $server;
        $this->files = $files;
        $this->header = $header;
        $this->raw = $raw;
    }

    public function rawContent()
    {
        $this->rawCalls++;
        return $this->raw;
    }
}

class FakeSwooleRequestNoRaw
{
    public $get = ['a' => 1];
    public $server = ['request_method' => 'get', 'request_uri' => '/echo'];
}

class FakeSwooleRequestSparse
{
    public $server = ['request_method' => 'get', 'request_uri' => '/echo'];
    public $get = ['only' => 'g'];
    public $post = 'not-an-array';

    public function rawContent()
    {
        return '';
    }
}

class FakeSwooleResponse
{
    public $endCount = 0;
    public $body = null;
    public $status = null;
    public $headers = [];
    public $writes = [];
    public $writable = true;
    public $ended = false;
    public $redirected = null;

    public function end($data = '')
    {
        if ($this->ended) {
            return false;
        }
        $this->endCount++;
        $this->body = $data;
        $this->ended = true;
        return true;
    }

    public function isWritable()
    {
        return $this->writable && !$this->ended;
    }

    public function status($code)
    {
        $this->status = $code;
        return true;
    }

    public function header($key, $value)
    {
        $this->headers[$key] = $value;
        return true;
    }

    public function redirect($url, $code = 302)
    {
        $this->status = $code;
        $this->redirected = $url;
        $this->ended = true;
        return true;
    }

    public function write($data)
    {
        $this->writes[] = $data;
        return true;
    }

    public function sendfile($file, $offset = 0, $length = 0)
    {
        $this->ended = true;
        $this->body = 'file:' . $file;
        return true;
    }
}

class SwooleEntryState
{
    public static $resp;
}

class SwooleEchoController
{
    public function run()
    {
        echo 'swoole-echo';
    }
}

class SwooleJsonController
{
    public function run()
    {
        Response::json(['ok' => true]);
    }
}

class SwooleBoomController
{
    public function run()
    {
        echo 'partial-';
        throw new \RuntimeException('boom-boom');
    }
}

class SwooleLeakObController
{
    public function run()
    {
        echo 'a';
        ob_start();
        echo 'inner';   // never closed — must converge into the entry buffer
    }
}

class SwooleDirectEndController
{
    public function run()
    {
        /* business bypasses Gene\Response and ends the Swoole response directly */
        SwooleEntryState::$resp->end('direct-end');
    }
}

class SwooleGeneEndController
{
    public function run()
    {
        Response::end('gene-end');
    }
}

class SwooleRedirectController
{
    public function run()
    {
        Response::redirect('/to');
    }
}

class SwooleWriteController
{
    public function run()
    {
        Response::write('chunk1');
        echo 'tail';
    }
}

class SwooleEntryTest
{
    private $app;
    private $prevRuntime;

    public function __construct()
    {
        echo "=== Gene Swoole Entry (initSwoole / handleSwoole) Test ===\n\n";
        /* runtime_type=2 makes set_server_val fill ctx->method/path and turns
         * cleanup() into the resident-ctx destroy path — no ext-swoole needed. */
        $this->prevRuntime = Application::getRuntimeType();
        Application::setRuntimeType('swoole');
    }

    public function testInitSwooleBags()
    {
        echo "Testing Request::initSwoole bags:\n";
        $req = new FakeSwooleRequest(
            ['g' => '1'],
            ['p' => '2'],
            ['c' => '3'],
            ['request_method' => 'put', 'request_uri' => '/echo?x=1', 'content_type' => 'application/json'],
            ['f' => ['name' => 'a.txt', 'size' => 3]],
            ['x-h' => 'hv'],
            '{"j":1}'
        );
        $ok = Request::initSwoole($req)
            && Request::get('g') === '1'
            && Request::post('p') === '2'
            && Request::cookie('c') === '3'
            && Request::server('request_method') === 'put'
            && Request::server('REQUEST_METHOD') === 'put'
            && Request::files('f') === ['name' => 'a.txt', 'size' => 3]
            && Request::header('X-H') === 'hv'
            && Request::request('g') === '1'
            && Request::request('p') === '2'
            && Request::rawContent() === '{"j":1}'
            && $req->rawCalls === 1;
        if ($ok) {
            echo "✓ initSwoole populates get/post/cookie/server/files/header/request/raw\n";
        } else {
            echo "✗ initSwoole bag population failed\n";
        }
        $j = Request::json();
        if (is_array($j) && ($j['j'] ?? null) === 1) {
            echo "✓ json() parses raw body, bytes unchanged\n";
        } else {
            echo "✗ json() after initSwoole failed\n";
        }
        if (Request::isPut() && Request::input('j') === 1 && Request::input('g') === '1') {
            echo "✓ lowercase request_method fills ctx, input() merges GET+JSON\n";
        } else {
            echo "✗ ctx method / input() merge failed\n";
        }
        Application::cleanup();
        echo "\n";
    }

    public function testInitSwooleMissing()
    {
        echo "Testing Request::initSwoole missing/illegal props:\n";
        $req = new FakeSwooleRequestSparse();
        Request::initSwoole($req);
        if (Request::get('only') === 'g'
            && Request::post() === []
            && Request::cookie() === []
            && Request::files() === []
            && Request::header() === []
            && Request::request() === ['only' => 'g']
            && Request::rawContent() === '') {
            echo "✓ missing/non-array props become [], empty body stays ''\n";
        } else {
            echo "✗ sparse request normalization failed\n";
        }
        Application::cleanup();

        $threw = false;
        try {
            Request::initSwoole(new FakeSwooleRequestNoRaw());
        } catch (\Error $e) {
            $threw = true;
        }
        if ($threw) {
            echo "✓ missing rawContent() throws Error (no silent empty body)\n";
        } else {
            echo "✗ missing rawContent() did not throw\n";
        }
        Application::cleanup();
        echo "\n";
    }

    public function testInitSwooleReinit()
    {
        echo "Testing initSwoole re-init invalidation:\n";
        Request::init([], [], [], [], null, [], null, [], '{"old":true}');
        Request::json();
        Request::initSwoole(new FakeSwooleRequest(['n' => '1'], [], [],
            ['request_method' => 'get', 'request_uri' => '/echo'], [], [], '{"new":2}'));
        $j = Request::json();
        if (($j['new'] ?? null) === 2 && Request::rawContent() === '{"new":2}'
            && Request::get('n') === '1' && !isset($j['old'])) {
            echo "✓ re-init replaces raw body and drops stale JSON cache\n";
        } else {
            echo "✗ stale raw/JSON leaked across re-init\n";
        }
        Application::cleanup();
        echo "\n";
    }

    private function swooleRequest($uri, $method = 'get', $raw = '', array $extra = [])
    {
        return new FakeSwooleRequest(
            $extra['get'] ?? [],
            $extra['post'] ?? [],
            $extra['cookie'] ?? [],
            ['request_method' => $method, 'request_uri' => $uri] + ($extra['server'] ?? []),
            $extra['files'] ?? [],
            $extra['header'] ?? [],
            $raw
        );
    }

    public function testHandleSwoole()
    {
        echo "Testing Application::handleSwoole:\n";
        $router = new \Gene\Router('swoole-entry');
        $router->clear()
            ->get('/echo', 'SwooleEchoController@run')
            ->get('/json', 'SwooleJsonController@run')
            ->get('/boom', 'SwooleBoomController@run')
            ->get('/leak', 'SwooleLeakObController@run')
            ->get('/direct', 'SwooleDirectEndController@run')
            ->get('/gend', 'SwooleGeneEndController@run')
            ->get('/redir', 'SwooleRedirectController@run')
            ->get('/write', 'SwooleWriteController@run');
        $this->app = new Application('swoole-entry');
        $this->app->workerReady();

        /* echo → collected buffer ended once */
        $resp = new FakeSwooleResponse();
        $this->app->handleSwoole($this->swooleRequest('/echo'), $resp);
        if ($resp->endCount === 1 && $resp->body === 'swoole-echo' && $resp->status === null) {
            echo "✓ echo controller: buffered output ended once, no default status\n";
        } else {
            echo "✗ echo path failed: " . json_encode([$resp->endCount, $resp->body, $resp->status]) . "\n";
        }

        /* Response::json → buffered body still flushed exactly once */
        $resp = new FakeSwooleResponse();
        $this->app->handleSwoole($this->swooleRequest('/json'), $resp);
        if ($resp->endCount === 1 && $resp->body === '{"ok":true}') {
            echo "✓ Response::json body flushed once (no double end)\n";
        } else {
            echo "✗ json path failed: " . json_encode([$resp->endCount, $resp->body]) . "\n";
        }

        /* controller throws → default catch: Log::exception + 500 + buffered out */
        $resp = new FakeSwooleResponse();
        $this->app->handleSwoole($this->swooleRequest('/boom'), $resp);
        if ($resp->endCount === 1 && $resp->status === 500 && $resp->body === 'partial-') {
            echo "✓ exception → minimal 500 + buffered output, still ended\n";
        } else {
            echo "✗ exception default path failed: " . json_encode([$resp->endCount, $resp->status, $resp->body]) . "\n";
        }

        /* custom catch receives (e, request, response) and owns the reply */
        $resp = new FakeSwooleResponse();
        $seen = null;
        $this->app->handleSwoole($this->swooleRequest('/boom'), $resp, [
            'catch' => function (\Throwable $e, $request, $response) use (&$seen) {
                $seen = $e->getMessage();
                echo 'caught';
            },
        ]);
        if ($seen === 'boom-boom' && $resp->endCount === 1 && $resp->body === 'partial-caught' && $resp->status === null) {
            echo "✓ catch callable consumes exception, output appended, no 500\n";
        } else {
            echo "✗ catch path failed: " . json_encode([$seen, $resp->endCount, $resp->body]) . "\n";
        }

        /* catch throwing → exception propagates, cleanup still ran */
        $resp = new FakeSwooleResponse();
        $propagated = false;
        try {
            $this->app->handleSwoole($this->swooleRequest('/boom'), $resp, [
                'catch' => function (\Throwable $e) {
                    throw new \LogicException('catch-fail');
                },
            ]);
        } catch (\Throwable $e) {
            $propagated = $e->getMessage() === 'catch-fail';
        }
        if ($propagated && $resp->endCount === 0 && Request::get('x') === null) {
            echo "✓ re-thrown catch propagates; cleanup ran, no end\n";
        } else {
            echo "✗ catch rethrow semantics failed\n";
        }

        /* leaked nested ob_start converges into entry buffer */
        $resp = new FakeSwooleResponse();
        $baseLevel = ob_get_level();
        $this->app->handleSwoole($this->swooleRequest('/leak'), $resp);
        if ($resp->endCount === 1 && $resp->body === 'ainner' && ob_get_level() === $baseLevel) {
            echo "✓ leaked nested buffer converged, outer level intact\n";
        } else {
            echo "✗ buffer convergence failed: " . json_encode([$resp->body, ob_get_level(), $baseLevel]) . "\n";
        }

        /* business ended the swoole response directly → no second end */
        $resp = new FakeSwooleResponse();
        SwooleEntryState::$resp = $resp;
        $this->app->handleSwoole($this->swooleRequest('/direct'), $resp);
        if ($resp->endCount === 1 && $resp->body === 'direct-end') {
            echo "✓ direct \$response->end() not doubled\n";
        } else {
            echo "✗ direct end doubled: endCount=" . $resp->endCount . "\n";
        }

        /* Gene\Response::end() → isWritable()=false → single end */
        $resp = new FakeSwooleResponse();
        $this->app->handleSwoole($this->swooleRequest('/gend'), $resp);
        if ($resp->endCount === 1 && $resp->body === 'gene-end') {
            echo "✓ Response::end() not doubled\n";
        } else {
            echo "✗ Response::end doubled\n";
        }

        /* redirect → status/location kept, no body appended */
        $resp = new FakeSwooleResponse();
        $this->app->handleSwoole($this->swooleRequest('/redir'), $resp);
        if ($resp->endCount === 0 && $resp->redirected === '/to' && $resp->status === 302) {
            echo "✓ redirect keeps status/location, no extra end\n";
        } else {
            echo "✗ redirect path failed: " . json_encode([$resp->endCount, $resp->redirected, $resp->status]) . "\n";
        }

        /* write() streams then buffered tail flushed by end() */
        $resp = new FakeSwooleResponse();
        $this->app->handleSwoole($this->swooleRequest('/write'), $resp);
        if ($resp->writes === ['chunk1'] && $resp->endCount === 1 && $resp->body === 'tail') {
            echo "✓ write() chunks kept, buffered tail ended\n";
        } else {
            echo "✗ write() path failed: " . json_encode([$resp->writes, $resp->body]) . "\n";
        }

        /* cleanup_gc option accepted */
        $resp = new FakeSwooleResponse();
        $this->app->handleSwoole($this->swooleRequest('/echo'), $resp, ['cleanup_gc' => true]);
        if ($resp->endCount === 1 && $resp->body === 'swoole-echo') {
            echo "✓ cleanup_gc option works\n";
        } else {
            echo "✗ cleanup_gc option failed\n";
        }

        /* non-callable catch → ValueError */
        $threw = false;
        try {
            $this->app->handleSwoole($this->swooleRequest('/echo'), new FakeSwooleResponse(), ['catch' => 'not-a-fn-xyz']);
        } catch (\ValueError $e) {
            $threw = true;
        }
        if ($threw) {
            echo "✓ non-callable catch rejected\n";
        } else {
            echo "✗ non-callable catch accepted\n";
        }
        echo "\n";
    }

    public function testBootstrapAndPools()
    {
        echo "Testing bootstrap + pool orchestration:\n";
        /* 无名 getInstance：app_key 未设置 → Config::set / Application::config /
         * pool 预检一致落到 app_root 前缀。 */
        $this->app = Application::getInstance();

        /* bootstrap: autoload + load(router) + {env} config + setMode */
        $dir = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'gene_boot_' . uniqid();
        mkdir($dir);
        file_put_contents($dir . '/router.ini.php', '<?php // router fixture');
        file_put_contents($dir . '/config.ini.gray.php',
            '<?php (new \Gene\Config())->set("bootmark", ["ok" => 1]);');

        $prevEnv = Application::getEnvironment();
        Application::setEnvironment(3); // gray
        $ret = $this->app->bootstrap($dir, $dir, [
            'router'     => 'router.ini.php',
            'config'     => 'config.ini.{env}.php',
            'mode'       => 1,
            'debug_envs' => ['dev'],
        ]);
        Application::setEnvironment($prevEnv);
        /* setMode(1, debug) 注册了错误/异常处理器，拆掉避免影响后续断言 */
        restore_error_handler();
        restore_exception_handler();
        $loaded = Application::config('bootmark');
        if ($ret === $this->app && is_array($loaded) && ($loaded['ok'] ?? null) === 1) {
            echo "✓ bootstrap autoload+router+{env} config+setMode\n";
        } else {
            echo "✗ bootstrap: ret=" . gettype($ret) . " config=" . var_export($loaded, true) . "\n";
        }

        /* bootstrap rejects bad option types */
        $err = 0;
        try { $this->app->bootstrap($dir, $dir, ['router' => 123]); } catch (\ValueError $e) { $err++; }
        try { $this->app->bootstrap($dir, $dir, ['debug_envs' => 'dev']); } catch (\ValueError $e) { $err++; }
        if ($err === 2) {
            echo "✓ bootstrap validates option types\n";
        } else {
            echo "✗ bootstrap option validation: $err/2\n";
        }

        /* pools() declaration validation */
        $err = 0;
        try { $this->app->pools([123 => ['driver' => 'db', 'component' => 'db']]); } catch (\ValueError $e) { $err++; }
        try { $this->app->pools(['p1' => 'not-array']); } catch (\ValueError $e) { $err++; }
        try { $this->app->pools(['p2' => ['driver' => 'mongo', 'component' => 'db']]); } catch (\ValueError $e) { $err++; }
        try { $this->app->pools(['p3' => ['driver' => 'db']]); } catch (\ValueError $e) { $err++; }
        try { $this->app->pools(['p4' => ['driver' => 'db', 'component' => 'db', 'params' => 'x']]); } catch (\ValueError $e) { $err++; }
        if ($err === 5) {
            echo "✓ pools() validates declarations\n";
        } else {
            echo "✗ pools() validation: $err/5\n";
        }

        /* missing component config → explicit Exception; decl stays
         * un-started so a retry fails cleanly instead of half-init */
        $this->app->pools(['badpool' => ['driver' => 'db', 'component' => 'missing_cfg_xyz']]);

        /* startPools refuses under FPM (decls untouched) */
        Application::setRuntimeType('fpm');
        $fpm = $this->app->startPools();
        Application::setRuntimeType('swoole');
        if ($fpm === false) {
            echo "✓ startPools refused under FPM\n";
        } else {
            echo "✗ FPM startPools returned " . var_export($fpm, true) . "\n";
        }

        $n = 0;
        try { $this->app->startPools(); } catch (\Exception $e) { $n += (strpos($e->getMessage(), 'missing_cfg_xyz') !== false); }
        try { $this->app->startPools(); } catch (\Exception $e) { $n += (strpos($e->getMessage(), 'missing_cfg_xyz') !== false); }
        if ($n === 2) {
            echo "✓ missing config throws; retry stays un-poisoned\n";
        } else {
            echo "✗ pool failure handling: n=$n\n";
        }

        /* redeclaring an un-started pool is allowed */
        $ok = true;
        try { $this->app->pools(['badpool' => ['driver' => 'redis', 'component' => 'redis']]); }
        catch (\Throwable $e) { $ok = false; }
        echo $ok ? "✓ redeclare un-started pool allowed\n" : "✗ redeclare un-started pool threw\n";

        /* fail-fast: redeclared badpool (redis/redis, config absent) still blocks */
        $stillBad = false;
        try { $this->app->startPools(); } catch (\Exception $e) { $stillBad = (strpos($e->getMessage(), 'redis') !== false); }
        echo $stillBad ? "✓ fail-fast ordering preserved\n" : "✗ unexpected startPools result\n";

        /* nothing started → stop/close are no-ops returning true */
        if ($this->app->stopPoolTimers() === true && $this->app->closePools() === true) {
            echo "✓ stopPoolTimers/closePools no-op cleanly\n";
        } else {
            echo "✗ stop/close without started pools\n";
        }
        echo "\n";
    }

    public function testEnvironmentName()
    {
        echo "Testing getEnvironmentName gray mapping:\n";
        $prev = Application::getEnvironment();
        Application::setEnvironment(3);
        $gray = Application::getEnvironmentName();
        Application::setEnvironment(9);
        $unknown = @Application::getEnvironmentName();
        Application::setEnvironment($prev);
        if ($gray === 'gray' && $unknown === 'dev') {
            echo "✓ 3→gray, unknown→dev fallback kept (warns once)\n";
        } else {
            echo "✗ env mapping failed: gray=$gray unknown=$unknown\n";
        }
        Application::setRuntimeType($this->prevRuntime);
        echo "\n";
    }

    public function runAllTests()
    {
        $this->testInitSwooleBags();
        $this->testInitSwooleMissing();
        $this->testInitSwooleReinit();
        /* bootstrap 的 load() 受 workerReady 冻结约束 —— 必须先于
         * testHandleSwoole（其中会调 workerReady()）执行。 */
        $this->testBootstrapAndPools();
        $this->testHandleSwoole();
        $this->testEnvironmentName();
        echo "=== Swoole Entry Test Complete ===\n\n";
    }
}

$test = new SwooleEntryTest();
$test->runAllTests();
