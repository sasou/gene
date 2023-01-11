<?php
namespace Gene;

/**
 * Service
 * 
 * @property \Gene\Db\Mysql $db
 * @property \Gene\Cache\Memcached $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Gene\Validate $validate
 * @property \Ext\Services\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  3.0.5
 */
 
class Service
{

    /**
     * __construct
     * 
     * @return mixed
     */
    public function __construct() {

    }

    /**
     * __destruct
     * 
     * @return mixed
     */
    public function __destruct() {

    }

    /**
     * __get
     * 
     * @param mixed $name name
     * @return mixed
     */
    public function __get($name) {

    }

    /**
     * __set
     * 
     * @param mixed $name name
     * @param mixed $value value
     * @return mixed
     */
    public function __set($name, $value) {

    }

    /**
     * success
     * 
     * @param mixed $msg msg
     * @param mixed $code code
     * @return mixed
     */
    public function success($msg, $code = null) {

    }

    /**
     * error
     * 
     * @param mixed $msg msg
     * @param mixed $code code
     * @return mixed
     */
    public function error($msg, $code = null) {

    }

    /**
     * data
     * 
     * @param mixed $data data
     * @param mixed $count count
     * @param mixed $msg msg
     * @param mixed $code code
     * @return mixed
     */
    public function data($data, $count = 0, $msg = null, $code = null) {

    }

    /**
     * getInstance
     * 
     * @param mixed $params params
     * @return mixed
     */
    public static function getInstance($params = null) {
        return new static();
    }

}