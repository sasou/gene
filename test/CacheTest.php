<?php

/**
 * Gene Framework Cache Class Test
 *
 * Covers Gene\Cache\Cache against the real API:
 * - config keys: sign / hook / versionSign
 * - callable form: [object|class, method]
 * - processCached uses Gene\Memory (no external store)
 * - cached / unsetCached need a DI hook with get/set/delete
 */

use Gene\Cache\Cache;
use Gene\Di;

/**
 * In-memory store used as Gene\Cache\Cache hook (get/set/delete/incr).
 */
class GeneCacheTestStore
{
    private $data = [];

    public function get($key)
    {
        if (is_array($key)) {
            $out = [];
            foreach ($key as $k) {
                if (is_string($k) && array_key_exists($k, $this->data)) {
                    $out[$k] = $this->data[$k];
                }
            }
            return $out === [] ? false : $out;
        }
        return array_key_exists($key, $this->data) ? $this->data[$key] : false;
    }

    public function set($key, $value, $ttl = 0)
    {
        $this->data[$key] = $value;
        return true;
    }

    public function delete($key)
    {
        unset($this->data[$key]);
        return true;
    }

    public function incr($key, $val = 1)
    {
        if (!isset($this->data[$key])) {
            $this->data[$key] = 0;
        }
        $this->data[$key] += (int) $val;
        return $this->data[$key];
    }

    public function clear()
    {
        $this->data = [];
    }
}

/**
 * Callable producer for method-level cache tests.
 */
class GeneCacheTestHelper
{
    public static $calls = 0;

    public function produce($param = null)
    {
        self::$calls++;
        return [
            'param' => $param,
            'calls' => self::$calls,
            'ts' => time(),
        ];
    }
}

class CacheTest
{
    private $cache;
    private $store;
    private $helper;
    private $callable;

    public function __construct()
    {
        echo "=== Gene\\Cache\\Cache Test Suite ===\n\n";
        $this->store = new GeneCacheTestStore();
        $this->helper = new GeneCacheTestHelper();
        $this->callable = [$this->helper, 'produce'];
        Di::set('testCacheStore', $this->store);
        GeneCacheTestHelper::$calls = 0;
    }

    private function makeCache(array $extra = [])
    {
        return new Cache(array_merge([
            'hook' => 'testCacheStore',
            'sign' => 'test:',
            'versionSign' => 'ver:',
        ], $extra));
    }

