<?php
eval('class NA extends \\Gene\\Orm\\Model { protected static $table="orm_users"; protected static $connection="orm_db"; protected static $fields=["id","name","status"]; }');
eval('class BADT extends \\Gene\\Orm\\Model { protected static $table="no_such_table"; protected static $connection="orm_db"; }');
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:', 'username' => '', 'password' => '']);
$db->sql('CREATE TABLE orm_users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER)')->execute();
\Gene\Di::set('orm_db', $db);
$db->insert('orm_users', ['name' => 'a', 'status' => 1]);

// warm bounded history
for ($i = 0; $i < 800; $i++) { try { BADT::find(1); } catch (Throwable $e) {} }

function probe($label, callable $fn, $iters = 3000) {
    for ($i = 0; $i < 5; $i++) $fn();
    gc_collect_cycles();
    $u0 = memory_get_usage();
    for ($i = 0; $i < $iters; $i++) $fn();
    gc_collect_cycles();
    $d = memory_get_usage() - $u0;
    printf("%-46s %+9d B (%.2f B/call)\n", $label, $d, $d / $iters);
}
probe('failing find() on missing table', function () { try { BADT::find(1); } catch (Throwable $e) {} });
probe('failing create() on missing table', function () { try { BADT::create(['a' => 1]); } catch (Throwable $e) {} });

// state pollution after exception
try { BADT::find(1); } catch (Throwable $e) { echo "caught: ", substr($e->getMessage(), 0, 60), "\n"; }
$r = NA::find(1);
echo "healthy query after exception: ", json_encode($r), "\n";

// abandoned dirty query then reuse
$q = NA::query()->where('id=?', 1)->limit(1);
unset($q);
echo "after abandoned query: ", json_encode(NA::findAll([])), "\n";
echo "DONE\n";
