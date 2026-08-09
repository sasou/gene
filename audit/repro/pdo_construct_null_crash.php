<?php
/**
 * H1 复现：Gene\Db\* 构造函数在 config 缺少 username/password 时段错误（0xC0000005 / SIGSEGV）。
 * 根因：src/db/pdo.c gene_pdo_construct() 直接 *user / *pass 解引用 NULL。
 * 期望：抛异常或按空串处理；实际：进程崩溃。
 *
 * 用法：php audit/repro/pdo_construct_null_crash.php ; echo $?   (Windows: echo %errorlevel%)
 *   缺 username/password -> 崩溃；补上 -> 正常。
 */
fwrite(STDERR, "STEP A: about to construct with dsn only\n");
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);   // <-- 崩溃点
fwrite(STDERR, "STEP B: constructed (若能看到这一行说明已修复)\n");

$db2 = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:', 'username' => '', 'password' => '']);
fwrite(STDERR, "STEP C: 补齐 username/password 后正常\n");
