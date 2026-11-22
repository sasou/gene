<?php
namespace Gene;

/**
 * Exception
 *
 * Extends \Exception. Only the methods below are declared by the extension;
 * message/code/trace come from the parent.
 *
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */

class Exception extends \Exception
{
    /**
     * setErrorHandler
     * $error_type omitted keeps the handler's default error mask.
     *
     * @param mixed $callback callback
     * @param mixed $error_type error_type
     * @return bool
     */
    public static function setErrorHandler($callback, $error_type = null) {

    }

    /**
     * setExceptionHandler
     * No callback registers the built-in HTML handler.
     *
     * @param mixed $callback callback
     * @return bool
     */
    public static function setExceptionHandler($callback = null) {

    }

    /**
     * doException
     *
     * @param mixed $ex ex
     * @return mixed
     */
    public static function doException($ex) {

    }

    /**
     * doError
     *
     * @param mixed $code code
     * @param mixed $msg msg
     * @param mixed $file file
     * @param mixed $line line
     * @param mixed $params params
     * @return mixed
     */
    public static function doError($code, $msg, $file = null, $line = null, $params = null) {

    }

}
