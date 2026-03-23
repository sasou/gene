<?php
namespace Gene;

/**
 * Log
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.3.5
 */
 
class Log
{
    const LEVEL_DEBUG   = 1;
    const LEVEL_INFO    = 2;
    const LEVEL_NOTICE  = 3;
    const LEVEL_WARNING = 4;
    const LEVEL_ERROR   = 5;

    /**
     * @var string|null
     */
    protected static $file = null;

    /**
     * @var int
     */
    protected static $level = self::LEVEL_DEBUG;

    /**
     * debug
     * 
     * @param string $message
     * @return void
     */
    public static function debug(string $message) {

    }

    /**
     * info
     * 
     * @param string $message
     * @return void
     */
    public static function info(string $message) {

    }

    /**
     * notice
     * 
     * @param string $message
     * @return void
     */
    public static function notice(string $message) {

    }

    /**
     * warning
     * 
     * @param string $message
     * @return void
     */
    public static function warning(string $message) {

    }

    /**
     * error
     * 
     * @param string $message
     * @return void
     */
    public static function error(string $message) {

    }

    /**
     * exception
     * 
     * @param \Throwable $exception
     * @param string|null $message
     * @return void
     */
    public static function exception(\Throwable $exception, string $message = null) {

    }

    /**
     * setFile
     * 
     * @param string $file
     * @return void
     */
    public static function setFile(string $file) {

    }

    /**
     * getFile
     * 
     * @return string|null
     */
    public static function getFile() {

    }

    /**
     * setLevel
     * 
     * @param int $level
     * @return void
     */
    public static function setLevel(int $level) {

    }

    /**
     * getLevel
     * 
     * @return int
     */
    public static function getLevel() {

    }

}
