<?php
class PV extends \Gene\Orm\Model
{
    protected static $table = 'pv';
    protected static $primaryKey = 'id';
    protected static $connection = 'v_db';
    protected static $timestamps = false;
}
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$db->sql('CREATE TABLE pv (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER)')->execute();
\Gene\Di::set('v_db', $db);
foreach ([['a',1],['b',1],['c',0]] as $r) { PV::create(['name'=>$r[0],'status'=>$r[1]]); }
function show($t, $v) { echo str_pad($t, 50), ' => ', $v, "\n"; }

echo "--- §11.2 spot checks (dynamic) ---\n";
show('3x where AND accumulation', count(PV::query()->where('status=?',1)->where('id>?',0)->where(['name'=>'a'])->all()) . ' rows (expect 1)');
show('in([]) -> all()', json_encode(PV::query()->in('id', [])->all()));
show('in([]) -> count()', var_export(PV::query()->in('id', [])->count(), true));
try { PV::query()->in('id', [])->update(['name'=>'X']); show('in([])->update()', 'no exception'); }
catch (\Throwable $e) { show('in([])->update()', 'threw: ' . $e->getMessage()); }
show('  rows intact after in([])->update', json_encode(PV::query()->all()));
try { PV::query()->where('id', 'LIKE', 1)->all(); show('bad operator LIKE', 'ACCEPTED (bad)'); }
catch (\Throwable $e) { show('bad operator LIKE', 'threw: ' . $e->getMessage()); }
show('in([1,3]) count', var_export(PV::query()->in('id', [1,3])->count(), true) . ' (expect 2)');
show('findMany preserveOrder', implode(',', array_column(PV::findMany([3,1], true), 'id')) . ' (expect 3,1)');

echo "\n--- memory stability: 10k Query builds ---\n";
$base = memory_get_usage(true);
for ($i = 0; $i < 10000; $i++) {
    PV::query()->where(['status'=>1])->in('id',[1,2])->order('id desc')->limit(1)->all();
}
show('memory_get_usage(true) delta', (memory_get_usage(true) - $base) . ' bytes');
