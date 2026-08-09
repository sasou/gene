<?php
class U extends \Gene\Orm\Model {
    protected static $table = "orm_users";
    protected static $primaryKey = "id";
    protected static $fields = ["id", "name", "status"];
    protected static $connection = "orm_db";
    protected static $timestamps = false;
}

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:', 'username' => '', 'password' => '']);
$db->sql('CREATE TABLE orm_users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER)')->execute();
\Gene\Di::set('orm_db', $db);
U::create(['name' => 'a', 'status' => 1]);

function probe($label, callable $fn, $iters = 2000) {
    $fn(); $fn(); $fn();
    gc_collect_cycles();
    $m0 = memory_get_usage(true);
    $u0 = memory_get_usage();
    for ($i = 0; $i < $iters; $i++) { $fn(); }
    gc_collect_cycles();
    printf("%-34s real=%+8d bytes  used=%+8d bytes (%d iters)\n",
        $label, memory_get_usage(true) - $m0, memory_get_usage() - $u0, $iters);
}

probe('find()',            function () { U::find(1); });
probe('findAll(array)',    function () { U::findAll(['status' => 1]); });
probe('findAll(scalar)',   function () { U::findAll(1); });
probe('paginate()',        function () { U::paginate(['status' => 1], 0, 10); });
probe('query()->all()',    function () { U::query()->where(['status' => 1])->all(); });
probe('query()->count()',  function () { U::query()->where(['status' => 1])->count(); });
probe('query() abandoned', function () { $q = U::query()->where(['status' => 1])->order('id desc'); unset($q); });
probe('query()->in()',     function () { U::query()->in('id in(?)', [1])->all(); });
probe('updateBy()',        function () { U::updateBy(1, ['name' => 'z']); });
probe('fill()+toArray()',  function () { $m = new U(); $m->fill(['name' => 'q', 'status' => 1]); $m->toArray(); });
probe('__set/__get',       function () { $m = new U(); $m->name = 'q'; $x = $m->name; });
probe('Model::where()',    function () { U::where('status=?', 1)->all(); });

echo "\n-- edge cases --\n";
try { var_dump(U::find([1, 2])); } catch (Throwable $e) { echo "find(array) throws: ", get_class($e), "\n"; }
try { var_dump(U::destroy(new stdClass)); } catch (Throwable $e) { echo "destroy(obj) throws: ", get_class($e), "\n"; }
try { $q = new \Gene\Orm\Query(); } catch (Throwable $e) { echo "new Query throws: ", get_class($e), "\n"; }

echo "\n-- hydrate trap --\n";
$row = U::find(1);
$m = new U();
$m->fill($row);
$m->name = 'renamed';
$aff = $m->save();
echo "save() after fill(find()) returned: ", var_export($aff, true), "\n";
echo "rows now: ", count(U::findAll([])), " (was 1 before if UPDATE, 2 if duplicate INSERT)\n";

echo "\n-- return types --\n";
$id = U::create(['name' => 'typ', 'status' => 1]);
echo 'create() => ', gettype($id), "\n";
$mm = new U(); $mm->fill(['name' => 'typ2', 'status' => 1]);
echo 'save() insert => ', gettype($mm->save()), "\n";
echo 'count() => ', gettype(U::query()->count()), "\n";

echo "\n-- undeclared table --\n";
eval('class NoTable extends \\Gene\\Orm\\Model {}');
try { NoTable::find(1); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "\n-- missing DI service --\n";
eval('class BadConn extends \\Gene\\Orm\\Model { protected static $table = "t"; protected static $connection = "nope"; }');
try { var_dump(BadConn::find(1)); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

echo "\nDONE\n";
