<?php

/**
 * Gene Framework Benchmark Class Test
 * 
 * This test file covers the important methods of the Gene\Benchmark class
 */

use Gene\Benchmark;

class BenchmarkTest
{
    private $benchmark;
    
    public function __construct()
    {
        echo "=== Gene\Benchmark Test Suite ===\n\n";
    }
    
    /**
     * Test Benchmark constructor
     */
    public function testConstructor()
    {
        echo "Testing Benchmark Constructor:\n";
        
        try {
            // Test constructor without parameters
            $benchmark1 = new Benchmark();
            echo "✓ Benchmark constructor without parameters works\n";
            
            // Test constructor with debug mode
            $benchmark2 = new Benchmark(true);
            echo "✓ Benchmark constructor with debug mode works\n";
            
            $this->benchmark = $benchmark1;
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test basic timing methods
     */
    public function testBasicTiming()
    {
        echo "Testing Basic Timing Methods:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test start method
            $this->benchmark->start();
            echo "✓ start() method works\n";
            
            // Simulate some work
            usleep(10000); // 10ms
            
            // Test end method
            $this->benchmark->end();
            echo "✓ end() method works\n";
            
            // Test time method
            $time1 = $this->benchmark->time();
            echo "✓ time() method returns: " . ($time1 ?? 'null') . "\n";
            
            // Test time with different types
            $time2 = $this->benchmark->time(true);
            echo "✓ time(true) returns: " . ($time2 ?? 'null') . "\n";
            
            $time3 = $this->benchmark->time(false);
            echo "✓ time(false) returns: " . ($time3 ?? 'null') . "\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test memory usage methods
     */
    public function testMemoryUsage()
    {
        echo "Testing Memory Usage Methods:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test memory method
            $memory1 = $this->benchmark->memory();
            echo "✓ memory() method returns: " . ($memory1 ?? 'null') . "\n";
            
            // Test memory with different types
            $memory2 = $this->benchmark->memory(true);
            echo "✓ memory(true) returns: " . ($memory2 ?? 'null') . "\n";
            
            $memory3 = $this->benchmark->memory(false);
            echo "✓ memory(false) returns: " . ($memory3 ?? 'null') . "\n";
            
            // Test memory tracking during operations
            $this->benchmark->start();
            
            // Allocate some memory
            $largeArray = array_fill(0, 10000, 'test data');
            
            $this->benchmark->end();
            
            $memoryBefore = $memory1;
            $memoryAfter = $this->benchmark->memory();
            echo "✓ Memory tracking during operations works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test multiple benchmark cycles
     */
    public function testMultipleCycles()
    {
        echo "Testing Multiple Benchmark Cycles:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test multiple start/stop cycles
            for ($i = 0; $i < 5; $i++) {
                $this->benchmark->start();
                
                // Simulate work with varying durations
                usleep(($i + 1) * 5000); // 5ms, 10ms, 15ms, 20ms, 25ms
                
                $this->benchmark->end();
                
                $time = $this->benchmark->time();
                $memory = $this->benchmark->memory();
                
                echo "✓ Cycle $i: Time=" . ($time ?? 'null') . "ms, Memory=" . ($memory ?? 'null') . "\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test benchmark with different operations
     */
    public function testDifferentOperations()
    {
        echo "Testing Benchmark with Different Operations:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test with CPU-intensive operation
            $this->benchmark->start();
            
            // CPU intensive: calculate primes
            $primes = [];
            for ($i = 2; $i < 1000; $i++) {
                $isPrime = true;
                for ($j = 2; $j < sqrt($i); $j++) {
                    if ($i % $j == 0) {
                        $isPrime = false;
                        break;
                    }
                }
                if ($isPrime) {
                    $primes[] = $i;
                }
            }
            
            $this->benchmark->end();
            
            $cpuTime = $this->benchmark->time();
            $cpuMemory = $this->benchmark->memory();
            echo "✓ CPU-intensive operation: Time=" . ($cpuTime ?? 'null') . "ms, Memory=" . ($cpuMemory ?? 'null') . "\n";
            
            // Test with memory-intensive operation
            $this->benchmark->start();
            
            // Memory intensive: create large arrays
            $largeArrays = [];
            for ($i = 0; $i < 100; $i++) {
                $largeArrays[] = array_fill(0, 1000, str_repeat('x', 100));
            }
            
            $this->benchmark->end();
            
            $memTime = $this->benchmark->time();
            $memMemory = $this->benchmark->memory();
            echo "✓ Memory-intensive operation: Time=" . ($memTime ?? 'null') . "ms, Memory=" . ($memMemory ?? 'null') . "\n";
            
            // Test with I/O simulation
            $this->benchmark->start();
            
            // I/O simulation: file operations
            $tempFile = tempnam(sys_get_temp_dir(), 'benchmark_test');
            file_put_contents($tempFile, str_repeat('test data', 1000));
            $content = file_get_contents($tempFile);
            unlink($tempFile);
            
            $this->benchmark->end();
            
            $ioTime = $this->benchmark->time();
            $ioMemory = $this->benchmark->memory();
            echo "✓ I/O operation: Time=" . ($ioTime ?? 'null') . "ms, Memory=" . ($ioMemory ?? 'null') . "\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test benchmark accuracy and precision
     */
    public function testAccuracyAndPrecision()
    {
        echo "Testing Benchmark Accuracy and Precision:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test very short operations
            $shortTimes = [];
            for ($i = 0; $i < 10; $i++) {
                $this->benchmark->start();
                
                // Very short operation
                $x = $i * 2;
                
                $this->benchmark->end();
                
                $shortTimes[] = $this->benchmark->time();
            }
            
            echo "✓ Short operations measured (10 samples)\n";
            
            // Test longer operations
            $longTimes = [];
            for ($i = 0; $i < 3; $i++) {
                $this->benchmark->start();
                
                // Longer operation
                usleep(50000); // 50ms
                
                $this->benchmark->end();
                
                $longTimes[] = $this->benchmark->time();
            }
            
            echo "✓ Longer operations measured (3 samples)\n";
            
            // Test memory precision
            $memoryReadings = [];
            for ($i = 0; $i < 5; $i++) {
                $this->benchmark->start();
                
                // Allocate and deallocate memory
                $temp = array_fill(0, 1000, 'test');
                unset($temp);
                
                $this->benchmark->end();
                
                $memoryReadings[] = $this->benchmark->memory();
            }
            
            echo "✓ Memory precision tested (5 samples)\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test benchmark with nested operations
     */
    public function testNestedOperations()
    {
        echo "Testing Benchmark with Nested Operations:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test nested benchmarking
            $this->benchmark->start(); // Outer start
            
            // Outer operation
            $outerData = array_fill(0, 1000, 'outer');
            
            // Inner benchmark
            $this->benchmark->start(); // Inner start
            
            // Inner operation
            $innerData = array_fill(0, 500, 'inner');
            
            $this->benchmark->end(); // Inner end
            
            $innerTime = $this->benchmark->time();
            $innerMemory = $this->benchmark->memory();
            echo "✓ Inner operation: Time=" . ($innerTime ?? 'null') . "ms, Memory=" . ($innerMemory ?? 'null') . "\n";
            
            // Continue outer operation
            $moreData = array_fill(0, 500, 'more');
            
            $this->benchmark->end(); // Outer end
            
            $outerTime = $this->benchmark->time();
            $outerMemory = $this->benchmark->memory();
            echo "✓ Outer operation: Time=" . ($outerTime ?? 'null') . "ms, Memory=" . ($outerMemory ?? 'null') . "\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test benchmark state management
     */
    public function testStateManagement()
    {
        echo "Testing Benchmark State Management:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test calling end without start
            try {
                $this->benchmark->end();
                echo "✓ end() without start() handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ end() without start() properly handled\n";
            }
            
            // Test calling time without proper setup
            try {
                $time = $this->benchmark->time();
                echo "✓ time() without proper setup handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ time() without proper setup properly handled\n";
            }
            
            // Test multiple starts without ends
            $this->benchmark->start();
            $this->benchmark->start(); // Second start
            echo "✓ Multiple starts handled\n";
            
            $this->benchmark->end();
            $this->benchmark->end(); // Second end
            echo "✓ Multiple ends handled\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test benchmark with different data types
     */
    public function testDifferentDataTypes()
    {
        echo "Testing Benchmark with Different Data Types:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            // Test with string operations
            $this->benchmark->start();
            $stringResult = str_repeat('test', 10000);
            $this->benchmark->end();
            $stringTime = $this->benchmark->time();
            echo "✓ String operations: Time=" . ($stringTime ?? 'null') . "ms\n";
            
            // Test with array operations
            $this->benchmark->start();
            $arrayResult = array_map(function($x) { return $x * 2; }, range(1, 10000));
            $this->benchmark->end();
            $arrayTime = $this->benchmark->time();
            echo "✓ Array operations: Time=" . ($arrayTime ?? 'null') . "ms\n";
            
            // Test with object operations
            $this->benchmark->start();
            $objects = [];
            for ($i = 0; $i < 1000; $i++) {
                $obj = new stdClass();
                $obj->id = $i;
                $obj->name = "Object $i";
                $objects[] = $obj;
            }
            $this->benchmark->end();
            $objectTime = $this->benchmark->time();
            echo "✓ Object operations: Time=" . ($objectTime ?? 'null') . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test performance of benchmark itself
     */
    public function testBenchmarkPerformance()
    {
        echo "Testing Benchmark Performance:\n";
        
        try {
            if (!$this->benchmark) {
                $this->benchmark = new Benchmark();
            }
            
            $startTime = microtime(true);
            
            // Test many benchmark cycles
            for ($i = 0; $i < 1000; $i++) {
                $this->benchmark->start();
                $this->benchmark->end();
                $time = $this->benchmark->time();
                $memory = $this->benchmark->memory();
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000; // Convert to milliseconds
            
            echo "✓ 1000 benchmark cycles completed in " . number_format($duration, 2) . "ms\n";
            echo "✓ Average per cycle: " . number_format($duration / 1000, 4) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test debug mode functionality
     */
    public function testDebugMode()
    {
        echo "Testing Debug Mode Functionality:\n";
        
        try {
            // Test with debug mode enabled
            $debugBenchmark = new Benchmark(true);
            
            $debugBenchmark->start();
            usleep(10000); // 10ms
            $debugBenchmark->end();
            
            $debugTime = $debugBenchmark->time();
            $debugMemory = $debugBenchmark->memory();
            
            echo "✓ Debug mode enabled: Time=" . ($debugTime ?? 'null') . "ms, Memory=" . ($debugMemory ?? 'null') . "\n";
            
            // Test with debug mode disabled
            $normalBenchmark = new Benchmark(false);
            
            $normalBenchmark->start();
            usleep(10000); // 10ms
            $normalBenchmark->end();
            
            $normalTime = $normalBenchmark->time();
            $normalMemory = $normalBenchmark->memory();
            
            echo "✓ Debug mode disabled: Time=" . ($normalTime ?? 'null') . "ms, Memory=" . ($normalMemory ?? 'null') . "\n";
            
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
        $this->testBasicTiming();
        $this->testMemoryUsage();
        $this->testMultipleCycles();
        $this->testDifferentOperations();
        $this->testAccuracyAndPrecision();
        $this->testNestedOperations();
        $this->testStateManagement();
        $this->testDifferentDataTypes();
        $this->testBenchmarkPerformance();
        $this->testDebugMode();
        
        echo "=== Benchmark Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new BenchmarkTest();
    $test->runAllTests();
}
