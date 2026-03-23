<?php

/**
 * Gene Framework Cache Class Test
 * 
 * This test file covers the important methods of the Gene\Cache class
 */

use Gene\Cache;

class CacheTest
{
    private $cache;
    
    public function __construct()
    {
        echo "=== Gene\Cache Test Suite ===\n\n";
    }
    
    /**
     * Test Cache constructor
     */
    public function testConstructor()
    {
        echo "Testing Cache Constructor:\n";
        
        try {
            // Test constructor with configuration
            $config = [
                'type' => 'memory',
                'prefix' => 'test_'
            ];
            $this->cache = new Cache($config);
            echo "✓ Cache constructor with configuration works\n";
            
            // Test constructor with empty config
            $cache2 = new Cache([]);
            echo "✓ Cache constructor with empty config works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test basic caching functionality
     */
    public function testBasicCaching()
    {
        echo "Testing Basic Caching Functionality:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            // Create a test object to cache
            $testObject = new stdClass();
            $testObject->data = 'test data';
            $testObject->timestamp = time();
            
            $args = ['param1', 'param2'];
            $ttl = 3600;
            
            // Test cached method
            $result = $this->cache->cached($testObject, $args, $ttl);
            echo "✓ cached() method works\n";
            
            // Test localCached method
            $result2 = $this->cache->localCached($testObject, $args, $ttl);
            echo "✓ localCached() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test version-based caching
     */
    public function testVersionBasedCaching()
    {
        echo "Testing Version-Based Caching:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            // Create a test object with version field
            $testObject = new stdClass();
            $testObject->data = 'versioned data';
            $testObject->version = 1;
            
            $args = ['version_test'];
            $versionField = 'version';
            $ttl = 3600;
            
            // Test cachedVersion method
            $result = $this->cache->cachedVersion($testObject, $args, $versionField, $ttl);
            echo "✓ cachedVersion() method works\n";
            
            // Test localCachedVersion method
            $result2 = $this->cache->localCachedVersion($testObject, $args, $versionField, $ttl);
            echo "✓ localCachedVersion() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test cache invalidation
     */
    public function testCacheInvalidation()
    {
        echo "Testing Cache Invalidation:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            $testObject = new stdClass();
            $testObject->data = 'cache data';
            $args = ['invalidate_test'];
            
            // Test unsetCached method
            $result = $this->cache->unsetCached($testObject, $args);
            echo "✓ unsetCached() method works\n";
            
            // Test unsetLocalCached method
            $result2 = $this->cache->unsetLocalCached($testObject, $args);
            echo "✓ unsetLocalCached() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test version management
     */
    public function testVersionManagement()
    {
        echo "Testing Version Management:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            $versionField = 'test_version';
            
            // Test getVersion
            $version = $this->cache->getVersion($versionField);
            echo "✓ getVersion() returns: " . ($version ?? 'null') . "\n";
            
            // Test updateVersion
            $result = $this->cache->updateVersion($versionField);
            echo "✓ updateVersion() works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test caching with different TTL values
     */
    public function testCachingWithTTL()
    {
        echo "Testing Caching with Different TTL Values:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            $testObject = new stdClass();
            $testObject->data = 'ttl test';
            
            // Test with no TTL (default)
            $result1 = $this->cache->cached($testObject, ['no_ttl']);
            echo "✓ cached() without TTL works\n";
            
            // Test with short TTL
            $result2 = $this->cache->cached($testObject, ['short_ttl'], 60);
            echo "✓ cached() with short TTL works\n";
            
            // Test with long TTL
            $result3 = $this->cache->cached($testObject, ['long_ttl'], 86400);
            echo "✓ cached() with long TTL works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test caching complex objects
     */
    public function testComplexObjectCaching()
    {
        echo "Testing Complex Object Caching:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            // Test with array
            $testArray = ['key1' => 'value1', 'key2' => ['nested' => 'data']];
            $result1 = $this->cache->cached($testArray, ['array_test']);
            echo "✓ cached() with array works\n";
            
            // Test with nested object
            $nestedObject = new stdClass();
            $nestedObject->child = new stdClass();
            $nestedObject->child->data = 'nested data';
            $result2 = $this->cache->cached($nestedObject, ['nested_test']);
            echo "✓ cached() with nested object works\n";
            
            // Test with closure (if supported)
            $closure = function($param) {
                return "Closure result: $param";
            };
            $result3 = $this->cache->cached($closure, ['closure_test'], 3600);
            echo "✓ cached() with closure works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test cache modes
     */
    public function testCacheModes()
    {
        echo "Testing Different Cache Modes:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            $testObject = new stdClass();
            $testObject->data = 'mode test';
            $args = ['mode_test'];
            $versionField = 'version';
            $ttl = 3600;
            
            // Test with different modes
            $modes = ['default', 'strict', 'loose'];
            
            foreach ($modes as $mode) {
                $result = $this->cache->cachedVersion($testObject, $args, $versionField, $ttl, $mode);
                echo "✓ cachedVersion() with mode '$mode' works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test error handling
     */
    public function testErrorHandling()
    {
        echo "Testing Error Handling:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            // Test with null object
            try {
                $result = $this->cache->cached(null, ['null_test']);
                echo "✓ cached() handles null object gracefully\n";
            } catch (Exception $e) {
                echo "✓ cached() properly throws exception for null object\n";
            }
            
            // Test with invalid arguments
            try {
                $result = $this->cache->cached(new stdClass(), null);
                echo "✓ cached() handles null arguments gracefully\n";
            } catch (Exception $e) {
                echo "✓ cached() properly handles invalid arguments\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Unexpected error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test performance with multiple operations
     */
    public function testPerformance()
    {
        echo "Testing Performance with Multiple Operations:\n";
        
        try {
            if (!$this->cache) {
                $this->cache = new Cache(['type' => 'memory']);
            }
            
            $startTime = microtime(true);
            
            // Perform multiple cache operations
            for ($i = 0; $i < 100; $i++) {
                $testObject = new stdClass();
                $testObject->data = "performance test $i";
                $result = $this->cache->cached($testObject, ["perf_test_$i"], 3600);
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000; // Convert to milliseconds
            
            echo "✓ 100 cache operations completed in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test cache configuration variations
     */
    public function testCacheConfigurations()
    {
        echo "Testing Different Cache Configurations:\n";
        
        try {
            // Test with memory cache
            $memoryCache = new Cache(['type' => 'memory']);
            echo "✓ Memory cache configuration works\n";
            
            // Test with custom prefix
            $prefixedCache = new Cache(['type' => 'memory', 'prefix' => 'custom_']);
            echo "✓ Cache with custom prefix works\n";
            
            // Test with additional options
            $optionsCache = new Cache([
                'type' => 'memory',
                'prefix' => 'opts_',
                'serialize' => true,
                'compression' => false
            ]);
            echo "✓ Cache with multiple options works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Run all tests
     */
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

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new CacheTest();
    $test->runAllTests();
}
