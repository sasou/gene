<?php
/**
 * [GENE_FEATURE:2026-08-22] One-shot API smoke for lifecycle primitives.
 * php audit/repro/lifecycle_apis.php
 */
$fail = 0;
function expect($cond, $msg) {
    global $fail;
    echo ($cond ? "OK  " : "FAIL") . " $msg\n";
    if (!$cond) $fail++;
}

\Gene\Context::set('request_id', 'repro-1');
expect(\Gene\Context::get('request_id') === 'repro-1', 'Context::get');
expect(\Gene\Context::all()['request_id'] === 'repro-1', 'Context::all');

$s = \Gene\Json::encode(['a' => 1, 'p' => '/x']);
expect(strpos($s, '/x') !== false, 'Json::encode unescaped slashes');
expect(\Gene\Json::decode($s)['a'] === 1, 'Json::decode');
try {
    \Gene\Json::decode('not-json');
    expect(false, 'Json::decode invalid throws');
} catch (\Throwable $e) {
    expect(true, 'Json::decode invalid throws');
}

\Gene\Request::init([], [], [], [], null, [], null, [], '{"x":2}');
expect(\Gene\Request::json()['x'] === 2, 'Request::json');

ob_start();
\Gene\Response::sseStart();
\Gene\Response::sseEvent('e', 'd');
\Gene\Response::write('w');
$out = ob_get_clean();
expect(strpos($out, "event: e\n") !== false && strpos($out, 'w') !== false, 'SSE write');

$key = str_repeat('A', 32);
$c = \Gene\Crypto::encrypt('z', $key);
expect(\Gene\Crypto::decrypt($c, $key) === 'z', 'Crypto GCM');
$tok = \Gene\Crypto::hmacToken(['k' => 1], 'sec', 30);
expect(\Gene\Crypto::hmacVerify($tok, 'sec')['k'] === 1, 'Crypto hmac');
expect(strlen(\Gene\Crypto::randomId('', 8)) === 16, 'Crypto randomId');

$m = new \Gene\Memory('repro');
$rk = 'rl_' . bin2hex(random_bytes(3));
expect($m->rateLimit($rk, 1, 30) === true, 'Memory rateLimit first');
expect($m->rateLimit($rk, 1, 30) === false, 'Memory rateLimit over');
$lk = 'lk_' . bin2hex(random_bytes(3));
$t = $m->lock($lk, 5);
expect(is_string($t), 'Memory lock');
expect($m->lock($lk, 5) === false, 'Memory lock NX');
expect($m->unlock($lk, $t) === true, 'Memory unlock');

echo $fail ? "LIFECYCLE APIS FAILED ($fail)\n" : "LIFECYCLE APIS OK\n";
exit($fail ? 1 : 0);
