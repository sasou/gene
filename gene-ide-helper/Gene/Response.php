<?php
namespace Gene;

/**
 * Response
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  3.0.5
 */
 
class Response
{

    /**
     * redirect
     * 
     * @param mixed $url url
     * @param mixed $code code
     * @return mixed
     */
    public static function redirect($url, $code = null) {

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