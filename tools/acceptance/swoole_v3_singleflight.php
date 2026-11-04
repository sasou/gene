<?php
/**
 * Performance V3 §3.5 single-flight acceptance (Linux + Swoole only).
 *
 *   php -d gene.runtime_type=2 -d gene.cache_fill_wait_ms=0    tools/acceptance/swoole_v3_singleflight.php
 *   php -d gene.runtime_type=2 -d gene.cache_fill_wait_ms=2000 tools/acceptance/swoole_v3_singleflight.php
 *
 * N coroutines miss the same key while the producer yields (Co::sleep).
 *  - wait_ms=0   : every coroutine fills (producer calls == N), concurrency
 *                  peak == N, nothing coalesced.
 *  - wait_ms>0   : one fill, N-1 coalesced, all coroutines get equal data.
 * Also checked with wait_ms>0: a throwing leader makes waiters fill
 * themselves; results holding objects are never shared; cachedVersion is
 * tracked but never coalesced; distinct keys do not wait on each other;
 * no in-flight entry is left behind. Exit 0 = ALL-PASS.
 */

use Gene\Cache\Cache;
use Gene\Di;
use Swoole\Coroutine;

if (!extension_loaded('swoole')) {
    fwrite(STDERR, "swoole extension required\n");
    exit(2);
}
if ((int)ini_get('gene.runtime_type') < 2) {
    fwrite(STDERR, "run with -d gene.runtime_type=2\n");
    exit(2);
}

class SfStore
{
    public $data = [];
    public function get($k) { return $this->data[$k] ?? false; }
    public function set($k, $v, $ttl = 0) { $this->data[$k] = $v; return true; }
    public function incr($k, $v = 1) { $this->data[$k] = ($this->data[$k] ?? 0) + $v; return $this->data[$k]; }
    public function delete($k) { unset($this->data[$k]); return true; }
}

class SfProducer
{
    public static $calls = 0;
    public function rows($id) { self::$calls++; Coroutine::sleep(0.05); return ['id' => $id, 'rows' => range(1, 50)]; }
    public function boom($id) { self::$calls++; Coroutine::sleep(0.05); throw new RuntimeException('boom'); }
    public function obj($id) { self::$calls++; Coroutine::sleep(0.05); return ['o' => new ArrayObject([$id])]; }
}

$waitMs = (int)ini_get('gene.cache_fill_wait_ms');
$n = 50;
$fail = 0;
function check($name, $cond, $detail = '') {
    global $fail;
    echo ($cond ? '✓ ' : '✗ ') . $name . ($cond || $detail === '' ? '' : " -- $detail") . "\n";
    if (!$cond) $fail++;
}
function stats() { return (new Gene\Memory())->stats(); }

Coroutine\run(function () use ($n, $waitMs) {
    $store = new SfStore();
    Di::set('sfStore', $store);
    $cache = new Cache(['hook' => 'sfStore', 'sign' => 'sf:', 'versionSign' => 'sfv:']);
    $p = new SfProducer();

    /* Gene's DI registry is per coroutine context: register in each child. */
    $fan = function (callable $fn) use ($n, $store) {
        $wg = new Coroutine\WaitGroup();
        $out = [];
        for ($i = 0; $i < $n; $i++) {
            $wg->add();
            Coroutine::create(function () use ($fn, $i, &$out, $wg, $store) {
                Di::set('sfStore', $store);
                try { $out[$i] = $fn(); } catch (Throwable $e) { $out[$i] = 'EX:' . $e->getMessage(); }
                $wg->done();
            });
        }
        $wg->wait();
        return $out;
    };

    /* same key, plain cached() */
    SfProducer::$calls = 0;
    $s0 = stats();
    $out = $fan(function () use ($cache, $p) { return $cache->cached([$p, 'rows'], [1], 60); });
    $s1 = stats();
    $allEqual = count(array_unique(array_map('serialize', $out))) === 1 && ($out[0]['id'] ?? null) === 1;
    check("cached(): $n coroutines get equal data", $allEqual);
    if ($waitMs > 0) {
        check("cached(): one producer call", SfProducer::$calls === 1, (string)SfProducer::$calls);
        check("cached(): " . ($n - 1) . " coalesced", $s1['cache_fill_coalesced'] - $s0['cache_fill_coalesced'] === $n - 1,
            (string)($s1['cache_fill_coalesced'] - $s0['cache_fill_coalesced']));
    } else {
        check("cached(): every coroutine fills (no coalescing)", SfProducer::$calls === $n, (string)SfProducer::$calls);
        check("cached(): concurrency peak == $n", $s1['cache_fill_concurrent_max'] >= $n, (string)$s1['cache_fill_concurrent_max']);
    }

    /* distinct keys never wait on each other */
    SfProducer::$calls = 0;
    $t = microtime(true);
    $i = 0;
    $fan(function () use ($cache, $p, &$i) { return $cache->cached([$p, 'rows'], [1000 + $i++], 60); });
    check("distinct keys: $n fills, run in parallel", SfProducer::$calls === $n && microtime(true) - $t < 1.0,
        SfProducer::$calls . ' calls, ' . round(microtime(true) - $t, 3) . 's');

    if ($waitMs > 0) {
        /* throwing leader: waiters fill themselves, each sees the exception */
        SfProducer::$calls = 0;
        $out = $fan(function () use ($cache, $p) { return $cache->cached([$p, 'boom'], [2], 60); });
        $ex = count(array_filter($out, function ($v) { return $v === 'EX:boom'; }));
        check("throwing leader: no waiter gets a shared value", $ex === $n, "$ex/$n");

        /* objects are never shared across coroutines */
        SfProducer::$calls = 0;
        $fan(function () use ($cache, $p) { return $cache->localCached([$p, 'obj'], [3], 60); });
        check("object result: waiters refill, no shared instance", SfProducer::$calls > 1, (string)SfProducer::$calls);

        /* version caches are tracked but never coalesced */
        SfProducer::$calls = 0;
        $s0 = stats();
        $fan(function () use ($cache, $p) { return $cache->cachedVersion([$p, 'rows'], [4], ['sfver' => 1], 60); });
        $s1 = stats();
        check("cachedVersion(): not coalesced", SfProducer::$calls === $n && $s1['cache_fill_coalesced'] === $s0['cache_fill_coalesced'],
            SfProducer::$calls . ' calls');
    }

    check("no in-flight entries left", stats()['cache_fill_inflight'] === 0, (string)stats()['cache_fill_inflight']);
});

echo $fail ? "\nFAIL ($fail)\n" : "\nALL-PASS\n";
exit($fail ? 1 : 0);
