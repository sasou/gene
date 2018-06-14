<?php
namespace Gene;

/**
 * Db
 * 
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Db
{
    public $pdo;
    public $sql;
    public $where;
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
     * history
     * 
     * @return mixed
     */
    public function history() {

    }
}    
