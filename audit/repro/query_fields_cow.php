<?php
/**
 * [GENE_FIX:2026-08-24] Repro: gene_orm_db_select() (src/orm/meta.c) used to
 * call zend_hash_internal_pointer_reset()/zend_hash_get_current_data() -- the
 * non-_ex variants that write through ht->nInternalPointer -- directly on the
 * caller-supplied `$fields` array when it has exactly one element, in order
 * to special-case a literal ["*"] projection.
 *
 * That array can be a live reference into shared/COW storage: Query::fields()
 * stores the zval as-is (refcount bumped, not copied) into the ops list, and
 * apply() later hands that same zval straight to gene_orm_db_select() without
 * separating it. If the caller keeps using the same array value elsewhere
 * (refcount >= 2, not the compile-time-constant/immutable case), mutating
 * ht->nInternalPointer in place is a COW violation.
 *
 * Symptom on PHP debug builds / Swoole:
 *   zend_hash_internal_pointer_reset_ex() -> assertion
 *   `(&ht->nInternalPointer != pos || refcount==1) || immutable` aborts the
 *   worker, matching the production crash trace in gene_orm_db_select() at
 *   src/orm/meta.c:477 (called from Query::all() -> gene_orm_query_apply()).
 *
 * This script exercises the code path with a shared single-element $fields
 * array reused across two independent queries. Before the fix, this could
 * corrupt/crash under a debug build; after the fix (local HashPosition,
 * never touching the shared array's own internal pointer) both queries
 * return correct, independent results and the caller's array is untouched.
 */

if (!class_exists('OrmFieldsCowItem')) {
    eval('class OrmFieldsCowItem extends \\Gene\\Orm\\Model {
        protected static $table = "orm_fcow";
        protected static $primaryKey = "id";
        protected static $connection = "fcow_db";
    }');
}

$db = new \Gene\Db\Sqlite([
    'dsn' => 'sqlite::memory:',
]);
$db->sql('CREATE TABLE orm_fcow (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    status TEXT
)')->execute();
$db->sql("INSERT INTO orm_fcow (name, status) VALUES ('alice', 'ok')")->execute();

\Gene\Di::set('fcow_db', $db);

// A single-element fields array, kept alive and reused (refcount >= 2 once
// handed to fields()/db_select()).
$only = ['name'];
$alias = $only; // bump refcount, mirrors a caller holding onto its own copy

$row1 = OrmFieldsCowItem::query()->fields($only)->first();
$row2 = OrmFieldsCowItem::query()->fields($only)->first();

$ok = is_array($row1) && is_array($row2)
    && ($row1['name'] ?? null) === 'alice'
    && ($row2['name'] ?? null) === 'alice'
    && $only === ['name']
    && $alias === ['name'];

echo $ok
    ? "PASS: shared single-element \$fields array handled safely across repeated queries.\n"
    : "FAIL: shared \$fields array mutated or query result corrupted.\n";
exit($ok ? 0 : 1);
