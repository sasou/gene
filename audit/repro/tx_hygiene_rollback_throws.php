<?php
/**
 * [GENE_AUDIT:2026-08-19 N2 verification] The half of P1-4 that §13.5-4 lists
 * as "needs MySQL": hygiene runs while a business exception is IN FLIGHT *and*
 * PDO::rollBack() itself throws.
 *
 * Before N2 the code called zend_clear_exception(), which also releases
 * EG(prev_exception) — the very slot zend_exception_save() parks the business
 * exception in — so the business exception vanished silently.
 *
 * We desync PDO's transaction bookkeeping WITHOUT MySQL by issuing a raw
 * COMMIT through Gene while PDO believes a transaction is still open; the
 * subsequent rollBack() then errors under the forced ERRMODE_EXCEPTION.
 * If this environment does not desync (driver reads autocommit directly),
 * the script reports SKIP-LIKE state instead of a false pass.
 */
$file = sys_get_temp_dir() . '/gene_tx_rb_throws.db';
$log  = sys_get_temp_dir() . '/gene_tx_rb_throws.log';
@unlink($file);
@unlink($log);
ini_set('log_errors', '1');
ini_set('error_log', $log);

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->sql('CREATE TABLE IF NOT EXISTS t (a int)')->execute();
\Gene\Di::set('rb_db', $db);

$db->beginTransaction();
$db->sql('INSERT INTO t VALUES (1)')->execute();
/* Commit behind PDO's back so its in_txn bookkeeping desyncs. */
try {
    $db->sql('COMMIT')->execute();
} catch (Throwable $e) {
    echo "raw COMMIT threw: ", $e->getMessage(), "\n";
}
$desynced = $db->inTransaction();
echo "PDO still reports inTransaction after raw COMMIT: ",
     var_export($desynced, true), "\n";

/* The handle is now in the exact state hygiene must survive: PDO reports an
 * open transaction, but any rollBack() will throw. Business exception in
 * flight on top of that. */
$caught = null;
try {
    try {
        throw new RuntimeException('BIZ');
    } finally {
        \Gene\Application::clearState();
    }
} catch (Throwable $e) {
    $caught = get_class($e) . ':' . $e->getMessage();
}

echo "caught = ", var_export($caught, true), " (expect 'RuntimeException:BIZ')\n";
echo "log: ", trim((string) @file_get_contents($log)), "\n";
if (!$desynced) {
    echo "NOTE: could not desync PDO's in_txn in this driver; the N2\n",
         "      exception-discard branch was NOT exercised (needs MySQL).\n";
}
echo "-- end of script body (watch for anything printed after this line) --\n";
exit($caught === 'RuntimeException:BIZ' ? 0 : 1);
