<?php
/**
 * 会话存储 TTL 探针（AUDIT 2026-09-23 P1）
 *
 * 前提：已加载 gene 扩展。CLI 即可，不需要 Swoole。
 * 用法：php audit/repro/session_store_ttl.php
 *
 * 修复后语义 [GENE_FIX:2026-09-23 R8]：Gene\Session 落库把 cookie_lifetime
 * 作为 set() 第三参传给声明了 ≥3 个参数的句柄；ttl<=0 回落 86400。
 * Memory/Redis/Memcached 的第三参即 TTL，会话记录不再永不过期。
 *
 * 观察：
 *   三参数句柄 argc=3 且 ttl=86400
 *   两参数句柄 argc=2（不传第三个参数，避免 ArgumentCountError）
 *
 * 无扩展时 exit 2，禁止把 SKIP 当成通过。
 */

if (!extension_loaded('gene')) {
    fwrite(STDERR, "SKIP: gene extension is not loaded\n");
    exit(2);
}

final class SessionTtlProbe
{
    public static $argc = -1;
    public static $ttl = 'unset';

    public function get($key)
    {
        return null;
    }

    public function set($key, $value, $ttl = null)
    {
        self::$argc = func_num_args();
        self::$ttl = $ttl;
        return true;
    }

    public function delete($key)
    {
        return true;
    }
}

\Gene\Di::set('sessionTtlProbe', new SessionTtlProbe());

$session = new \Gene\Session([
    'driver' => 'sessionTtlProbe',
    'name'   => 'TTLPROBE',
    'ttl'    => 86400,
]);
$session->set('admin', ['user_id' => 1]);
$session->save();

echo 'argc=' . SessionTtlProbe::$argc . "\n";
echo 'ttl=' . var_export(SessionTtlProbe::$ttl, true) . "\n";

if (SessionTtlProbe::$argc !== 3 || SessionTtlProbe::$ttl !== 86400) {
    echo "BUG: store set() did not receive cookie lifetime\n";
    exit(1);
}

final class SessionTtlProbeTwo
{
    public static $argc = -1;

    public function get($key)
    {
        return null;
    }

    public function set($key, $value)
    {
        self::$argc = func_num_args();
        return true;
    }

    public function delete($key)
    {
        return true;
    }
}

\Gene\Di::set('sessionTtlProbeTwo', new SessionTtlProbeTwo());
$session2 = new \Gene\Session([
    'driver' => 'sessionTtlProbeTwo',
    'name'   => 'TTLPROBE2',
    'ttl'    => 86400,
]);
$session2->set('admin', ['user_id' => 2]);
$session2->save();
echo 'argc2=' . SessionTtlProbeTwo::$argc . "\n";
if (SessionTtlProbeTwo::$argc !== 2) {
    echo "BUG: two-arg handler received extra arguments\n";
    exit(1);
}
echo "OK: store set() received cookie lifetime\n";
exit(0);
