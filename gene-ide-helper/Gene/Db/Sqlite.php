<?php
namespace Gene\Db;

/**
 * Service
 * 
 * @property \Gene\Db\Sqlite $sqdb
 * @property \Gene\Cache\Memcache $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Ext\Services\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Sqlite
{
    public $config;
    public $pdo;
    public $sql;
    protected $pool;
    public $where;
    public $group;
    public $having;
    public $order;
    public $limit;
    public $lock;
    public $data;
    protected static $history;

    /**
     * __construct
     * 
     * @param mixed $config config
     * @return mixed
     */
    public function __construct($config) {

    }

    /**
     * getPdo
     * 
     * @return mixed
     */
    public function getPdo() {
        return self;
    }

    /**
     * select
     * 
     * @param mixed $table table
     * @param mixed $fields fields
     * @return mixed
     */
    public function select($table, $fields) {

    }

    /**
     * count
     * 
     * @param mixed $table table
     * @param mixed $fields fields
     * @return mixed
     */
    public function count($table, $fields) {

    }

    /**
     * insert
     * 
     * @param mixed $table table
     * @param mixed $fields fields
     * @return mixed
     */
    public function insert($table, $fields) {

    }

    /**
     * batchInsert
     * 
     * @param mixed $table table
     * @param mixed $fields fields
     * @return mixed
     */
    public function batchInsert($table, $fields) {

    }

    /**
     * update
     * 
     * @param mixed $table table
     * @param mixed $fields fields
     * @return mixed
     */
    public function update($table, $fields) {

    }

    /**
     * delete
     * 
     * @param mixed $table table
     * @return mixed
     */
    public function delete($table) {

    }

    /**
     * where
     * 
     * @param mixed $where where
     * @param mixed $fields fields
     * @return mixed
     */
    public function where($where, $fields = null) {

    }

    /**
     * in
     * 
     * @param mixed $in in
     * @param mixed $fields fields
     * @return mixed
     */
    public function in($in, $fields = null) {

    }

    /**
     * join
     *
     * @param string $table 关联表（可带别名，标识符自动加引号）
     * @param array $on ON 条件，关联数组 leftColumn => rightColumn（两侧均按标识符引用，多条件以 AND 连接）
     * @param string $type 连接类型：INNER/LEFT/RIGHT/CROSS/FULL/LEFT OUTER/RIGHT OUTER/FULL OUTER（默认 INNER）
     * @return static
     */
    public function join($table, $on, $type = 'INNER') {

    }

    /**
     * leftJoin
     *
     * @param string $table 关联表
     * @param array $on ON 条件，同 join()
     * @return static
     */
    public function leftJoin($table, $on) {

    }

    /**
     * rightJoin
     *
     * @param string $table 关联表
     * @param array $on ON 条件，同 join()
     * @return static
     */
    public function rightJoin($table, $on) {

    }

    /**
     * union
     *
     * @param string|static $query 子查询：SQL 字符串，或另一个构建器对象（自动括号包裹并合并绑定参数）
     * @param bool $all 是否 UNION ALL（默认 false）
     * @return static
     */
    public function union($query, $all = false) {

    }

    /**
     * reset
     * 重置构建器状态（sql/join/where/group/having/union/order/limit/data），便于复用同一实例
     *
     * @return static
     */
    public function reset() {

    }

    /**
     * sql
     * 
     * @param mixed $sql sql
     * @param mixed $fields fields
     * @return mixed
     */
    public function sql($sql, $fields) {

    }

    /**
     * limit
     *
     * SQLite 语法：LIMIT offset OFFSET num
     *
     * @param int $num 返回行数
     * @param int|null $offset 偏移量
     * @return static
     */
    public function limit($num, $offset = null) {

    }

    /**
     * insertIgnore — INSERT OR IGNORE（6.1.0+，等价 MySQL INSERT IGNORE）。
     * 惰性执行：由 lastId()/affectedRows() 触发。
     *
     * @param string $table
     * @param array $fields
     * @return static
     */
    public function insertIgnore($table, $fields) {

    }

    /**
     * upsert — SQLite 不支持折叠进构建器（ON CONFLICT 需要显式冲突目标），
     * 调用即抛异常；请用 sql() 书写 ON CONFLICT(col) DO UPDATE。
     *
     * @param string $table
     * @param array $fields
     * @param array $updateCols
     * @return static
     */
    public function upsert($table, $fields, $updateCols) {

    }

    /**
     * lockForUpdate — SQLite 无行锁语法（写锁为整库级），no-op + E_NOTICE（6.1.0+）。
     *
     * @return static
     */
    public function lockForUpdate() {

    }

    /**
     * sharedLock — 同上，no-op + E_NOTICE（6.1.0+）。
     *
     * @return static
     */
    public function sharedLock() {

    }

    /**
     * order
     * 
     * @param mixed $order order
     * @return mixed
     */
    public function order($order) {

    }

    /**
     * group
     * 
     * @param mixed $group group
     * @return mixed
     */
    public function group($group) {

    }

    /**
     * having
     * 
     * @param mixed $having having
     * @return mixed
     */
    public function having($having) {

    }

    /**
     * execute
     * 
     * @return mixed
     */
    public function execute() {

    }

    /**
     * all
     * 
     * @return mixed
     */
    public function all() {

    }

    /**
     * row
     * 
     * @return mixed
     */
    public function row() {

    }

    /**
     * cell
     * 
     * @return mixed
     */
    public function cell() {

    }

    /**
     * lastId
     * 
     * @return mixed
     */
    public function lastId() {

    }

    /**
     * affectedRows
     *
     * @return mixed
     */
    public function affectedRows() {

    }

    /**
     * lastInsertId — lastId 的 PDO 命名别名（5.7.0+）
     *
     * @return mixed
     */
    public function lastInsertId() {

    }

    /**
     * rowCount — affectedRows 的 PDO 命名别名（5.7.0+）
     *
     * @return mixed
     */
    public function rowCount() {

    }

    /**
     * quote — PDO::quote 透传，字符串字面量转义（5.7.0+）
     *
     * @param string $str
     * @param int $paramType 默认 PDO::PARAM_STR
     * @return string|false
     */
    public function quote($str, $paramType = \PDO::PARAM_STR) {

    }

    /**
     * print
     * 
     * @return mixed
     */
    public function print() {

    }

    /**
     * beginTransaction
     * 
     * @return mixed
     */
    public function beginTransaction() {

    }

    /**
     * inTransaction
     * 
     * @return mixed
     */
    public function inTransaction() {

    }

    /**
     * rollBack
     * 
     * @return mixed
     */
    public function rollBack() {

    }

    /**
     * commit
     * 
     * @return mixed
     */
    public function commit() {

    }

    /**
     * transaction — 回调事务。PDO 不支持嵌套 begin：已在事务中则只执行 $fn，
     * 由外层 commit / rollBack。异常时仅当本层 begin 时 rollBack 并原样抛出。
     * transact() 为别名。
     *
     * @param callable $fn
     * @return mixed $fn 的返回值
     * @since 6.1.0
     */
    public function transaction($fn) {
        return null;
    }

    /**
     * @see transaction()
     * @param callable $fn
     * @return mixed
     * @since 6.1.0
     */
    public function transact($fn) {
        return null;
    }

    /**
     * release
     * 
     * 将PDO连接归还到连接池（仅在启用pool时有效）
     * 非pool模式下为空操作
     *
     * @return void
     */
    public function release() {

    }

    /**
     * free
     * 
     * 释放PDO连接。启用pool时归还到池，否则销毁连接。
     *
     * @return void
     */
    public function free() {

    }

    /**
     * __destruct
     * 
     * 对象销毁时，若启用pool则自动归还PDO连接到池中
     *
     * @return void
     */
    public function __destruct() {

    }

    /**
     * history
     * 
     * @return mixed
     */
    public function history() {

    }

    /**
     * attach
     * Attach another SQLite database file to the current connection under
     * $schema name. The schema name is validated as an identifier.
     *
     * @param string $path path to the SQLite database file
     * @param string $schema schema alias name
     * @return bool
     */
    public function attach(string $path, string $schema) {

    }

    /**
     * detach
     * Detach a previously-attached schema from the current connection.
     *
     * @param string $schema schema alias name
     * @return bool
     */
    public function detach(string $schema) {

    }

}