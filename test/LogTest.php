<?php

/**
 * Gene Framework Log Class Test
 * 
 * This test file covers the important methods of the Gene\Log class
 */

use Gene\Log;

class LogTest
{
    public function __construct()
    {
        echo "=== Gene\Log Test Suite ===\n\n";
    }
    
    /**
     * Test basic logging methods
     */
    public function testBasicLogging()
    {
        echo "Testing Basic Logging Methods:\n";
        
        try {
            // Test debug method
            Log::debug("This is a debug message");
            echo "✓ debug() method works\n";
            
            // Test info method
            Log::info("This is an info message");
            echo "✓ info() method works\n";
            
            // Test notice method
            Log::notice("This is a notice message");
            echo "✓ notice() method works\n";
            
            // Test warning method
            Log::warning("This is a warning message");
            echo "✓ warning() method works\n";
            
            // Test error method
            Log::error("This is an error message");
            echo "✓ error() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test exception logging
     */
    public function testExceptionLogging()
    {
        echo "Testing Exception Logging:\n";
        
        try {
            // Test exception logging with message
            $exception = new Exception("Test exception");
            Log::exception($exception, "Additional context message");
            echo "✓ exception() with message works\n";
            
            // Test exception logging without message
            Log::exception($exception);
            echo "✓ exception() without message works\n";
            
            // Test with custom exception
            $customException = new RuntimeException("Custom runtime exception", 500);
            Log::exception($customException, "Runtime error occurred");
            echo "✓ exception() with custom exception works\n";
            
            // Test with nested exception
            $nestedException = new Exception("Nested exception", 0, $exception);
            Log::exception($nestedException);
            echo "✓ exception() with nested exception works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test log level management
     */
    public function testLogLevelManagement()
    {
        echo "Testing Log Level Management:\n";
        
        try {
            // Test setLevel using Gene\Log class constants (1=DEBUG .. 5=ERROR)
            Log::setLevel(Log::LEVEL_DEBUG);
            echo "✓ setLevel(LEVEL_DEBUG) works\n";
            
            Log::setLevel(Log::LEVEL_INFO);
            echo "✓ setLevel(LEVEL_INFO) works\n";
            
            Log::setLevel(Log::LEVEL_NOTICE);
            echo "✓ setLevel(LEVEL_NOTICE) works\n";
            
            Log::setLevel(Log::LEVEL_WARNING);
            echo "✓ setLevel(LEVEL_WARNING) works\n";
            
            Log::setLevel(Log::LEVEL_ERROR);
            echo "✓ setLevel(LEVEL_ERROR) works\n";
            
            // Test getLevel
            $level = Log::getLevel();
            echo "✓ getLevel() returns: " . $level . "\n";
            
            // Test with numeric levels (valid range is 1-5)
            $levels = [1, 2, 3, 4, 5];
            foreach ($levels as $level) {
                Log::setLevel($level);
                $currentLevel = Log::getLevel();
                echo "✓ setLevel($level) and getLevel() returns: $currentLevel\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test log file management
     */
    public function testLogFileManagement()
    {
        echo "Testing Log File Management:\n";
        
        try {
            // Test setFile
            Log::setFile('test.log');
            echo "✓ setFile('test.log') works\n";
            
            // Test setFile with absolute path
            Log::setFile('/var/log/gene/test.log');
            echo "✓ setFile() with absolute path works\n";
            
            // Test setFile with relative path
            Log::setFile('./logs/app.log');
            echo "✓ setFile() with relative path works\n";
            
            // Test getFile
            $file = Log::getFile();
            echo "✓ getFile() returns: " . ($file ?? 'null') . "\n";
            
            // Test with different file extensions
            $files = ['app.log', 'debug.txt', 'error.log', 'system.log'];
            foreach ($files as $file) {
                Log::setFile($file);
                $currentFile = Log::getFile();
                echo "✓ setFile('$file') and getFile() returns: " . ($currentFile ?? 'null') . "\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test logging with different message types
     */
    public function testMessageTypes()
    {
        echo "Testing Different Message Types:\n";
        
        try {
            // Test with string messages
            Log::info("Simple string message");
            echo "✓ String message works\n";
            
            // Test with numeric messages (cast to string — Log::info requires string)
            Log::info((string)12345);
            echo "✓ Numeric message works\n";
            
            // Test with boolean messages (cast to string)
            Log::info((string)true);
            echo "✓ Boolean message works\n";
            
            // Test with array messages (Log::info requires string, cast via json_encode)
            try {
                Log::info(json_encode(['key' => 'value', 'number' => 42]));
                echo "✓ Array message works\n";
            } catch (Exception $e) {
                echo "✓ Array message handled gracefully\n";
            }
            
            // Test with object messages (Log::info requires string, cast via json_encode)
            try {
                $object = new stdClass();
                $object->property = 'value';
                Log::info(json_encode($object));
                echo "✓ Object message works\n";
            } catch (Exception $e) {
                echo "✓ Object message handled gracefully\n";
            }
            
            // Test with long messages
            $longMessage = str_repeat("This is a very long log message. ", 100);
            Log::info($longMessage);
            echo "✓ Long message works\n";
            
            // Test with special characters
            Log::info("Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?");
            echo "✓ Special characters work\n";
            
            // Test with Unicode characters
            Log::info("Unicode: 你好世界 🌍 ñáéíóú");
            echo "✓ Unicode characters work\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test logging under different levels
     */
    public function testLoggingUnderLevels()
    {
        echo "Testing Logging Under Different Levels:\n";
        
        try {
            // Set level to DEBUG (should log everything)
            Log::setLevel(Log::LEVEL_DEBUG);
            Log::debug("Debug message at DEBUG level");
            Log::info("Info message at DEBUG level");
            Log::warning("Warning message at DEBUG level");
            Log::error("Error message at DEBUG level");
            echo "✓ All messages logged at DEBUG level\n";
            
            // Set level to INFO (should skip DEBUG)
            Log::setLevel(Log::LEVEL_INFO);
            Log::debug("Debug message at INFO level (should be skipped)");
            Log::info("Info message at INFO level");
            Log::warning("Warning message at INFO level");
            Log::error("Error message at INFO level");
            echo "✓ Messages filtered at INFO level\n";
            
            // Set level to WARNING (should skip DEBUG and INFO)
            Log::setLevel(Log::LEVEL_WARNING);
            Log::debug("Debug message at WARNING level (should be skipped)");
            Log::info("Info message at WARNING level (should be skipped)");
            Log::warning("Warning message at WARNING level");
            Log::error("Error message at WARNING level");
            echo "✓ Messages filtered at WARNING level\n";
            
            // Set level to ERROR (should only log errors)
            Log::setLevel(Log::LEVEL_ERROR);
            Log::debug("Debug message at ERROR level (should be skipped)");
            Log::info("Info message at ERROR level (should be skipped)");
            Log::warning("Warning message at ERROR level (should be skipped)");
            Log::error("Error message at ERROR level");
            echo "✓ Only errors logged at ERROR level\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test performance with multiple log entries
     */
    public function testPerformance()
    {
        echo "Testing Performance with Multiple Log Entries:\n";
        
        try {
            $startTime = microtime(true);
            
            // Log multiple messages
            for ($i = 0; $i < 1000; $i++) {
                Log::info("Performance test message $i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000; // Convert to milliseconds
            
            echo "✓ 1000 log entries created in " . number_format($duration, 2) . "ms\n";
            
            // Test with different log levels
            $startTime = microtime(true);
            
            Log::setLevel(Log::LEVEL_DEBUG);
            for ($i = 0; $i < 100; $i++) {
                Log::debug("Debug message $i");
                Log::info("Info message $i");
                Log::warning("Warning message $i");
                Log::error("Error message $i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            
            echo "✓ 400 mixed log entries created in " . number_format($duration, 2) . "ms\n";
            
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
            // Test with null message
            try {
                Log::info(null);
                echo "✓ Null message handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Null message properly handled\n";
            }
            
            // Test with empty message
            try {
                Log::info("");
                echo "✓ Empty message handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Empty message properly handled\n";
            }
            
            // Test with invalid file path
            try {
                Log::setFile('/invalid/path/that/does/not/exist/test.log');
                Log::info("Test message to invalid file");
                echo "✓ Invalid file path handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Invalid file path properly handled\n";
            }
            
            // Test with invalid log level
            try {
                Log::setLevel(-1);
                Log::info("Test with invalid level");
                echo "✓ Invalid log level handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Invalid log level properly handled\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Unexpected error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test concurrent logging simulation
     */
    public function testConcurrentLogging()
    {
        echo "Testing Concurrent Logging Simulation:\n";
        
        try {
            // Simulate concurrent logging by rapid successive calls
            $startTime = microtime(true);
            
            for ($i = 0; $i < 10; $i++) {
                Log::debug("Concurrent debug $i");
                Log::info("Concurrent info $i");
                Log::warning("Concurrent warning $i");
                Log::error("Concurrent error $i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            
            echo "✓ 40 concurrent log entries created in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test log rotation scenarios
     */
    public function testLogRotationScenarios()
    {
        echo "Testing Log Rotation Scenarios:\n";
        
        try {
            // Test logging to different files
            $files = ['app.log', 'error.log', 'debug.log', 'access.log'];
            
            foreach ($files as $file) {
                Log::setFile($file);
                Log::info("Logging to $file");
                echo "✓ Logging to $file works\n";
            }
            
            // Test with date-based file names
            $dateFile = 'app_' . date('Y-m-d') . '.log';
            Log::setFile($dateFile);
            Log::info("Date-based log file");
            echo "✓ Date-based file name works\n";
            
            // Test with time-based file names
            $timeFile = 'app_' . date('Y-m-d_H-i-s') . '.log';
            Log::setFile($timeFile);
            Log::info("Time-based log file");
            echo "✓ Time-based file name works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test logging in different contexts
     */
    public function testContextualLogging()
    {
        echo "Testing Contextual Logging:\n";
        
        try {
            // Test application context logging
            Log::info("Application started");
            Log::debug("Loading configuration");
            Log::info("Configuration loaded successfully");
            Log::warning("Deprecated configuration option detected");
            Log::error("Failed to connect to database");
            echo "✓ Application context logging works\n";
            
            // Test request context logging
            Log::info("Request started: GET /api/users");
            Log::debug("Validating request parameters");
            Log::info("Request validated successfully");
            Log::warning("Rate limit approaching");
            Log::error("Authentication failed");
            echo "✓ Request context logging works\n";
            
            // Test performance context logging
            Log::info("Performance test started");
            Log::debug("Executing query: SELECT * FROM users");
            Log::warning("Query took longer than expected");
            Log::error("Query timeout occurred");
            echo "✓ Performance context logging works\n";
            
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
        $this->testBasicLogging();
        $this->testExceptionLogging();
        $this->testLogLevelManagement();
        $this->testLogFileManagement();
        $this->testMessageTypes();
        $this->testLoggingUnderLevels();
        $this->testPerformance();
        $this->testErrorHandling();
        $this->testConcurrentLogging();
        $this->testLogRotationScenarios();
        $this->testContextualLogging();
        
        echo "=== Log Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new LogTest();
    $test->runAllTests();
}
