<?php
/**
 * [GENE_FIX:2026-08-19 P1-3] Transaction hygiene at the POOL boundary.
 *
 * gene_pool_return_pdo() is the single chokepoint through which all 4
 * drivers return a borrowed PDO (release() / free() / __destruct). Before
 * the fix it never checked inTransaction(): a coroutine that bailed with an
 * open transaction returned a dirty connection, and the next borrower
 * inherited the transaction and its row locks.
 *
 * ⚠️ HARD REQUIREMENT: the pool path is gated on runtime_type >= 2
 * (Swoole Server). Under CLI/FPM the pool is bypassed entirely, so this
 * script MUST be run inside a Swoole worker OR with `-d gene.runtime_type=2`
 * + ext-swoole loaded. If the runtime gate is not met the script SKIPS —
 * a "green" CLI pass without Swoole would be a FALSE PASS.
 *
 * DB selection via env vars:
 *   - GENE_MYSQL_DSN + GENE_MYSQL_USER set  -> MySQL (real transaction/lock semantics)
 *   - otherwise                             -> sqlite (hygiene logic only, no lock semantics)
 *
 * Run (Swoole CLI mode):
 *   php -d gene.runtime_type=2 -d extension=swoole -d extension=gene \
 *       audit/repro/tx_leak_pool.php
 *
 * Run (Swoole + MySQL):
 *   GENE_MYSQL_DSN='mysql:host=127.0.0.1;dbname=gene_test;charset=utf8mb4' \
 *   GENE_MYSQL_USER=root GENE_MYSQL_PASS=secret \
 *   php -d gene.runtime_type=2 -d extension=swoole -d extension=pdo_mysql \
 *       -d extension=gene audit/repro/tx_leak_pool.php
 *
 * Scenario (requires runtime_type >= 2):
 *   1. create a named pool, borrow a Db handle configured with 'pool'
 *   2. beginTransaction + insert, then release() WITHOUT commit
 *      → expect: E_WARNING on the standard error channel + rollback
 *   3. borrow again (same underlying connection) and assert:
 *      - inTransaction() === false
 *      - the uncommitted row is NOT visible
 */

if (!class_exists('\\Gene\\Application') || !class_exists('\\Gene\\Pool')) {
    echo "SKIP: gene pool classes unavailable\n";
    exit(77);
}

$rt = \Gene\Application::getRuntimeType();
if ($rt < 2) {
    echo "SKIP: runtime_type=$rt (<2); pool path is disabled outside Swoole — ",
        "run with `-d gene.runtime_type=2` + ext-swoole, a plain CLI pass proves NOTHING\n";
    exit(77);
}

$useMysql = getenv('GENE_MYSQL_DSN') && getenv('GENE_MYSQL_USER');
$dsn  = $useMysql ? getenv('GENE_MYSQL_DSN')  : 'sqlite:' . sys_get_temp_dir() . '/gene_tx_pool.sqlite';
$user = $useMysql ? getenv('GENE_MYSQL_USER') : null;
$pass = $useMysql ? getenv('GENE_MYSQL_PASS') : '';

if ($useMysql) {
    echo "=== tx_leak_pool: MySQL + Swoole (runtime_type=$rt) ===\n";
    $table = 't_pool';
    $ddl = "CREATE TABLE `$table` (`id` INT AUTO_INCREMENT PRIMARY KEY, `v` VARCHAR(32)) ENGINE=InnoDB";
    $driverClass = '\\Gene\\Db\\Mysql';
} else {
    echo "=== tx_leak_pool: sqlite + Swoole (runtime_type=$rt) ===\n";
    $table = 't';
    $ddl = "CREATE TABLE $table (id INTEGER PRIMARY KEY AUTOINCREMENT, v TEXT)";
    $driverClass = '\\Gene\\Db\\Sqlite';
    @unlink($dsn);
}

// Pool config is read from Gene\Config under key 'pooled_db' (params[0]).
$poolConfig = [$dsn];
if ($useMysql) {
    $poolConfig[] = $user;
    $poolConfig[] = $pass;
}
$config = new \Gene\Config();
$config->set('pooled_db', [
    'params' => [$poolConfig],
]);
\Gene\Pool::create('txpool', 'pooled_db', ['min' => 1, 'max' => 2]);

$cfg = ['dsn' => $dsn, 'pool' => 'txpool'];
if ($useMysql) {
    $cfg['username'] = $user;
    $cfg['password'] = $pass;
}

// --- borrower #1: open a tx, write, release WITHOUT commit ---
$db = new $driverClass($cfg);
if ($useMysql) {
    $db->sql("DROP TABLE IF EXISTS `$table`")->execute();
}
$db->sql($ddl)->execute();
$db->beginTransaction();
$db->insert($table, ['v' => 'dirty'])->affectedRows();
echo "borrower 1: inTransaction=", var_export($db->inTransaction(), true), "\n";

// release() returns the PDO to the pool; hygiene must roll back + warn
// (warning bypasses user error handlers by design — see P1-4).
$db->release();

// --- borrower #2: same connection back from the pool ---
$db2 = new $driverClass($cfg);
$inTx = $db2->inTransaction();
$n = (int) $db2->select($table)->cell();
echo "borrower 2: inTransaction=", var_export($inTx, true),
    ", rows=$n (expect inTransaction=false, rows=0)\n";

// healthy committed write still works on the reused connection
$db2->insert($table, ['v' => 'clean'])->affectedRows();
$committed = (int) $db2->select($table)->cell();
$db2->release();

$ok = ($inTx === false) && ($n === 0) && ($committed === 1);
echo $ok ? "POOL TX HYGIENE OK\n" : "POOL TX HYGIENE FAILED\n";

// cleanup
if ($useMysql) {
    $db3 = new $driverClass($cfg);
    $db3->sql("DROP TABLE IF EXISTS `$table`")->execute();
    $db3->release();
} else {
    @unlink($dsn);
}
\Gene\Pool::closeAll();
exit($ok ? 0 : 1);
