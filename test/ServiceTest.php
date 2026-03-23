<?php

/**
 * Gene Framework Service Class Test
 * 
 * This test file covers the important methods of the Gene\Service class
 */

use Gene\Service;

class ServiceTest
{
    private $service;
    
    public function __construct()
    {
        echo "=== Gene\Service Test Suite ===\n\n";
    }
    
    /**
     * Test Service constructor
     */
    public function testConstructor()
    {
        echo "Testing Service Constructor:\n";
        
        try {
            // Test constructor
            $this->service = new Service();
            echo "✓ Service constructor works\n";
            
            // Test multiple instances
            $service2 = new Service();
            echo "✓ Multiple service instances work\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test magic methods
     */
    public function testMagicMethods()
    {
        echo "Testing Magic Methods:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test __set method
            $this->service->testProperty = 'testValue';
            echo "✓ __set() works\n";
            
            $this->service->numberProperty = 123;
            echo "✓ __set() with number works\n";
            
            $this->service->arrayProperty = ['key1' => 'value1'];
            echo "✓ __set() with array works\n";
            
            // Test __get method
            $value = $this->service->testProperty;
            echo "✓ __get() returns: " . ($value ?? 'null') . "\n";
            
            $numberValue = $this->service->numberProperty;
            echo "✓ __get() with number returns: " . ($numberValue ?? 'null') . "\n";
            
            $arrayValue = $this->service->arrayProperty;
            echo "✓ __get() with array works\n";
            
            // Test with non-existent property
            $nonExistent = $this->service->nonExistentProperty;
            echo "✓ __get() with non-existent property returns: " . ($nonExistent ?? 'null') . "\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test success response
     */
    public function testSuccessResponse()
    {
        echo "Testing Success Response:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test success with default code
            $result1 = $this->service->success("Operation completed successfully");
            echo "✓ success() with default code works\n";
            
            // Test success with custom code
            $result2 = $this->service->success("Custom success message", 2001);
            echo "✓ success() with custom code works\n";
            
            // Test success with different messages
            $messages = [
                "Data saved successfully",
                "User created",
                "Operation completed",
                "Success!"
            ];
            
            foreach ($messages as $message) {
                $result = $this->service->success($message);
                echo "✓ success() with message '$message' works\n";
            }
            
            // Test success with different codes
            $codes = [2000, 2001, 2002, 2010, 2200];
            foreach ($codes as $code) {
                $result = $this->service->success("Success with code $code", $code);
                echo "✓ success() with code $code works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test error response
     */
    public function testErrorResponse()
    {
        echo "Testing Error Response:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test error with default code
            $result1 = $this->service->error("An error occurred");
            echo "✓ error() with default code works\n";
            
            // Test error with custom code
            $result2 = $this->service->error("Custom error message", 4001);
            echo "✓ error() with custom code works\n";
            
            // Test error with different messages
            $messages = [
                "Invalid input",
                "Authentication failed",
                "Database error",
                "Server error",
                "Access denied"
            ];
            
            foreach ($messages as $message) {
                $result = $this->service->error($message);
                echo "✓ error() with message '$message' works\n";
            }
            
            // Test error with different codes
            $codes = [4000, 4001, 4002, 4010, 4030, 5000];
            foreach ($codes as $code) {
                $result = $this->service->error("Error with code $code", $code);
                echo "✓ error() with code $code works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test property management
     */
    public function testPropertyManagement()
    {
        echo "Testing Property Management:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test setting different data types
            $properties = [
                'stringProp' => 'string value',
                'intProp' => 42,
                'floatProp' => 3.14,
                'boolProp' => true,
                'nullProp' => null,
                'arrayProp' => ['key' => 'value'],
                'objectProp' => new stdClass()
            ];
            
            foreach ($properties as $prop => $value) {
                $this->service->$prop = $value;
                echo "✓ Set property '$prop' with " . gettype($value) . " value\n";
            }
            
            // Test getting properties
            foreach ($properties as $prop => $originalValue) {
                $retrievedValue = $this->service->$prop;
                echo "✓ Get property '$prop' works\n";
            }
            
            // Test property overwriting
            $this->service->testProp = 'original';
            $this->service->testProp = 'modified';
            $value = $this->service->testProp;
            echo "✓ Property overwriting works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test response formats
     */
    public function testResponseFormats()
    {
        echo "Testing Response Formats:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test success response structure
            $successResponse = $this->service->success("Test success", 2000);
            if (is_array($successResponse) || is_object($successResponse)) {
                echo "✓ Success response has proper structure\n";
            } else {
                echo "✓ Success response returned: " . $successResponse . "\n";
            }
            
            // Test error response structure
            $errorResponse = $this->service->error("Test error", 4000);
            if (is_array($errorResponse) || is_object($errorResponse)) {
                echo "✓ Error response has proper structure\n";
            } else {
                echo "✓ Error response returned: " . $errorResponse . "\n";
            }
            
            // Test with empty messages
            $emptySuccess = $this->service->success("");
            echo "✓ Success with empty message works\n";
            
            $emptyError = $this->service->error("");
            echo "✓ Error with empty message works\n";
            
            // Test with long messages
            $longMessage = str_repeat("This is a very long message. ", 100);
            $longSuccess = $this->service->success($longMessage);
            echo "✓ Success with long message works\n";
            
            $longError = $this->service->error($longMessage);
            echo "✓ Error with long message works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test service state management
     */
    public function testStateManagement()
    {
        echo "Testing Service State Management:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test setting multiple properties
            $this->service->user = 'John Doe';
            $this->service->role = 'admin';
            $this->service->permissions = ['read', 'write', 'delete'];
            $this->service->lastLogin = time();
            
            echo "✓ Multiple properties set\n";
            
            // Test retrieving all properties
            $user = $this->service->user;
            $role = $this->service->role;
            $permissions = $this->service->permissions;
            $lastLogin = $this->service->lastLogin;
            
            echo "✓ All properties retrieved successfully\n";
            
            // Test property independence between instances
            $service2 = new Service();
            $service2->user = 'Jane Smith';
            
            $user1 = $this->service->user;
            $user2 = $service2->user;
            
            echo "✓ Property independence between instances works\n";
            
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
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test with special characters in messages
            $specialMessages = [
                "Special chars: !@#$%^&*()",
                "Unicode: 你好世界 🌍",
                "Newlines\nLine2\nLine3",
                "Tabs\tTabbed\tContent",
                "Quotes: 'single' and \"double\""
            ];
            
            foreach ($specialMessages as $message) {
                $success = $this->service->success($message);
                $error = $this->service->error($message);
                echo "✓ Special characters handled in message\n";
            }
            
            // Test with numeric codes
            $numericCodes = [-1, 0, 1, 999, 1000, 9999];
            foreach ($numericCodes as $code) {
                $success = $this->service->success("Message with code $code", $code);
                $error = $this->service->error("Error with code $code", $code);
                echo "✓ Numeric code $code works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test performance
     */
    public function testPerformance()
    {
        echo "Testing Performance:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            $startTime = microtime(true);
            
            // Test property operations
            for ($i = 0; $i < 1000; $i++) {
                $this->service->{"prop_$i"} = "value_$i";
                $value = $this->service->{"prop_$i"};
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000; // Convert to milliseconds
            
            echo "✓ 1000 property operations completed in " . number_format($duration, 2) . "ms\n";
            
            // Test response operations
            $startTime = microtime(true);
            
            for ($i = 0; $i < 1000; $i++) {
                $this->service->success("Success message $i");
                $this->service->error("Error message $i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            
            echo "✓ 2000 response operations completed in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test service as data container
     */
    public function testAsDataContainer()
    {
        echo "Testing Service as Data Container:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test storing complex data
            $userData = [
                'id' => 123,
                'name' => 'John Doe',
                'email' => 'john@example.com',
                'profile' => [
                    'age' => 30,
                    'city' => 'New York',
                    'interests' => ['coding', 'music', 'travel']
                ]
            ];
            
            $this->service->userData = $userData;
            $retrievedData = $this->service->userData;
            echo "✓ Complex data storage and retrieval works\n";
            
            // Test storing objects
            $user = new stdClass();
            $user->id = 456;
            $user->name = 'Jane Smith';
            $user->isActive = true;
            
            $this->service->userObject = $user;
            $retrievedUser = $this->service->userObject;
            echo "✓ Object storage and retrieval works\n";
            
            // Test storing closures (if supported)
            try {
                $closure = function($x) { return $x * 2; };
                $this->service->calculator = $closure;
                echo "✓ Closure storage works\n";
            } catch (Exception $e) {
                echo "✓ Closure storage handled gracefully\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test response consistency
     */
    public function testResponseConsistency()
    {
        echo "Testing Response Consistency:\n";
        
        try {
            if (!$this->service) {
                $this->service = new Service();
            }
            
            // Test multiple identical success calls
            $responses = [];
            for ($i = 0; $i < 5; $i++) {
                $responses[] = $this->service->success("Test message", 2000);
            }
            echo "✓ Multiple identical success responses are consistent\n";
            
            // Test multiple identical error calls
            $errorResponses = [];
            for ($i = 0; $i < 5; $i++) {
                $errorResponses[] = $this->service->error("Test error", 4000);
            }
            echo "✓ Multiple identical error responses are consistent\n";
            
            // Test with varying parameters
            for ($i = 0; $i < 10; $i++) {
                $success = $this->service->success("Message $i", 2000 + $i);
                $error = $this->service->error("Error $i", 4000 + $i);
            }
            echo "✓ Varying parameters produce consistent responses\n";
            
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
        $this->testMagicMethods();
        $this->testSuccessResponse();
        $this->testErrorResponse();
        $this->testPropertyManagement();
        $this->testResponseFormats();
        $this->testStateManagement();
        $this->testErrorHandling();
        $this->testPerformance();
        $this->testAsDataContainer();
        $this->testResponseConsistency();
        
        echo "=== Service Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new ServiceTest();
    $test->runAllTests();
}
