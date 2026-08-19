<?php
/**
 * P1-4: user error handler converting E_WARNING to ErrorException during
 * request-boundary tx hygiene (RSHUTDOWN path).
 *
 * Before the fix: hygiene warned FIRST and rolled back SECOND — the handler
 * threw at the warning, the engine skipped the rollback call, and the script
 * ended with "Fatal error: Uncaught ErrorException" during RSHUTDOWN.
 *
 * After the fix: rollback runs BEFORE the warning and the warning bypasses
 * the user handler, so the script ends cleanly: no handler output, no fatal,
 * and the uncommitted row is rolled back (verified in `check` mode).
 *
 * Usage:
 *   php tx_hygiene_error_handler.php         # phase 1: leak + RSHUTDOWN
 *   php tx_hygiene_error_handler.php check   # phase 2: assert row was rolled back
 */
$file = sys_get_temp_dir() . '/gene_tx_hygiene_handler.db';

if (($argv[1] ?? '') === 'check') {
    $pdo = new PDO('sqlite:' . $file);
    $n = (int)$pdo->query('SELECT count(*) FROM t')->fetchColumn();
    echo "rows after rollback-check: $n (expect 0)\n";
    @unlink($file);
    exit($n === 0 ? 0 : 1);
}

@unlink($file);
$log = sys_get_temp_dir() . '/gene_tx_hygiene_handler.log';
@unlink($log);
ini_set('log_errors', '1');
ini_set('error_log', $log);

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->sql('CREATE TABLE IF NOT EXISTS t (a int)')->execute();
\Gene\Di::set('p14_db', $db);
$db->beginTransaction();
$db->sql('INSERT INTO t VALUES (1)')->execute();
echo "inTransaction=", var_export($db->inTransaction(), true), "\n";
set_error_handler(function ($no, $str) {
    // Post-fix this must NEVER fire for the hygiene warning; if it does,
    // the throw proves the ordering regressed (rollback already done, but
    // the uncaught exception during RSHUTDOWN is the fatal we must avoid).
    echo "[handler] converting to ErrorException: $str\n";
    throw new ErrorException($str, 0, $no);
});
echo "-- end of script, RSHUTDOWN hygiene follows --\n";
// Expected post-fix: PHP Warning line on stderr/log, no [handler] line,
// no Fatal error; then run with `check` to assert the row is gone.
