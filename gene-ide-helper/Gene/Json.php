<?php
namespace Gene;

/**
 * JSON encode/decode that never swallows errors.
 * Flags: JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES.
 * Invalid input throws; never returns [].
 */
class Json
{
    /**
     * @param mixed $data
     * @return string
     * @throws \Exception
     */
    public static function encode($data) {}

    /**
     * @param string $json
     * @return mixed
     * @throws \Exception
     */
    public static function decode($json) {}
}
