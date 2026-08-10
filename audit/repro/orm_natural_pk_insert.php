<?php
/* [AUDIT 2026-08-10 N2] The M1 hydration fix makes any non-empty primary key in
 * fill()'s payload imply exists=1. For tables whose primary key is client
 * generated (UUID / natural key / data import) there is now no way to insert:
 * save() always takes the UPDATE branch, updates 0 rows and returns 0 without
 * any error. There is no setExists()/forceInsert()/isNew() escape hatch, and
 * __set('exists', ...) / __unset('exists') are hard-rejected. */

$file = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'orm_natural_pk.sqlite';
@unlink($file);
$db = new Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->sql('CREATE TABLE doc (uuid TEXT PRIMARY KEY, title TEXT)')->execute();
Gene\Di::set('db', $db);

class Doc extends Gene\Orm\Model
{
    protected static $table = 'doc';
    protected static $primaryKey = 'uuid';
}

$d = new Doc();
$d->fill(['uuid' => 'u-0001', 'title' => 'hello']);
echo "exists after fill : ", var_export($d->exists, true), "\n";
$ret = $d->save();
echo "save() returned   : ", var_export($ret, true), "\n";

$rows = $db->select('doc')->all();
echo "rows in table     : ", count($rows), " -> ", json_encode($rows), "\n";

/* escape hatches */
$d2 = new Doc();
$d2->fill(['uuid' => 'u-0002', 'title' => 'x']);
echo "__set('exists',false) => ", var_export($d2->exists = false, true), "\n";
echo "exists still        : ", var_export($d2->exists, true), "\n";
unset($d2->exists);
echo "after unset(exists) : ", var_export($d2->exists, true), "\n";
echo "has setExists()     : ", var_export(method_exists($d2, 'setExists'), true), "\n";
echo "has forceInsert()   : ", var_export(method_exists($d2, 'forceInsert'), true), "\n";

/* create() still works — the only usable path */
echo "create() rows       : ", var_export(Doc::create(['uuid' => 'u-0003', 'title' => 'y']), true), "\n";
$rows = $db->select('doc')->all();
echo "rows in table       : ", count($rows), "\n";

/* [FIX 2026-08-10 N2] escape hatches must turn fill()+save() back into INSERT */
$d3 = new Doc();
$d3->fill(['uuid' => 'u-0004', 'title' => 'via fill-hydrate-off'], false);
echo "fill(,false) exists : ", var_export($d3->exists, true), "\n";
$ret3 = $d3->save();
echo "fill(,false) save() : ", var_export($ret3, true), "\n";

$d4 = new Doc();
$d4->fill(['uuid' => 'u-0005', 'title' => 'via setExists']);
$d4->setExists(false);
echo "setExists(false)    : ", var_export($d4->exists, true), "\n";
$ret4 = $d4->save();
echo "setExists save()    : ", var_export($ret4, true), "\n";

$rows = $db->select('doc')->all();
echo "rows in table       : ", count($rows), " -> ", json_encode(array_column($rows, 'uuid')), "\n";
echo (count($rows) === 3 && $ret3 === 'u-0004' && $ret4 === 'u-0005') ? "N2 FIXED\n" : "N2 STILL BROKEN\n";
@unlink($file);
