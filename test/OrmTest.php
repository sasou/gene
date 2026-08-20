<?php

/**
 * Gene\Orm\Model / Query smoke tests
 *
 * Uses SQLite in-memory when available via DI; otherwise validates class surface only.
 */

class OrmTest
{
    private $passed = 0;
    private $failed = 0;

    public function __construct()
    {
        echo "=== Gene ORM Test Suite ===\n\n";
    }

    private function ok($msg)
    {
        echo "✓ $msg\n";
        $this->passed++;
    }

    private function fail($msg)
    {
        echo "✗ $msg\n";
        $this->failed++;
    }

    public function testClassSurface()
    {
        echo "Testing ORM class surface:\n";

        if (!class_exists('\\Gene\\Orm\\Model')) {
            $this->fail('Gene\\Orm\\Model not loaded (rebuild gene extension)');
            return;
        }
        $this->ok('Gene\\Orm\\Model exists');

        if (!class_exists('\\Gene\\Orm\\Query')) {
            $this->fail('Gene\\Orm\\Query not loaded');
            return;
        }
        $this->ok('Gene\\Orm\\Query exists');

        if (!is_subclass_of('\\Gene\\Orm\\Model', '\\Gene\\Model')) {
            $this->fail('Orm\\Model should extend Gene\\Model');
        } else {
            $this->ok('Orm\\Model extends Gene\\Model');
        }

        $methods = [
            'find', 'findAll', 'paginate', 'query', 'where',
            'create', 'updateBy', 'destroy', 'destroyAll',
            'fill', 'save', 'delete', 'toArray', 'getInstance', 'setExists',
            'findMany', 'createMany', 'insertIgnore', 'updateOrCreate', 'toggle',
            'transaction', 'transact',
            '__get', '__set', '__isset', '__unset',
        ];
        foreach ($methods as $m) {
            if (!method_exists('\\Gene\\Orm\\Model', $m)) {
                $this->fail("missing method Model::$m");
            } else {
                $this->ok("Model::$m");
            }
        }

        foreach (['where', 'in', 'order', 'limit', 'all', 'row', 'cell', 'count',
            'join', 'group', 'having', 'fields', 'first', 'paginate', 'update', 'delete',
            'lockForUpdate', 'sharedLock', 'selectSub', 'whereLike'] as $m) {
            if (!method_exists('\\Gene\\Orm\\Query', $m)) {
                $this->fail("missing method Query::$m");
            } else {
                $this->ok("Query::$m");
            }
        }
    }

