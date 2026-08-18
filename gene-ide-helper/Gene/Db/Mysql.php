<?php
namespace Gene\Db;

/**
 * Service
 * 
 * @property \Gene\Db\Mysql $mydb
 * @property \Gene\Cache\Memcache $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Ext\Services\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Mysql
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
    public function select($table, $fields = null) {

    }

    /**
     * count
     * 
     * @param mixed $table table
     * @param mixed $fields fields
     * @return mixed
     */
    public function count($table, $fields = null) {

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
     * insertIgnore — INSERT IGNORE，唯一键冲突时静默忽略（6.1.0+）。
     * 与 insert 同为惰性执行：由 lastId()/affectedRows() 触发。
     *
     * @param string $table
     * @param array $fields
     * @return static
     */
    public function insertIgnore($table, $fields) {

    }

    /**
     * upsert — INSERT ... ON DUPLICATE KEY UPDATE `c`=VALUES(`c`)（6.1.0+，MySQL 专属）。
     * 惰性执行，同 insert。
     *
     * @param string $table
     * @param array $fields
     * @param array $updateCols 冲突时要更新的列名（非空，否则抛异常）
     * @return static
     */
    public function upsert($table, $fields, $updateCols) {

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
     * @param array|string $where 条件：关联数组（自动解析操作符/IN）或原始 SQL 字符串
     * @param array|mixed|null $fields 绑定参数
     * @return static
     */
    public function where($where, $fields = null) {

    }

    /**
     * in
     *
     * @param string $in 含 in(?) 占位符的 SQL 片段
     * @param array|mixed|null $fields 值（数组或标量）
     * @return static
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
    public function sql($sql, $fields = null) {

    }

    /**
     * limit
     *
     * MySQL 语法：LIMIT $num 或 LIMIT $num, $offset
     * 两参数时等价于 LIMIT $offset OFFSET $num（$num 为起始偏移量，$offset 为返回行数）
     *
     * @param int $num 起始偏移行数（单参时为返回行数）
     * @param int|null $offset 返回行数（双参模式，对应 MySQL LIMIT offset, count 中的 count）
     * @return static
     */
    public function limit($num, $offset = null) {

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
     * lockForUpdate — SELECT ... FOR UPDATE（6.1.0+）。拼在 LIMIT 之后。
     * 锁只在事务内有效：不在事务中调用会发 E_NOTICE。
     *
     * @return static
     */
    public function lockForUpdate() {

    }

    /**
     * sharedLock — SELECT ... LOCK IN SHARE MODE（6.1.0+）。同须在事务内。
     *
     * @return static
     */
    public function sharedLock() {

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

}