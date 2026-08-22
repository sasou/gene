<?php
/**
 * [GENE_FEATURE:2026-08-22] Request-bag / Json / Crypto / Memory lock leak probe.
 * 10k iterations; memory_get_usage(true) must not grow (delta <= 0 after warmup).
 */
function probe($label, $iters, $fn) {
    $fn();
    gc_collect_cycles();
    $before = memory_get_usage(true);
    for ($i = 0; $i < $iters; $i++) {
        $fn();
    }
    gc_collect_cycles();
    $after = memory_get_usage(true);
    $delta = $after - $before;
    printf("%-46s %+d B (%.2f B/call)\n", $label, $delta, $delta / $iters);
    return $delta <= 0;
}

$ok = true;
$ok &= probe('Context set/get + cleanup', 10000, function () {
    \Gene\Context::set('request_id', bin2hex(random_bytes(8)));
    \Gene\Context::set('blob', str_repeat('x', 64));
    \Gene\Context::get('request_id');
    \Gene\Context::all();
    if (method_exists(\Gene\Application::class, 'cleanup')) {
        \Gene\Application::cleanup();
    } elseif (method_exists(\Gene\Application::class, 'clearState')) {
        \Gene\Application::clearState();
    }
});

$ok &= probe('Json encode/decode', 10000, function () {
    $s = \Gene\Json::encode(['n' => 1, 's' => '你好']);
    \Gene\Json::decode($s);
});

$key = str_repeat('B', 32);
$ok &= probe('Crypto hmac + gcm', 5000, function () use ($key) {
    $t = \Gene\Crypto::hmacToken(['u' => 1], 'sec', 10);
    \Gene\Crypto::hmacVerify($t, 'sec');
    $c = \Gene\Crypto::encrypt('p', $key);
    \Gene\Crypto::decrypt($c, $key);
    \Gene\Crypto::randomId('id', 8);
});

$m = new \Gene\Memory('leak');
$ok &= probe('Memory rateLimit + lock/unlock', 5000, function () use ($m) {
    $k = 'rl';
    $m->rateLimit($k, 100000, 60);
    $t = $m->lock('lk', 5);
    if (is_string($t)) {
        $m->unlock('lk', $t);
    }
});

$ok &= probe('Request::json + SSE write', 5000, function () {
    \Gene\Request::init([], [], [], [], null, [], null, [], '{"a":1}');
    \Gene\Request::json();
    ob_start();
    \Gene\Response::write('x');
    \Gene\Response::sseEvent('e', 'd');
    ob_end_clean();
    if (method_exists(\Gene\Application::class, 'cleanup')) {
        \Gene\Application::cleanup();
    }
});

echo $ok ? "LEAK PROBE OK\n" : "LEAK PROBE FAILED\n";
exit($ok ? 0 : 1);
