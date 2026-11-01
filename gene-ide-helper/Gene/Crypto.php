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

    /**
     * Register the process keyring (kid => 32-byte key). $active selects the
     * sealing kid; omitted → last entry. Call once at bootstrap from env/config.
     *
     * @param array $keys
     * @param string|null $active
     * @return bool
     */
    public static function keyring(array $keys, $active = null) {}

    /**
     * HKDF-derive $len bytes (1..255) from the active keyring key + context.
     *
     * @param string $ctx purpose string, e.g. 'rest'
     * @param int $len
     * @return string binary
     */
    public static function derive($ctx, $len = 32) {}

    /**
     * AES-256-GCM seal under the keyring. Wire: "g1.<kid>.<base64url>",
     * $ctx bound as AAD (different context → decrypt fails).
     *
     * @param string $plain
     * @param string|null $ctx
     * @return string
     * @throws \Exception when no keyring / openssl unavailable
     */
    public static function seal($plain, $ctx = null) {}

    /**
     * @param string $cipher "g1.<kid>.<base64url>"
     * @param string|null $ctx must match seal()'s context
     * @return string|null plaintext, null on tamper/wrong ctx/unknown kid
     */
    public static function unseal($cipher, $ctx = null) {}

    /**
     * REST HMAC signature headers. Covers method + path(+query) + ts + nonce
     * + sha256(body). $opts: secret (required), body, ts, nonce.
     *
     * @param string $method
     * @param string $path path optionally including ?query
     * @param array|null $opts
     * @return array{ts: int, nonce: string, sign: string}
     */
    public static function signRequest($method, $path, array $opts = null) {}

    /**
     * Random base32 TOTP secret (RFC 4648, no padding).
     *
     * @param int $bytes 10..64, default 20
     * @return string
     */
    public static function totpSecret($bytes = 20) {}

    /**
     * TOTP code for $secret (base32) at $ts (0 = now).
     *
     * @param string $secret base32
     * @param int $ts unix ts, 0 = time()
     * @param int $step 1..300, default 30
     * @param int $digits 6..8, default 6
     * @return string zero-padded code
     */
    public static function totp($secret, $ts = 0, $step = 30, $digits = 6) {}

    /**
     * Verify a TOTP code (strict: exactly $digits ASCII digits).
     * $opts['used'] = store object (Memory/Redis/memcached-shaped) → the
     * accepted code is marked single-use for the whole window: a second
     * verify with the same code returns false (replay protection).
     *
     * @param string $secret base32
     * @param string $code
     * @param int $window +-window steps accepted, default 1 (max 10)
     * @param int $step default 30
     * @param int $digits default 6
     * @param array|null $opts
     * @return bool
     */
    public static function totpVerify($secret, $code, $window = 1, $step = 30, $digits = 6, array $opts = null) {}

    /**
     * CSPRNG code of $len chars over $alphabet (default digits),
     * unbiased rejection sampling. CAPTCHA/2FA codes.
     *
     * @param int $len 1..128
     * @param string|null $alphabet 2..256 chars
     * @return string
     */
    public static function randomCode($len = 6, $alphabet = null) {}
}
