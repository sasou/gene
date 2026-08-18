<?php
/**
 * [GENE_FIX:2026-08-18 4.3'] Transaction hygiene at the request boundary.
 *
 * Simulates the FPM + PDO::ATTR_PERSISTENT scenario in CLI: a request opens
 * a transaction and "bails" without commit/rollBack. At request teardown
 * (here: Gene\Application::clearState(), the same free_fields path RSHUTDOWN
 * takes) the framework must E_WARNING and rollBack() — otherwise the open
 * transaction rides the persistent connection into the NEXT request.
 *
 * Before the fix: after clearState() the PDO was still inTransaction() and
 * the uncommitted row was visible on the reused connection.
 * After the fix: E_WARNING is emitted, transaction rolled back, row gone.
 */

$f = sys_get_temp_dir() . '/gene_tx_hygiene.sqlite';
@unlink($f);

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

$warned = null;
set_error_handler(function ($no, $str) use (&$warned) { $warned = $str; return true; }, E_WARNING);

// --- request boundary (RSHUTDOWN / clearState) ---
\Gene\Application::clearState();
restore_error_handler();

echo "boundary warning: ", $warned !== null ? "YES: $warned" : "NONE (BUG)", "\n";
echo "after boundary: inTransaction=", var_export($db->inTransaction(), true), "\n";

// --- simulated request #2: same persistent connection reused ---
$n = $db->select('t')->cell();
echo "rows visible to request 2: ", var_export($n, true), " (expect 0 — rolled back)\n";

$ok = $warned !== null
    && $db->inTransaction() === false
    && (int)$n === 0;
echo $ok ? "TX HYGIENE OK\n" : "TX HYGIENE FAILED\n";

// a healthy committed write still persists
$db->insert('t', ['v' => 'clean'])->affectedRows();
echo "committed rows: ", $db->select('t')->cell(), " (expect 1)\n";

@unlink($f);
exit($ok ? 0 : 1);
