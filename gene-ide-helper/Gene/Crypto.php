<?php
namespace Gene;

/**
 * HMAC tokens, random ids, AES-256-GCM. Not JWT.
 * Keys come from config/env — never derive from database passwords.
 */
class Crypto
{
    /**
     * @param string $data
     * @return string
     */
    public static function base64UrlEncode($data) {}

    /**
     * @param string $data
     * @return string
     */
    public static function base64UrlDecode($data) {}

    /**
     * @param array $payload purpose/aud belong in $payload
     * @param string $secret
     * @param int $ttl seconds; 0 = no exp
     * @return string
     */
    public static function hmacToken(array $payload, $secret, $ttl = 0) {}

    /**
     * @param string $token
     * @param string $secret
     * @return array
     * @throws \Exception invalid or expired
     */
    public static function hmacVerify($token, $secret, $leeway = 0) {}

    /**
     * HMAC-SHA256 then base64url.
     */
    public static function hmacSign($data, $secret) {}

    /**
     * Constant-time check. Returns false, never throws.
     */
    public static function hmacCheck($data, $sig, $secret) {}

    /**
     * |now - $unix| <= $maxSkew
     */
    public static function tsSkew($unix, $maxSkew = 1800) {}

    /**
     * @param string $prefix
     * @param int $bytes 1..64, default 16
     * @return string
     */
    public static function randomId($prefix = '', $bytes = 16) {}

    /**
     * AES-256-GCM. $key must be exactly 32 bytes.
     * Wire format: base64url(iv[12] + tag[16] + ciphertext).
     *
     * @param string $plain
     * @param string $key
     * @return string
     */
    public static function encrypt($plain, $key) {}

    /**
     * @param string $cipher
     * @param string $key 32 bytes
     * @return string
     */
    public static function decrypt($cipher, $key) {}
}
