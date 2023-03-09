<?php
namespace Gene\Cache;

/**
 * Memcached
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  3.0.5
 */
 
class Memcached
{
    public $config;
    public $obj;
    public $mehandler;

    /**
     * decr
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public function decr($key, $value = 1) {

    }

    /**
     * incr
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public function incr($key, $value = 1) {

    }

    /**
     * get
     * 
     * @param mixed $key key
     * @return mixed
     */
    public function get($key) {

    }

    /**
     * set
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @param mixed $ttl ttl
     * @param mixed $flag flag
     * @return mixed
     */
    public function set($key, $value, $ttl = 1, $flag = null) {

    }

    /**
     * __call
     * 
     * @param mixed $method method
     * @param mixed $params params
     * @return mixed
     */
    public function __call($method, $params) {

    }

    /**
     * __construct
     * 
     * @return mixed
     */
    public function __construct() {

    }

}