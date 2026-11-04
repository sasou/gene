<?php
/**
 * Performance V3 FPM-equivalent acceptance (Windows-capable).
 *
 * Starts `php-cgi -b 127.0.0.1:<port>` -- one persistent FastCGI worker with
 * the same request lifecycle as an FPM child (RINIT/RSHUTDOWN per request,
 * module globals and the business table survive) -- and talks FastCGI to it
 * directly, so no web server is needed. For each INI combination of
 * gene.route_eval_compile x gene.memory_business_zero_copy (opcache on) it:
 *   - serves the PerfV3 fixture (closure routes, closure hooks, closure 404,
 *     string route) and the demo app (GENE_DEMO_LOCAL=1),
 *   - checks every response body against the expected one and that all
 *     combinations produce the same digest,
 *   - reports route_eval / compiled counters and requests per second.
 *
 * Usage:
 *   php tools/acceptance/fcgi_v3_verify.php [--cgi=path/php-cgi.exe]
 *       [--args="-n -d extension_dir=... -d extension=..."] [--requests=2000]
 * --args defaults to GENE_TEST_PHP_ARGS. Exit code 0 = ALL-PASS.
 */

$opt = getopt('', ['cgi::', 'args::', 'requests::', 'port::']);
$cgi = $opt['cgi'] ?? dirname(PHP_BINARY) . DIRECTORY_SEPARATOR . (DIRECTORY_SEPARATOR === '\\' ? 'php-cgi.exe' : 'php-cgi');
$args = $opt['args'] ?? (getenv('GENE_TEST_PHP_ARGS') ?: '');
$requests = max(10, (int)($opt['requests'] ?? 2000));
$port = (int)($opt['port'] ?? 19123);
$root = dirname(__DIR__, 2);
$fixture = $root . '/test/fixtures/perfv3';
$demo = $root . '/demo/public';

function fcgi_params(array $p) {
    $out = '';
    foreach ($p as $k => $v) {
        foreach ([strlen($k), strlen($v)] as $n) {
            $out .= $n < 128 ? chr($n) : pack('N', $n | 0x80000000);
        }
        $out .= $k . $v;
    }
    return $out;
}

function fcgi_record($type, $content, $id = 1) {
    $len = strlen($content);
    $pad = (8 - $len % 8) % 8;
    return pack('CCnnCC', 1, $type, $id, $len, $pad, 0) . $content . str_repeat("\0", $pad);
}

function fcgi_request($port, array $params) {
    $s = @stream_socket_client("tcp://127.0.0.1:$port", $errno, $errstr, 5);
    if (!$s) {
        return null;
    }
    $req = fcgi_record(1, pack('nC', 1, 0) . str_repeat("\0", 5));
    $body = fcgi_params($params);
    foreach (str_split($body, 65535) as $chunk) {
        $req .= fcgi_record(4, $chunk);
    }
    $req .= fcgi_record(4, '') . fcgi_record(5, '');
    fwrite($s, $req);
    $stdout = '';
    while (!feof($s)) {
        $h = fread($s, 8);
        if (strlen($h) < 8) break;
        $r = unpack('Cver/Ctype/nid/nlen/Cpad/Cres', $h);
        $c = '';
        $need = $r['len'] + $r['pad'];
        while (strlen($c) < $need && !feof($s)) {
            $c .= fread($s, $need - strlen($c));
        }
        $c = substr($c, 0, $r['len']);
        if ($r['type'] === 6) $stdout .= $c;
        if ($r['type'] === 3) break;
    }
    fclose($s);
    $pos = strpos($stdout, "\r\n\r\n");
    return $pos === false ? $stdout : substr($stdout, $pos + 4);
}

function start_worker($cgi, $args, $ini, $port, array $env) {
    $cmd = escapeshellarg($cgi) . " $args $ini -b 127.0.0.1:$port";
    $env = array_merge(getenv(), ['PHP_FCGI_MAX_REQUESTS' => '1000000'], $env);
    $proc = proc_open($cmd, [0 => ['pipe', 'r'], 1 => ['file', sys_get_temp_dir() . '/fcgi_v3.log', 'a'], 2 => ['file', sys_get_temp_dir() . '/fcgi_v3.log', 'a']], $pipes, null, $env);
    for ($i = 0; $i < 50; $i++) {
        usleep(100000);
        $s = @stream_socket_client("tcp://127.0.0.1:$port", $e1, $e2, 1);
        if ($s) { fclose($s); return $proc; }
    }
    proc_terminate($proc);
    return null;
}

function stop_worker($proc) {
    if (!$proc) return;
    $st = proc_get_status($proc);
    if (DIRECTORY_SEPARATOR === '\\' && !empty($st['pid'])) {
        exec('taskkill /F /T /PID ' . (int)$st['pid'] . ' 2>NUL');
    } else {
        proc_terminate($proc);
    }
    proc_close($proc);
}

function req_params($script, $uri) {
    return ['GATEWAY_INTERFACE' => 'FastCGI/1.0', 'REQUEST_METHOD' => 'GET', 'REQUEST_URI' => $uri,
        'SCRIPT_FILENAME' => $script, 'SCRIPT_NAME' => '/index.php', 'DOCUMENT_ROOT' => dirname($script),
        'QUERY_STRING' => '', 'SERVER_NAME' => 'localhost', 'SERVER_PORT' => '80', 'SERVER_PROTOCOL' => 'HTTP/1.1',
        'REMOTE_ADDR' => '127.0.0.1', 'HTTP_HOST' => 'localhost', 'REDIRECT_STATUS' => '200'];
}

