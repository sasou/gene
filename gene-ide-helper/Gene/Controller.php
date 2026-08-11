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
 * @version  5.4.3
 */
 
class Controller
{

    /**
     * __construct
     * 
     * @return mixed
     */
    public function __construct($debug = 0) {

    }

    /**
     * get
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function get($key = null, $value = null) {

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
     * forward
     * 内部转发：在进程内直接派发另一个 controller/action，返回被转发动作的返回值。
     * $controller 为类名原样（与路由 src 字符串同约定）。转发深度上限为 5，
     * 超限返回 false 并触发 E_WARNING（防转发循环）。
     *
     * @param string $controller 控制器类名
     * @param string $action 动作名
     * @param array $params 传给动作的参数
     * @return mixed|false
     */
    public function forward($controller, $action, $params = []) {

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
     * redirectJs
     * JavaScript 跳转
     *
     * @param string $url url
     * @return mixed
     */
    public function redirectJs($url) {

    }

    /**
     * alert
     * JavaScript 弹窗提示
     *
     * @param string $text text
     * @param string|null $url url
     * @return mixed
     */
    public function alert($text, $url = null) {

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
     * @param string $msg msg
     * @param int $code code
     * @return mixed
     */
    public static function success($msg, $code = 2000) {

    }

    /**
     * error
     *
     * @param string $msg msg
     * @param int $code code
     * @return mixed
     */
    public static function error($msg, $code = 4000) {

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
    public static function data($data, $count = -1, $msg = null, $code = 2000) {

    }

    /**
     * json
     *
     * @param mixed $data data
     * @param string|null $callback callback
     * @param int $code code
     * @return mixed
     */
    public static function json($data, $callback = null, $code = 256) {

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