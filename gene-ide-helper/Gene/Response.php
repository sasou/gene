<?php
namespace Gene;

/**
 * Response
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Response
{

    /**
     * redirect
     * 
     * @param string $url url
     * @param int $code code
     * @return mixed
     */
    public static function redirect($url, $code = null) {

    }

    /**
     * redirectJs
     * JavaScript 跳转
     *
     * @param string $url url
     * @return mixed
     */
    public static function redirectJs($url) {

    }

    /**
     * alert
     * 
     * @param mixed $text text
     * @param mixed $url url
     * @return mixed
     */
    public static function alert($text, $url = null) {

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
     * header
     * 
     * @param mixed $key key
     * @param mixed $value value
     * @return mixed
     */
    public static function header($key, $value = null) {

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
     * end
     * 结束响应并发送数据（Swoole模式下调用Swoole\Response::end）
     *
     * @param string|null $data 响应数据
     * @return bool
     */
    public static function end($data = null) {

    }

    /**
     * setJsonHeader
     * 
     * @return mixed
     */
    public static function setJsonHeader() {

    }

    /**
     * setHtmlHeader
     * 
     * @return mixed
     */
    public static function setHtmlHeader() {

    }

    /**
     * __construct
     * 
     * @return mixed
     */
    public function __construct() {

    }

}