<?php
namespace Gene;

/**
 * Controller
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
 
class Controller
{

    /**
     * __construct
     * 
     * @return mixed
     */
    public function __construct() {

    }

    /**
     * get
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function get($key, $value = null) {

    }

    /**
     * request
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function request($key = null, $value = null) {

    }

    /**
     * post
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function post($key = null, $value = null) {

    }

    /**
     * cookie
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function cookie($key = null, $value = null) {

    }

    /**
     * files
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function files($key = null, $value = null) {

    }

    /**
     * server
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function server($key = null, $value = null) {

    }

    /**
     * env
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function env($key = null, $value = null) {

    }

    /**
     * isAjax
     * 
     * @return mixed
     */
    public static function isAjax() {

    }

    /**
     * params
     * 
     * @param mixed $key key
     * @return mixed
     */
    public static function params($key = null) {

    }

    /**
     * getMethod
     * 
     * @return string|null
     */
    public static function getMethod() {

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
     * isGet
     * 
     * @return mixed
     */
    public static function isGet() {

    }

    /**
     * isPost
     * 
     * @return mixed
     */
    public static function isPost() {

    }

    /**
     * isPut
     * 
     * @return mixed
     */
    public static function isPut() {

    }

    /**
     * isHead
     * 
     * @return mixed
     */
    public static function isHead() {

    }

    /**
     * isOptions
     * 
     * @return mixed
     */
    public static function isOptions() {

    }

    /**
     * isDelete
     * 
     * @return mixed
     */
    public static function isDelete() {

    }

    /**
     * isCli
     * 
     * @return mixed
     */
    public static function isCli() {

    }

    /**
     * redirect
     * 
     * @param mixed $url url
     * @param mixed $code code
     * @return mixed
     */
    public function redirect($url, $code = null) {

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
    public function display($file, $parent_file = null) {

    }

    /**
     * displayExt
     * 
     * @param mixed $file file
     * @param mixed $parent_file parent_file
     * @param mixed $isCompile isCompile
     * @return mixed
     */
    public function displayExt($file, $parent_file = null, $isCompile = null) {

    }

    /**
     * contains
     * 
     * @return mixed
     */
    public static function contains() {

    }

    /**
     * containsExt
     * 
     * @return mixed
     */
    public static function containsExt() {

    }

    /**
     * url
     * 返回带当前语言前缀的 URL，如 url("login.html") => "/en/login.html"
     * 传入 "/" 也会加上语言前缀，如 url("/") => "/en/"
     *
     * @param string $path 路径，如 login.html 或 "/"
     * @return string
     */
    public static function url($path) {

    }

    /**
     * success
     * 
     * @param mixed $msg msg
     * @param mixed $code code
     * @return mixed
     */
    public static function success($msg, $code = null) {

    }

    /**
     * error
     * 
     * @param mixed $msg msg
     * @param mixed $code code
     * @return mixed
     */
    public static function error($msg, $code = null) {

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
    public static function data($data, $count = 0, $msg = null, $code = null) {

    }

    /**
     * json
     * 
     * @param mixed $data data
     * @param mixed $callback callback
     * @param mixed $code code
     * @return mixed
     */
    public static function json($data, $callback = null, $code = null) {

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