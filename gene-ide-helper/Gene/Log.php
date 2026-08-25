<?php
namespace Gene;

/**
 * Log
 * 
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
 
class Log
{
    const LEVEL_DEBUG   = 1;
    const LEVEL_INFO    = 2;
    const LEVEL_NOTICE  = 3;
    const LEVEL_WARNING = 4;
    const LEVEL_ERROR   = 5;
    const LEVEL_CRITICAL  = 6;
    const LEVEL_ALERT     = 7;
    const LEVEL_EMERGENCY = 8;

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
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function debug(string $message, array $context = []) {

    }

    /**
     * info
     * 
     * @param string $message
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function info(string $message, array $context = []) {

    }

    /**
     * notice
     * 
     * @param string $message
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function notice(string $message, array $context = []) {

    }

    /**
     * warning
     * 
     * @param string $message
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function warning(string $message, array $context = []) {

    }

    /**
     * error
     * 
     * @param string $message
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function error(string $message, array $context = []) {

    }

    /**
     * critical
     * Log a critical-level message (RFC-5424 severity above ERROR).
     *
     * @param string $message
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function critical(string $message, array $context = []) {

    }

    /**
     * alert
     * Log an alert-level message (RFC-5424 severity).
     *
     * @param string $message
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function alert(string $message, array $context = []) {

    }

    /**
     * emergency
     * Log an emergency-level message (RFC-5424 severity, highest).
     *
     * @param string $message
     * @param array $context structured context data (JSON-encoded in log line).
     *   Gene\Context request_id is merged in automatically when set.
     * @return void
     */
    public static function emergency(string $message, array $context = []) {

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
