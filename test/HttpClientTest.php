<?php
/**
 * Gene\Http outbound client.
 * FPM/CLI uses curl; Swoole (runtime_type>=2) uses Coroutine Http Client.
 * No environment → explicit SKIP (never fake-pass).
 */

class HttpClientTest
{
    private $port;
    private $proc;
    private $pipes = [];

    public function __construct()
    {
        echo "=== Gene\\Http Client Test Suite ===\n\n";
    }

    private function startEchoServer()
    {
        $this->port = 18080 + (getmypid() % 200);
        $echo = __DIR__ . DIRECTORY_SEPARATOR . 'fixtures' . DIRECTORY_SEPARATOR . 'http_echo.php';
        if (!is_file($echo)) {
            return false;
        }
        $cmd = [PHP_BINARY, '-n', '-S', '127.0.0.1:' . $this->port, $echo];
        $desc = [
            0 => ['pipe', 'r'],
            1 => ['file', sys_get_temp_dir() . '/gene_http_echo.out', 'a'],
            2 => ['file', sys_get_temp_dir() . '/gene_http_echo.err', 'a'],
        ];
        $this->proc = @proc_open($cmd, $desc, $this->pipes);
        if (!is_resource($this->proc)) {
            return false;
        }
        usleep(250000);
        return true;
    }

    private function stopEchoServer()
    {
        if (is_resource($this->proc)) {
            foreach ($this->pipes as $p) {
                if (is_resource($p)) {
                    fclose($p);
                }
            }
            proc_terminate($this->proc);
            proc_close($this->proc);
            $this->proc = null;
        }
    }

    public function testCurlGet()
    {
        echo "Testing Gene\\Http curl (FPM/CLI):\n";
        if (!function_exists('curl_init')) {
            echo "SKIP curl (ext-curl not loaded)\n\n";
            return;
        }
        if (!$this->startEchoServer()) {
            echo "SKIP local echo server could not start\n\n";
            return;
        }
        try {
            $url = 'http://127.0.0.1:' . $this->port . '/echo?x=1';
            $r = \Gene\Http::request([
                'method' => 'GET',
                'url' => $url,
                'timeout' => 3,
                'connect_timeout' => 1,
                'ssl_verify' => false,
            ]);
            if (is_array($r) && ($r['status'] ?? 0) === 200 && strpos($r['body'], '"method":"GET"') !== false) {
                echo "✓ GET status+body\n";
            } else {
                echo "✗ GET: " . var_export($r, true) . "\n";
            }

            $r2 = \Gene\Http::request([
                'method' => 'POST',
                'url' => $url,
                'json' => ['a' => 1],
                'timeout' => 3,
                'connect_timeout' => 1,
            ]);
            $decoded = json_decode($r2['body'] ?? '', true);
            $inner = is_array($decoded) ? json_decode($decoded['body'] ?? '', true) : null;
            if (($r2['status'] ?? 0) === 200 && is_array($inner) && ($inner['a'] ?? null) === 1) {
                echo "✓ POST json body\n";
            } else {
                echo "✗ POST json: " . var_export($r2, true) . "\n";
            }

            $chunks = [];
            $r3 = \Gene\Http::request([
                'method' => 'GET',
                'url' => $url,
                'timeout' => 3,
                'stream' => function ($c) use (&$chunks) {
                    $chunks[] = $c;
                },
            ]);
            if (($r3['status'] ?? 0) === 200 && $chunks !== []) {
                echo "✓ stream callback received bytes\n";
            } else {
                echo "✗ stream callback\n";
            }

            $r4 = \Gene\Http::request([
                'method' => 'GET',
                'url' => 'http://127.0.0.1:' . $this->port . '/echo?code=503',
                'timeout' => 3,
                'retry' => 1,
            ]);
            if (($r4['status'] ?? 0) === 503) {
                echo "✓ 5xx returned after retry budget\n";
            } else {
                echo "✗ 5xx retry: " . var_export($r4, true) . "\n";
            }

            $tmp = tempnam(sys_get_temp_dir(), 'gf');
            file_put_contents($tmp, 'hello-gene');
            $r5 = \Gene\Http::request([
                'method' => 'POST',
                'url' => 'http://127.0.0.1:' . $this->port . '/echo',
                'files' => ['up' => ['tmp_name' => $tmp, 'name' => 'a.txt', 'type' => 'text/plain']],
                'body' => ['note' => 'x'],
                'timeout' => 3,
            ]);
            $echo = json_decode($r5['body'] ?? '', true);
            if (($r5['status'] ?? 0) === 200 && ($echo['files']['up']['name'] ?? '') === 'a.txt'
                && ($echo['post']['note'] ?? '') === 'x') {
                echo "✓ multipart files+form\n";
            } else {
                echo "✗ multipart: " . var_export($r5, true) . "\n";
            }
            @unlink($tmp);
        } catch (\Throwable $e) {
            echo "✗ curl path exception: " . $e->getMessage() . "\n";
        }
        $this->stopEchoServer();
        echo "\n";
    }

    public function testSwooleBranch()
    {
        echo "Testing Gene\\Http Swoole branch:\n";
        if (!extension_loaded('swoole') || !class_exists('Swoole\\Coroutine\\Http\\Client')) {
            echo "SKIP Swoole Http (no environment)\n\n";
            return;
        }
        if (!function_exists('curl_init') && !$this->proc) {
            /* still need an echo server */
        }
        if (!$this->startEchoServer()) {
            echo "SKIP Swoole Http (echo server not started)\n\n";
            return;
        }
        $url = 'http://127.0.0.1:' . $this->port . '/echo';
        $ok = false;
        $err = '';
        \Swoole\Coroutine\run(function () use ($url, &$ok, &$err) {
            $prev = \Gene\Application::getRuntimeType();
            \Gene\Application::setRuntimeType('swoole');
            try {
                $r = \Gene\Http::request([
                    'method' => 'GET',
                    'url' => $url,
                    'timeout' => 3,
                    'connect_timeout' => 1,
                ]);
                $ok = is_array($r) && ($r['status'] ?? 0) === 200;
                if (!$ok) {
                    $err = var_export($r, true);
                }
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
            echo "✓ Swoole Coroutine Http Client GET\n";
        } else {
            echo "✗ Swoole branch: $err\n";
        }
        $this->stopEchoServer();
        echo "\n";
    }

    public function runAllTests()
    {
        $this->testCurlGet();
        $this->testSwooleBranch();
        echo "=== Gene\\Http Client Test Suite Complete ===\n";
    }
}

if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new HttpClientTest();
    $test->runAllTests();
}
