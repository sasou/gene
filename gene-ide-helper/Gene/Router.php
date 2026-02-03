<?php
namespace Gene;

/**
 * Router
 * 
 * @property \Gene\Db\Mysql $db
 * @property \Gene\Cache\Memcached $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Ext\Services\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  3.0.5
 */
 
class Router
{
    public $safe;
    public $prefix;

    /**
     * __construct
     * 
     * @return mixed
     */
    public function __construct() {

    }

    /**
     * getEvent
     * 
     * @return mixed
     */
    public function getEvent() {

    }

    /**
     * getTree
     * 
     * @return mixed
     */
    public function getTree() {

    }

    /**
     * delTree
     * 
     * @return mixed
     */
    public function delTree() {

    }

    /**
     * delEvent
     * 
     * @return mixed
     */
    public function delEvent() {

    }

    /**
     * clear
     * 
     * @return mixed
     */
    public function clear() {

    }

    /**
     * getTime
     * 
     * @return mixed
     */
    public function getTime() {

    }

    /**
     * getRouter
     * 
     * @return mixed
     */
    public function getRouter() {

    }

    /**
     * getConf
     *
     * @return mixed
     */
    public function getConf() {

    }

    /**
     * delConf
     *
     * @return mixed
     */
    public function delConf() {

    }

    /**
     * getLang
     * 获取当前语言
     *
     * @return string|null
     */
    public static function getLang() {

    }
    
    /**
     * assign
     * 
     * @param mixed $name
     * @param mixed $value
     * @return mixed
     */
    public function assign($name, $value) {

    }

    /**
     * display
     * 
     * @param mixed $file file
     * @param mixed $parent_file parent_file
     * @return mixed
     */
    public static function display($file, $parent_file) {

    }

    /**
     * displayExt
     * 
     * @param mixed $file file
     * @param mixed $parent_file parent_file
     * @param mixed $isCompile isCompile
     * @return mixed
     */
    public static function displayExt($file, $parent_file, $isCompile) {

    }

    /**
     * runError
     * 
     * @param mixed $method method
     * @return mixed
     */
    public static function runError($method) {

    }

    /**
     * run
     * 
     * @param mixed $method method
     * @param mixed $uri uri
     * @return mixed
     */
    public function run($method, $uri) {

    }

    /**
     * readFile
     * 
     * @param mixed $file file
     * @return mixed
     */
    public function readFile($file) {

    }

    /**
     * dispatch
     * 
     * @param mixed $class class
     * @param mixed $action action
     * @param mixed $params params
     * @return mixed
     */
    public static function dispatch($class, $action, $params) {

    }

    /**
     * params
     * 
     * @return mixed
     */
    public static function params() {

    }

    /**
     * prefix
     *
     * @param string $prefix
     * @return $this
     */
    public function prefix($prefix) {

    }

    /**
     * group
     *
     * @param mixed $callback
     * @return mixed
     */
    public function group($callback) {

    }

    /**
     * lang
     *
     * @param string $lang
     * @return $this
     */
    public function lang($lang) {

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

}