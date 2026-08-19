<?php
/**
 * [GENE_AUDIT:2026-08-19 §15.6-1] MySQL integration test for 6.1.0 v2 APIs.
 *
 * The sqlite test suite (DatabaseTest::testSqliteV2WriteApis) only does SQL
 * TEXT assertions for lockForUpdate/sharedLock (sqlite has no such syntax —
 * they are E_NOTICE no-ops there) and cannot exercise ON DUPLICATE KEY UPDATE
 * (upsert throws on sqlite). This script runs the real MySQL paths:
 *
 *   - insertIgnore:  INSERT IGNORE on a UNIQUE-key table
 *   - upsert:        ON DUPLICATE KEY UPDATE
 *   - lockForUpdate: SELECT ... FOR UPDATE inside a transaction
 *   - sharedLock:    SELECT ... LOCK IN SHARE MODE inside a transaction
 *   - tx hygiene:    dirty transaction rolled back at clearState() + E_WARNING
 *
 * Config via env vars (all required for MySQL mode):
 *   GENE_MYSQL_DSN  e.g. mysql:host=127.0.0.1;port=3306;dbname=gene_test;charset=utf8mb4
 *   GENE_MYSQL_USER e.g. root
 *   GENE_MYSQL_PASS e.g. secret
 *
 * If any is missing the script SKIPs (exit=77) — a green run without MySQL
 * proves nothing.
 *
 * Run:
 *   GENE_MYSQL_DSN='mysql:host=127.0.0.1;dbname=gene_test;charset=utf8mb4' \
 *   GENE_MYSQL_USER=root GENE_MYSQL_PASS=secret \
 *   php -d extension=pdo_mysql -d extension=gene audit/repro/mysql_v2_apis.php
 */

$dsn  = getenv('GENE_MYSQL_DSN');
$user = getenv('GENE_MYSQL_USER');
$pass = getenv('GENE_MYSQL_PASS');

if (!$dsn || !$user) {
    echo "SKIP: GENE_MYSQL_DSN / GENE_MYSQL_USER not set — MySQL integration test\n",
         "      requires a real MySQL/MariaDB. See script header for env vars.\n";
    exit(77);
}

$fail = 0;
function ok($msg) { echo "  ✓ $msg\n"; }
function bad($msg) { global $fail; echo "  ✗ $msg\n"; $fail++; }

function dropAndCreate($db, $table, $ddl) {
    $db->sql("DROP TABLE IF EXISTS `$table`")->execute();
    $db->sql($ddl)->execute();
}

echo "=== MySQL v2 API integration test ===\n";
echo "DSN: $dsn\n";
echo "gene version: " . phpversion('gene') . "\n\n";