function split_stats($body) {
    $p = strrpos($body, "\nSTATS ");
    if ($p === false) return [trim($body), null];
    return [trim(substr($body, 0, $p)), json_decode(trim(substr($body, $p + 7)), true)];
}

function rrmdir($dir) {
    if (!is_dir($dir)) return;
    foreach (scandir($dir) as $f) {
        if ($f === '.' || $f === '..') continue;
        $p = "$dir/$f";
        is_dir($p) ? rrmdir($p) : @unlink($p);
    }
    @rmdir($dir);
}

if (!is_file($cgi)) {
    fwrite(STDERR, "php-cgi not found: $cgi\n");
    exit(2);
}

$fixtureCases = ['/c/7' => '[before]closure:7[after]', '/g' => '[before][guard]guarded[after]',
    '/s' => 'string-route', '/nope' => 'not-found'];
$demoCases = ['/healthz', '/no-such-route-xyz', '/'];
$normalize = function ($s) { return preg_replace('/\d+/', '#', $s); };

$opcache = '-d zend_extension=' . (DIRECTORY_SEPARATOR === '\\' ? 'php_opcache.dll' : 'opcache') . ' -d opcache.enable=1';
$matrix = [];
foreach ([0, 1] as $compile) {
    foreach ([0, 1] as $zc) {
        $matrix["compile=$compile,zero_copy=$zc"] = "$opcache -d gene.route_eval_compile=$compile -d gene.memory_business_zero_copy=$zc";
    }
}

$fail = 0;
$digests = [];
$perf = [];
echo "php-cgi: $cgi\nrequests per closure route: $requests\n\n";
foreach ($matrix as $label => $ini) {
    rrmdir("$fixture/app/Cache");
    $proc = start_worker($cgi, $args, $ini, $port, ['GENE_DEMO_LOCAL' => '1']);
    if (!$proc) {
        echo "✗ [$label] worker did not start\n";
        $fail++;
        continue;
    }
    $digest = '';
    foreach ($fixtureCases as $uri => $expect) {
        $bad = 0;
        $last = null;
        for ($i = 0; $i < 5; $i++) {
            [$body, $stats] = split_stats((string)fcgi_request($port, req_params("$fixture/index.php", $uri)));
            if ($body !== $expect) $bad++;
            $last = $stats;
            $digest .= $body;
        }
        echo ($bad ? '✗' : '✓') . " [$label] fixture $uri x5" . ($bad ? " ($bad wrong)" : '') . "\n";
        $fail += $bad ? 1 : 0;
    }
    foreach ($demoCases as $uri) {
        $bodies = [];
        for ($i = 0; $i < 3; $i++) {
            $bodies[] = $normalize((string)fcgi_request($port, req_params("$demo/index.php", $uri)));
        }
        $ok = $bodies[0] !== '' && count(array_unique($bodies)) === 1;
        echo ($ok ? '✓' : '✗') . " [$label] demo $uri x3 stable\n";
        $fail += $ok ? 0 : 1;
        $digest .= $bodies[0];
    }

    /* Server-side Application::run() time (hrtime inside the fixture): the
     * per-request TCP connect on Windows would otherwise dominate. */
    $bad = 0;
    $runs = [];
    foreach (['/c/7', '/g'] as $uri) {
        for ($i = 0; $i < $requests; $i++) {
            [$body, $st] = split_stats((string)fcgi_request($port, req_params("$fixture/index.php", $uri)));
            if ($body !== $fixtureCases[$uri]) $bad++;
            if (isset($st['run_ns'])) $runs[] = $st['run_ns'];
            $stats = $st;
        }
    }
    sort($runs);
    $median = $runs ? $runs[intdiv(count($runs), 2)] / 1000 : 0;
    $p90 = $runs ? $runs[intdiv(count($runs) * 9, 10)] / 1000 : 0;
    unset($stats['run_ns']);
    echo ($bad ? '✗' : '✓') . sprintf(" [%s] %d closure-route requests, run() median %.1fus p90 %.1fus, stats=%s\n",
        $label, count($runs), $median, $p90, json_encode($stats));
    $fail += $bad ? 1 : 0;
    $perf[$label] = $median;
    $digests[$label] = md5($digest);
    stop_worker($proc);
}
rrmdir("$fixture/app/Cache");

$same = count(array_unique($digests)) === 1;
echo "\n" . ($same ? '✓' : '✗') . " RESULT-DIGEST identical across matrix: " . json_encode($digests) . "\n";
$fail += $same ? 0 : 1;
if (isset($perf['compile=0,zero_copy=0'], $perf['compile=1,zero_copy=0'])) {
    printf("closure route run() median compile=1 vs 0: %+.1f%%\n", ($perf['compile=1,zero_copy=0'] / $perf['compile=0,zero_copy=0'] - 1) * 100);
}
echo $fail ? "\nFAIL ($fail)\n" : "\nALL-PASS\n";
exit($fail ? 1 : 0);
