<?php
namespace Gene;

/**
 * IP literal / CIDR utilities shared by trusted-proxy resolution,
 * internal-network gates and application code.
 *
 * Parsing is strict (IPv4-mapped IPv6 internal representation):
 * malformed literals return false instead of matching loosely.
 */
final class Ip
{
    /**
     * Strict literal validation: "1.2.3.4", "2001:db8::1", "::ffff:10.0.0.1".
     * Rejects duplicate "::", trailing garbage, bad embedded v4, leading zeros.
     *
     * @param string $ip
     * @return bool
     */
    public static function isValid($ip) {}

    /**
     * Membership test against one CIDR or a list.
     * v4 prefixes /0../32 map onto the v4-mapped IPv6 space, so a v6-shaped
     * "::ffff:10.1.2.3" still matches "10.0.0.0/8". Invalid CIDR entries are
     * ignored (E_WARNING) and simply never match.
     *
     * @param string $ip
     * @param string|array $cidrs e.g. '10.0.0.0/8' or ['10.0.0.0/8','172.18.0.0/16']
     * @return bool
     */
    public static function inCidr($ip, $cidrs) {}

    /**
     * Internal-network check: RFC1918, loopback, link-local, CGNAT,
     * ULA fc00::/7, fe80::/10, ::1.
     *
     * @param string $ip
     * @return bool
     */
    public static function isInternal($ip) {}
}
