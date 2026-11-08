<?php
/**
 * Performance V3 leak probe (plan/Performance-tuning-V3.closed.md).
 *
 *   php audit/repro/perfv3_leak_probe.php
 *
 * 1. Request arena (memory_get_usage, CLI = FPM path): Validate direct PCRE,
 *    business zero copy borrow/unset, full security policy chain + dispatch.
 * 2. Process-persistent table: distinct processCached keys with
 *    cache_max_items=0 vs >0 (items + private bytes).
 * 3. php-cgi -T worker: N requests through route programs (compiled/eval),
 *    zero copy and the policy chain; private bytes must plateau.
 * Does not cover Swoole coroutines / single-flight (see tools/acceptance).
 */
if (getenv('PV3_CHILD') === 'persist') {
    $n = (int)getenv('PV3_N');
    $c = new Gene\Cache\Cache(['sign' => 'lk:']);
    $h = new class { function p($k) { return ['k' => $k, 'pad' => str_repeat('x', 256)]; } };
    $mark = function () { return private_bytes(); };
    $m0 = $mark();
    for ($i = 0; $i < $n; $i++) {
        $c->processCached([$h, 'p'], ["k$i"]);
    }
    $s = (new Gene\Memory())->stats();
    echo json_encode(['items' => $s['business_cache_items'], 'grow_kb' => intdiv($mark() - $m0, 1024)]);
    exit;
}

function private_bytes() {
    if (DIRECTORY_SEPARATOR === '\\') {
        $o = shell_exec('powershell -NoProfile -Command "(Get-Process -Id ' . getmypid() . ').PrivateMemorySize64"');
        return (int)trim((string)$o);
    }
    if (preg_match('/VmRSS:\s+(\d+)/', (string)@file_get_contents('/proc/self/status'), $m)) {
        return (int)$m[1] * 1024;
    }
    return 0;
}

function probe($label, $iters, $fn) {
    for ($i = 0; $i < 200; $i++) $fn($i);
    gc_collect_cycles();
    $before = memory_get_usage();
    for ($i = 0; $i < $iters; $i++) $fn($i);
    gc_collect_cycles();
    $delta = memory_get_usage() - $before;
    printf("%-48s %+d B (%.3f B/call)\n", $label, $delta, $delta / $iters);
    return $delta <= 1024;
}

function child($cmd, array $env = [], $cwd = null) {
    $p = proc_open($cmd, [1 => ['pipe', 'w'], 2 => ['redirect', 1]], $pipes, $cwd, array_merge(getenv(), $env));
    $out = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    proc_close($p);
    return $out;
}

$ok = true;
$php = escapeshellarg(PHP_BINARY);

echo "== 1. request arena ==\n";
$ok &= probe('Validate mobile/date/match (direct PCRE)', 20000, function ($i) {
    $v = new Gene\Validate();
    $v->init(['m' => '1380013' . sprintf('%04d', $i % 10000), 'd' => '2026-11-08', 's' => "abc$i"]);
    $v->name('m')->rule_mobile();
    $v->name('d')->rule_date();
    $v->name('s')->rule_match('/^abc\d+$/');
});

$c = new Gene\Cache\Cache(['sign' => 'pv3leak:']);
$h = new class { function p($k) { return ['k' => $k, 'list' => range(1, 8), 's' => str_repeat('y', 64)]; } };
$ok &= probe('processCached hit/unset/refill (zero_copy=' . ini_get('gene.memory_business_zero_copy') . ')', 20000, function ($i) use ($c, $h) {
    $k = 'k' . ($i % 50);
    $a = $c->processCached([$h, 'p'], [$k]);
    $b = $c->processCached([$h, 'p'], [$k]);
    $b['list'][] = 9;
    if ($i % 7 === 0) $c->unsetProcessCached([$h, 'p'], [$k]);
});

$app = Gene\Application::getInstance();
(new Gene\Router())->clear()
    ->get('/admin/user/lists', function () { echo 'ok'; })
    ->post('/admin/user/save', function () { echo 'saved'; })
    ->get('/c/:id', function ($p) { echo 'c', $p['id'] ?? ''; })
    ->error(404, function () { echo '404'; });
$app->trustProxies(['proxies' => ['127.0.0.1/32'], 'proto' => true])
    ->requestId(['header' => 'X-Request-Id', 'bytes' => 8, 'trust' => true, 'max_length' => 128])
    ->webscan(['white_dir' => ['cms']])
    ->secureHeaders()
    ->restGuard(['internal' => true])
    ->csrf(['allow_same_site' => true, 'except' => ['/api/*']])
    ->readonly(['/admin' => ['user' => ['lists'], '*' => []]]);
$reqs = [
    ['get', '/admin/user/lists', ['q' => 'a b'], []],
    ['get', '/c/42', ['x' => "1' or 1=1"], []],
    ['post', '/admin/user/save', [], ['name' => '<script>alert(1)</script>']],
    ['get', '/admin/user/del', [], []],
    ['get', '/nope', [], []],
];
$ok &= probe('policy chain + webscan + dispatch + cleanup', 20000, function ($i) use ($app, $reqs) {
    [$m, $u, $g, $p] = $reqs[$i % count($reqs)];
    Gene\Request::init($g, $p, ['sid' => 'z'], [
        'REQUEST_METHOD' => strtoupper($m), 'REQUEST_URI' => $u, 'REMOTE_ADDR' => '10.0.0.' . ($i % 200),
        'HTTP_ORIGIN' => $i % 3 ? 'http://evil.example' : null, 'HTTP_HOST' => 'localhost',
    ]);
    ob_start();
    try { @$app->run($m, $u); } catch (Throwable $e) {}
    ob_end_clean();
    Gene\Application::cleanup();
});

