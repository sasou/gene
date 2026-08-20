<?php
/**
 * Hygiene running while a business exception is IN FLIGHT (the Swoole
 * `finally { Application::clearState(); }` shape).
 *
 * Asserts the P1-4 zend_exception_save()/restore() window actually preserves
 * the caller's exception while the dirty transaction is rolled back.
 *
 * NOTE the untested half: if PDO::rollBack() itself throws (ERRMODE_EXCEPTION
 * is forced by Gene; e.g. MySQL "server has gone away", or a DDL implicit
 * commit desyncing PDO's in_txn flag), gene.c calls zend_clear_exception(),
 * which ALSO releases EG(prev_exception) — i.e. the parked business exception
 * is destroyed and vanishes. That path needs MySQL to reproduce.
 */
$file = sys_get_temp_dir() . '/gene_tx_pending.db';
$log  = sys_get_temp_dir() . '/gene_tx_pending.log';
@unlink($file);
@unlink($log);
ini_set('log_errors', '1');
ini_set('error_log', $log);

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->sql('CREATE TABLE IF NOT EXISTS t (a int)')->execute();
\Gene\Di::set('pend_db', $db);

$caught = null;
try {
    try {
        $db->beginTransaction();
        $db->sql('INSERT INTO t VALUES (1)')->execute();
        throw new RuntimeException('BIZ');
    } finally {
        \Gene\Application::clearState();
    }
} catch (Throwable $e) {
    $caught = get_class($e) . ':' . $e->getMessage();
}

echo "caught = ", var_export($caught, true), " (expect 'RuntimeException:BIZ')\n";
echo "inTransaction after hygiene = ", var_export($db->inTransaction(), true), "\n";
echo "log: ", trim((string)@file_get_contents($log)), "\n";
exit($caught === 'RuntimeException:BIZ' ? 0 : 1);
