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

            $queryResult = \Gene\Http::request([
                'method' => 'GET',
                'url' => 'http://127.0.0.1:' . $this->port . '/echo?existing=one#client',
                'query' => [
                    'page' => 2,
                    'tag' => ['a b', '中'],
                    'nested' => ['x' => 1],
                    'empty' => '',
                    'null' => null,
                ],
                'timeout' => 3,
            ]);
            $queryEcho = json_decode($queryResult['body'] ?? '', true);
            $expectedUri = '/echo?existing=one&page=2&tag%5B0%5D=a%20b&tag%5B1%5D=%E4%B8%AD&nested%5Bx%5D=1&empty=';
            if (($queryResult['status'] ?? 0) === 200 && ($queryEcho['uri'] ?? '') === $expectedUri) {
                echo "PASS RFC3986 query preserves existing query and fragment position\n";
            } else {
                echo "FAIL query encoding: " . var_export($queryResult, true) . "\n";
            }

            $emptyQueryResult = \Gene\Http::request([
                'url' => 'http://127.0.0.1:' . $this->port . '/echo?existing=one#client',
                'query' => [],
                'timeout' => 3,
            ]);
            $emptyQueryEcho = json_decode($emptyQueryResult['body'] ?? '', true);
            if (($emptyQueryEcho['uri'] ?? '') === '/echo?existing=one') {
                echo "PASS empty query leaves URL unchanged\n";
            } else {
                echo "FAIL empty query: " . var_export($emptyQueryResult, true) . "\n";
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

            $formResult = \Gene\Http::request([
                'method' => 'POST',
                'url' => 'http://127.0.0.1:' . $this->port . '/echo',
                'form' => ['grant_type' => 'client credentials', 'tag' => ['a b', '中'], 'empty' => ''],
                'timeout' => 3,
            ]);
            $formEcho = json_decode($formResult['body'] ?? '', true);
            $expectedForm = 'grant_type=client%20credentials&tag%5B0%5D=a%20b&tag%5B1%5D=%E4%B8%AD&empty=';
            if (($formResult['status'] ?? 0) === 200
                && ($formEcho['body'] ?? '') === $expectedForm
                && ($formEcho['content_type'] ?? '') === 'application/x-www-form-urlencoded') {
                echo "PASS RFC3986 urlencoded form body and Content-Type\n";
            } else {
                echo "FAIL form encoding: " . var_export($formResult, true) . "\n";
            }

            $customTypeResult = \Gene\Http::request([
                'method' => 'POST',
                'url' => 'http://127.0.0.1:' . $this->port . '/echo',
                'headers' => ['content-type' => 'application/vnd.gene.test'],
                'form' => ['a' => 'b c'],
                'timeout' => 3,
            ]);
            $customTypeEcho = json_decode($customTypeResult['body'] ?? '', true);
            if (($customTypeEcho['content_type'] ?? '') === 'application/vnd.gene.test'
                && ($customTypeEcho['body'] ?? '') === 'a=b%20c') {
                echo "PASS custom Content-Type is case-insensitively preserved\n";
            } else {
                echo "FAIL custom Content-Type: " . var_export($customTypeResult, true) . "\n";
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
                echo "PASS multipart files+array body compatibility\n";
            } else {
                echo "FAIL multipart body compatibility: " . var_export($r5, true) . "\n";
            }

            $multipartFormResult = \Gene\Http::request([
                'method' => 'POST',
                'url' => 'http://127.0.0.1:' . $this->port . '/echo',
                'files' => ['up' => ['tmp_name' => $tmp, 'name' => 'b.txt', 'type' => 'text/plain']],
                'form' => ['note' => 'a b', 'nested' => ['x' => '中']],
                'timeout' => 3,
            ]);
            $multipartFormEcho = json_decode($multipartFormResult['body'] ?? '', true);
            if (($multipartFormResult['status'] ?? 0) === 200
                && ($multipartFormEcho['files']['up']['name'] ?? '') === 'b.txt'
                && ($multipartFormEcho['post']['note'] ?? '') === 'a b'
                && ($multipartFormEcho['post']['nested']['x'] ?? '') === '中') {
                echo "PASS multipart files+form fields\n";
            } else {
                echo "FAIL multipart form: " . var_export($multipartFormResult, true) . "\n";
            }

            $notice = '';
            set_error_handler(function ($severity, $message) use (&$notice) {
                if ($severity === E_NOTICE) {
                    $notice = $message;
                    return true;
                }
                return false;
            });
            try {
                $noticeResult = \Gene\Http::request([
                    'url' => 'http://127.0.0.1:' . $this->port . '/echo',
                    'query' => ['ok' => 1],
                    'typo_option' => true,
                    'timeout' => 3,
                ]);
            } finally {
                restore_error_handler();
            }
            $noticeEcho = json_decode($noticeResult['body'] ?? '', true);
            if (strpos($notice, 'typo_option') !== false && ($noticeEcho['uri'] ?? '') === '/echo?ok=1') {
                echo "PASS unknown option emits E_NOTICE and is not forwarded\n";
            } else {
                echo "FAIL unknown option notice: " . var_export($notice, true) . "\n";
            }

            $conflicts = [
                ['json' => ['a' => 1], 'form' => ['b' => 2]],
                ['body' => 'raw', 'form' => ['b' => 2]],
                ['body' => ['legacy' => 1], 'form' => ['b' => 2], 'files' => ['up' => $tmp]],
            ];
            $conflictCount = 0;
            foreach ($conflicts as $conflict) {
                try {
                    \Gene\Http::request($conflict + [
                        'method' => 'POST',
                        'url' => 'http://127.0.0.1:' . $this->port . '/echo',
                    ]);
                } catch (\Throwable $e) {
                    $conflictCount++;
                }
            }
            if ($conflictCount === count($conflicts)) {
                echo "PASS json/string body/form conflicts rejected\n";
            } else {
                echo "FAIL body/form conflict count: $conflictCount\n";
            }

            $invalidValueCount = 0;
            $resource = fopen('php://memory', 'r');
            foreach ([['query' => ['bad' => new \stdClass()]], ['form' => ['bad' => $resource]]] as $invalid) {
                try {
                    \Gene\Http::request($invalid + ['url' => 'http://127.0.0.1:' . $this->port . '/echo']);
                } catch (\Throwable $e) {
                    $invalidValueCount++;
                }
            }
            fclose($resource);
            if ($invalidValueCount === 2) {
                echo "PASS query/form reject object and resource values\n";
            } else {
                echo "FAIL query/form invalid value count: $invalidValueCount\n";
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
                    'method' => 'POST',
                    'url' => $url . '?existing=one#client',
                    'query' => ['q' => 'a b', 'nested' => ['x' => '中']],
                    'form' => ['grant_type' => 'client credentials', 'tag' => ['a b', '中']],
                    'timeout' => 3,
                    'connect_timeout' => 1,
                ]);
                $echo = json_decode($r['body'] ?? '', true);
                $ok = is_array($r) && ($r['status'] ?? 0) === 200
                    && ($echo['uri'] ?? '') === '/echo?existing=one&q=a%20b&nested%5Bx%5D=%E4%B8%AD'
                    && ($echo['body'] ?? '') === 'grant_type=client%20credentials&tag%5B0%5D=a%20b&tag%5B1%5D=%E4%B8%AD'
                    && ($echo['content_type'] ?? '') === 'application/x-www-form-urlencoded';
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
            echo "PASS Swoole query/form encoding matches curl contract\n";
        } else {
            echo "FAIL Swoole branch: $err\n";
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
