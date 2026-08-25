<?php
$child = <<<'PHP'
<?php
class GeneUninitializedPdo extends PDO
{
    public function __construct()
    {
    }
}

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$property = new ReflectionProperty(\Gene\Db\Sqlite::class, 'pdo');
$property->setAccessible(true);
$property->setValue($db, new GeneUninitializedPdo());
\Gene\Di::set('uninitialized_pdo', $db);
echo "EXCEPTION_PAGE_COMPLETE\n";
PHP;

$childFile = sys_get_temp_dir() . '/gene_uninitialized_pdo_child.php';
file_put_contents($childFile, $child);
$cmd = escapeshellarg(PHP_BINARY) . ' -n'
    . ' -d ' . escapeshellarg('extension_dir=' . ini_get('extension_dir'))
    . ' -d extension=pdo_sqlite'
    . ' -d ' . escapeshellarg('extension=' . (getenv('GENE_DLL') ?: 'gene'))
    . ' ' . escapeshellarg($childFile) . ' 2>&1';
$output = (string) shell_exec($cmd);
@unlink($childFile);

echo $output;
$extraFatal = strpos($output, 'PDO object is not initialized') !== false;
$completed = strpos($output, 'EXCEPTION_PAGE_COMPLETE') !== false;
echo $completed && !$extraFatal
    ? "OK: shutdown ignored the uninitialized PDO handle\n"
    : "FAIL: shutdown appended an extra fatal error\n";
exit($completed && !$extraFatal ? 0 : 1);
