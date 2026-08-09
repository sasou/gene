<?php
function step($s) { fwrite(STDERR, "STEP: $s\n"); }

class U extends \Gene\Orm\Model {
    protected static $table = "orm_users";
    protected static $primaryKey = "id";
    protected static $fields = ["id", "name", "status"];
    protected static $connection = "orm_db";
}

step('connect');
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:', 'username' => '', 'password' => '']);
$db->sql('CREATE TABLE orm_users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, status INTEGER DEFAULT 1)')->execute();
\Gene\Di::set('orm_db', $db);

step('create');
$id = U::create(['name' => 'alice', 'status' => 1]);
echo "id=$id\n";

step('find');
var_dump(U::find($id));

step('updateBy');
var_dump(U::updateBy($id, ['name' => 'bob']));

step('find2');
var_dump(U::find($id));

step('paginate');
var_dump(U::paginate(['status' => 1], 0, 10));

step('query all');
var_dump(U::query()->where(['status' => 1])->order('id desc')->all());

step('query count');
var_dump(U::query()->where(['status' => 1])->count());

step('query row limit');
var_dump(U::query()->where(['id' => $id])->limit(1)->row());

step('instance save');
$m = new U();
$m->fill(['name' => 'dave', 'status' => 1]);
var_dump($m->save(), $m->toArray());

step('__set/__get');
$m2 = new U();
$m2->name = 'via_set';
$m2->status = 1;
var_dump($m2->name, $m2->save(), $m2->exists);

step('save update path');
$m2->name = 'via_update';
var_dump($m2->save());

step('instance delete');
var_dump($m2->delete(), $m2->toArray());

step('destroy');
var_dump(U::destroy($id));

step('destroyAll');
var_dump(U::destroyAll([2]));

step('DONE');
