<?php
namespace Gene\Db;

/**
 * Service
 * 
 * @property \Gene\Db\Mssql $msdb
 * @property \Gene\Cache\Memcache $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Ext\Services\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Mssql
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
     * SQL Server 语法：OFFSET num ROWS FETCH NEXT offset ROWS ONLY
     *
     * @param int $num OFFSET 行数
     * @param int|null $offset FETCH NEXT 行数
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