<?php
/* [AUDIT 2026-08-23 SW-LOG] 线上事故复现：Swoole workerStart 里调用
 * workerReady()，扩展对矛盾配置（cache_reserve <= cache_max_items）发
 * E_WARNING；框架通过 set_error_handler 注册的 Gene\Exception::doError 把
 * warning 转成 Exception 抛出，workerStart 无人捕获 → Swoole 判定 worker
 * 致命（ERRNO 503）→ master 无限重启 worker → 服务始终不可用。
 *
 * 修复后语义：runtime_type >= 2（Swoole/resident/coroutine）时，
 * workerReady() 内的诊断一律走 gene_log_diag() 只写 error_log，不触发用户
 * 错误处理器；FPM 模式行为不变（warning 仍交给用户处理器，可转异常）。
 *
 * 运行：php audit\repro\swoole_workerready_logonly.php
 */

error_reporting(E_ALL);

$fail = 0;
function check($label, $cond) {
    global $fail;
    echo ($cond ? "ok" : "FAIL"), "  {$label}\n";
    if (!$cond) {
        $fail++;
    }
}

/* 子进程脚本：注册 Gene 错误处理器后调用 workerReady()。 */
$childScript = sys_get_temp_dir() . '\\sw_workerready_logonly.php';
file_put_contents($childScript, <<<'PHP'
<?php
if (getenv('GENE_SWOOLE_MODE') === '1') {
    \Gene\Application::setRuntimeType('swoole');
}
\Gene\Exception::setErrorHandler('Gene\Exception::doError');
try {
    \Gene\Application::workerReady();
    echo "no-throw\n";
} catch (\Throwable $e) {
    echo "threw: ", $e->getMessage(), "\n";
}
PHP
);

$runChild = function ($swoole, $reserve, $maxItems) use ($childScript) {
    $cmd = sprintf('"%s" -n -d extension_dir=%s -d extension=pdo_sqlite -d extension=%s'
        . ' -d display_errors=stderr -d log_errors=1'
        . ' -d gene.cache_reserve=%d -d gene.cache_max_items=%d %s 2>&1',
        PHP_BINARY, 'D:\\wampServer-php8.1_x64_nts\\php_ext',
        'F:\\php_src\\php-8.1.30-src\\x64\\Release\\php_gene.dll',
        $reserve, $maxItems, escapeshellarg($childScript));
    if ($swoole) {
        putenv('GENE_SWOOLE_MODE=1');
    } else {
        putenv('GENE_SWOOLE_MODE');
    }
    exec($cmd, $o, $c);
    putenv('GENE_SWOOLE_MODE');
    return [implode("\n", $o), $c];
};

echo "== SW-LOG-1: Swoole 模式 + 矛盾配置：记日志、不抛异常 ==\n";
[$out, $code] = $runChild(true, 4096, 10000);
echo "  child exit={$code}\n";
check('workerReady 未抛异常（no-throw）', strpos($out, 'no-throw') !== false);
check('warning 仍写入日志（含 cache_reserve/cache_max_items）',
    strpos($out, 'gene.cache_reserve') !== false && strpos($out, 'gene.cache_max_items') !== false);
check('子进程正常退出', $code === 0);

echo "== SW-LOG-2: Swoole 模式 + max_items=0：unbounded 提示只记日志 ==\n";
[$out, $code] = $runChild(true, 4096, 0);
echo "  child exit={$code}\n";
check('workerReady 未抛异常（no-throw）', strpos($out, 'no-throw') !== false);
check('E_NOTICE 提示写入日志（unbounded）', strpos($out, 'unbounded') !== false);
check('子进程正常退出', $code === 0);

echo "== SW-LOG-3: FPM 模式行为不变：warning 仍经用户处理器转异常 ==\n";
[$out, $code] = $runChild(false, 4096, 10000);
echo "  child exit={$code}\n";
check('FPM 下仍抛出（threw: ... cache_reserve）',
    strpos($out, 'threw:') !== false && strpos($out, 'gene.cache_reserve') !== false);

