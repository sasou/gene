<?php
/**
 * [GENE_AUDIT:2026-08-19 N6] Tx hygiene running from RSHUTDOWN (no active
 * userland stack frame) cannot discard an exception thrown by PDO::rollBack().
 *
 * Repro shape: request ends dirty because of an UNCAUGHT exception (the very
 * case §4.3' hygiene exists for) AND the connection's rollBack() throws
 * (PDO in_txn desynced here via a raw COMMIT; in production: MySQL "server has
 * gone away", DDL implicit commit, ...).
 *
 * At RSHUTDOWN EG(current_execute_data) is NULL, so zend_throw_exception_internal
 * escalates the PDOException to an immediate "Uncaught PDOException" E_ERROR and
 * bails out — gene_discard_current_exception() (N2) never runs. Two consequences:
 *   1. a bogus second "Fatal error: Uncaught PDOException ... [no active file]"
 *      per affected request;
 *   2. the bailout aborts the rest of gene_request_context_free_fields(), so any
 *      FURTHER dirty DI connection in the same request is never rolled back.
 *
 * Note the Swoole shape is unaffected: clearState() is called from userland, so
 * a frame exists and N2's discard works (see tx_hygiene_rollback_throws.php).
 *
 * Suggested fix: in gene_db_tx_hygiene(), force PDO::ATTR_ERRMODE to
 * PDO::ERRMODE_SILENT around the rollBack() (restoring it afterwards) so the
 * cleanup path cannot throw at all, instead of throwing then discarding.
 *
 * This script spawns a child with the same extensions and inspects its output.
 * exit 0 = only the business fatal (fixed); exit 1 = extra PDOException fatal.
 */

$child = <<<'PHP'
<?php
$f = sys_get_temp_dir() . '/gene_rsd_throw.db';
@unlink($f);
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $f]);
$db->sql('CREATE TABLE t (a int)')->execute();
\Gene\Di::set('rsd_db', $db);          // stays in di_regs -> RSHUTDOWN hygiene
$db->beginTransaction();
$db->sql('INSERT INTO t VALUES (1)')->execute();
try { $db->sql('COMMIT')->execute(); } catch (Throwable $e) {}   // desync in_txn
throw new RuntimeException('BIZ-UNCAUGHT');  // no clearState(): RSHUTDOWN path
PHP;

$childFile = sys_get_temp_dir() . '/gene_rsd_throw_child.php';
file_put_contents($childFile, $child);

$args = [];
foreach (['pdo_sqlite', 'gene'] as $ext) {
    $args[] = '-d';
    $args[] = 'extension=' . $ext;
}
$cmd = escapeshellarg(PHP_BINARY) . ' -n'
     . ' -d ' . escapeshellarg('extension_dir=' . ini_get('extension_dir'))
     . ' -d extension=pdo_sqlite'
     . ' -d ' . escapeshellarg('extension=' . (getenv('GENE_DLL') ?: 'gene'))
     . ' ' . escapeshellarg($childFile) . ' 2>&1';

$out = shell_exec($cmd);
echo "--- child output ---\n", $out, "--------------------\n";
@unlink($childFile);

if (strpos((string) $out, 'BIZ-UNCAUGHT') === false) {
    echo "INCONCLUSIVE: child did not run as expected (check GENE_DLL env var,\n",
         "  e.g. set GENE_DLL to the absolute php_gene.dll path).\n";
    exit(2);
}
$extra = strpos((string) $out, 'Uncaught PDOException') !== false;
echo $extra
    ? "FAIL: hygiene leaked an uncaught PDOException out of RSHUTDOWN\n"
    : "OK: no extra PDOException fatal from RSHUTDOWN hygiene\n";
exit($extra ? 1 : 0);
