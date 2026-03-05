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
 * @version  3.2.0
 */
 
class Mysql
{
    public $config;
    public $pdo;
    public $sql;
    public $where;
    public $group;
    public $having;
    public $order;
    public $limit;
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
     * free
     * 
     * @return mixed
     */
    public function free() {

    }

    /**
     * history
     * 
     * @return mixed
     */
    public function history() {

    }

}