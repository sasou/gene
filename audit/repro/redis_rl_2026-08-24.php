<?php
/**
 * Live verification of Gene\Cache\Redis::rateLimit against a real redis
 * server (config lifted from gene_web config/config.ini.dev.php).
 * Run with: php -n -d extension_dir=... -d extension=php_redis.dll
 *   -d extension=php_gene.dll redis_rl_2026-08-24.php
 */
$cfg = [
    'persistent' => false,
    'host' => '192.168.5.102',
    'port' => 6379,
    'timeout' => 3,
    'ttl' => 0,
    'password' => 'rds2024',
    'serializer' => 0,
];

echo "=== 1) Normal path: real server, max=2 within window ===\n";
$r = new \Gene\Cache\Redis($cfg);
$k = 'gene:rl:test:' . bin2hex(random_bytes(4));
$ok1 = $r->rateLimit($k, 2, 30);
$ok2 = $r->rateLimit($k, 2, 30);
$ok3 = $r->rateLimit($k, 2, 30);
var_dump($ok1, $ok2, $ok3);
if ($ok1 === true && $ok2 === true && $ok3 === false) {
    echo "PASS: allowed, allowed, blocked(false) — real over-limit distinguishable from error\n";
} else {
    echo "FAIL: unexpected sequence\n";
}
$r->__call('del', [$k]);
$r->free();
echo "\n";

echo "=== 2) Error path: unreachable host/port ===\n";
$badCfg = $cfg;
$badCfg['host'] = '192.168.5.102';
$badCfg['port'] = 6390; // nothing listening here
$badCfg['timeout'] = 1;
try {
    $r2 = new \Gene\Cache\Redis($badCfg);
    $res = $r2->rateLimit('gene:rl:unreachable:' . bin2hex(random_bytes(4)), 2, 30);
    var_dump($res);
    if ($res === null) {
        echo "PASS: connection failure surfaced as null, not a false 'blocked'\n";
    } else {
        echo "INFO: got " . var_export($res, true) . " (construct-time exception path may have been taken instead — see below)\n";
    }
    $r2->free();
} catch (\Throwable $e) {
    echo "Construct/eval threw instead of returning null: " . get_class($e) . ": " . $e->getMessage() . "\n";
}
echo "\n";

echo "=== 3) Wrong password (auth failure) ===\n";
$authCfg = $cfg;
$authCfg['password'] = 'definitely-wrong-password';
try {
    $r3 = new \Gene\Cache\Redis($authCfg);
    $res = $r3->rateLimit('gene:rl:badauth:' . bin2hex(random_bytes(4)), 2, 30);
    var_dump($res);
    if ($res === null) {
        echo "PASS: auth failure surfaced as null\n";
    } else {
        echo "INFO: got " . var_export($res, true) . "\n";
    }
    $r3->free();
} catch (\Throwable $e) {
    echo "Construct/eval threw instead of returning null: " . get_class($e) . ": " . $e->getMessage() . "\n";
}
