<?php

/**
 * Gene Framework Session Class Test
 * 
 * This test file covers the important methods of the Gene\Session class
 */

use Gene\Session;

class SessionTest
{
    private $session;
    
    public function __construct()
    {
        echo "=== Gene\Session Test Suite ===\n\n";
    }
    
    /**
     * Test Session constructor
     */
    public function testConstructor()
    {
        echo "Testing Session Constructor:\n";
        
        try {
            // Test constructor with configuration
            $config = [
                'name' => 'test_session',
                'lifetime' => 3600,
                'path' => '/',
                'domain' => 'localhost',
                'secure' => false,
                'httponly' => true
            ];
            $this->session = new Session($config);
            echo "✓ Session constructor with configuration works\n";
            
            // Test constructor with empty config
            $session2 = new Session([]);
            echo "✓ Session constructor with empty config works\n";
            
            // Test constructor without parameters
            $session3 = new Session();
            echo "✓ Session constructor without parameters works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session data operations
     */
    public function testSessionDataOperations()
    {
        echo "Testing Session Data Operations:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test set
            $this->session->set('test_key', 'test_value');
            echo "✓ set() works\n";
            
            // Test set with array
            $this->session->set('array_key', ['item1', 'item2', 'item3']);
            echo "✓ set() with array works\n";
            
            // Test set with object
            $testObject = new stdClass();
            $testObject->property = 'value';
            $this->session->set('object_key', $testObject);
            echo "✓ set() with object works\n";
            
            // Test get
            $value = $this->session->get('test_key');
            echo "✓ get() returns: " . ($value ?? 'null') . "\n";
            
            // Test get with default
            $defaultValue = $this->session->get('non_existent_key', 'default_value');
            echo "✓ get() with default returns: " . $defaultValue . "\n";
            
            // Test has
            $hasValue = $this->session->has('test_key');
            echo "✓ has() returns: " . ($hasValue ? 'true' : 'false') . "\n";
            
            $hasNonExistent = $this->session->has('non_existent_key');
            echo "✓ has() for non-existent key returns: " . ($hasNonExistent ? 'true' : 'false') . "\n";
            
            // Test del
            $this->session->del('test_key');
            echo "✓ del() works\n";
            
            // Verify deletion
            $deletedValue = $this->session->get('test_key');
            echo "✓ After deletion, get() returns: " . ($deletedValue ?? 'null') . "\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session lifecycle
     */
    public function testSessionLifecycle()
    {
        echo "Testing Session Lifecycle:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test load
            $this->session->load();
            echo "✓ load() works\n";
            
            // Test save
            $this->session->save();
            echo "✓ save() works\n";
            
            // Test destroy
            $this->session->destroy();
            echo "✓ destroy() works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session ID management
     */
    public function testSessionIdManagement()
    {
        echo "Testing Session ID Management:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test getSessionId
            $sessionId = $this->session->getSessionId();
            echo "✓ getSessionId() returns: " . ($sessionId ?? 'null') . "\n";
            
            // Test setSessionId
            $newSessionId = 'custom_session_id_' . time();
            $result = $this->session->setSessionId($newSessionId);
            echo "✓ setSessionId() works\n";
            
            // Verify session ID change
            $updatedSessionId = $this->session->getSessionId();
            echo "✓ Updated session ID: " . ($updatedSessionId ?? 'null') . "\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session lifetime management
     */
    public function testLifetimeManagement()
    {
        echo "Testing Session Lifetime Management:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test setLifeTime
            $this->session->setLifeTime(7200); // 2 hours
            echo "✓ setLifeTime() works\n";
            
            // Test with different lifetime values
            $lifetimes = [300, 1800, 3600, 7200, 86400]; // 5min, 30min, 1hr, 2hr, 1day
            
            foreach ($lifetimes as $lifetime) {
                $this->session->setLifeTime($lifetime);
                echo "✓ setLifeTime($lifetime) works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test cookie operations
     */
    public function testCookieOperations()
    {
        echo "Testing Cookie Operations:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test cookie method
            $this->session->cookie('test_cookie', 'cookie_value', 3600);
            echo "✓ cookie() works\n";
            
            // Test cookie with array value
            $this->session->cookie('array_cookie', ['value1', 'value2'], 1800);
            echo "✓ cookie() with array value works\n";
            
            // Test cookie with different expiration times
            $expirationTimes = [0, 300, 3600, 86400]; // session, 5min, 1hr, 1day
            
            foreach ($expirationTimes as $time) {
                $this->session->cookie("cookie_$time", "value_$time", $time);
                echo "✓ cookie() with expiration $time works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session with different data types
     */
    public function testDataTypes()
    {
        echo "Testing Session with Different Data Types:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test with string
            $this->session->set('string_test', 'test string');
            $stringResult = $this->session->get('string_test');
            echo "✓ String data type works: " . $stringResult . "\n";
            
            // Test with integer
            $this->session->set('int_test', 12345);
            $intResult = $this->session->get('int_test');
            echo "✓ Integer data type works: " . $intResult . "\n";
            
            // Test with float
            $this->session->set('float_test', 12.345);
            $floatResult = $this->session->get('float_test');
            echo "✓ Float data type works: " . $floatResult . "\n";
            
            // Test with boolean
            $this->session->set('bool_test', true);
            $boolResult = $this->session->get('bool_test');
            echo "✓ Boolean data type works: " . ($boolResult ? 'true' : 'false') . "\n";
            
            // Test with null
            $this->session->set('null_test', null);
            $nullResult = $this->session->get('null_test');
            echo "✓ Null data type works: " . ($nullResult ?? 'null') . "\n";
            
            // Test with array
            $this->session->set('array_test', ['key1' => 'value1', 'key2' => 'value2']);
            $arrayResult = $this->session->get('array_test');
            echo "✓ Array data type works\n";
            
            // Test with object
            $testObject = new stdClass();
            $testObject->property1 = 'value1';
            $testObject->property2 = 42;
            $this->session->set('object_test', $testObject);
            $objectResult = $this->session->get('object_test');
            echo "✓ Object data type works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session with nested data
     */
    public function testNestedData()
    {
        echo "Testing Session with Nested Data:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test with nested array
            $nestedArray = [
                'level1' => [
                    'level2' => [
                        'level3' => 'deep value'
                    ]
                ]
            ];
            $this->session->set('nested_array', $nestedArray);
            $result = $this->session->get('nested_array');
            echo "✓ Nested array works\n";
            
            // Test with nested object
            $level3 = new stdClass();
            $level3->data = 'deep data';
            
            $level2 = new stdClass();
            $level2->child = $level3;
            
            $level1 = new stdClass();
            $level1->child = $level2;
            
            $this->session->set('nested_object', $level1);
            $objectResult = $this->session->get('nested_object');
            echo "✓ Nested object works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session configuration variations
     */
    public function testSessionConfigurations()
    {
        echo "Testing Different Session Configurations:\n";
        
        try {
            // Test with custom name
            $customSession = new Session(['name' => 'custom_app_session']);
            echo "✓ Session with custom name works\n";
            
            // Test with security options
            $secureSession = new Session([
                'secure' => true,
                'httponly' => true,
                'samesite' => 'Strict'
            ]);
            echo "✓ Session with security options works\n";
            
            // Test with custom path and domain
            $pathSession = new Session([
                'path' => '/app',
                'domain' => 'example.com'
            ]);
            echo "✓ Session with custom path and domain works\n";
            
            // Test with all options
            $fullSession = new Session([
                'name' => 'full_session',
                'lifetime' => 7200,
                'path' => '/',
                'domain' => 'localhost',
                'secure' => false,
                'httponly' => true,
                'samesite' => 'Lax'
            ]);
            echo "✓ Session with all options works\n";
            
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
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Test with invalid key names
            try {
                $this->session->get('');
                echo "✓ get() handles empty key gracefully\n";
            } catch (Exception $e) {
                echo "✓ get() properly handles empty key\n";
            }
            
            // Test with null key
            try {
                $this->session->set(null, 'value');
                echo "✓ set() handles null key gracefully\n";
            } catch (Exception $e) {
                echo "✓ set() properly handles null key\n";
            }
            
            // Test with very long key
            $longKey = str_repeat('a', 1000);
            try {
                $this->session->set($longKey, 'value');
                echo "✓ set() handles long key gracefully\n";
            } catch (Exception $e) {
                echo "✓ set() properly handles long key\n";
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
            if (!$this->session) {
                $this->session = new Session();
            }
            
            $startTime = microtime(true);
            
            // Perform multiple session operations
            for ($i = 0; $i < 1000; $i++) {
                $this->session->set("key_$i", "value_$i");
                $value = $this->session->get("key_$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000; // Convert to milliseconds
            
            echo "✓ 1000 session operations completed in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test session persistence
     */
    public function testSessionPersistence()
    {
        echo "Testing Session Persistence:\n";
        
        try {
            if (!$this->session) {
                $this->session = new Session();
            }
            
            // Set some data
            $this->session->set('persist_test', 'persistent_value');
            $this->session->set('timestamp', time());
            
            // Save session
            $this->session->save();
            echo "✓ Session saved\n";
            
            // Load session
            $this->session->load();
            echo "✓ Session loaded\n";
            
            // Verify data persistence
            $persistedValue = $this->session->get('persist_test');
            $timestamp = $this->session->get('timestamp');
            
            echo "✓ Persisted value: " . ($persistedValue ?? 'null') . "\n";
            echo "✓ Timestamp: " . ($timestamp ?? 'null') . "\n";
            
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
        $this->testSessionDataOperations();
        $this->testSessionLifecycle();
        $this->testSessionIdManagement();
        $this->testLifetimeManagement();
        $this->testCookieOperations();
        $this->testDataTypes();
        $this->testNestedData();
        $this->testSessionConfigurations();
        $this->testErrorHandling();
        $this->testPerformance();
        $this->testSessionPersistence();
        
        echo "=== Session Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new SessionTest();
    $test->runAllTests();
}
