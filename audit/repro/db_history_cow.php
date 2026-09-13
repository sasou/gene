<?php
/**
 * [GENE_FIX:2026-09-12] Repro: sqliteSaveHistory()/mysqlSaveHistory()/
 * pgsqlSaveHistory()/mssqlSaveHistory() appended rows to the request-global
 * db_*_history array in place. history() returns that same array to
 * userland via RETURN_ZVAL(...,1,0), so after the first history() call the
 * HashTable is shared (GC_REFCOUNT > 1). Every subsequent executed
 * statement then performed add_next_index_zval() -- and, at the
 * GENE_DB_HISTORY_MAX cap, zend_hash_index_del() -- on the shared table.
 *
 * That is a COW violation: on release builds it silently mutates the
 * caller's snapshot (the array they hold keeps growing / losing its head
 * element); on PHP debug builds it aborts the process at
 *   _zend_hash_index_add_or_update_i:
 *   `GC_REFCOUNT(ht) == 1 || immutable`  (zend_hash.c)
 * which is exactly the crash observed in DatabaseTest right after
 * "SQLite history() returns array".
 *
 * Fix: SEPARATE_ARRAY() before mutating, in all four drivers
 * (src/db/{mysql,sqlite,pgsql,mssql}.c).
 *
 * This script snapshots history() mid-stream and runs more statements.
 * Before the fix (debug build): the process aborts inside the next
 * executed statement; (release build): $snap grows along with the
 * engine's copy. After the fix: $snap stays frozen while history()
 * keeps recording.
 *
 * Requires history recording to be enabled, i.e. run with:
 *   php -d gene.run_environment=0 audit/repro/db_history_cow.php
 * (gene.run_environment is PHP_INI_SYSTEM and defaults to 1 = off.)
 */

if (ini_get('gene.run_environment')) {
    echo "SKIP: needs -d gene.run_environment=0 (history recording is off).\n";
    exit(0);
}

$db = new \Gene\Db\Sqlite(['dsn' => 'sqlite::memory:']);
$db->sql('CREATE TABLE hcow (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)')->execute();
$db->sql("INSERT INTO hcow (name) VALUES ('a')")->execute();

$snap = $db->history();
$before = is_array($snap) ? count($snap) : -1;

$db->sql("INSERT INTO hcow (name) VALUES ('b')")->execute();
$db->select('hcow')->all();

$after = $db->history();

$ok = $before >= 2
    && count($snap) === $before                            // caller snapshot untouched
    && is_array($after) && count($after) === $before + 2;  // engine keeps recording

echo $ok
    ? "PASS: history() snapshot decoupled from subsequent statements.\n"
    : "FAIL: snapshot mutated (before=$before, snap=" . count($snap)
      . ", after=" . (is_array($after) ? count($after) : 'n/a') . ").\n";
exit($ok ? 0 : 1);
