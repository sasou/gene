<?php
/* [AUDIT 2026-08-10 N1b] Variant without Di::del(): the slot stays alive but its
 * contents are replaced. The M5 fix ADDREF'd object A, yet cleanup runs
 * gene_orm_db_reset(slot) + zval_ptr_dtor(slot) — releasing a reference that
 * belongs to object B. A is leaked, B's refcount is decremented once too often. */

class FakeDb
{
    public $tag;
    public function __construct($tag) { $this->tag = $tag; }
    public function select($table, $fields = null)
    {
        Gene\Di::set('db', new FakeDb('B'));   /* replace slot contents */
        return $this;
    }
    public function where($a, $b = null) { return $this; }
    public function limit($a, $b = null)  { return $this; }
    public function row()                 { return ['id' => 1]; }
    public function reset()               { return $this; }
    public function __destruct()          { echo "  [dtor ", $this->tag, "]\n"; }
}

class U extends Gene\Orm\Model
{
    protected static $table = 'u';
    protected static $primaryKey = 'id';
}

Gene\Di::set('db', new FakeDb('A'));
echo "STEP A\n";
$r = U::find(1);
echo "STEP B result=", json_encode($r), "\n";
$b = Gene\Di::get('db');
echo "slot tag = ", (is_object($b) ? $b->tag : gettype($b)), " refcount-sensitive access ok\n";
unset($b);
echo "STEP C: touch slot again\n";
$b2 = Gene\Di::get('db');
echo "slot tag = ", (is_object($b2) ? $b2->tag : gettype($b2)), "\n";
unset($b2);
echo "STEP D done\n";
