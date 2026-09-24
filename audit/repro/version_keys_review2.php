<?php
/**
 * AUDIT_REPORT_2026_09_23 §9 (second landing review of 91d0442):
 * versionKeys edge cases.
 *
 *   S1  find($id, true) -> rename mapped column -> save():
 *       the OLD key must be bumped (fails on 91d0442: save() reuses the
 *       modified attributes as the pre-write row).
 *   S2  transaction left open at request end: tx hygiene rolls it back and
 *       the pending bump must be discarded; control case: raw
 *       $pdo->commit() must be flushed by the teardown safety net.
 *
 * Run (no deploy):
 *   set GENE_VK_PHP_ARGS=-n -d extension=pdo_sqlite -d extension=<php_gene.dll>
 *   php %GENE_VK_PHP_ARGS% audit/repro/version_keys_review2.php
 * GENE_VK_PHP_ARGS is forwarded to the S2 child processes.
 * Exit 0 = S1 and S2 both OK; 1 = at least one reproduces.
 */
if (!extension_loaded('gene') || !class_exists('Gene\\Db\\Sqlite')) {
    echo "SKIP: gene + pdo_sqlite required\n";
    exit(0);
}

class RvUser extends \Gene\Orm\Model {
    protected static $table = 'u';
    protected static $primaryKey = 'id';
    protected static $fields = ['id', 'name', 'status'];
    protected static $connection = 'rv_db';
    protected static $versionKeys = ['v.id' => 'id', 'v.name' => 'name'];
}

$log = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'gene_vk_review2.log';

$cache = new class($log) {
    public $bumps = [];
    private $log;
    public function __construct($log) { $this->log = $log; }
    public function updateVersion($f) {
        $this->bumps[] = $f;
        file_put_contents($this->log, json_encode($f) . "\n", FILE_APPEND);
        return true;
    }
};

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$db->sql('CREATE TABLE u (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER)')->execute();
\Gene\Di::set('rv_db', $db);
\Gene\Di::set('cache', $cache);
$id = RvUser::create(['name' => 'old', 'status' => 1]);

if (getenv('GENE_VK_CHILD') === '1') {
    // S2 child: pending bump inside an open transaction, then exit.
    file_put_contents($log, '');
    $pdo = $db->getPdo();
    $pdo->beginTransaction();
    RvUser::updateBy($id, ['status' => 7]);
    if (getenv('GENE_VK_MODE') === 'commit') {
        $pdo->commit();
    }
    exit(0);
}

// S1
$cache->bumps = [];
$m = RvUser::find($id, true);
$m->name = 'new';
$m->save();
$names = [];
foreach ($cache->bumps as $b) {
    foreach ((array)($b['v.name'] ?? []) as $v) { $names[] = $v; }
}
$s1 = in_array('old', $names, true);
echo 'S1 bumps: ' . json_encode($cache->bumps) . "\n";
echo 'S1 ' . ($s1 ? 'OK: old key bumped' : 'BUG: old name key "old" not bumped') . "\n";

// S2
$child = function ($mode) use ($log) {
    putenv('GENE_VK_CHILD=1');
    putenv($mode ? "GENE_VK_MODE=$mode" : 'GENE_VK_MODE');
    $cmd = escapeshellarg(PHP_BINARY) . ' ' . (getenv('GENE_VK_PHP_ARGS') ?: '') . ' ' . escapeshellarg(__FILE__);
    passthru($cmd . ' > ' . (PHP_OS_FAMILY === 'Windows' ? 'NUL' : '/dev/null') . ' 2>&1');
    putenv('GENE_VK_CHILD');
    putenv('GENE_VK_MODE');
    return file($log, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) ?: [];
};
$rolled = $child(null);
$ctl = $child('commit');
echo 'S2 open tx at teardown -> bumps: ' . json_encode($rolled) . "\n";
echo 'S2 control raw commit  -> bumps: ' . json_encode($ctl) . "\n";
if (count($ctl) === 0) {
    echo "S2 INCONCLUSIVE: teardown safety net never reached updateVersion\n";
    $s2 = false;
} else {
    $s2 = count($rolled) === 0;
    echo 'S2 ' . ($s2 ? 'OK: rolled-back bucket discarded, committed bucket flushed' : 'BUG: rolled-back tx flushed its bumps') . "\n";
}
@unlink($log);
exit(($s1 && $s2) ? 0 : 1);
