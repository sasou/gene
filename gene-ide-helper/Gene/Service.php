<?php
namespace Gene;

/**
 * Service
 * 
 * @property \Gene\Db $db
 * @property \Ext\Cache\Memcache $memcache
 * @property \Ext\Cache\Redis $redis
 * @property \Ext\Cache $cache
 * @property \Ext\Services\Rest $rest
 * @author  sasou
 * @version  1.0
 * @date  2018-05-15
 */
class Service
{
    public $attr;

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
    public function success($msg, $code) {

    }

    /**
     * error
     * 
     * @param mixed $msg msg
     * @param mixed $code code
     * @return mixed
     */
    public function error($msg, $code) {

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
    public function data($data, $count, $msg, $code) {

    }

    /**
     * getInstance
     * 
     * @return mixed
     */
    public static function getInstance() {
        return new static();
    }

}