/* [AUDIT 2026-08-23 AUTO-RESERVE] 矛盾配置不再只是警告：reserve 自动向上
 * 矫正为 max_items + margin（10000 + 2500 = 12500），冻结后写入超过
 * 4096 个业务 key 不再被插入守卫拒绝。 */
echo "== SW-LOG-4: 矛盾配置自动矫正：冻结后 5000 个业务写入全部接受 ==\n";
$childScript4 = sys_get_temp_dir() . '\\sw_autoreserve_accept.php';
file_put_contents($childScript4, <<<'PHP'
<?php
class H { public function produce($p) { return ['p' => $p]; } }
\Gene\Application::setRuntimeType('swoole');
\Gene\Application::workerReady();
$cache = new \Gene\Cache\Cache(['sign' => 't:']);
$h = new H();
$ok = 0;
for ($i = 0; $i < 5000; $i++) {
    $r = $cache->processCached([$h, 'produce'], ["arg_$i"], 3600);
    if (is_array($r) && ($r['p'] ?? null) === "arg_$i") { $ok++; }
}
echo "accepted=$ok\n";
PHP
);
putenv('GENE_SWOOLE_MODE=1');
$cmd = sprintf('"%s" -n -d extension_dir=%s -d extension=%s'
    . ' -d display_errors=stderr -d log_errors=1'
    . ' -d gene.cache_reserve=4096 -d gene.cache_max_items=10000 %s 2>&1',
    PHP_BINARY, 'D:\\wampServer-php8.1_x64_nts\\php_ext',
    'F:\\php_src\\php-8.1.30-src\\x64\\Release\\php_gene.dll',
    escapeshellarg($childScript4));
exec($cmd, $o4, $c4);
putenv('GENE_SWOOLE_MODE');
$out4 = implode("\n", $o4);
echo "  child exit={$c4}\n";
check('5000 个业务写入全部接受（accepted=5000）', strpos($out4, 'accepted=5000') !== false);
check('日志含矫正值（auto-corrected to 12500）', strpos($out4, 'auto-corrected to 12500') !== false);
check('子进程正常退出', $c4 === 0);
@unlink($childScript4);

/* [AUDIT 2026-08-23 IDEMPOTENT] workerReady() 被误放在 onRequest 里每请求
 * 调用时：第二次起必须早返回——警告只写一次 error_log，且 post-freeze
 * 不再触发 zend_hash_extend 扩容（扩容会移动 arData → 读者裸指针悬垂）。 */
echo "== SW-LOG-5: 重复调用 workerReady()：幂等，警告只记一次 ==\n";
$childScript5 = sys_get_temp_dir() . '\\sw_workerready_idempotent.php';
file_put_contents($childScript5, <<<'PHP'
<?php
\Gene\Application::setRuntimeType('swoole');
\Gene\Application::workerReady();
\Gene\Application::workerReady();
\Gene\Application::workerReady();
echo "done\n";
PHP
);
putenv('GENE_SWOOLE_MODE=1');
$cmd = sprintf('"%s" -n -d extension_dir=%s -d extension=%s'
    . ' -d display_errors=stderr -d log_errors=1'
    . ' -d gene.cache_reserve=4096 -d gene.cache_max_items=10000 %s 2>&1',
    PHP_BINARY, 'D:\\wampServer-php8.1_x64_nts\\php_ext',
    'F:\\php_src\\php-8.1.30-src\\x64\\Release\\php_gene.dll',
    escapeshellarg($childScript5));
exec($cmd, $o5, $c5);
putenv('GENE_SWOOLE_MODE');
$out5 = implode("\n", $o5);
echo "  child exit={$c5}\n";
check('三次调用正常完成（done）', strpos($out5, 'done') !== false);
/* display_errors=stderr 与 log_errors=1 各向 stderr 写一份，故一次逻辑警告
 * 在输出中出现 2 次；若非幂等，3 次调用会产生 6 次。 */
check('矫正警告恰好出现一次（display+log 双通道 = 2 行）',
    substr_count($out5, 'auto-corrected to 12500') === 2);
check('子进程正常退出', $c5 === 0);
@unlink($childScript5);

@unlink($childScript);

echo $fail === 0 ? "\nPASS\n" : "\nFAIL ({$fail})\n";
exit($fail === 0 ? 0 : 1);
