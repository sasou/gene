<?php
namespace Gene;

/**
 * Unified password API over PHP's password_hash/verify builtins plus the
 * app legacy format md5(salt . sha1(salt . pwd)), with transparent-upgrade
 * rehash reporting and a dummy-verify path for timing resistance.
 */
final class Password
{
    /**
     * @param string $password
     * @param array|null $opts {'algo':'bcrypt'|'argon2i'|'argon2id'|'default'|int, ...PHP options}
     * @return string
     * @throws \Exception on hash failure / bad algo
     */
    public static function hash($password, array $opts = null) {}

    /**
     * Modern-format verify ('$'-prefixed hash). With opts.salt / opts.legacy
     * a legacy hash is accepted too (same as verifyAny).
     *
     * @param string $password
     * @param string $hash
     * @param array|null $opts
     * @return array{ok: bool, needs_rehash: bool, hash?: string}
     *         'hash' (fresh PASSWORD_DEFAULT hash) only when ok && needs_rehash —
     *         persist it to complete the transparent upgrade.
     */
    public static function verify($password, $hash, array $opts = null) {}

    /**
     * App legacy format: md5($salt . sha1($salt . $password)) === $hash,
     * constant-time compare. $salt defaults to ''.
     *
     * @param string $password
     * @param string $hash 32-hex
     * @param string|null $salt
     * @return bool
     */
    public static function verifyLegacy($password, $hash, $salt = null) {}

    /**
     * Auto-detect verify: '$'-prefixed → modern; 32-hex → legacy md5salt
     * (opts.salt); opts.legacy callable overrides the legacy scheme.
     *
     * @param string $password
     * @param string $hash
     * @param array|null $opts
     * @return array{ok: bool, algo: 'bcrypt'|'argon2i'|'argon2id'|'legacy'|'unknown',
     *               needs_rehash: bool, hash?: string}
     */
    public static function verifyAny($password, $hash, array $opts = null) {}

    /**
     * True when $hash should be re-hashed with the current default
     * algo/options — also true for any non-password_hash() format (legacy).
     *
     * @param string $hash
     * @param array|null $opts
     * @return bool
     */
    public static function needsRehash($hash, array $opts = null) {}

    /**
     * Always false, at the same cost as a real PASSWORD_DEFAULT verify.
     * Call for unknown users so "user not found" isn't timing-detectable.
     *
     * @param string $password
     * @return false
     */
    public static function dummy($password) {}
}
