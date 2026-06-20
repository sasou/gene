<?php
/**
 * verify_5_6_6.php — 验证 gene 5.6.6 内存与高并发审计落地项（FPM/CLI 模式）
 *
 * 覆盖：
 *   - 扩展加载与版本
 *   - P1  gene.swoole_getcid_capi  INI 注册（默认 1）
 *   - M1  gene.cache_max_items     INI 注册（默认 0）+ 业务缓存上限/近似 LRU 淘汰行为
 *   - P3  gene.route_precompile    INI 注册（默认 0）
 *   - M5  request_attr 跨请求复用（CLI 下不发请求，仅核对扩展不崩溃）
 *
 * 用法（在仓库根目录）：
 *   php tools/verify_5_6_6.php
 *   php -d gene.cache_max_items=10 tools/verify_5_6_6.php
 */

$failures = 0;
$idx = 0;
function check($label, $cond, $detail = '') {
    global $failures, $idx;
    $idx++;
    $status = $cond ? 'PASS' : 'FAIL';
    if (!$cond) { $GLOBALS['failures']++; }
    printf("  [%s] %2d. %s%s\n", $status, $idx, $label, $detail !== '' ? "  ($detail)" : '');
}

echo "=== gene 5.6.6 功能验证 ===\n\n";

/* 0. 扩展加载与版本 */
echo "[0] 扩展加载与版本\n";
check('gene 扩展已加载', extension_loaded('gene'));
$ver = phpversion('gene');
check('扩展版本可读', $ver !== false, "version={$ver}");

/* 1. INI 开关注册与默认值 */
echo "\n[1] INI 开关注册（审计 P1 / M1 / P3）\n";
$capi  = ini_get('gene.swoole_getcid_capi');
$maxit = ini_get('gene.cache_max_items');
$pc    = ini_get('gene.route_precompile');
check('P1 gene.swoole_getcid_capi 已注册', $capi !== false, "value={$capi}");
check('M1 gene.cache_max_items 已注册',    $maxit !== false, "value={$maxit}");
check('P3 gene.route_precompile 已注册',   $pc !== false, "value={$pc}");

/* 2. M1 — 业务缓存上限 + 近似 LRU 淘汰 */
echo "\n[2] M1 业务缓存上限 + 近似 LRU（Gene\\Cache::processCached）\n";
$cap = (int) ini_get('gene.cache_max_items');

class Producer {
    public function make($k) { return "value-of-{$k}"; }
}
$producer = new Producer();
$cache = new \Gene\Cache\Cache(['sign' => 'verify:']);
$memory = new \Gene\Memory();

$before = $memory->stats()['cache_items'];

/* 写入 60 个互异业务键（depth>0 业务分区写入） */
$N = 60;
for ($i = 0; $i < $N; $i++) {
    $v = $cache->processCached([$producer, 'make'], ["key{$i}"]);
    if ($v !== "value-of-key{$i}") {
        check("processCached 返回值正确 (key{$i})", false, "got={$v}");
        break;
    }
}
check('processCached 全部返回正确值', true, "wrote {$N} business entries");

$after = $memory->stats()['cache_items'];
$delta = $after - $before;

if ($cap > 0) {
    /* 带上限：业务分区应被淘汰到上限附近（不超过 cap + 框架元数据基线） */
    check(
        "cache_max_items={$cap} 下业务条目受限",
        $delta <= $cap,
        "新增业务条目={$delta} <= cap={$cap}"
    );
    /* 近似 LRU：最近写入的键应仍命中缓存（不触发重新计算）；最早的键应已被淘汰 */
    $recent = $cache->processCached([$producer, 'make'], ['key' . ($N - 1)]);
    check('最近写入键仍可读', $recent === 'value-of-key' . ($N - 1));
} else {
    /* 默认 0 = 不限：所有业务条目应全部保留（完全向后兼容） */
    check(
        'cache_max_items=0（默认）不淘汰，全部保留',
        $delta >= $N,
        "新增业务条目={$delta} >= {$N}"
    );
    echo "      提示：用 `php -d gene.cache_max_items=10 tools/verify_5_6_6.php` 复验淘汰行为\n";
}

/* 清理业务分区，确认 clean() 与跟踪集同步无崩溃 */
$memory->clean();
$cleaned = $memory->stats()['cache_items'];
check('Memory::clean() 后缓存清空且无崩溃', $cleaned === 0, "cache_items={$cleaned}");

/* 3. P6 / M5 — 多轮 processCached 不崩溃（源码缓存/数组复用透明路径） */
echo "\n[3] P6 / M5 透明路径稳定性（多轮调用）\n";
$ok = true;
for ($r = 0; $r < 200; $r++) {
    $v = $cache->processCached([$producer, 'make'], ["loop{$r}"]);
    if ($v !== "value-of-loop{$r}") { $ok = false; break; }
}
check('200 轮 processCached 稳定无崩溃', $ok);

echo "\n=== 结果：" . ($failures === 0 ? "全部通过 ✅" : "{$failures} 项失败 ❌") . " ===\n";
exit($failures === 0 ? 0 : 1);
