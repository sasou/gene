<?php
eval('class NA extends \\Gene\\Orm\\Model { protected static $table="orm_users"; protected static $connection="orm_db"; protected static $fields = ["id","name","status"]; }');
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:', 'username' => '', 'password' => '']);
$db->sql('CREATE TABLE orm_users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER)')->execute();
\Gene\Di::set('orm_db', $db);
$db->insert('orm_users', ['name' => 'a', 'status' => 1]);

// Warm up: fill the request-scoped SQL history to its 200-entry cap first.
for ($i = 0; $i < 1000; $i++) { NA::find(1); }


function probe($label, callable $fn, $iters = 5000) {
    for ($i = 0; $i < 5; $i++) $fn();
    gc_collect_cycles();
    $u0 = memory_get_usage();
    for ($i = 0; $i < $iters; $i++) $fn();
    gc_collect_cycles();
    $d = memory_get_usage() - $u0;
    printf("%-52s %+9d B (%.2f B/call)\n", $label, $d, $d / $iters);
}
probe('NA::find(1)',                                  function () { NA::find(1); });
probe('NA::query()->where(str,bind)->limit(1)->row()', function () { NA::query()->where('id=?', 1)->limit(1)->row(); });
probe('NA::paginate()',                               function () { NA::paginate(['status' => 1], 0, 5); });
probe('NA::create()+destroy()',                       function () { $id = NA::create(['name' => 'x', 'status' => 1]); NA::destroy($id); });
probe('save/delete instance',                         function () { $m = new NA(); $m->fill(['name' => 'y', 'status' => 1]); $m->save(); $m->delete(); });
probe('destroyAll',                                   function () { NA::destroyAll([999999]); });
probe('Model::where()->count()',                      function () { NA::where('status=?', 1)->count(); });
echo "DONE\n";
