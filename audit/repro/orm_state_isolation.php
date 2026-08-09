<?php
eval('class NA extends \\Gene\\Orm\\Model { protected static $table="orm_users"; protected static $connection="orm_db"; }');
eval('class BADT extends \\Gene\\Orm\\Model { protected static $table="no_such_table"; protected static $connection="orm_db"; }');
$f = sys_get_temp_dir() . '/gene_orm_audit.sqlite';
@unlink($f);
$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $f, 'username' => '', 'password' => '']);
$db->sql('CREATE TABLE orm_users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER)')->execute();
\Gene\Di::set('orm_db', $db);
$id = NA::create(['name' => 'a', 'status' => 1]);
echo "created id=", var_export($id, true), "\n";
echo "1 ORM find before        : ", json_encode(NA::find($id)), "\n";
try { BADT::find(1); } catch (Throwable $e) { echo "2 exception caught\n"; }
echo "3 ORM find after         : ", json_encode(NA::find($id)), "\n";
echo "4 ORM findAll after      : ", json_encode(NA::findAll([])), "\n";

// dirty abandoned query then a fresh query
$q = NA::query()->where('id=?', $id)->limit(1);
unset($q);
echo "5 after abandoned query  : ", json_encode(NA::findAll([])), "\n";

// exception inside a chain: does state leak into next call?
try { BADT::query()->where('1=1')->all(); } catch (Throwable $e) { echo "6 exception caught\n"; }
echo "7 ORM find after chain   : ", json_encode(NA::find($id)), "\n";

// nested/mixed driver use while ORM holds state
echo "8 mixed driver row       : ", json_encode($db->select('orm_users')->row()), "\n";
echo "9 ORM find after mixed   : ", json_encode(NA::find($id)), "\n";
@unlink($f);
echo "DONE\n";
