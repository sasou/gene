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
            'fill', 'save', 'delete', 'toArray', 'getInstance',
            '__get', '__set',
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
