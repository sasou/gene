<?php
namespace Gene;

/** UTF-8 ingest primitives. No request state. */
class Text
{
    /**
     * UTF-8 code points after NUL strip / invalid → U+FFFD.
     * @return int
     */
    public static function utf8Len($s) {}

    /**
     * Strip NUL; replace invalid UTF-8 with U+FFFD; keep 4-byte emoji.
     * @return string
     */
    public static function sanitizeMb4($s) {}

    /**
     * Paragraph merge then overlapping hard split. maxChars capped at 8192.
     * @return list<string>
     */
    public static function chunk($s, $maxChars = 1200, $overlap = 80) {}
}