    public function testSqliteCrud()
    {
        echo "\nTesting ORM CRUD (SQLite):\n";

        if (!class_exists('\\Gene\\Orm\\Model') || !class_exists('\\Gene\\Db\\Sqlite')) {
            $this->fail('skip CRUD — extension classes missing');
            return;
        }

        // Inline model for the test
        if (!class_exists('OrmTestUser')) {
            eval('class OrmTestUser extends \\Gene\\Orm\\Model {
                protected static $table = "orm_users";
                protected static $primaryKey = "id";
                protected static $fields = ["id", "name", "status"];
                protected static $connection = "orm_db";
            }');
        }

        try {
            $db = new \Gene\Db\Sqlite([
                'dsn' => 'sqlite::memory:',
            ]);
            $db->sql('CREATE TABLE orm_users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                status INTEGER DEFAULT 1
            )')->execute();

            \Gene\Di::set('orm_db', $db);

            $id = OrmTestUser::create(['name' => 'alice', 'status' => 1]);
            if ($id > 0) {
                $this->ok("create() returned id=$id");
            } else {
                $this->fail('create() failed');
                return;
            }

            $row = OrmTestUser::find($id);
            if (is_array($row) && ($row['name'] ?? '') === 'alice') {
                $this->ok('find() returns row');
            } else {
                $this->fail('find() mismatch: ' . json_encode($row));
            }

            $n = OrmTestUser::updateBy($id, ['name' => 'bob']);
            if ($n >= 0) {
                $this->ok("updateBy() affected=$n");
            } else {
                $this->fail('updateBy() failed');
            }

            $row2 = OrmTestUser::find($id);
            if (is_array($row2) && ($row2['name'] ?? '') === 'bob') {
                $this->ok('update reflected in find()');
            } else {
                $this->fail('update not visible');
            }

            $page = OrmTestUser::paginate(['status' => 1], 0, 10);
            if (is_array($page) && isset($page['count'], $page['list']) && $page['count'] >= 1) {
                $this->ok('paginate() ok count=' . $page['count']);
            } else {
                $this->fail('paginate() bad: ' . json_encode($page));
            }

            $list = OrmTestUser::query()->where(['status' => 1])->order('id desc')->all();
            if (is_array($list) && count($list) >= 1) {
                $this->ok('query()->where()->order()->all()');
            } else {
                $this->fail('query chain failed');
            }

            $cnt = OrmTestUser::query()->where(['status' => 1])->count();
            if (is_int($cnt) && $cnt >= 1) {
                $this->ok("query()->count() returns int=$cnt");
            } else {
                $this->fail('count() type/value: ' . var_export($cnt, true));
            }

            $countUsers = function () use ($db) {
                $rows = $db->select('orm_users')->all();
                return is_array($rows) ? count($rows) : 0;
            };
            $before = $countUsers();
            $newId = OrmTestUser::transaction(function () {
                return OrmTestUser::create(['name' => 'tx-ok', 'status' => 1]);
            });
            $afterOk = $countUsers();
            if ($newId > 0 && $afterOk === $before + 1 && !$db->inTransaction()) {
                $this->ok('Model::transaction() commits');
            } else {
                $this->fail("transaction commit: id=$newId count $before->$afterOk inTx=" . var_export($db->inTransaction(), true));
            }

            $beforeFail = $countUsers();
            try {
                OrmTestUser::transaction(function () {
                    OrmTestUser::create(['name' => 'tx-fail', 'status' => 1]);
                    throw new \RuntimeException('orm tx rollback');
                });
                $this->fail('transaction should rethrow');
            } catch (\RuntimeException $e) {
                $afterFail = $countUsers();
                if ($afterFail === $beforeFail && !$db->inTransaction() && strpos($e->getMessage(), 'orm tx rollback') !== false) {
                    $this->ok('Model::transaction() rolls back and rethrows');
                } else {
                    $this->fail("transaction rollback: count $beforeFail->$afterFail inTx=" . var_export($db->inTransaction(), true));
                }
            }

            $nested = $db->transaction(function () use ($db) {
                $id1 = OrmTestUser::create(['name' => 'outer', 'status' => 1]);
                $id2 = OrmTestUser::transaction(function () {
                    return OrmTestUser::create(['name' => 'inner', 'status' => 1]);
                });
                return [$id1, $id2, $db->inTransaction()];
            });
            if (is_array($nested) && $nested[0] > 0 && $nested[1] > 0 && $nested[2] === true && !$db->inTransaction()) {
                $this->ok('nested transaction() does not double-begin');
            } else {
                $this->fail('nested transaction: ' . json_encode($nested) . ' inTx=' . var_export($db->inTransaction(), true));
            }

            $one = OrmTestUser::query()->where(['id' => $id])->limit(1)->row();
            if (is_array($one) && ($one['name'] ?? '') === 'bob') {
                $this->ok('query()->row() + limit(1)');
            } else {
                $this->fail('row/limit failed: ' . json_encode($one));
            }

            // Chain isolation: second find must not inherit prior where
            OrmTestUser::create(['name' => 'carol', 'status' => 0]);
            $a = OrmTestUser::find($id);
            $b = OrmTestUser::findAll(['status' => 0]);
            if (is_array($a) && ($a['name'] ?? '') === 'bob' && is_array($b) && count($b) >= 1) {
                $this->ok('no chain pollution across static calls');
            } else {
                $this->fail('chain pollution suspected');
            }

            $m = new OrmTestUser();
            $m->fill(['name' => 'dave', 'status' => 1]);
            $newId = $m->save();
            $arr = $m->toArray();
            if ($newId > 0 && !empty($arr['id']) && ($arr['name'] ?? '') === 'dave') {
                $this->ok("instance fill/save/toArray id=$newId");
            } else {
                $this->fail('instance save failed');
            }

            // __get/__set attributes
            $m2 = new OrmTestUser();
            $m2->name = 'via_set';
            $m2->status = 1;
            if ($m2->name === 'via_set' && ($m2->toArray()['name'] ?? '') === 'via_set') {
                $this->ok('__get/__set attributes');
            } else {
                $this->fail('__get/__set failed');
            }
            $updId = $m2->save();
            if ($updId > 0 && $m2->exists) {
                $this->ok("save via __set id=$updId exists=true");
            } else {
                $this->fail('save via __set failed');
            }

            // save() update path
            $m2->name = 'via_update';
            $aff = $m2->save();
            $again = OrmTestUser::find($updId);
            if ($aff >= 0 && is_array($again) && ($again['name'] ?? '') === 'via_update') {
                $this->ok('save() update path');
            } else {
                $this->fail('save update path failed');
            }

            // instance delete clears pk
            $delN = $m2->delete();
            $afterDel = $m2->toArray();
            if ($delN >= 0 && empty($afterDel['id']) && !$m2->exists && OrmTestUser::find($updId) === null) {
                $this->ok('instance delete() clears pk + exists');
            } else {
                $this->fail('instance delete failed: ' . json_encode($afterDel));
            }

            $del = OrmTestUser::destroy($id);
            if ($del >= 0 && OrmTestUser::find($id) === null) {
                $this->ok('destroy() + find null');
            } else {
                $this->fail('destroy/find null failed');
            }

            $delAll = OrmTestUser::destroyAll([$newId]);
            if ($delAll >= 0) {
                $this->ok("destroyAll()=$delAll");
            } else {
                $this->fail('destroyAll failed');
            }

            // create() must not mutate caller array
            $payload = ['name' => 'immutable', 'status' => 1];
            $copy = $payload;
            OrmTestUser::create($payload);
            if ($payload === $copy) {
                $this->ok('create() does not mutate caller array');
            } else {
                $this->fail('create() mutated caller: ' . json_encode($payload));
            }

            // find rejects non-scalar
            $threw = false;
            try {
                OrmTestUser::find(['id' => 1]);
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                $this->ok('find() rejects non-scalar id');
            } else {
                $this->fail('find() should reject array id');
            }

            // M2 (2026-08-09): create()/save() return int ids, not strings
            $intId = OrmTestUser::create(['name' => 'typed', 'status' => 1]);
            if (is_int($intId) && $intId > 0) {
                $this->ok("create() returns int id=$intId");
            } else {
                $this->fail('create() type: ' . gettype($intId));
            }

            // M1 (2026-08-09): fill(find()) + save() must UPDATE, not re-INSERT
            $before = count(OrmTestUser::findAll([]));
            $m3 = new OrmTestUser();
            $m3->fill(OrmTestUser::find($intId));
            if ($m3->exists) {
                $this->ok('fill(row with pk) sets exists=true');
            } else {
                $this->fail('fill() did not set exists');
            }
            $m3->name = 'hydrated';
            $affUpd = $m3->save();
            $after = count(OrmTestUser::findAll([]));
            $check = OrmTestUser::find($intId);
            if ($affUpd >= 0 && $after === $before && ($check['name'] ?? '') === 'hydrated') {
                $this->ok('fill(find())->save() is UPDATE (no duplicate row)');
            } else {
                $this->fail("hydrate trap: before=$before after=$after aff=$affUpd");
            }

            // M1: find($id, true) returns a hydrated model instance
            $inst = OrmTestUser::find($intId, true);
            if ($inst instanceof OrmTestUser && $inst->exists && ($inst->name ?? '') === 'hydrated') {
                $this->ok('find($id, true) returns model instance');
            } else {
                $this->fail('find(asModel) mismatch: ' . gettype($inst));
            }
            $inst->name = 'model_save';
            $inst->save();
            if ((OrmTestUser::find($intId)['name'] ?? '') === 'model_save') {
                $this->ok('model instance save() UPDATE path');
            } else {
                $this->fail('model instance save failed');
            }
            if (OrmTestUser::find(999999, true) === null) {
                $this->ok('find(missing, true) returns null');
            } else {
                $this->fail('find(missing, true) should be null');
            }

            // M7 (2026-08-09): __isset/__unset operate on attributes
            $m4 = new OrmTestUser();
            $m4->name = 'isset_test';
            if (isset($m4->name) && !isset($m4->missing)) {
                $this->ok('isset() sees attributes');
            } else {
                $this->fail('__isset broken');
            }
            unset($m4->name);
            if (!isset($m4->name)) {
                $this->ok('unset() removes attribute');
            } else {
                $this->fail('__unset broken');
            }

            // N2/N3 (2026-08-10): client-generated primary keys — escape hatches,
            // zero-row UPDATE warning, payload-pk passthrough, ctor on hydrate
            if (!class_exists('OrmTestDoc')) {
                eval('class OrmTestDoc extends \\Gene\\Orm\\Model {
                    protected static $table = "orm_docs";
                    protected static $primaryKey = "uuid";
                    protected static $connection = "orm_db";
                }');
                eval('class OrmTestDocCtor extends \\Gene\\Orm\\Model {
                    protected static $table = "orm_docs";
                    protected static $primaryKey = "uuid";
                    protected static $connection = "orm_db";
                    public function __construct() { $this->ctorRan = true; }
                }');
            }
            $db->sql('CREATE TABLE IF NOT EXISTS orm_docs (uuid TEXT PRIMARY KEY, title TEXT)')->execute();

            $d1 = new OrmTestDoc();
            $d1->fill(['uuid' => 't-0001', 'title' => 'x']);
            $notice = null;
            set_error_handler(function ($no, $str) use (&$notice) { $notice = $str; return true; }, E_NOTICE);
            $r1 = $d1->save();
            restore_error_handler();
            if ($r1 === 0 && $notice !== null && count(OrmTestDoc::findAll([])) === 0) {
                $this->ok('N2: hydrated save() on missing row warns (E_NOTICE), no silent write');
            } else {
                $this->fail('N2: save()=' . var_export($r1, true) . ' notice=' . var_export($notice, true));
            }

            $d2 = new OrmTestDoc();
            $d2->fill(['uuid' => 't-0002', 'title' => 'y'], false);
            $r2 = $d2->save();
            if ($r2 === 't-0002' && $d2->exists && count(OrmTestDoc::findAll([])) === 1) {
                $this->ok('N2/N3: fill($data, false) + save() INSERTs, returns payload pk');
            } else {
                $this->fail('N2 fill-hydrate-off: ' . var_export($r2, true));
            }

            $d3 = new OrmTestDoc();
            $d3->fill(['uuid' => 't-0003', 'title' => 'z']);
            $d3->setExists(false);
            $r3 = $d3->save();
            if ($r3 === 't-0003' && count(OrmTestDoc::findAll([])) === 2) {
                $this->ok('N2: setExists(false) escape hatch INSERTs');
            } else {
                $this->fail('N2 setExists: ' . var_export($r3, true));
            }

            $r4 = OrmTestDoc::create(['uuid' => 't-0004', 'title' => 'w']);
            if ($r4 === 't-0004' && count(OrmTestDoc::findAll([])) === 3) {
                $this->ok('N3: create() returns payload pk (not rowid) on natural-key table');
            } else {
                $this->fail('N3 create pk: ' . var_export($r4, true));
            }

            $di = OrmTestDocCtor::find('t-0002', true);
            if ($di instanceof OrmTestDocCtor && $di->ctorRan === true) {
                $this->ok('N3: find($id, true) runs no-arg subclass constructor');
            } else {
                $this->fail('N3 ctor on hydrate broken');
            }

            OrmTestUser::destroy($intId);
        } catch (\Throwable $e) {
            $this->fail('CRUD exception: ' . $e->getMessage());
        }
    }

