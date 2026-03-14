<?php
namespace Gene;

/**
 * Request
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  3.0.5
 */
 
class Request
{
    protected static $_attr;

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
    public static function request($key, $value = null) {

    }

    /**
     * post
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function post($key, $value = null) {

    }

    /**
     * cookie
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function cookie($key, $value = null) {

    }

    /**
     * files
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function files($key, $value = null) {

    }

    /**
     * server
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function server($key, $value = null) {

    }

    /**
     * env
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function env($key, $value = null) {

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
     * @return mixed
     */
    public static function getMethod() {

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
     * isCli
     * 
     * @return mixed
     */
    public static function isCli() {

    }

    /**
     * init
     * 
     * 初始化请求数据（Swoole模式下替代PHP超全局变量）
     * 当 $request 参数未传入时，自动合并 $get 和 $post 生成 request 数据（类似 $_REQUEST）
     * server 数据的 key 会自动生成大写副本（如 request_method → REQUEST_METHOD）
     *
     * @param array|null $get GET参数
     * @param array|null $post POST参数
     * @param array|null $cookie Cookie参数
     * @param array|null $server Server参数
     * @param array|null $env 环境变量
     * @param array|null $files 上传文件
     * @param array|null $request Request参数（未传入则自动合并GET+POST）
     * @return bool
     */
    public static function init($get = null, $post = null, $cookie = null, $server = null, $env = null, $files = null, $request = null) {

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
     * __get
     * 
     * @param mixed $name name
     * @return mixed
     */
    public function __get($name = null) {

    }

}