try {
    $db = new \Gene\Db\Mysql([
        'dsn' => $dsn,
        'username' => $user,
        'password' => $pass,
    ]);

    // ---- insertIgnore: INSERT IGNORE on a UNIQUE-key table ----
    echo "Testing insertIgnore (MySQL INSERT IGNORE):\n";
    dropAndCreate($db, 'ig_test',
        'CREATE TABLE `ig_test` (`id` INT AUTO_INCREMENT PRIMARY KEY, `sku` VARCHAR(32) NOT NULL UNIQUE, `qty` INT DEFAULT 0) ENGINE=InnoDB');
    $n = $db->insertIgnore('ig_test', ['sku' => 'A1', 'qty' => 1])->affectedRows();
    if ((int)$n === 1) { ok("insertIgnore first write affected=1"); }
    else { bad("insertIgnore first affected=" . var_export($n, true)); }

    $n = $db->insertIgnore('ig_test', ['sku' => 'A1', 'qty' => 99])->affectedRows();
    $row = $db->select('ig_test')->where('sku=?', ['A1'])->row();
    if ((int)$n === 0 && (int)($row['qty'] ?? -1) === 1) {
        ok("insertIgnore duplicate ignored (affected=0, qty stays 1)");
    } else {
        bad("insertIgnore dup: affected=" . var_export($n, true) . " row=" . json_encode($row));
    }
    echo "\n";

    // ---- upsert: ON DUPLICATE KEY UPDATE ----
    echo "Testing upsert (MySQL ON DUPLICATE KEY UPDATE):\n";
    dropAndCreate($db, 'up_test',
        'CREATE TABLE `up_test` (`id` INT AUTO_INCREMENT PRIMARY KEY, `sku` VARCHAR(32) NOT NULL UNIQUE, `qty` INT DEFAULT 0) ENGINE=InnoDB');
    $db->insert('up_test', ['sku' => 'B1', 'qty' => 5])->affectedRows();

    $n = $db->upsert('up_test', ['sku' => 'B1', 'qty' => 99], ['qty'])->affectedRows();
    $row = $db->select('up_test')->where('sku=?', ['B1'])->row();
    if ((int)$n >= 1 && (int)($row['qty'] ?? -1) === 99) {
        ok("upsert updated existing row (qty 5 -> 99, affected=$n)");
    } else {
        bad("upsert: affected=" . var_export($n, true) . " row=" . json_encode($row));
    }

    // upsert on non-existing key -> INSERT
    $n = $db->upsert('up_test', ['sku' => 'B2', 'qty' => 7], ['qty'])->affectedRows();
    $row = $db->select('up_test')->where('sku=?', ['B2'])->row();
    if ((int)$n === 1 && (int)($row['qty'] ?? -1) === 7) {
        ok("upsert inserted new row (sku=B2, qty=7, affected=1)");
    } else {
        bad("upsert new: affected=" . var_export($n, true) . " row=" . json_encode($row));
    }
    echo "\n";

    // ---- lockForUpdate: SELECT ... FOR UPDATE inside a transaction ----
    echo "Testing lockForUpdate (MySQL FOR UPDATE):\n";
    dropAndCreate($db, 'lock_test',
        'CREATE TABLE `lock_test` (`id` INT AUTO_INCREMENT PRIMARY KEY, `v` INT DEFAULT 0) ENGINE=InnoDB');
    $db->insert('lock_test', ['v' => 42])->affectedRows();

    $db->beginTransaction();
    $sql = $db->select('lock_test')->where('id=?', [1])->lockForUpdate()->print();
    $hasForUpdate = is_array($sql) && stripos($sql['sql'] ?? '', 'FOR UPDATE') !== false;
    if ($hasForUpdate) {
        ok("lockForUpdate SQL: " . $sql['sql']);
    } else {
        bad("lockForUpdate SQL missing FOR UPDATE: " . json_encode($sql));
    }
    // Actually execute the locked SELECT
    $row = $db->select('lock_test')->where('id=?', [1])->lockForUpdate()->row();
    if (is_array($row) && (int)($row['v'] ?? -1) === 42) {
        ok("lockForUpdate SELECT executed (v=42)");
    } else {
        bad("lockForUpdate row: " . json_encode($row));
    }
    $db->commit();

    // lockForUpdate outside transaction -> E_NOTICE (per §3.4 spec)
    $notice = null;
    set_error_handler(function ($no, $str) use (&$notice) { $notice = $str; return true; }, E_NOTICE);
    $db->select('lock_test')->lockForUpdate()->all();
    restore_error_handler();
    if ($notice !== null && stripos($notice, 'transaction') !== false) {
        ok("lockForUpdate outside transaction: E_NOTICE (as designed)");
    } else {
        bad("lockForUpdate outside tx: notice=" . var_export($notice, true));
    }
    echo "\n";

    // ---- sharedLock: SELECT ... LOCK IN SHARE MODE ----
    echo "Testing sharedLock (MySQL LOCK IN SHARE MODE):\n";
    $db->beginTransaction();
    $sql = $db->select('lock_test')->where('id=?', [1])->sharedLock()->print();
    $hasShareLock = is_array($sql) && stripos($sql['sql'] ?? '', 'LOCK IN SHARE MODE') !== false;
    if ($hasShareLock) {
        ok("sharedLock SQL: " . $sql['sql']);
    } else {
        bad("sharedLock SQL missing LOCK IN SHARE MODE: " . json_encode($sql));
    }
    $row = $db->select('lock_test')->where('id=?', [1])->sharedLock()->row();
    if (is_array($row) && (int)($row['v'] ?? -1) === 42) {
        ok("sharedLock SELECT executed (v=42)");
    } else {
        bad("sharedLock row: " . json_encode($row));
    }
    $db->commit();
    echo "\n";

    // ---- tx hygiene: dirty transaction rolled back at clearState() ----
    echo "Testing tx hygiene (MySQL + clearState):\n";
    $log = sys_get_temp_dir() . '/gene_mysql_hygiene.log';
    @unlink($log);
    ini_set('log_errors', '1');
    ini_set('error_log', $log);

    \Gene\Di::set('mysql_hygiene_db', $db);
    $db->beginTransaction();
    $db->insert('lock_test', ['v' => 999])->affectedRows();
    $inTxBefore = $db->inTransaction();

    \Gene\Application::clearState();

    $inTxAfter = $db->inTransaction();
    // A fresh connection should NOT see the uncommitted row
    $check = new \Gene\Db\Mysql(['dsn' => $dsn, 'username' => $user, 'password' => $pass]);
    $dirtyCount = (int) $check->select('lock_test')->where('v=?', [999])->cell();
    $logContent = (string) @file_get_contents($log);

    if ($inTxBefore && !$inTxAfter) {
        ok("dirty transaction rolled back (inTransaction: true -> false)");
    } else { bad("tx hygiene: inTx before=" . var_export($inTxBefore, true) . " after=" . var_export($inTxAfter, true)); }

    if ($dirtyCount === 0) {
        ok("uncommitted row NOT visible to a fresh connection (rolled back)");
    } else { bad("tx hygiene: dirty row visible (count=$dirtyCount) — rollback failed"); }

    if (strpos($logContent, 'open transaction') !== false) {
        ok("E_WARNING emitted to error_log (handler bypassed)");
    } else { bad("tx hygiene warning missing in log: " . substr($logContent, 0, 200)); }

    @unlink($log);
    echo "\n";

    // ---- cleanup ----
    $db->sql('DROP TABLE IF EXISTS `ig_test`')->execute();
    $db->sql('DROP TABLE IF EXISTS `up_test`')->execute();
    $db->sql('DROP TABLE IF EXISTS `lock_test`')->execute();

} catch (Throwable $e) {
    bad("EXCEPTION: " . get_class($e) . ": " . $e->getMessage());
}

echo $fail === 0 ? "=== MySQL v2 API integration: ALL PASS ===\n" : "=== MySQL v2 API integration: {$fail} FAILED ===\n";
exit($fail === 0 ? 0 : 1);
