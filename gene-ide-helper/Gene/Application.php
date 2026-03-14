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
    public function __construct($safe = null) {

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
     * @param string $file 配置文件名
     * @param string|null $path 文件所在目录（默认使用 app_root）
     * @param int $validity 文件变更检测缓存时间（秒，默认10）
     * @return static
     */
    public function load($file, $path = null, $validity = null) {

    }

    /**
     * autoload
     * 
     * @param string|null $app_root 应用根目录
     * @param callable|null $auto_function 自定义自动加载函数
     * @return static
     */
    public function autoload($app_root = null, $auto_function = null) {

    }

    /**
     * setMode
     * 
     * @param int|null $error_type 错误处理类型：0=内置HTML，1=自定义
     * @param int|null $exception_type 异常处理类型：0=内置HTML，1=自定义
     * @param callable|null $ex_callback 自定义异常处理回调
     * @param callable|null $error_callback 自定义错误处理回调
     * @return static
     */
    public function setMode($error_type = null, $exception_type = null, $ex_callback = null, $error_callback = null) {

    }

    /**
     * setView
     * 
     * @param mixed $view view
     * @param mixed $tpl_ext tpl_ext
     * @return mixed
     */
    public function setView($view, $tpl_ext = null) {

    }

    /**
     * error
     * 
     * @param mixed $type type
     * @param mixed $callback callback
     * @param mixed $error_type error_type
     * @return mixed
     */
    public function error($type, $callback, $error_type = null) {

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
     * webscan
     * 
     * @param int|null $webscan_switch
     * @param string|null $webscan_white_directory
     * @param callable|null $callback
     * @param array|null $webscan_white_url
     * @param int|null $webscan_get
     * @param int|null $webscan_post
     * @param int|null $webscan_cookie
     * @param int|null $webscan_referer
     * @return static
     */
    public function webscan(
        $webscan_switch = null,
        $webscan_white_directory = null,
        $callback = null,
        $webscan_white_url = null,
        $webscan_get = null,
        $webscan_post = null,
        $webscan_cookie = null,
        $webscan_referer = null
    ) {

    }
    /**
     * run
     * 
     * FPM模式：自动从 $_SERVER 读取 REQUEST_METHOD 和 REQUEST_URI
     * Swoole模式：自动从 Request::init() 初始化的 server 数据中读取，无需手动传参
     * 也可手动传入 method/uri 覆盖自动检测
     *
     * @param string|null $method HTTP 请求方法（可选，默认自动检测）
     * @param string|null $uri 请求路径（可选，默认自动检测）
     * @return static
     */
    public function run($method = null, $uri = null) {

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
     * @return string|null
     */
    public static function getRouterUri() {

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
     * @param mixed $type type(int): 1-dev, 2-test, 3-prod
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
     * getEnvironmentName
     * 
     * @return string
     */
    public static function getEnvironmentName() {

    }

    /**
     * setRuntimeType
     * 
     * @param mixed $type type(int|string): 1/fpm, 2/swoole, 3/coroutine
     * @return mixed
     */
    public static function setRuntimeType($type) {

    }

    /**
     * getRuntimeType
     * 
     * @return mixed
     */
    public static function getRuntimeType() {

    }

    /**
     * getRuntimeTypeName
     * 
     * @return string
     */
    public static function getRuntimeTypeName() {

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
     * @param string|null $name 路径参数名，不传则返回全部参数数组 
     * @return mixed
     */
    public static function params($name = null) {

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