    /**
     * Last executed SQL for a db handle via history(). Returns null when
     * history is disabled (gene.run_environment=1) — callers skip text
     * assertions gracefully in that case.
     */
    private function lastSql($db)
    {
        $h = $db->history();
        if (!is_array($h) || !count($h)) {
            return null;
        }
        $last = end($h);
        return $last['sql'] ?? null;
    }

    public function testQueryOpsList()
    {
        echo "\nTesting Query ops list / v2 API (SQLite):\n";

        if (!class_exists('\\Gene\\Orm\\Model') || !class_exists('\\Gene\\Db\\Sqlite')) {
            $this->fail('skip Query ops — extension classes missing');
            return;
        }

        try {
            $db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
            $db->sql('CREATE TABLE q_users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, status INTEGER DEFAULT 1)')->execute();
            $db->sql('CREATE TABLE q_orders (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, amount INTEGER)')->execute();
            \Gene\Di::set('q_db', $db);

            if (!class_exists('OrmTestQUser')) {
                eval('class OrmTestQUser extends \\Gene\\Orm\\Model {
                    protected static $table = "q_users";
                    protected static $connection = "q_db";
                }');
                eval('class OrmTestQOrder extends \\Gene\\Orm\\Model {
                    protected static $table = "q_orders";
                    protected static $connection = "q_db";
                }');
            }

            OrmTestQUser::create(['name' => 'u1', 'status' => 1]);
            OrmTestQUser::create(['name' => 'u2', 'status' => 1]);
            OrmTestQUser::create(['name' => 'u3', 'status' => 0]);
            OrmTestQOrder::create(['user_id' => 1, 'amount' => 10]);
            OrmTestQOrder::create(['user_id' => 1, 'amount' => 20]);
            OrmTestQOrder::create(['user_id' => 2, 'amount' => 5]);

            // [A0] 3 wheres (mixed array/string/3-arg) must ALL apply — v1
            // silently kept only the last one.
            $rows = OrmTestQUser::query()
                ->where(['status' => 1])
                ->where('name != ?', 'u9')
                ->where('id', '>=', 1)
                ->all();
            if (is_array($rows) && count($rows) === 2) {
                $this->ok('A0: 3 mixed wheres accumulate (2 rows)');
            } else {
                $this->fail('A0 multi-where: ' . json_encode($rows));
            }
            $sql = $this->lastSql($db);
            if ($sql !== null) {
                if (strpos($sql, ' AND ') !== false && strpos($sql, 'name != ?') !== false
                    && strpos($sql, 'id >= ?') !== false) {
                    $this->ok("A0: SQL has AND connectors: $sql");
                } else {
                    $this->fail("A0 SQL text: $sql");
                }
            }

            // [A0] multiple joins + group + having
            $rows = OrmTestQUser::query()
                ->fields(['q_users.id', 'name'])
                ->join('q_orders', ['q_orders.user_id' => 'q_users.id'], 'INNER')
                ->group('q_users.id')
                ->having('count(q_orders.id) >= 2')
                ->selectSub('SELECT count(*) FROM q_orders', 'oc')
                ->all();
            $sql = $this->lastSql($db);
            if (is_array($rows) && count($rows) === 1 && ($rows[0]['name'] ?? '') === 'u1'
                && isset($rows[0]['oc']) && (int)$rows[0]['oc'] === 3) {
                $this->ok('A0/4.2: join+group+having+selectSub result');
            } else {
                $this->fail('join/group/having/selectSub: ' . json_encode($rows));
            }
            if ($sql !== null) {
                if (strpos($sql, 'INNER JOIN') !== false && strpos($sql, 'GROUP BY') !== false
                    && strpos($sql, 'HAVING') !== false && strpos($sql, ') AS `oc`') !== false) {
                    $this->ok("A0: SQL text: $sql");
                } else {
                    $this->fail("A0 join SQL: $sql");
                }
            }

            // 3-arg where rejects non-whitelisted operator
            $threw = false;
            try {
                OrmTestQUser::query()->where('id', '; DROP TABLE q_users; --', 1)->all();
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                $this->ok('where($col, $op, $val) rejects rogue operator');
            } else {
                $this->fail('where() operator whitelist not enforced');
            }

            // in() column form + empty-array semantics
            $rows = OrmTestQUser::query()->in('id', [1, 3])->order('id asc')->all();
            if (is_array($rows) && count($rows) === 2 && ($rows[0]['id'] ?? 0) == 1 && ($rows[1]['id'] ?? 0) == 3) {
                $this->ok('in($col, array) expands');
            } else {
                $this->fail('in($col, array): ' . json_encode($rows));
            }
            $before = count((array)$db->history());
            $rows = OrmTestQUser::query()->in('id', [])->all();
            $after = count((array)$db->history());
            if (is_array($rows) && count($rows) === 0 && $after === $before) {
                $this->ok('in($col, []) returns [] with NO sql');
            } else {
                $this->fail("empty in: rows=" . json_encode($rows) . " sql_delta=" . ($after - $before));
            }
            $cnt = OrmTestQUser::query()->in('id', [])->count();
            if ($cnt === 0) {
                $this->ok('in($col, []) -> count()=0');
            } else {
                $this->fail('empty in count: ' . var_export($cnt, true));
            }
            $pg = OrmTestQUser::query()->in('id', [])->paginate(0, 10);
            if (is_array($pg) && $pg['count'] === 0 && $pg['list'] === []) {
                $this->ok('in($col, []) -> paginate {0, []}');
            } else {
                $this->fail('empty in paginate: ' . json_encode($pg));
            }

            // first()
            $one = OrmTestQUser::query()->order('id desc')->first();
            if (is_array($one) && ($one['name'] ?? '') === 'u3') {
                $this->ok('first() = limit(1)+row()');
            } else {
                $this->fail('first(): ' . json_encode($one));
            }

            // Query::paginate inherits order on list, count unpolluted
            $pg = OrmTestQUser::query()->where('status', '=', 1)->order('id desc')->paginate(0, 1);
            if (is_array($pg) && $pg['count'] === 2 && count($pg['list']) === 1
                && ($pg['list'][0]['name'] ?? '') === 'u2') {
                $this->ok('Query::paginate(offset, limit) + order');
            } else {
                $this->fail('Query::paginate: ' . json_encode($pg));
            }

            // Model::paginate 4th arg order
            $pg = OrmTestQUser::paginate(['status' => 1], 0, 1, 'id desc');
            if (is_array($pg) && $pg['count'] === 2 && ($pg['list'][0]['name'] ?? '') === 'u2') {
                $this->ok('Model::paginate(..., $order)');
            } else {
                $this->fail('Model::paginate order: ' . json_encode($pg));
            }

            // Query::update / delete (immediate, require conditions)
            $n = OrmTestQUser::query()->where(['name' => 'u3'])->update(['status' => 9]);
            $chk = OrmTestQUser::query()->where(['name' => 'u3'])->first();
            if ($n === 1 && ($chk['status'] ?? null) == 9) {
                $this->ok('Query::update executes (affected=1)');
            } else {
                $this->fail("Query::update n=$n row=" . json_encode($chk));
            }
            $threw = false;
            try {
                OrmTestQUser::query()->update(['status' => 0]);
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                $this->ok('Query::update without where throws');
            } else {
                $this->fail('Query::update without where did not throw');
            }
            $threw = false;
            try {
                OrmTestQUser::query()->join('q_orders', ['q_orders.user_id' => 'q_users.id'])
                    ->where('id', '>=', 1)->delete();
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                $this->ok('Query::delete with join throws');
            } else {
                $this->fail('Query::delete with join did not throw');
            }
            $n = OrmTestQUser::query()->where(['name' => 'u3'])->delete();
            if ($n === 1 && OrmTestQUser::query()->where(['name' => 'u3'])->count() === 0) {
                $this->ok('Query::delete executes (affected=1)');
            } else {
                $this->fail("Query::delete n=$n");
            }

            // [P0-2] where([]) / where('') replay to NOTHING — update()/delete()
            // must throw (guard lives in apply(), not a tag pre-check) and the
            // table must stay untouched. 4 paths: {array,string} x {update,delete}.
            $rowsBefore = OrmTestQUser::query()->count();
            foreach ([
                'where([])->update' => function () { OrmTestQUser::query()->where([])->update(['status' => 7]); },
                "where('')->update" => function () { OrmTestQUser::query()->where('')->update(['status' => 7]); },
                'where([])->delete' => function () { OrmTestQUser::query()->where([])->delete(); },
                "where('')->delete" => function () { OrmTestQUser::query()->where('')->delete(); },
            ] as $label => $fn) {
                $threw = false;
                try {
                    $fn();
                } catch (\Throwable $e) {
                    $threw = true;
                }
                if ($threw) {
                    $this->ok("P0-2: $label throws (no unscoped write)");
                } else {
                    $this->fail("P0-2: $label did NOT throw");
                }
            }
            if (OrmTestQUser::query()->count() === $rowsBefore
                && OrmTestQUser::query()->where(['status' => 7])->count() === 0) {
                $this->ok('P0-2: table untouched by rejected writes');
            } else {
                $this->fail('P0-2: rejected write still modified rows');
            }

            // [P0-2 order constraint] in([]) stays a SAFE no-op (emptyResult
            // early-exit runs before the guard): returns 0, never throws.
            $threw = false;
            $nUpd = $nDel = -1;
            try {
                $nUpd = OrmTestQUser::query()->in('id', [])->update(['status' => 7]);
                $nDel = OrmTestQUser::query()->in('id', [])->delete();
            } catch (\Throwable $e) {
                $threw = true;
            }
            if (!$threw && $nUpd === 0 && $nDel === 0 && OrmTestQUser::query()->count() === $rowsBefore) {
                $this->ok('P0-2: in([])->update()/delete() = safe no-op (no throw, 0 rows)');
            } else {
                $this->fail("P0-2: in([]) semantics broke (threw=" . var_export($threw, true) . " upd=$nUpd del=$nDel)");
            }

            // [N1] Same semantic guard on the NON-Query write entries:
            // updateBy([]) / updateBy(null) / updateOrCreate([]) must throw
            // BEFORE affectedRows() (Db is lazy — no SQL has executed) and
            // the table must stay untouched.
            foreach ([
                'updateBy([])'        => function () { OrmTestQUser::updateBy([], ['status' => 7]); },
                'updateBy(null)'      => function () { OrmTestQUser::updateBy(null, ['status' => 7]); },
                'updateOrCreate([])'  => function () { OrmTestQUser::updateOrCreate([], ['name' => 'x', 'status' => 7]); },
            ] as $label => $fn) {
                $threw = false;
                try {
                    $fn();
                } catch (\Throwable $e) {
                    $threw = true;
                }
                if ($threw) {
                    $this->ok("N1: $label throws (no unscoped write)");
                } else {
                    $this->fail("N1: $label did NOT throw");
                }
            }
            if (OrmTestQUser::query()->count() === $rowsBefore
                && OrmTestQUser::query()->where(['status' => 7])->count() === 0) {
                $this->ok('N1: table untouched by rejected model writes');
            } else {
                $this->fail('N1: rejected model write still modified rows');
            }

            // [P2-5] group() + count()/paginate() must throw (count over
            // GROUP BY would silently return the first group's row count).
            $threw = false;
            try {
                OrmTestQUser::query()->group('status')->count();
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                $this->ok('P2-5: group()->count() throws');
            } else {
                $this->fail('P2-5: group()->count() did not throw');
            }
            $threw = false;
            try {
                OrmTestQUser::query()->group('status')->paginate(0, 10);
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                $this->ok('P2-5: group()->paginate() throws');
            } else {
                $this->fail('P2-5: group()->paginate() did not throw');
            }
            // group() + all() keeps working (only the count phase is refused)
            $grows = OrmTestQUser::query()->group('status')->all();
            if (is_array($grows) && count($grows) >= 1) {
                $this->ok('P2-5: group()->all() unaffected');
            } else {
                $this->fail('P2-5: group()->all() broke: ' . json_encode($grows));
            }

            // whereLike escapes % and _
            OrmTestQUser::create(['name' => '100%real', 'status' => 1]);
            OrmTestQUser::create(['name' => '100xreal', 'status' => 1]);
            $rows = OrmTestQUser::query()->whereLike('name', '100%r')->all();
            if (is_array($rows) && count($rows) === 1 && ($rows[0]['name'] ?? '') === '100%real') {
                $this->ok('whereLike escapes % (did not match 100xreal)');
            } else {
                $this->fail('whereLike: ' . json_encode($rows));
            }
            $sql = $this->lastSql($db);
            if ($sql !== null) {
                if (strpos($sql, 'LIKE ? ESCAPE') !== false) {
                    $this->ok("whereLike SQL: $sql");
                } else {
                    $this->fail("whereLike SQL: $sql");
                }
            }

            // lockForUpdate on sqlite: E_NOTICE no-op, query still works
            $notice = null;
            set_error_handler(function ($no, $str) use (&$notice) { $notice = $str; return true; }, E_NOTICE);
            $rows = OrmTestQUser::query()->where('id', '>=', 1)->lockForUpdate()->all();
            restore_error_handler();
            if ($notice !== null && is_array($rows) && count($rows) >= 1) {
                $this->ok('lockForUpdate no-op + E_NOTICE on sqlite');
            } else {
                $this->fail('sqlite lockForUpdate: notice=' . var_export($notice, true));
            }

            // [N7] Non-empty but semantically-empty $where arrays must NOT
            // silently rewrite every row. The N1 guard keys on "did
            // gene_orm_apply_where() call db->where()?", so these shapes pass
            // the guard; makeWhere() then either emits a bare " WHERE " (loud
            // PDOException) or returns no predicate (0 rows). Either outcome
            // is acceptable; the only failure mode is a silent full-table write.
            $rowsBeforeEdge = OrmTestQUser::query()->count();
            $edgeBad = 0;
            foreach ([
                'numeric-key scalar [0=>1]'   => [0 => 1],
                'numeric-key null   [0=>null]' => [0 => null],
                'empty op array     [id=>[]]' => ['id' => []],
                'op array no value  [id=>[[]]]' => ['id' => [[]]],
            ] as $label => $where) {
                $threw = false;
                try {
                    OrmTestQUser::updateBy($where, ['status' => 7]);
                } catch (\Throwable $e) {
                    $threw = true;
                }
                $after = OrmTestQUser::query()->where(['status' => 7])->count();
                if ($after > 0) {
                    $edgeBad++;
                    $this->fail("N7: $label silently rewrote rows (status=7 count=$after)");
                } else {
                    $this->ok("N7: $label " . ($threw ? 'throws (loud)' : 'no-op (0 rows)') . " — no silent full-table write");
                }
            }
            if (OrmTestQUser::query()->count() === $rowsBeforeEdge && $edgeBad === 0) {
                $this->ok('N7: table untouched by semantically-empty $where shapes');
            } else {
                $this->fail("N7: rows changed (before=$rowsBeforeEdge after=" . OrmTestQUser::query()->count() . " bad=$edgeBad)");
            }

            OrmTestQUser::destroyAll([]); // no-op sanity (0 rows)
        } catch (\Throwable $e) {
            $this->fail('Query ops exception: ' . $e->getMessage());
        }
    }

