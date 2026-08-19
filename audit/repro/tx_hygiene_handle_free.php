<?php
/**
 * N3 (2026-08-19): tx hygiene for handles that are NEITHER in the DI
 * registry NOR pool-managed — e.g. `new \Gene\Db\Sqlite(...)` used directly.
 * Before the fix, free() without a pool just nulled the pdo property and
 * __destruct() did nothing at all, so an open transaction rode the
 * (persistent) connection into the next request.
 *
 * After the fix: free() and __destruct() on a no-pool handle run
 * gene_db_tx_hygiene() — rollBack() first, then an E_WARNING that bypasses
 * the user error handler (goes to error_log).
 *
 * Assertions (file-based sqlite, second connection verifies durability):
 *  - free() with an open tx: warning logged, row rolled back
 *  - __destruct (unset) with an open tx: warning logged, row rolled back
 */

$file = sys_get_temp_dir() . '/gene_tx_hygiene_free.db';
$log  = sys_get_temp_dir() . '/gene_tx_hygiene_free.log';
@unlink($file);
@unlink($log);
ini_set('log_errors', '1');
ini_set('error_log', $log);

$fail = 0;

function rowCount($file)
{
    $pdo = new PDO('sqlite:' . $file);
    return (int)$pdo->query('SELECT count(*) FROM t')->fetchColumn();
}

// --- setup ---
$setup = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$setup->sql('CREATE TABLE t (a int)')->execute();
unset($setup);

// --- case 1: free() with open transaction ---
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->beginTransaction();
$db->sql('INSERT INTO t VALUES (1)')->execute();
echo "case free(): inTransaction=", var_export($db->inTransaction(), true), "\n";
$db->free();   // must rollBack + E_WARNING, then null the pdo
echo "case free(): rows visible to 2nd connection: ", rowCount($file), " (expect 0)\n";
if (rowCount($file) !== 0) {
    echo "case free(): FAIL — dirty row survived\n";
    $fail++;
}

// --- case 2: __destruct (unset) with open transaction ---
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->beginTransaction();
$db->sql('INSERT INTO t VALUES (2)')->execute();
unset($db);    // __destruct must rollBack + E_WARNING
echo "case __destruct: rows visible to 2nd connection: ", rowCount($file), " (expect 0)\n";
if (rowCount($file) !== 0) {
    echo "case __destruct: FAIL — dirty row survived\n";
    $fail++;
}

// --- warnings reached error_log? (bypassing user handler means: they are
//     written by the standard error channel) ---
$warnCount = 0;
if (is_file($log)) {
    $content = (string)file_get_contents($log);
    $warnCount = substr_count($content, 'open transaction');
}
echo "hygiene warnings in error_log: $warnCount (expect 2)\n";
if ($warnCount !== 2) {
    $fail++;
}

@unlink($file);
@unlink($log);
echo $fail ? "RESULT: $fail check(s) FAILED\n" : "RESULT: all guarded\n";
exit($fail ? 1 : 0);
