<?php
namespace Gene;

/**
 * Exception
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  3.0.2
 */
 
class Exception
{
    protected $file;
    protected $line;
    protected $message;
    protected $code;
    protected $previous;

    /**
     * setErrorHandler
     * 
     * @param mixed $callback callback
     * @param mixed $error_type error_type
     * @return mixed
     */
    public static function setErrorHandler($callback, $error_type) {

    }

    /**
     * setExceptionHandler
     * 
     * @param mixed $callback callback
     * @return mixed
     */
    public static function setExceptionHandler($callback) {

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
    public static function doError($code, $msg, $file, $line, $params) {

    }

    /**
     * __clone
     * 
     * @return mixed
     */
    private function __clone() {

    }

    /**
     * __construct
     * 
     * @param mixed $message message
     * @param mixed $code code
     * @param mixed $previous previous
     * @return mixed
     */
    public function __construct($message, $code, $previous) {

    }

    /**
     * __wakeup
     * 
     * @return mixed
     */
    public function __wakeup() {

    }

    /**
     * getMessage
     * 
     * @return mixed
     */
    public function getMessage() {

    }

    /**
     * getCode
     * 
     * @return mixed
     */
    public function getCode() {

    }

    /**
     * getFile
     * 
     * @return mixed
     */
    public function getFile() {

    }

    /**
     * getLine
     * 
     * @return mixed
     */
    public function getLine() {

    }

    /**
     * getTrace
     * 
     * @return mixed
     */
    public function getTrace() {

    }

    /**
     * getPrevious
     * 
     * @return mixed
     */
    public function getPrevious() {

    }

    /**
     * getTraceAsString
     * 
     * @return mixed
     */
    public function getTraceAsString() {

    }

    /**
     * __toString
     * 
     * @return mixed
     */
    public function __toString() {

    }

}