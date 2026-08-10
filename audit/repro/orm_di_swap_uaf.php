<?php
/* [AUDIT 2026-08-10 N1] gene_orm_get_db() only ADDREFs the object but keeps the
 * borrowed DI hashtable slot pointer. If user code invoked from inside an ORM db
 * call does Di::del('db') / Di::set('db', $other), the later
 * gene_orm_db_reset(db) / zval_ptr_dtor(db) operate on the slot, not on the
 * object we took a reference to. */

class FakeDb
{
    public $calls = [];

    public function select($table, $fields = null)
    {
        $this->calls[] = 'select';
        /* User code running inside the ORM call sequence mutates the registry. */
        Gene\Di::del('db');
        for ($i = 0; $i < 64; $i++) {
            Gene\Di::set('filler' . $i, new stdClass());
        }
        Gene\Di::set('db', new FakeDb()); /* different object in the slot */
        return $this;
    }
    public function where($a, $b = null) { $this->calls[] = 'where'; return $this; }
    public function limit($a, $b = null)  { $this->calls[] = 'limit'; return $this; }
    public function row()                 { $this->calls[] = 'row'; return ['id' => 1]; }
    public function reset()               { $this->calls[] = 'reset'; return $this; }
}

class U extends Gene\Orm\Model
{
    protected static $table = 'u';
    protected static $primaryKey = 'id';
}

$first = new FakeDb();
Gene\Di::set('db', $first);

echo "STEP A: calling U::find(1) with a db that swaps itself out mid-call\n";
$r = U::find(1);
echo "STEP B: survived, result=", json_encode($r), "\n";
echo "first->calls  = ", implode(',', $first->calls), "\n";
$now = Gene\Di::get('db');
echo "slot object   = ", ($now === $first ? 'ORIGINAL' : 'REPLACED'), "\n";
echo "slot->calls   = ", implode(',', $now->calls), "\n";
echo "refcount sanity: ", (is_object($now) ? 'ok' : 'BAD'), "\n";
gc_collect_cycles();
echo "STEP C: done\n";
