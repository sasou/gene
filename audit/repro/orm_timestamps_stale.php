<?php
eval('class T extends \\Gene\\Orm\\Model { protected static $table="ts"; protected static $connection="orm_db"; protected static $timestamps = true; }');
$f = sys_get_temp_dir() . '/gene_orm_ts.sqlite';
@unlink($f);
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $f, 'username' => '', 'password' => '']);
$db->sql('CREATE TABLE ts (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, created_at TEXT, updated_at TEXT)')->execute();
\Gene\Di::set('orm_db', $db);
echo "request_time    : ", date('Y-m-d H:i:s', (int)$_SERVER['REQUEST_TIME']), "\n";
T::create(['name' => 'first']);
sleep(3);
T::create(['name' => 'second (3s later)']);
foreach (T::findAll([]) as $r) {
    echo str_pad($r['name'], 20), " created_at=", $r['created_at'], " updated_at=", $r['updated_at'], "\n";
}
echo "wall clock now  : ", date('Y-m-d H:i:s'), "\n";
@unlink($f);
