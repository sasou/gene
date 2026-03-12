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
     * @param string $file 閰嶇疆鏂囦欢鍚?
     * @param string|null $path 鏂囦欢鎵€鍦ㄧ洰褰曪紙榛樿浣跨敤 app_root锛?
     * @param int $validity 鏂囦欢鍙樻洿妫€娴嬬紦瀛樻椂闂达紙绉掞紝榛樿10锛?
     * @return static
     */
    public function load($file, $path = null, $validity = null) {

    }

    /**
     * autoload
     * 
     * @param string|null $app_root 搴旂敤鏍圭洰褰?
     * @param callable|null $auto_function 鑷畾涔夎嚜鍔ㄥ姞杞藉嚱鏁?
     * @return static
     */
    public function autoload($app_root = null, $auto_function = null) {

    }

    /**
     * setMode
     * 
     * @param int|null $error_type 閿欒澶勭悊绫诲瀷锛?=鍐呯疆HTML锛?=鑷畾涔夛級
     * @param int|null $exception_type 寮傚父澶勭悊绫诲瀷锛?=鍐呯疆HTML锛?=鑷畾涔夛級
     * @param callable|null $ex_callback 鑷畾涔夊紓甯稿鐞嗗洖璋?
     * @param callable|null $error_callback 鑷畾涔夐敊璇鐞嗗洖璋?
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
     * @param string|null $method HTTP 璇锋眰鏂规硶锛堥粯璁や粠 $_SERVER 璇诲彇锛?
     * @param string|null $uri 璇锋眰璺緞锛堥粯璁や粠 $_SERVER 璇诲彇锛?
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
     * 鑾峰彇褰撳墠璇█
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
     * @param string|null $name 璺緞鍙傛暟鍚嶏紝涓嶄紶鍒欒繑鍥炲叏閮ㄥ弬鏁版暟缁?
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
