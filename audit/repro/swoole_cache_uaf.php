<?php
/* [AUDIT 2026-08-23 UAF] Swoole worker signal-11 根因复现（单进程模拟两个协程交错）。
 *
 * 机制：Gene\Cache 业务读路径（processCached/processCachedVersion）命中缓存时，
 * 旧实现通过 gene_memory_zval_local 把进程级持久缓存里的 zend_string* / bucket key
 * 直接借给请求态 zval（零拷贝、不加引用计数）。另一"协程"随后对同一 key 覆盖写
 * （gene_memory_zval_edit_persistent 先 pefree 旧值再重建），先前返回的 PHP 数组
 * 里的字符串全部悬垂 —— 访问即 SIGSEGV 或读到垃圾。
 *
 * 本脚本用 Gene\Memory 直接驱动同一张 GENE_G(cache) 持久表（与 Gene\Cache 层
 * 写入路径相同），在 CLI 下即可复现，无需 Swoole：
 *   修复前：STEP C 读取 $borrowed 内容时崩溃/输出垃圾（配合 valgrind/ASAN 必报）。
 *   修复后：业务读路径走 gene_memory_zval_local_copy 深拷贝，$borrowed 完全独立。
 *
 * 运行：php audit\repro\swoole_cache_uaf.php
 */

echo "STEP A: 写入业务缓存数组（模拟 processCached 首次回源落盘）\n";
$m = new Gene\Memory('uaf-probe');
$payload = [];
for ($i = 0; $i < 32; $i++) {
    $payload['k' . $i] = str_repeat("value-{$i}-", 8);
}
$m->set('biz:doc', $payload);

echo "STEP B: 读取（修复前此处返回的数组借用持久指针）\n";
$borrowed = $m->get('biz:doc');
$checksum = 0;
foreach ($borrowed as $k => $v) {
    $checksum += strlen($k) + strlen($v);
}
echo "  borrowed checksum = {$checksum}\n";
$borrowed_snapshot = $borrowed; /* 请求态独立副本，用于 STEP D 对照 */

echo "STEP C: 另一协程覆盖同 key（修复前会 pefree $borrowed 借用的内存）\n";
$payload2 = [];
for ($i = 0; $i < 32; $i++) {
    $payload2['k' . $i] = str_repeat("REPLACED-{$i}#", 16);
}
$m->set('biz:doc', $payload2);
unset($payload); /* 释放源数组，避免其生命周期干扰复现 */

/* 强制内存复用：分配一批同尺寸字符串，覆盖刚释放的持久堆块。 */
$churn = [];
for ($i = 0; $i < 256; $i++) {
    $churn[] = str_repeat('X', 96);
}

echo "STEP D: 访问先前返回的数组（修复前此处悬垂）\n";
$checksum2 = 0;
$bad = 0;
foreach ($borrowed as $k => $v) {
    $checksum2 += strlen($k) + strlen($v);
    if (strpos($v, 'value-') !== 0 && strpos($v, 'REPLACED-') !== 0) {
        $bad++;
    }
}
echo "  borrowed checksum after overwrite = {$checksum2}\n";
echo "  corrupted entries = {$bad}\n";

if ($checksum !== $checksum2 || $bad > 0) {
    echo "FAIL: borrowed array was mutated/corrupted by the overwrite (UAF)\n";
    exit(1);
}

echo "STEP E: 引用类型与不支持类型加固\n";
$refVal = 'via-reference';
$refArr = ['a' => &$refVal, 'b' => 1];
$m->set('biz:ref', $refArr);
$got = $m->get('biz:ref');
echo "  ref roundtrip: ", (($got['a'] ?? null) === 'via-reference' ? 'ok' : 'BAD'), "\n";

/* [P2-1] 拒写语义已从 E_ERROR 改为取锁前 E_WARNING + 拒写；子进程验证
 * "不支持类型（含嵌套/自引用）被拒绝、进程不致命、写锁不泄漏"。 */
$child = sprintf('"%s" -n -d extension_dir=%s -d extension=pdo_sqlite -d extension=%s %s',
    PHP_BINARY, 'D:\\wampServer-php8.1_x64_nts\\php_ext',
    'F:\\php_src\\php-8.1.30-src\\x64\\Release\\php_gene.dll',
    escapeshellarg(__DIR__ . '\\swoole_cache_uaf_obj.php'));
exec($child, $out, $code);
echo "  object set child exit={$code} output=", implode(' ', $out), "\n";
echo "  object set refused safely: ", ($code === 0 && strpos(implode(' ', $out), 'refused=yes alive=yes') !== false ? 'ok' : 'BAD'), "\n";

echo "STEP F: done\n";
echo "PASS\n";
