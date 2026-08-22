<?php
/**
 * NEW FINDING (2026-08-19): the P0-2 write guard was pushed down into
 * Gene\Orm\Query::apply() only. The NON-Query write paths still accept an
 * empty condition and emit an UNSCOPED UPDATE:
 *
 *   Model::updateBy([],   $data)   -> UPDATE t SET ...          (whole table)
 *   Model::updateBy(null, $data)   -> UPDATE t SET ...          (whole table)
 *   Model::updateOrCreate([], $d)  -> same, via its update branch
 *
 * Cause: gene_orm_apply_where() (src/orm/model.c:133-151) returns SUCCESS
 * without calling db->where() for NULL / empty-array conditions, and
 * updateBy()/updateOrCreate() never check that a condition was emitted.
 * plan/orm-v2.md §11.1 P0-2 explicitly assumed this path was
 * safe ("v1 的 Model::updateBy 要求显式 where 字符串，不存在该路径").
 *
 * Expected AFTER a fix: both calls throw and leave the rows untouched.
 */
$file = sys_get_temp_dir() . '/gene_guard_updateby.db';
@unlink($file);

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $file]);
$db->sql('CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)')->execute();
\Gene\Di::set('db', $db);

class GU extends \Gene\Orm\Model
{
    protected static $table = 't';
    protected static $primaryKey = 'id';
    protected static $timestamps = false;
    protected static $connection = 'db';
}

function seed($db)
{
    $db->sql('DELETE FROM t')->execute();
    foreach (['a', 'b', 'c'] as $i => $n) {
        $db->sql("INSERT INTO t (id, name) VALUES (" . ($i + 1) . ", '$n')")->execute();
    }
}
function names($db)
{
    return implode(',', array_column($db->select('t', 'name')->order('id asc')->all(), 'name'));
}

$fail = 0;
foreach ([['updateBy([])', []], ['updateBy(null)', null]] as [$label, $w]) {
    seed($db);
    try {
        $n = GU::updateBy($w, ['name' => 'HACKED']);
        $after = names($db);
        $bad = $after !== 'a,b,c';
        echo sprintf("%-16s no exception, affected=%s, rows=[%s] %s\n",
            $label, var_export($n, true), $after, $bad ? '<= UNSCOPED WRITE' : '');
        $fail += $bad ? 1 : 0;
    } catch (Throwable $e) {
        echo sprintf("%-16s threw: %s | rows=[%s]\n", $label, $e->getMessage(), names($db));
    }
}

seed($db);
try {
    GU::updateOrCreate([], ['name' => 'HACKED2']);
    $after = names($db);
    $bad = $after !== 'a,b,c';
    echo sprintf("%-16s no exception, rows=[%s] %s\n", 'updateOrCreate([])', $after,
        $bad ? '<= UNSCOPED WRITE' : '');
    $fail += $bad ? 1 : 0;
} catch (Throwable $e) {
    echo sprintf("%-16s threw: %s | rows=[%s]\n", 'updateOrCreate([])', $e->getMessage(), names($db));
}

echo $fail ? "RESULT: $fail unscoped-write path(s) STILL OPEN\n" : "RESULT: all guarded\n";
exit($fail ? 1 : 0);
