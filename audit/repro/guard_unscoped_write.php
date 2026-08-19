<?php
/**
 * Dynamic probe for plan §11 findings (P0-2, P2-5, P2-6, P1-3, P1-4).
 * Temporary verification script.
 */
class PU extends \Gene\Orm\Model
{
    protected static $table = 'pu';
    protected static $primaryKey = 'id';
    protected static $connection = 'p_db';
    protected static $timestamps = false;
}

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$db->sql('CREATE TABLE pu (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER, grp TEXT)')->execute();
\Gene\Di::set('p_db', $db);
foreach ([['a',1,'x'],['b',1,'x'],['c',0,'y']] as $r) {
    PU::create(['name'=>$r[0],'status'=>$r[1],'grp'=>$r[2]]);
}
function rows($db) { return $db->select('pu')->all(); }
function show($t, $v) { echo str_pad($t, 46), ' => ', $v, "\n"; }

echo "--- P0-2: where([]) / where('') + update/delete guard ---\n";
foreach ([['where([])->update', function () { return PU::query()->where([])->update(['name'=>'HACKED']); }],
          ["where('')->update", function () { return PU::query()->where('')->update(['name'=>'HACKED2']); }],
          ['where([])->delete', function () { return PU::query()->where([])->delete(); }],
          ["where('')->delete", function () { return PU::query()->where('')->delete(); }]] as $c) {
    try { $r = $c[1](); show($c[0], 'NO EXCEPTION, returned ' . var_export($r, true)); }
    catch (\Throwable $e) { show($c[0], 'threw: ' . $e->getMessage()); }
    show('  rows after', json_encode(rows($db), JSON_UNESCAPED_UNICODE));
}

echo "\n--- P2-5: group + count / paginate ---\n";
try {
    $c = PU::query()->group('grp')->count();
    show('group(grp)->count()', var_export($c, true) . ' (groups=2, total=3)');
} catch (\Throwable $e) { show('group(grp)->count()', 'threw: ' . $e->getMessage()); }
try {
    $p = PU::query()->group('grp')->paginate(0, 10);
    show('group(grp)->paginate()', json_encode($p));
} catch (\Throwable $e) { show('group(grp)->paginate()', 'threw: ' . $e->getMessage()); }

echo "\n--- P1-3: pool return with open transaction ---\n";
if (class_exists('\Gene\Pool') && method_exists('\Gene\Pool', 'get')) {
    show('Gene\\Pool available', 'yes');
} else {
    show('Gene\\Pool available', 'no (skip)');
}
$d2 = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$d2->sql('CREATE TABLE t (a int)')->execute();
$d2->beginTransaction();
show('inTransaction before release', var_export($d2->inTransaction(), true));
if (method_exists($d2, 'release')) {
    $d2->release();
    show('release() called', 'ok');
}
unset($d2);
show('after unset', 'no crash');

echo "\n(P1-4 -> tx_hygiene_error_handler.php; pool path needs runtime_type>=2 i.e. Swoole)\n";
