<?php
namespace Gene;

/**
 * Router
 * 
 * @property \Gene\Db\Mysql $db
 * @property \Gene\Cache\Memcached $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Gene\Rest $rest
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
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
     * match
     * 纯路由匹配：复用与 run() 相同的路由树查找逻辑，但不执行 handler、
     * 不触发 hook，query string 会被剥离但不合并进 $_GET。
     * 用于路由单元测试与预检。
     *
     * @param string $method HTTP 方法（大小写不敏感）
     * @param string $uri 请求路径
     * @return array|false 命中时返回
     *   ['module'=>?, 'controller'=>?, 'action'=>?, 'params'=>[...], 'route'=>array]，
     *   未命中返回 false
     */
    public function match($method, $uri) {

    }

    /**
     * getRouterUri
     * 获取当前匹配到的路由注册键（路由模式串）
     *
     * @return string|null
     */
    public function getRouterUri() {

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
     * 开始或结束路由分组。传入字符串前缀开始分组，不传参或传 null 结束当前分组。
     *
     * @param string|null $prefix 分组前缀，如 '/admin'；null 表示结束分组
     * @return $this
     */
    public function group($prefix = null) {

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