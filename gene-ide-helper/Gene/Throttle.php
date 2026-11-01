<?php
namespace Gene;

/**
 * Atomic failure counting / throttling over cache stores.
 * Replaces read-compare-write counters that race under concurrency.
 *
 * Semantics (fixed window, "count failures, clear on success"):
 *   hit()      atomic +1; $ttl is attached when the window opens
 *   tooMany()  READ-ONLY gate: count >= max (does not consume budget)
 *   attempt()  counts THIS attempt atomically; true while within budget
 *   remaining / count / clear
 *
 * Keys are namespaced under "gene:thr:".
 * Store dispatch: Gene\Memory → C incr-with-ttl under wrlock;
 * Gene\Cache\Redis → one Lua (INCR + EXPIRE on create);
 * memcached/phpredis-shaped stores → add / set(nx,ex) fallback.
 * A store outage returns -1 from hit() and fail-opens attempt().
 */
final class Throttle
{
    /**
     * @param object $store Memory | Cache\Redis | memcached-shaped store
     * @param string $key
     * @param int $ttl window seconds, attached on create (default 3600)
     * @return int new count, or -1 when the store refused
     */
    public static function hit($store, $key, $ttl = 3600) {}

    /**
     * Read-only: true when the recorded count already reached $max.
     * Pattern: if (tooMany) reject; ...; $ok ? clear() : hit().
     *
     * @param object $store
     * @param string $key
     * @param int $max
     * @return bool
     */
    public static function tooMany($store, $key, $max) {}

    /**
     * Atomic hit + check: true while the new count is within $max.
     * Fail-open (true) when the store is unavailable.
     *
     * @param object $store
     * @param string $key
     * @param int $max
     * @param int $ttl window seconds (default 3600)
     * @return bool
     */
    public static function attempt($store, $key, $max, $ttl = 3600) {}

    /**
     * @param object $store
     * @param string $key
     * @return int current count (0 when absent)
     */
    public static function count($store, $key) {}

    /**
     * @param object $store
     * @param string $key
     * @param int $max
     * @return int max - count, floored at 0
     */
    public static function remaining($store, $key, $max) {}

    /**
     * Delete the counter (call on success to end the failure window).
     *
     * @param object $store
     * @param string $key
     * @return bool
     */
    public static function clear($store, $key) {}
}
