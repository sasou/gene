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
 * @version  3.2.0
 */
 
class Sqlite
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
    public function where($where, $fields) {

    }

    /**
     * in
     * 
     * @param mixed $in in
     * @param mixed $fields fields
     * @return mixed
     */
    public function in($in, $fields) {

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
     * @param mixed $limit limit
     * @return mixed
     */
    public function limit($limit) {

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