    public function testBatchAndIdempotent()
    {
        echo "\nTesting createMany / insertIgnore / updateOrCreate / findMany / toggle (SQLite):\n";

        if (!class_exists('\\Gene\\Orm\\Model') || !class_exists('\\Gene\\Db\\Sqlite')) {
            $this->fail('skip batch — extension classes missing');
            return;
        }

        try {
            $db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
            $db->sql('CREATE TABLE b_items (id INTEGER PRIMARY KEY AUTOINCREMENT, sku TEXT UNIQUE, qty INTEGER DEFAULT 0)')->execute();
            \Gene\Di::set('b_db', $db);

            if (!class_exists('OrmTestBItem')) {
                eval('class OrmTestBItem extends \\Gene\\Orm\\Model {
                    protected static $table = "b_items";
                    protected static $connection = "b_db";
                }');
            }

            // createMany: one round-trip, returns affected rows
            $n = OrmTestBItem::createMany([
                ['sku' => 'a', 'qty' => 1],
                ['sku' => 'b', 'qty' => 2],
                ['sku' => 'c', 'qty' => 3],
            ]);
            if ($n === 3 && OrmTestBItem::query()->count() === 3) {
                $this->ok('createMany 3 rows, affected=3');
            } else {
                $this->fail("createMany n=$n");
            }

            // key-order mismatch must throw (VALUES align by position)
            $threw = false;
            try {
                OrmTestBItem::createMany([
                    ['sku' => 'd', 'qty' => 4],
                    ['qty' => 5, 'sku' => 'e'],
                ]);
            } catch (\Throwable $e) {
                $threw = true;
            }
            if ($threw) {
                $this->ok('createMany rejects reordered keys');
            } else {
                $this->fail('createMany accepted reordered keys (misaligned VALUES)');
            }

            // insertIgnore: duplicate sku ignored, affected=0
            $n = OrmTestBItem::insertIgnore(['sku' => 'a', 'qty' => 99]);
            if ($n === 0 && (int)OrmTestBItem::query()->where(['sku' => 'a'])->cell() !== 99) {
                $this->ok('insertIgnore duplicate -> 0 affected, row untouched');
            } else {
                $this->fail("insertIgnore n=$n");
            }
            $n = OrmTestBItem::insertIgnore(['sku' => 'z', 'qty' => 7]);
            if ($n === 1) {
                $this->ok('insertIgnore new row -> 1 affected');
            } else {
                $this->fail("insertIgnore new n=$n");
            }

            // updateOrCreate both branches
            $r = OrmTestBItem::updateOrCreate(['sku' => 'b'], ['qty' => 42]);
            $q = OrmTestBItem::query()->where(['sku' => 'b'])->first();
            if ($r >= 1 && (int)($q['qty'] ?? 0) === 42) {
                $this->ok('updateOrCreate updates existing');
            } else {
                $this->fail("updateOrCreate update: r=$r " . json_encode($q));
            }
            $r = OrmTestBItem::updateOrCreate(['sku' => 'fresh'], ['qty' => 5]);
            $q = OrmTestBItem::query()->where(['sku' => 'fresh'])->first();
            if ($r > 0 && is_array($q) && (int)$q['qty'] === 5) {
                $this->ok('updateOrCreate inserts with where attrs merged');
            } else {
                $this->fail("updateOrCreate create: r=" . var_export($r, true) . ' ' . json_encode($q));
            }

            // findMany + preserveOrder
            $all = OrmTestBItem::query()->fields(['id', 'sku'])->order('id asc')->all();
            $ids = array_column($all, 'id');
            $rev = array_reverse($ids);
            $rows = OrmTestBItem::findMany($rev, true);
            $got = array_column($rows, 'id');
            if ($got === $rev) {
                $this->ok('findMany preserveOrder');
            } else {
                $this->fail('findMany order: ' . json_encode($got) . ' want ' . json_encode($rev));
            }
            $rows = OrmTestBItem::findMany([]);
            if (is_array($rows) && count($rows) === 0) {
                $this->ok('findMany([]) = [] (no SQL)');
            } else {
                $this->fail('findMany([])');
            }

            // toggle with CAS
            $id = (int)$all[0]['id'];
            $n = OrmTestBItem::toggle($id, 'qty', [1, 100]); // qty=1 -> 100
            $q = OrmTestBItem::find($id);
            if ($n === 1 && (int)$q['qty'] === 100) {
                $this->ok('toggle flips value (1 -> 100)');
            } else {
                $this->fail("toggle n=$n " . json_encode($q));
            }
            $n = OrmTestBItem::toggle($id, 'qty', [1, 100]); // 100 not in[0] pos... cur=100 -> values[0]=1
            $q = OrmTestBItem::find($id);
            if ($n === 1 && (int)$q['qty'] === 1) {
                $this->ok('toggle flips back (100 -> 1)');
            } else {
                $this->fail("toggle back n=$n " . json_encode($q));
            }
            $n = OrmTestBItem::toggle(999999, 'qty');
            if ($n === 0) {
                $this->ok('toggle missing row -> 0');
            } else {
                $this->fail("toggle missing n=$n");
            }
        } catch (\Throwable $e) {
            $this->fail('batch exception: ' . $e->getMessage());
        }
    }

