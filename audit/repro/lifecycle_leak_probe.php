<?php
/**
 * [GENE_FEATURE:2026-08-22] Request-bag / Json / Crypto / Memory lock leak probe.
 * 10k iterations; memory_get_usage(true) must not grow (delta <= 0 after warmup).
 * CLI = FPM path (request arena). Does not prove Swoole coroutine isolation.
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
    \Gene\Request::init(['g' => 1], ['p' => 2], [], ['CONTENT_TYPE' => 'application/json'], null, [], null, [], '{"a":1}');
    \Gene\Request::json();
    \Gene\Request::input();
    ob_start();
    \Gene\Response::write('x');
    \Gene\Response::sseEvent('e', 'd');
    ob_end_clean();
    if (method_exists(\Gene\Application::class, 'cleanup')) {
        \Gene\Application::cleanup();
    }
});

$runtime = \Gene\Application::getRuntimeType();
\Gene\Application::setRuntimeType('swoole');
$ok &= probe('Swoole request input + cleanup', 10000, function () {
    static $i = 0;
    $i++;
    \Gene\Request::init(['i' => $i], [], [], ['CONTENT_TYPE' => 'application/json'], null, [], null, [], '{"json":true}');
    $input = \Gene\Request::input();
    if (($input['i'] ?? null) !== $i) throw new \RuntimeException('request context bleed');
    \Gene\Application::cleanup();
});
\Gene\Application::setRuntimeType($runtime);

if (extension_loaded('swoole') && class_exists('Swoole\\Coroutine')) {
    $swooleOk = true;
    \Swoole\Coroutine\run(function () use (&$swooleOk) {
        $runtime = \Gene\Application::getRuntimeType();
        \Gene\Application::setRuntimeType('swoole');
        $swooleOk = probe('Swoole coroutine input + cleanup', 10000, function () {
            static $i = 0;
            $i++;
            \Gene\Request::init(['i' => $i], [], [], ['CONTENT_TYPE' => 'application/json'], null, [], null, [], '{"json":true}');
            $input = \Gene\Request::input();
            if (($input['i'] ?? null) !== $i) throw new \RuntimeException('coroutine context bleed');
            \Gene\Application::cleanup();
        });
        \Gene\Application::setRuntimeType($runtime);
    });
    $ok &= $swooleOk;
} else {
    echo str_pad('Swoole coroutine input + cleanup', 46) . " SKIP (no Swoole)\n";
}

if (function_exists('curl_init')) {
    $ok &= probe('Http curl handle + cleanup', 50, function () {
        try {
            \Gene\Http::request([
                'url' => 'http://127.0.0.1:1/',
                'timeout' => 1,
                'connect_timeout' => 1,
            ]);
        } catch (\Throwable $e) {
        }
        if (method_exists(\Gene\Application::class, 'cleanup')) {
            \Gene\Application::cleanup();
        }
    });
} else {
    echo str_pad('Http curl handle + cleanup', 46) . " SKIP (no curl)\n";
}

echo $ok ? "LEAK PROBE OK\n" : "LEAK PROBE FAILED\n";
exit($ok ? 0 : 1);
