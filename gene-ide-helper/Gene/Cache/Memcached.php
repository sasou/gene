<?php
namespace Gene\Cache;

/**
 * Memcached
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
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
     * add — atomic create-only write (NX); fails when the key exists
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @param mixed $ttl ttl
     * @return mixed
     */
    public function add($key, $value, $ttl = 0) {

    }

    /**
     * del — delete a key
     * 
     * @param mixed $key key
     * @return mixed
     */
    public function del($key) {

    }

    /**
     * delete — alias of del()
     * 
     * @param mixed $key key
     * @return mixed
     */
    public function delete($key) {

    }

    /**
     * expire — reset a key's TTL (memcached protocol: touch)
     * 
     * @param mixed $key key
     * @param mixed $ttl ttl
     * @return mixed
     */
    public function expire($key, $ttl) {

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