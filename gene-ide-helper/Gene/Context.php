<?php
namespace Gene;

/**
 * Per-request KV bag. Destroyed on FPM RSHUTDOWN / Swoole cleanup().
 * Use this instead of static properties so Swoole workers never leak
 * request-A values into request-B.
 */
class Context
{
    /**
     * @param string $key
     * @param mixed $value
     * @return bool
     */
    public static function set($key, $value) {}

    /**
     * @param string $key
     * @param mixed $default
     * @return mixed
     */
    public static function get($key, $default = null) {}

    public static function has($key) {}

    /**
     * @return array
     */
    public static function all() {}
}
