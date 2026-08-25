<?php

class RestInvokePing extends \Gene\Controller
{
    public function pong()
    {
        return [
            'name' => \Gene\Request::post('name'),
            'outer' => \Gene\Request::post('outer'),
        ];
    }

    public function boom()
    {
        throw new \Exception('inner-boom');
    }

    public function nest()
    {
        return \Gene\Invoke::local(self::class, 'nest', ['name' => 'n']);
    }
}

class RestInvokeTest
{
    public function __construct()
    {
        echo "=== Gene Rest/Invoke Test Suite ===\n\n";
    }

    public function testRequestIsolation()
    {
        echo "Testing Request isolation:\n";
        \Gene\Request::init(['q' => 1], ['outer' => 'keep', 'name' => 'outer'], [], [], [], [], null, ['X-Request-Id' => 'rid-1']);
        $inner = \Gene\Invoke::local(RestInvokePing::class, 'pong', ['name' => 'inner']);
        $after = \Gene\Request::post();
        if (($inner['name'] ?? null) === 'inner' && ($after['outer'] ?? null) === 'keep' && ($after['name'] ?? null) === 'outer') {
            echo "✓ local scopes params then restores outer Request\n";
        } else {
            echo "✗ isolation: " . var_export([$inner, $after], true) . "\n";
        }

        $threw = false;
        try {
            \Gene\Invoke::local(RestInvokePing::class, 'boom', ['name' => 'x']);
        } catch (\Throwable $e) {
            $threw = ($e->getMessage() === 'inner-boom');
        }
        $after2 = \Gene\Request::post('outer');
        if ($threw && $after2 === 'keep') {
            echo "✓ exception still restores Request\n";
        } else {
            echo "✗ exception restore\n";
        }
    }

    public function testDepthLimit()
    {
        echo "Testing Invoke depth:\n";
        $hit = false;
        try {
            \Gene\Invoke::local(RestInvokePing::class, 'nest', ['name' => '0']);
        } catch (\Throwable $e) {
            $hit = strpos($e->getMessage(), 'nesting exceeds') !== false
                || strpos($e->getMessage(), 'stack overflow') !== false;
        }
        if ($hit) {
            echo "✓ depth overflow throws\n";
        } else {
            echo "✗ depth overflow did not throw\n";
        }
        $outer = \Gene\Request::post('outer');
        if ($outer === 'keep') {
            echo "✓ overflow still leaves outer Request\n";
        } else {
            echo "✗ overflow leaked Request: " . var_export($outer, true) . "\n";
        }
    }

    public function testRestProxyAndHttp()
    {
        echo "Testing Gene\\Rest:\n";
        $port = 18080 + (getmypid() % 200);
        $echo = __DIR__ . DIRECTORY_SEPARATOR . 'fixtures' . DIRECTORY_SEPARATOR . 'http_echo.php';
        $okServer = false;
        $proc = null;
        $pipes = [];
        if (function_exists('curl_init') && is_file($echo)) {
            $cmd = [PHP_BINARY, '-n', '-S', '127.0.0.1:' . $port, $echo];
            $desc = [0 => ['pipe', 'r'], 1 => ['file', sys_get_temp_dir() . '/gene_rest_echo.out', 'a'], 2 => ['file', sys_get_temp_dir() . '/gene_rest_echo.err', 'a']];
            $proc = @proc_open($cmd, $desc, $pipes);
            $okServer = is_resource($proc);
            if ($okServer) {
                usleep(250000);
            }
        }

        $rest = new \Gene\Rest([
            'timeout' => 3,
            'pass_request_id' => true,
            'services' => [
                'demo' => [
                    'base_url' => 'http://127.0.0.1:' . $port,
                    'local' => '',
                ],
                'api' => [
                    'base_url' => 'http://127.0.0.1:' . $port,
                    'local' => '',
                ],
            ],
        ]);
        $a = $rest->use('demo');
        $b = $rest->use('api');
        if ($a !== $rest && $b !== $a && $a instanceof \Gene\Rest) {
            echo "✓ use() returns new proxy\n";
        } else {
            echo "✗ use() proxy identity\n";
        }

        $local = $a->call(RestInvokePing::class, 'pong', ['name' => 'via-call']);
        if (($local['name'] ?? null) === 'via-call') {
            echo "✓ Rest::call local branch\n";
        } else {
            echo "✗ Rest::call local: " . var_export($local, true) . "\n";
        }

        if (!$okServer) {
            echo "SKIP Rest http/decode (no curl/echo)\n";
        } else {
            try {
                $r = $a->http('GET', '/echo?x=1', ['decode' => true]);
                if (($r['status'] ?? 0) === 200 && is_array($r['body'] ?? null) && ($r['body']['method'] ?? '') === 'GET') {
                    echo "✓ Rest::http decode\n";
                } else {
                    echo "✗ Rest::http decode: " . var_export($r, true) . "\n";
                }
                $bad = false;
                try {
                    $a->http('GET', '/echo', ['decode' => true, 'headers' => []]);
                    /* echo returns json so this should succeed; force bad decode via empty */
                } catch (\Throwable $e) {
                    $bad = true;
                }
                try {
                    $a->http('GET', '/echo?invalid=1', ['decode' => true]);
                    echo "✗ decode should throw on invalid JSON\n";
                } catch (\Throwable $e) {
                    echo "✓ decode throws on invalid JSON\n";
                }
                unset($bad);
            } catch (\Throwable $e) {
                echo "✗ Rest http: " . $e->getMessage() . "\n";
            }
        }
        if (is_resource($proc)) {
            foreach ($pipes as $p) {
                if (is_resource($p)) {
                    fclose($p);
                }
            }
            proc_terminate($proc);
            proc_close($proc);
        }
    }

    public function testSwooleCleanup()
    {
        echo "Testing Swoole cleanup after invoke:\n";
        if (!extension_loaded('swoole')) {
            echo "SKIP Swoole invoke cleanup (no environment)\n\n";
            return;
        }
        $ok = false;
        $err = '';
        \Swoole\Coroutine\run(function () use (&$ok, &$err) {
            $prev = \Gene\Application::getRuntimeType();
            \Gene\Application::setRuntimeType('swoole');
            try {
                \Gene\Request::init([], ['outer' => 'co'], [], [], [], []);
                \Gene\Invoke::local(RestInvokePing::class, 'pong', ['name' => 'co']);
                \Gene\Application::cleanup();
                $ok = \Gene\Request::post('outer') === null || \Gene\Request::post('outer') === '';
            } catch (\Throwable $e) {
                $err = $e->getMessage();
            } finally {
                \Gene\Application::setRuntimeType($prev);
                if (method_exists(\Gene\Application::class, 'cleanup')) {
                    \Gene\Application::cleanup();
                }
            }
        });
        if ($ok) {
            echo "✓ cleanup() clears Request after invoke\n";
        } else {
            echo ($err ? "✗ Swoole cleanup: $err\n" : "✗ Swoole cleanup Request not empty\n");
        }
        echo "\n";
    }

    public function runAllTests()
    {
        $this->testRequestIsolation();
        $this->testDepthLimit();
        $this->testRestProxyAndHttp();
        $this->testSwooleCleanup();
        echo "=== Gene Rest/Invoke Test Suite Complete ===\n";
    }
}

if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'] ?? '')) {
    $t = new RestInvokeTest();
    $t->runAllTests();
}
