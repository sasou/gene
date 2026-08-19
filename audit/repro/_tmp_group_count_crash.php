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
foreach (['x','x','x','y'] as $g) { PG::create(['grp'=>$g]); }

echo "A: print on fresh db\n";
var_dump($db->print());

echo "B: throw path count\n";
try { PG::query()->group('grp')->count(); } catch (\Throwable $e) { echo "caught\n"; }
echo "B alive\n";

echo "C: print after throw\n";
var_dump($db->print());
echo "C alive\n";

echo "D: plain count then print\n";
PG::query()->count();
var_dump($db->print());
echo "D alive\n";

echo "E: unset db, shutdown\n";
unset($db);
echo "E alive\n";
