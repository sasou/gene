<?php
namespace Gene;

/**
 * Service
 * 
 * @property \Gene\Db\Mysql $db
 * @property \Gene\Cache\Memcache $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Ext\Services\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  3.0.5
 */
 
class Application
{
    protected static $instance;

    /**
     * __construct
     * 
     * @param mixed $safe safe
     * @return mixed
     */
    public function __construct($safe) {

    }

    /**
     * getInstance
     * 
     * @param mixed $safe safe
     * @return mixed
     */
    public static function getInstance($safe = null) {
        return new static(null);
    }

    /**
     * load
     * 
     * @param mixed $file file
     * @param mixed $path path
     * @param mixed $validity validity
     * @return mixed
     */
    public function load($file, $path, $validity = null) {

    }

    /**
     * autoload
     * 
     * @param mixed $app_root app_root
     * @param mixed $auto_function auto_function
     * @return mixed
     */
    public function autoload($app_root, $auto_function) {

    }

    /**
     * setMode
     * 
     * @param mixed $error_type error_type
     * @param mixed $exception_type exception_type
     * @return mixed
     */
    public function setMode($error_type, $exception_type) {

    }

    /**
     * setView
     * 
     * @param mixed $view view
     * @param mixed $tpl_ext tpl_ext
     * @return mixed
     */
    public function setView($view, $tpl_ext) {

    }

    /**
     * error
     * 
     * @param mixed $type type
     * @param mixed $callback callback
     * @param mixed $error_type error_type
     * @return mixed
     */
    public function error($type, $callback, $error_type) {

    }

    /**
     * exception
     * 
     * @param mixed $type type
     * @param mixed $callback callback
     * @return mixed
     */
    public function exception($type, $callback) {

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
     * getMethod
     * 
     * @return mixed
     */
    public static function getMethod() {

    }

    /**
     * getPath
     * 
     * @return mixed
     */
    public static function getPath() {

    }

    /**
     * getRouterUri
     * 
     * @return mixed
     */
    public static function getRouterUri() {

    }

    /**
     * getModule
     * 
     * @return mixed
     */
    public static function getModule() {

    }

    /**
     * getController
     * 
     * @return mixed
     */
    public static function getController() {

    }

    /**
     * getAction
     * 
     * @return mixed
     */
    public static function getAction() {

    }

    /**
     * setEnvironment
     * 
     * @param mixed $type type
     * @return mixed
     */
    public static function setEnvironment($type) {

    }

    /**
     * getEnvironment
     * 
     * @return mixed
     */
    public static function getEnvironment() {

    }

    /**
     * config
     * 
     * @param mixed $key key
     * @return mixed
     */
    public static function config($key) {

    }

    /**
     * params
     * 
     * @param mixed $key key
     * @return mixed
     */
    public static function params($key) {

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

}