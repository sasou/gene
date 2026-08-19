<?php
/**
 * [GENE_AUDIT:2026-08-19 N6] Edge cases for the N1 guard on ORM static write
 * entry points: arrays that are NON-empty but carry no usable condition.
 *
 * N1 gates updateBy()/updateOrCreate() on "did gene_orm_apply_where() call
 * db->where()?", NOT on "did makeWhere() actually emit a predicate". These
 * shapes pass the guard; this script records what they really do so the
 * residual risk is documented instead of assumed.
 *
 * Run:
 *   php -n -d extension_dir="..." -d extension=pdo_sqlite \
 *       -d extension="...\php_gene.dll" audit/repro/guard_where_semantic_edges.php
 *
 * Exit 0 = every shape either throws or fails loudly (no silent full-table
 * write). Exit 1 = a shape silently rewrote every row (data destruction).
 */

$dbFile = sys_get_temp_dir() . '/gene_guard_edges.sqlite';
@unlink($dbFile);

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite:' . $dbFile]);
$db->sql('create table gu (id integer primary key, name text)')->execute();
\Gene\Di::set('db', $db);

class GuEdge extends \Gene\Orm\Model
{
    protected static $table = 'gu';
    protected static $primaryKey = 'id';
    protected static $connection = 'db';
    protected static $timestamps = false;
}

function seed($db)
{
    $db->sql('delete from gu')->execute();
    foreach (['a', 'b', 'c'] as $n) {
        $db->insert('gu', ['name' => $n])->affectedRows();
    }
    $db->reset();
}

function names($db)
{
    $rows = $db->select('gu')->all();
    $db->reset();
    return implode(',', array_column($rows, 'name'));
}

$shapes = [
    'numeric-key scalar  [0=>1]'   => [0 => 1],
    'numeric-key null    [0=>null]' => [0 => null],
    'empty op array      [id=>[]]' => ['id' => []],
    'op array no value   [id=>[[]]]' => ['id' => [[]]],
];

$bad = 0;
foreach ($shapes as $label => $where) {
    seed($db);
    $before = names($db);
    $outcome = '';
    try {
        $n = GuEdge::updateBy($where, ['name' => 'HACKED']);
        $outcome = "returned {$n}";
    } catch (\Throwable $e) {
        $outcome = 'threw ' . get_class($e) . ': ' . substr($e->getMessage(), 0, 60);
    }
    $after = names($db);
    $destroyed = ($after === 'HACKED,HACKED,HACKED');
    if ($destroyed) {
        $bad++;
    }
    printf("%-32s %-58s rows: %s -> %s  %s\n",
        $label, $outcome, $before, $after,
        $destroyed ? '[DATA DESTROYED]' : '[safe]');
}

/* Control: the N1-guarded shapes must throw. */
foreach (['empty array' => [], 'null' => null] as $label => $where) {
    seed($db);
    try {
        GuEdge::updateBy($where, ['name' => 'HACKED']);
        printf("%-32s %s\n", "N1 control {$label}", 'NO EXCEPTION [REGRESSION]');
        $bad++;
    } catch (\Throwable $e) {
        printf("%-32s %s\n", "N1 control {$label}", 'threw (correct)');
    }
    if (names($db) !== 'a,b,c') {
        echo "  rows modified after guard [REGRESSION]\n";
        $bad++;
    }
}

/* Leak probe: 5k guard-exception round trips must not grow ZMM, and the
 * shared Db handle must stay usable (reset must have run each time). */
$base = memory_get_usage(true);
for ($i = 0; $i < 5000; $i++) {
    try {
        GuEdge::updateBy([], ['name' => 'x']);
    } catch (\Throwable $e) {
    }
}
$delta = memory_get_usage(true) - $base;
echo "5k guard-throw loop delta: {$delta} bytes\n";
echo 'handle still usable after guard throws: ' . (names($db) === 'a,b,c' ? 'yes' : 'NO') . "\n";
if ($delta !== 0) {
    $bad++;
}

unset($db);
@unlink($dbFile);
echo $bad === 0 ? "OK\n" : "FAIL ({$bad})\n";
exit($bad === 0 ? 0 : 1);