echo "\n== 2. persistent business table (100k distinct keys) ==\n";
foreach ([0, 10000] as $max) {
    $out = child("$php -d gene.cache_max_items=$max -d gene.cache_reserve=" . ($max ? 12800 : 4096) . ' ' . escapeshellarg(__FILE__),
        ['PV3_CHILD' => 'persist', 'PV3_N' => '100000']);
    $r = json_decode(trim($out), true) ?: [];
    printf("cache_max_items=%-6d items=%-7s private +%s KB\n", $max, $r['items'] ?? '?', $r['grow_kb'] ?? '?');
    if ($max && ($r['items'] ?? PHP_INT_MAX) > $max) $ok = false;
}

echo "\n== 3. php-cgi -T worker ==\n";
$cgi = dirname(PHP_BINARY) . DIRECTORY_SEPARATOR . (DIRECTORY_SEPARATOR === '\\' ? 'php-cgi.exe' : 'php-cgi');
if (!is_file($cgi)) {
    echo "SKIP (php-cgi not found)\n";
} else {
    $dir = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'pv3leak';
    @mkdir($dir . '/config', 0777, true);
    @mkdir($dir . '/app/Cache', 0777, true);
    file_put_contents("$dir/config/router.php", '<?php
(new \Gene\Router())->clear()
    ->hook("before", function () { echo "[b]"; })
    ->get("/c/:id", function ($p) { echo "c", $p["id"] ?? ""; })
    ->error(404, function () { echo "404"; });');
    file_put_contents("$dir/index.php", '<?php
if (getenv("PV3_MODE") !== "plain") {
$app = \Gene\Application::getInstance();
$app->autoload(__DIR__ . "/app")->load("router.php", __DIR__ . "/config/");
$app->trustProxies(["proxies" => ["127.0.0.1/32"]])->requestId(["bytes" => 8])
    ->webscan(["white_dir" => ["cms"]])->secureHeaders()->csrf(["allow_same_site" => true]);
$c = new \Gene\Cache\Cache(["sign" => "w:"]);
$c->processCached("strrev", ["k" . ($GLOBALS["_GET"]["i"] ?? 0)]);
$app->run();
} else {
echo "plain";
}
$n = (int)getenv("PV3_TOTAL");
$i = (int)file_get_contents(__DIR__ . "/cnt") + 1;
file_put_contents(__DIR__ . "/cnt", $i);
if ($i === 2000 || $i === intdiv($n, 2) || $i === $n) {
    $o = DIRECTORY_SEPARATOR === "\\\\"
        ? (int)trim(shell_exec("powershell -NoProfile -Command \"(Get-Process -Id " . getmypid() . ").PrivateMemorySize64\""))
        : 0;
    echo "\nMARK $i ", memory_get_usage(), " ", $o, "\n";
}');
    $total = 20000;
    $base = null;
    $modes = [
        'plain (no gene)'    => ['plain', ''],
        'eval'               => ['', '-d gene.route_eval_compile=0'],
        'compiled+zero_copy' => ['', '-d gene.route_eval_compile=1 -d opcache.enable=1 -d gene.memory_business_zero_copy=1'],
    ];
    foreach ($modes as $name => [$mode, $ini]) {
        file_put_contents("$dir/cnt", '0');
        $out = child(escapeshellarg($cgi) . " -q $ini -T $total index.php", [
            'REQUEST_METHOD' => 'GET', 'REQUEST_URI' => '/c/7?x=1%27%20or%201=1', 'QUERY_STRING' => 'x=1%27%20or%201=1',
            'REDIRECT_STATUS' => '200', 'SCRIPT_FILENAME' => "$dir/index.php", 'PV3_TOTAL' => (string)$total,
            'PV3_MODE' => $mode,
        ], $dir);
        preg_match_all('/MARK (\d+) (\d+) (\d+)/', $out, $m, PREG_SET_ORDER);
        if (count($m) === 3) {
            $dHeap = $m[2][2] - $m[0][2];
            $d1 = intdiv($m[1][3] - $m[0][3], 1024);
            $d2 = intdiv($m[2][3] - $m[1][3], 1024);
            printf("%-20s heap 2k->%d %+d B, private 2k->%d %+d KB, ->%d %+d KB\n",
                $name, $total, $dHeap, $m[1][1], $d1, $total, $d2);
            if ($base === null) {
                $base = $d1 + $d2;
            } elseif ($dHeap > 1024 || ($d1 + $d2) - $base > 2048) {
                $ok = false;
            }
        } else {
            echo "$name: no marks\n" . substr($out, 0, 600) . "\n";
            $ok = false;
        }
    }
}

echo $ok ? "\nPERFV3 LEAK PROBE OK\n" : "\nPERFV3 LEAK PROBE FAILED\n";
exit($ok ? 0 : 1);
