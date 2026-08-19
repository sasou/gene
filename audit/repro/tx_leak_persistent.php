<?php
/**
 * [GENE_FIX:2026-08-18 4.3'] Transaction hygiene at the request boundary.
 * [GENE_FIX:2026-08-19 P1-4] Hardened: rollback BEFORE warning, warning
 * bypasses the user error handler (it goes to PHP's standard error channel
 * — error_log — so a Laravel-style "warnings become ErrorException" handler
 * can no longer abort shutdown hygiene mid-way).
 *
 * Simulates the FPM + PDO::ATTR_PERSISTENT scenario in CLI: a request opens
 * a transaction and "bails" without commit/rollBack. At request teardown
 * (here: Gene\Application::clearState(), the same free_fields path RSHUTDOWN
 * takes) the framework must rollBack() and E_WARNING — otherwise the open
 * transaction rides the persistent connection into the NEXT request.
 *
 * Assertions:
 *  - the rollback happened (inTransaction=false, row invisible to request 2)
 *  - the warning was emitted to error_log
 *  - the user error handler was NOT invoked for the hygiene warning
 */

$f = sys_get_temp_dir() . '/gene_tx_hygiene.sqlite';
$log = sys_get_temp_dir() . '/gene_tx_hygiene.log';
@unlink($f);
@unlink($log);
ini_set('log_errors', '1');
ini_set('error_log', $log);

// ATTR_PERSISTENT (index 12) — same as apistore's FPM config
$db = new \Gene\Db\Sqlite([
    'dsn' => 'sqlite:' . $f,
    'options' => [12 => true],
]);
$db->sql('CREATE TABLE t (id INTEGER PRIMARY KEY AUTOINCREMENT, v TEXT)')->execute();
\Gene\Di::set('db', $db);

// --- simulated request #1: opens a tx, writes, never commits ---
$db->beginTransaction();
$db->insert('t', ['v' => 'dirty'])->affectedRows();
echo "request 1: inTransaction=", var_export($db->inTransaction(), true), "\n";

// Laravel-style handler: would convert the hygiene E_WARNING into an
// exception. Post-P1-4 it must NOT be invoked by the hygiene path at all.
$handlerHit = false;
set_error_handler(function ($no, $str) use (&$handlerHit) {
    $handlerHit = true;
    throw new ErrorException($str, 0, $no);
}, E_WARNING);

// --- request boundary (RSHUTDOWN / clearState) ---
\Gene\Application::clearState();
restore_error_handler();

$warned = is_file($log) && strpos((string)file_get_contents($log), 'open transaction') !== false;
echo "boundary warning (error_log): ", $warned ? "YES" : "NONE (BUG)", "\n";
echo "user handler invoked: ", $handlerHit ? "YES (BUG — hygiene must bypass it)" : "no (correct)", "\n";
echo "after boundary: inTransaction=", var_export($db->inTransaction(), true), "\n";

// --- simulated request #2: same persistent connection reused ---
$n = $db->select('t')->cell();
echo "rows visible to request 2: ", var_export($n, true), " (expect 0 — rolled back)\n";

$ok = $warned
    && !$handlerHit
    && $db->inTransaction() === false
    && (int)$n === 0;
echo $ok ? "TX HYGIENE OK\n" : "TX HYGIENE FAILED\n";

// a healthy committed write still persists
$db->insert('t', ['v' => 'clean'])->affectedRows();
echo "committed rows: ", $db->select('t')->cell(), " (expect 1)\n";

@unlink($f);
@unlink($log);
exit($ok ? 0 : 1);