    public function testConstructor()
    {
        echo "Testing Cache Constructor:\n";

        try {
            $this->cache = $this->makeCache();
            echo "✓ Cache constructor with sign/hook works\n";

            $cache2 = new Cache([]);
            echo "✓ Cache constructor with empty config works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testBasicCaching()
    {
        echo "Testing Basic Caching Functionality:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();
            GeneCacheTestHelper::$calls = 0;

            $args = ['param1'];
            $ttl = 3600;

            $result = $this->cache->cached($this->callable, $args, $ttl);
            if (!is_array($result) || ($result['param'] ?? null) !== 'param1') {
                throw new RuntimeException('cached() returned unexpected value');
            }
            echo "✓ cached() method works\n";

            $result2 = $this->cache->cached($this->callable, $args, $ttl);
            if (GeneCacheTestHelper::$calls !== 1) {
                throw new RuntimeException('cached() did not hit store on second call');
            }
            echo "✓ cached() hits storage on repeat call\n";

            $proc = $this->cache->processCached($this->callable, ['proc1'], $ttl);
            if (!is_array($proc) || ($proc['param'] ?? null) !== 'proc1') {
                throw new RuntimeException('processCached() returned unexpected value');
            }
            echo "✓ processCached() method works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testVersionBasedCaching()
    {
        echo "Testing Version-Based Caching:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();
            GeneCacheTestHelper::$calls = 0;

            $args = ['version_test'];
            $versionField = ['user', 1];
            $ttl = 3600;

            $result = $this->cache->cachedVersion($this->callable, $args, $versionField, $ttl);
            if (!is_array($result)) {
                throw new RuntimeException('cachedVersion() returned null (check hook/versionSign)');
            }
            echo "✓ cachedVersion() method works\n";

            $result2 = $this->cache->processCachedVersion($this->callable, $args, $versionField, $ttl);
            if (!is_array($result2)) {
                throw new RuntimeException('processCachedVersion() returned null');
            }
            echo "✓ processCachedVersion() method works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testCacheInvalidation()
    {
        echo "Testing Cache Invalidation:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();
            GeneCacheTestHelper::$calls = 0;

            $args = ['invalidate_test'];
            $this->cache->cached($this->callable, $args, 3600);
            $callsAfterSet = GeneCacheTestHelper::$calls;

            $this->cache->unsetCached($this->callable, $args);
            echo "✓ unsetCached() method works\n";

            $this->cache->cached($this->callable, $args, 3600);
            if (GeneCacheTestHelper::$calls <= $callsAfterSet) {
                throw new RuntimeException('unsetCached() did not clear entry');
            }
            echo "✓ unsetCached() forces recompute\n";

            $this->cache->processCached($this->callable, ['proc_inv'], 3600);
            $this->cache->unsetProcessCached($this->callable, ['proc_inv']);
            echo "✓ unsetProcessCached() method works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testVersionManagement()
    {
        echo "Testing Version Management:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();

            $versionField = ['user', 1];
            $version = $this->cache->getVersion($versionField);
            echo "✓ getVersion() returns: " . (is_array($version) ? 'array' : var_export($version, true)) . "\n";

            $this->cache->updateVersion($versionField);
            echo "✓ updateVersion() works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testCachingWithTTL()
    {
        echo "Testing Caching with Different TTL Values:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();

            $this->cache->cached($this->callable, ['no_ttl']);
            echo "✓ cached() without TTL works\n";

            $this->cache->cached($this->callable, ['short_ttl'], 60);
            echo "✓ cached() with short TTL works\n";

            $this->cache->cached($this->callable, ['long_ttl'], 86400);
            echo "✓ cached() with long TTL works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testComplexObjectCaching()
    {
        echo "Testing Complex Object Caching:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();

            $result1 = $this->cache->processCached($this->callable, [['key1' => 'value1']]);
            if (!is_array($result1)) {
                throw new RuntimeException('processCached with array arg failed');
            }
            echo "✓ processCached() with array argument works\n";

            $result2 = $this->cache->processCached($this->callable, ['nested']);
            if (!is_array($result2)) {
                throw new RuntimeException('processCached nested call failed');
            }
            echo "✓ processCached() with scalar argument works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testCacheModes()
    {
        echo "Testing Different Cache Modes:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();

            $args = ['mode_test'];
            $versionField = ['user', 1];
            $ttl = 3600;

            // mode is bool (strict element-count check when true)
            $this->cache->cachedVersion($this->callable, $args, $versionField, $ttl, false);
            echo "✓ cachedVersion() with mode false works\n";

            $this->cache->cachedVersion($this->callable, $args, $versionField, $ttl, true);
            echo "✓ cachedVersion() with mode true works\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testErrorHandling()
    {
        echo "Testing Error Handling:\n";

        try {
            $this->cache = $this->makeCache();

            // Non-array callable object is ignored by gene_cache_call → null
            $result = $this->cache->cached(new stdClass(), ['null_test']);
            echo "✓ cached() handles non-array object gracefully (returns " . var_export($result, true) . ")\n";

            $result2 = $this->cache->cached($this->callable, null);
            echo "✓ cached() handles null arguments gracefully (returns " . var_export($result2, true) . ")\n";

            $empty = new Cache([]);
            $result3 = $empty->cached($this->callable, ['x']);
            if ($result3 !== null) {
                throw new RuntimeException('empty config should make cached() return null');
            }
            echo "✓ cached() without sign/hook returns null\n";
        } catch (\Throwable $e) {
            echo "✗ Unexpected error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testPerformance()
    {
        echo "Testing Performance with Multiple Operations:\n";

        try {
            $this->cache = $this->makeCache();
            $this->store->clear();

            $startTime = microtime(true);
            for ($i = 0; $i < 100; $i++) {
                $this->cache->processCached($this->callable, ["perf_test_$i"], 3600);
            }
            $duration = (microtime(true) - $startTime) * 1000;

            echo "✓ 100 processCached operations completed in " . number_format($duration, 2) . "ms\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function testCacheConfigurations()
    {
        echo "Testing Different Cache Configurations:\n";

        try {
            $memoryCache = $this->makeCache();
            echo "✓ Cache with sign/hook configuration works\n";

            $hashed = $this->makeCache(['hash_mode' => 1]);
            $hashed->processCached($this->callable, ['hash_mode_test']);
            echo "✓ Cache with hash_mode works\n";

            $full = $this->makeCache([
                'hash_mode' => 2,
                'versionSign' => 'database:',
            ]);
            echo "✓ Cache with multiple options works\n";
            unset($memoryCache, $full);
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function runAllTests()
    {
        $this->testConstructor();
        $this->testBasicCaching();
        $this->testVersionBasedCaching();
        $this->testCacheInvalidation();
        $this->testVersionManagement();
        $this->testCachingWithTTL();
        $this->testComplexObjectCaching();
        $this->testCacheModes();
        $this->testErrorHandling();
        $this->testPerformance();
        $this->testCacheConfigurations();

        echo "=== Cache Test Suite Complete ===\n";
    }
}

if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new CacheTest();
    $test->runAllTests();
}
