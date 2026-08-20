<?php
namespace Gene;

/**
 * Hook
 * 
 * Base class for router hooks. Extend this class to create
 * class-based hooks that avoid eval() and use direct C dispatch.
 * Provides the same controller-related methods as Gene\Controller.
 * 
 * Usage in router config:
 *   ->hook("adminAuth", "Hooks\AdminAuth@handle")
 *   ->hook("before", "Hooks\BeforeHook@handle")
 *   ->hook("after", "Hooks\AfterHook@handle")
 * 
 * @property \Gene\Db\Mysql $db
 * @property \Gene\Cache\Memcached $memcache
 * @property \Gene\Cache\Redis $redis
 * @property \Gene\Cache\Cache $cache
 * @property \Gene\Validate $validate
 * @property \Gene\Session $session
 * @property \Gene\Request $request
 * @property \Gene\Response $response
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Hook
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
     * @param mixed $value default value
     * @return mixed
     */
    public static function get($key, $value = null) {

    }

    /**
     * request
     * 
     * @param mixed $key key
     * @param mixed $value default value
     * @return mixed
     */
    public static function request($key = null, $value = null) {

    }

    /**
     * post
     * 
     * @param mixed $key key
     * @param mixed $value default value
     * @return mixed
     */
    public static function post($key = null, $value = null) {

    }

    /**
     * cookie
     * 
     * @param mixed $key key
     * @param mixed $value default value
     * @return mixed
     */
    public static function cookie($key = null, $value = null) {

    }

    /**
     * files
     * 
     * @param mixed $key key
     * @param mixed $value default value
     * @return mixed
     */
    public static function files($key = null, $value = null) {

    }

    /**
     * server
     * 
     * @param mixed $key key
     * @param mixed $value default value
     * @return mixed
     */
    public static function server($key = null, $value = null) {

    }

    /**
     * env
     * 
     * @param mixed $key key
     * @param mixed $value default value
     * @return mixed
     */
    public static function env($key = null, $value = null) {

    }

    /**
     * isAjax
     * 
     * @return bool
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
     * 
     * @return string|null
     */
    public static function getLang() {

    }

    /**
     * isGet
     * 
     * @return bool
     */
    public static function isGet() {

    }

    /**
     * isPost
     * 
     * @return bool
     */
    public static function isPost() {

    }

    /**
     * isPut
     * 
     * @return bool
     */
    public static function isPut() {

    }

    /**
     * isHead
     * 
     * @return bool
     */
    public static function isHead() {

    }

    /**
     * isOptions
     * 
     * @return bool
     */
    public static function isOptions() {

    }

    /**
     * isDelete
     * 
     * @return bool
     */
    public static function isDelete() {

    }

    /**
     * isCli
     * 
     * @return bool
     */
    public static function isCli() {

    }

    /**
     * redirect
     * 
     * @param string $url url
     * @param int $code HTTP status code (default 302)
     * @return bool
     */
    public function redirect($url, $code = 302) {

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
     * @param string $name variable name
     * @param mixed $value variable value
     * @return void
     */
    public static function assign($name, $value) {

    }

    /**
     * display
     * 
     * @param string $file view file
     * @param string $parent_file parent layout file
     * @return mixed
     */
    public function display($file, $parent_file = null) {

    }

    /**
     * displayExt
     * 
     * @param string $file view file
     * @param string $parent_file parent layout file
     * @param bool $isCompile whether to compile
     * @return mixed
     */
    public function displayExt($file, $parent_file = null, $isCompile = false) {

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
     * 返回带语言前缀的 URL。$lang 未传时使用当前请求语言；传空串则不加语言前缀。
     *
     * @param string $path path
     * @param string $lang 指定语言；未传时使用当前请求语言，传 "" 则返回不含语言前缀的 URL
     * @return string
     */
    public static function url($path, $lang = null) {

    }

    /**
     * getPath
     * 返回当前请求路径。$withoutLang=true 时去除语言前缀。
     *
     * @param bool $withoutLang 是否去除语言前缀
     * @return string|null
     */
    public static function getPath($withoutLang = false) {

    }

    /**
     * getRouterUri
     * 返回当前路由 URI（:m/:c/:a 替换后，小写）。
     *
     * @return string|null
     */
    public static function getRouterUri() {

    }

    /**
     * success
     * 
     * @param string $msg message
     * @param int $code code (default 2000)
     * @return array
     */
    public static function success($msg, $code = 2000) {

    }

    /**
     * error
     * 
     * @param string $msg message
     * @param int $code code (default 4000)
     * @return array
     */
    public static function error($msg, $code = 4000) {

    }

    /**
     * data
     * 
     * @param mixed $data data
     * @param int $count count
     * @param string $msg message
     * @param int $code code
     * @return array
     */
    public static function data($data, $count = -1, $msg = null, $code = 2000) {

    }

    /**
     * json
     * 
     * @param mixed $data data
     * @param string $callback JSONP callback
     * @param int $code json_encode options
     * @return bool
     */
    public static function json($data, $callback = null, $code = 256) {

    }

    /**
     * before
     * Default before hook handler. Override in subclass.
     * Return false to abort the request.
     * 
     * @return bool
     */
    public function before() {
        return true;
    }

    /**
     * after
     * Default after hook handler. Override in subclass.
     * Receives the dispatch result as parameter.
     * 
     * @param mixed $params dispatch result
     * @return void
     */
    public function after($params = null) {

    }

    /**
     * handle
     * Default named hook handler. Override in subclass.
     * Return false to abort the request.
     * 
     * @return bool
     */
    public function handle() {
        return true;
    }

    /**
     * __get
     * DI property accessor
     * 
     * @param string $name property name
     * @return mixed
     */
    public function __get($name) {

    }

    /**
     * __set
     * DI property setter
     * 
     * @param string $name property name
     * @param mixed $value property value
     * @return bool
     */
    public function __set($name, $value) {

    }

}
