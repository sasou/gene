<?php
/* R1: find($id, true) hydration calls the constructor without checking its
 * visibility, so a private/protected constructor (factory / singleton models)
 * is invoked from outside class scope — `new T()` in the same place would be a
 * fatal Error. Expected after a fix: ctorCalls stays 0 (or hydration is
 * documented as constructor-bypassing for non-public constructors). */
$db = new Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
Gene\Di::set('db', $db);
$db->sql("CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT)")->execute();
$db->sql("INSERT INTO t(id,name) VALUES(1,'a')")->execute();

class HidT extends Gene\Orm\Model
{
    protected static $table = 't';
    public static $ctorCalls = 0;
    private function __construct()
    {
        self::$ctorCalls++;
    }
    public static function make()
    {
        return new self();
    }
}

$m = HidT::find(1, true);
echo "hydrated class          : ", get_class($m), "\n";
echo "private ctor invoked    : ", var_export(HidT::$ctorCalls > 0, true), " (expected: false)\n";

try {
    new HidT();
    echo "new HidT()              : allowed (unexpected)\n";
} catch (Throwable $e) {
    echo "new HidT()              : ", get_class($e), " (visibility enforced here)\n";
}

/* N3(3) regression guard: zero-padded string primary keys must survive intact. */
$db->sql("CREATE TABLE p(code TEXT PRIMARY KEY, v TEXT)")->execute();
class PadP extends Gene\Orm\Model
{
    protected static $table = 'p';
    protected static $primaryKey = 'code';
}
$id = PadP::create(['code' => '007', 'v' => 'x']);
echo "create() returned       : ", var_export($id, true), " (expected: '007')\n";
$row = PadP::find('007');
$mm = (new PadP)->fill($row);
$mm->v = 'y';
echo "save() affected         : ", var_export($mm->save(), true), " (expected: 1)\n";
echo "row after save          : ", json_encode(PadP::find('007')), "\n";
echo "DONE\n";
