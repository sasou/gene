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
 * @version  5.4.3
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
     * @param string $msg msg
     * @param int $code code
     * @return mixed
     */
    public function success($msg, $code = 2000) {

    }

    /**
     * error
     * 
     * @param string $msg msg
     * @param int $code code
     * @return mixed
     */
    public function error($msg, $code = 4000) {

    }

    /**
     * data
     * 
     * @param mixed $data data
     * @param int $count count
     * @param string|null $msg msg
     * @param int $code code
     * @return mixed
     */
    public function data($data, $count = -1, $msg = null, $code = 2000) {

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