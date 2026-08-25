<?php
/**
 * End-to-end check of the exact isLoginRateLimited() logic against a real
 * redis server: 20 allowed in 60s, then blocked, then fail-open when redis
 * is unreachable.
 */
/**
 * Mirrors the real controller shape: $this->redis is lazily constructed by
 * Gene's DI on first property access, i.e. the construct call happens
 * *inside* the same expression as rateLimit(), same as `$this->redis->
 * rateLimit(...)` in Index.php. So here we build the connection lazily
 * inside the try block too, not beforehand, to match that scope exactly.
 */
function isLoginRateLimited(array $cfg, string $ip): bool
{
    $max = 20;
    $windowSec = 60;
    $key = 'geneweb:login:test:' . $ip;
    try {
        $redis = new \Gene\Cache\Redis($cfg); // stands in for $this->redis lazy DI construct
        $allowed = $redis->rateLimit($key, $max, $windowSec);
    } catch (\Throwable $e) {
        echo "  caught: " . get_class($e) . ": " . $e->getMessage() . "\n";
        return false;
    }
    if ($allowed === null) {
        echo "  null (indeterminate) -> fail-open\n";
        return false;
    }
    return !$allowed;
}

$cfg = [
    'persistent' => false, 'host' => '192.168.5.102', 'port' => 6379,
    'timeout' => 3, 'ttl' => 0, 'password' => 'rds2024', 'serializer' => 0,
];
$ip = '203.0.113.' . random_int(1, 254);

echo "=== 20 attempts should all be allowed, 21st blocked ===\n";
$blockedAt = null;
for ($i = 1; $i <= 21; $i++) {
    $limited = isLoginRateLimited($cfg, $ip);
    if ($limited) { $blockedAt = $i; break; }
}
echo "Blocked at attempt: " . var_export($blockedAt, true) . "\n";
echo ($blockedAt === 21 ? "PASS" : "FAIL") . ": exactly 20 allowed, 21st blocked\n\n";

$cleanup = new \Gene\Cache\Redis($cfg);
$cleanup->__call('del', ['geneweb:login:test:' . $ip]);
$cleanup->free();

echo "=== Redis fully unreachable -> fail-open (no fatal) ===\n";
$down = $cfg; $down['port'] = 6390; $down['timeout'] = 1;
$limited = isLoginRateLimited($down, $ip);
echo "isLoginRateLimited result while down: " . var_export($limited, true) . "\n";
echo ($limited === false ? "PASS" : "FAIL") . ": fails open (does not block login) when redis is down\n";
