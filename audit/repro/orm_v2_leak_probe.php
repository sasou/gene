<?php
/**
 * [GENE_FEATURE:2026-08-18] Memory-stability probe for the v2 APIs:
 * 10k iterations in ONE process, asserting memory_get_usage(true) stays
 * flat (FPM approximation; M1/M3 compliance check for the new ops array,
 * meta timestamp strings, createMany row copies, findMany reorder map).
 */

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$db->sql('CREATE TABLE m (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER, addtime INTEGER, updatetime INTEGER)')->execute();
\Gene\Di::set('orm_db', $db);
eval('class LM extends \\Gene\\Orm\\Model {
    protected static $table = "m";
    protected static $connection = "orm_db";
    protected static $timestamps = true;
    protected static $createdAt = "addtime";
    protected static $updatedAt = "updatetime";
    protected static $timestampFormat = "unix";
}');

LM::create(['name' => 'seed', 'status' => 1]);

function probe($label, $iters, $fn) {
    $fn(); // warm-up (meta cache, op buffers)
    gc_collect_cycles();
    $before = memory_get_usage(true);
    for ($i = 0; $i < $iters; $i++) {
        $fn();
    }
    gc_collect_cycles();
    $after = memory_get_usage(true);
    $delta = $after - $before;
    printf("%-46s %+d B (%.2f B/call)\n", $label, $delta, $delta / $iters);
    return $delta <= 0;
}

$ok = true;
$ok &= probe('query ops build+all (where/join/group/having/order/limit)', 10000, function () {
    LM::query()->where(['status' => 1])->where('name != ?', 'q')
        ->where('id', '>=', 1)->order('id desc')->limit(5)->all();
});
$ok &= probe('query paginate (count+list replay)', 10000, function () {
    LM::query()->where(['status' => 1])->paginate(0, 3);
});
$ok &= probe('findMany + preserveOrder', 10000, function () {
    LM::findMany([1, 2, 3], true);
});
$ok &= probe('in(col, []) empty latch', 10000, function () {
    LM::query()->in('id', [])->all();
});
$ok &= probe('whereLike op build', 10000, function () {
    LM::query()->whereLike('name', 'a%b_c')->all();
});
$ok &= probe('selectSub op build', 10000, function () {
    LM::query()->selectSub('SELECT count(*) FROM m', 'c')->first();
});
$ok &= probe('createMany 3 rows (meta ts per row)', 2000, function () {
    LM::createMany([
        ['name' => 'b1', 'status' => 1],
        ['name' => 'b2', 'status' => 0],
        ['name' => 'b3', 'status' => 1],
    ]);
});
$ok &= probe('updateOrCreate + toggle (timestamps meta)', 5000, function () {
    LM::updateOrCreate(['name' => 'seed'], ['status' => 1]);
    LM::toggle(1, 'status', [0, 1]);
});

echo $ok ? "LEAK PROBE OK\n" : "LEAK PROBE FAILED\n";
exit($ok ? 0 : 1);