    public function testConfigurableTimestamps()
    {
        echo "\nTesting configurable timestamps (3.2, SQLite):\n";

        if (!class_exists('\\Gene\\Orm\\Model') || !class_exists('\\Gene\\Db\\Sqlite')) {
            $this->fail('skip timestamps — extension classes missing');
            return;
        }

        try {
            $db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
            $db->sql('CREATE TABLE t_items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, addtime INTEGER, updatetime INTEGER, note TEXT)')->execute();
            \Gene\Di::set('t_db', $db);

            if (!class_exists('OrmTestTItem')) {
                eval('class OrmTestTItem extends \\Gene\\Orm\\Model {
                    protected static $table = "t_items";
                    protected static $connection = "t_db";
                    protected static $timestamps = true;
                    protected static $createdAt = "addtime";
                    protected static $updatedAt = "updatetime";
                    protected static $timestampFormat = "unix";
                }');
                eval('class OrmTestTOnlyAdd extends \\Gene\\Orm\\Model {
                    protected static $table = "t_items";
                    protected static $connection = "t_db";
                    protected static $timestamps = true;
                    protected static $createdAt = "addtime";
                    protected static $updatedAt = null;
                    protected static $timestampFormat = "unix";
                }');
            }

            $t0 = time();
            $id = OrmTestTItem::create(['name' => 'x']);
            $row = OrmTestTItem::find($id);
            if (is_array($row) && is_int($row['addtime']) && $row['addtime'] >= $t0
                && is_int($row['updatetime']) && $row['updatetime'] >= $t0) {
                $this->ok('unix timestamps on custom columns (addtime/updatetime)');
            } else {
                $this->fail('custom ts create: ' . json_encode($row));
            }

            // second call in the same request must hit the meta cache and
            // still use the custom columns (from_array round-trip)
            $id2 = OrmTestTItem::create(['name' => 'y']);
            $row2 = OrmTestTItem::find($id2);
            if (is_array($row2) && !empty($row2['addtime']) && !empty($row2['updatetime'])) {
                $this->ok('meta cache keeps custom ts config (2nd call)');
            } else {
                $this->fail('meta cache lost ts config: ' . json_encode($row2));
            }

            // payload-supplied values are never overwritten
            $id3 = OrmTestTItem::create(['name' => 'z', 'addtime' => 111, 'updatetime' => 222]);
            $row3 = OrmTestTItem::find($id3);
            if ((int)$row3['addtime'] === 111 && (int)$row3['updatetime'] === 222) {
                $this->ok('payload ts values win');
            } else {
                $this->fail('payload ts overwritten: ' . json_encode($row3));
            }

            // update path fills updatetime only
            sleep(1);
            OrmTestTItem::updateBy($id, ['name' => 'x2']);
            $row = OrmTestTItem::find($id);
            if ((int)$row['updatetime'] >= $t0 + 1 && (int)$row['addtime'] >= $t0) {
                $this->ok('updateBy fills updatetime, keeps addtime');
            } else {
                $this->fail('updateBy ts: ' . json_encode($row));
            }

            // updatedAt = null -> column never written (sqlite returns ''
            // for NULL columns through this driver, so assert empty())
            $id4 = OrmTestTOnlyAdd::create(['name' => 'n']);
            $row4 = OrmTestTOnlyAdd::find($id4);
            if (!empty($row4['addtime']) && empty($row4['updatetime'])) {
                $this->ok('updatedAt=null disables that column');
            } else {
                $this->fail('updatedAt=null: ' . json_encode($row4));
            }

            // toggle syncs updatetime via meta timestamps
            OrmTestTItem::updateBy($id, ['updatetime' => 5, 'note' => 'a']);
            OrmTestTItem::toggle($id, 'note', ['a', 'b']);
            $row = OrmTestTItem::find($id);
            if (($row['note'] ?? '') === 'b' && (int)$row['updatetime'] > 5) {
                $this->ok('toggle syncs updatetime');
            } else {
                $this->fail('toggle ts sync: ' . json_encode($row));
            }
        } catch (\Throwable $e) {
            $this->fail('timestamps exception: ' . $e->getMessage());
        }
    }

    public function run()
    {
        $this->testClassSurface();
        $this->testSqliteCrud();
        $this->testQueryOpsList();
        $this->testBatchAndIdempotent();
        $this->testConfigurableTimestamps();
        echo "\n--- ORM results: {$this->passed} passed, {$this->failed} failed ---\n";
        return $this->failed === 0;
    }

    public function runAllTests()
    {
        $this->run();
    }
}

if (php_sapi_name() === 'cli' && realpath($_SERVER['SCRIPT_FILENAME'] ?? '') === __FILE__) {
    $t = new OrmTest();
    exit($t->run() ? 0 : 1);
}
