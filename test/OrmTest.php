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
            '__get', '__set', '__isset', '__unset',
        ];
        foreach ($methods as $m) {
            if (!method_exists('\\Gene\\Orm\\Model', $m)) {
                $this->fail("missing method Model::$m");
            } else {
                $this->ok("Model::$m");
            }
        }

        foreach (['where', 'in', 'order', 'limit', 'all', 'row', 'cell', 'count'] as $m) {
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

    public function run()
    {
        $this->testClassSurface();
        $this->testSqliteCrud();
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
