<?php
class PG extends \Gene\Orm\Model
{
    protected static $table = 'pg';
    protected static $primaryKey = 'id';
    protected static $connection = 'g_db';
    protected static $timestamps = false;
}
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$db->sql('CREATE TABLE pg (id INTEGER PRIMARY KEY AUTOINCREMENT, grp TEXT)')->execute();
\Gene\Di::set('g_db', $db);
// grp x: 3 rows, grp y: 1 row  => groups=2, total=4, first group count=3
foreach (['x','x','x','y'] as $g) { PG::create(['grp'=>$g]); }
function show($t, $v) { echo str_pad($t, 44), ' => ', $v, "\n"; }

echo "--- P2-5 group + count (isolated) ---\n";
show('plain count()', var_export(PG::query()->count(), true) . ' (expect 4)');
try { show('group(grp)->count()', var_export(PG::query()->group('grp')->count(), true) . ' (groups=2, firstGroup=3)'); }
catch (\Throwable $e) { show('group(grp)->count()', 'threw: ' . $e->getMessage()); }
try { show('group(grp)->paginate(0,10)', json_encode(PG::query()->group('grp')->paginate(0, 10))); }
catch (\Throwable $e) { show('group(grp)->paginate', 'threw: ' . $e->getMessage()); }
$db->reset();
echo "SQL of group count: ";
try { PG::query()->group('grp')->count(); } catch (\Throwable $e) {}
echo (method_exists($db,'print') ? var_export($db->print(), true) : 'n/a'), "\n";
