<?php
/**
 * [GENE_FEATURE:2026-08-18 A0] Query v2 ordered op list — SQL text
 * regression. 3 wheres + 2 joins + group + having must ALL survive into the
 * final SQL (v1 kept only the last where/join). Run with
 * -d gene.run_environment=0 so history() records SQL.
 */

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$db->sql('CREATE TABLE a (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER)')->execute();
$db->sql('CREATE TABLE b (id INTEGER PRIMARY KEY AUTOINCREMENT, a_id INTEGER, v INTEGER)')->execute();
$db->sql('CREATE TABLE c (id INTEGER PRIMARY KEY AUTOINCREMENT, b_id INTEGER, w INTEGER)')->execute();
\Gene\Di::set('orm_db', $db);
eval('class QA extends \\Gene\\Orm\\Model { protected static $table="a"; protected static $connection="orm_db"; }');

$db->insert('a', ['name' => 'x', 'status' => 1])->lastId();
$db->insert('b', ['a_id' => 1, 'v' => 5])->lastId();
$db->insert('b', ['a_id' => 1, 'v' => 7])->lastId();
$db->insert('c', ['b_id' => 1, 'w' => 1])->lastId();

$rows = QA::query()
    ->fields(['a.id', 'a.name'])
    ->join('b', ['b.a_id' => 'a.id'], 'INNER')
    ->join('c', ['c.b_id' => 'b.id'], 'LEFT')
    ->where(['a.status' => 1])
    ->where('a.name != ?', 'zzz')
    ->where('a.id', '>=', 1)
    ->group('a.id')
    ->having('count(b.id) >= 1')
    ->order('a.id desc')
    ->limit(0, 10)
    ->all();

$h = $db->history();
$sql = end($h)['sql'] ?? '';
echo "SQL: $sql\n";
echo "rows: ", json_encode($rows), "\n";

$checks = [
    'both joins'      => strpos($sql, 'INNER JOIN') !== false && strpos($sql, 'LEFT JOIN') !== false,
    'AND connectors'  => substr_count($sql, ' AND ') >= 2,
    'array where'     => strpos($sql, '`a`.`status` = ?') !== false,
    'string where'    => strpos($sql, 'a.name != ?') !== false,
    '3-arg where'     => strpos($sql, 'a.id >= ?') !== false,
    'group'           => strpos($sql, 'GROUP BY') !== false,
    'having'          => strpos($sql, 'HAVING') !== false,
    'order'           => strpos($sql, 'ORDER BY') !== false,
    'limit'           => stripos($sql, 'limit') !== false,
    'result row'      => is_array($rows) && count($rows) === 1 && ($rows[0]['name'] ?? '') === 'x',
];
$fail = 0;
foreach ($checks as $label => $pass) {
    echo ($pass ? 'OK   ' : 'FAIL '), $label, "\n";
    if (!$pass) $fail++;
}

// AND: count phase must not carry order/limit
$cnt = QA::query()->where(['a.status' => 1])->order('a.id desc')->limit(5)->count();
$h = $db->history();
$csql = end($h)['sql'] ?? '';
echo "count SQL: $csql\n";
if (strpos($csql, 'ORDER BY') === false && stripos($csql, 'limit') === false && $cnt === 1) {
    echo "OK   count phase drops order/limit\n";
} else {
    echo "FAIL count phase\n";
    $fail++;
}

exit($fail ? 1 : 0);
