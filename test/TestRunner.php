<?php

/**
 * Gene Framework Test Runner
 * 
 * This file runs all the test suites for the Gene framework classes
 */

// Define test files
$testFiles = [
    'ApplicationTest.php',
    'CacheTest.php',
    'ConfigTest.php',
    'RouterTest.php',
    'SessionTest.php',
    'LogTest.php',
    'LanguageTest.php',
    'ServiceTest.php',
    'BenchmarkTest.php',
    'ExecuteTest.php',
    'HttpTest.php',
    'MvcTest.php',
    'DatabaseTest.php',
    'OrmTest.php',
    'DiTest.php',
    'HookTest.php',
    'LifecycleTest.php',
    'HttpClientTest.php',
    'RestInvokeTest.php'
];

// Test runner class
class TestRunner
{
    private $results = [];
    private $startTime;
    private $endTime;
    
    public function __construct()
    {
        $this->startTime = microtime(true);
    }

    private function runIsolated($testPath)
    {
        $command = [escapeshellarg(PHP_BINARY)];
        $phpArgs = getenv('GENE_TEST_PHP_ARGS');
        if (is_string($phpArgs) && $phpArgs !== '') {
            $command[] = $phpArgs;
        }
        $command[] = escapeshellarg($testPath);
        $pipes = [];
        $process = proc_open(implode(' ', $command), [
            0 => ['pipe', 'r'],
            1 => ['pipe', 'w'],
            2 => ['redirect', 1],
        ], $pipes, __DIR__);
        if (!is_resource($process)) {
            return ["✗ Unable to start isolated test process\n", 1];
        }
        fclose($pipes[0]);
        $output = stream_get_contents($pipes[1]);
        fclose($pipes[1]);
        return [$output, proc_close($process)];
    }
    
    /**
     * Run all test suites
     */
    public function runAll()
    {
        echo "========================================\n";
        echo "    Gene Framework Test Suite Runner    \n";
        echo "========================================\n\n";
        
        $testFiles = [
            'ApplicationTest.php',
            'CacheTest.php',
            'ConfigTest.php',
            'RouterTest.php',
            'SessionTest.php',
            'LogTest.php',
            'LanguageTest.php',
            'ServiceTest.php',
            'BenchmarkTest.php',
            'ExecuteTest.php',
            'HttpTest.php',
            'MvcTest.php',
            'DatabaseTest.php',
            'OrmTest.php',
            'DiTest.php',
            'HookTest.php',
            'LifecycleTest.php',
            'HttpClientTest.php',
            'RestInvokeTest.php'
        ];
        
        $totalTests = 0;
        $passedTests = 0;
        $failedTests = 0;
        
        foreach ($testFiles as $testFile) {
            $testPath = __DIR__ . DIRECTORY_SEPARATOR . $testFile;
            if (file_exists($testPath)) {
                echo "Running $testFile...\n";
                echo str_repeat("-", 50) . "\n";
                
                // Capture output from an isolated PHP process
                [$output, $exitCode] = $this->runIsolated($testPath);
                
                echo $output;
                echo "\n";
                
                // Count test results and process failures
                $passed = substr_count($output, '✓');
                $failed = substr_count($output, '✗');
                if ($exitCode !== 0 && $failed === 0) {
                    $failed = 1;
                }
                
                $totalTests += $passed + $failed;
                $passedTests += $passed;
                $failedTests += $failed;
                
                $this->results[$testFile] = [
                    'passed' => $passed,
                    'failed' => $failed,
                    'total' => $passed + $failed
                ];
                
                echo str_repeat("-", 50) . "\n";
                echo "Results for $testFile: $passed passed, $failed failed\n\n";
            } else {
                echo "Test file $testFile not found!\n\n";
                $failedTests++;
            }
        }
        
        $this->endTime = microtime(true);
        $this->printSummary($totalTests, $passedTests, $failedTests);
        return $failedTests;
    }
    
    /**
     * Run specific test file
     */
    public function runTest($testFile)
    {
        // Resolve relative to the runner's directory if the file isn't found as-is
        if (!file_exists($testFile)) {
            $testFile = __DIR__ . DIRECTORY_SEPARATOR . $testFile;
        }
        if (!file_exists($testFile)) {
            echo "Test file $testFile not found!\n";
            return 1;
        }
        
        echo "Running $testFile...\n";
        echo str_repeat("-", 50) . "\n";
        
        $startTime = microtime(true);
        
        [$output, $exitCode] = $this->runIsolated($testFile);
        
        $endTime = microtime(true);
        
        echo $output;
        
        $passed = substr_count($output, '✓');
        $failed = substr_count($output, '✗');
        if ($exitCode !== 0 && $failed === 0) {
            $failed = 1;
        }
        $duration = ($endTime - $startTime) * 1000;
        
        echo str_repeat("-", 50) . "\n";
        echo "Results for $testFile:\n";
        echo "  Passed: $passed\n";
        echo "  Failed: $failed\n";
        echo "  Duration: " . number_format($duration, 2) . "ms\n\n";
        return $failed;
    }
    
    /**
     * Print summary of all tests
     */
    private function printSummary($total, $passed, $failed)
    {
        $duration = ($this->endTime - $this->startTime) * 1000;
        $successRate = $total > 0 ? ($passed / $total) * 100 : 0;
        
        echo "========================================\n";
        echo "           Test Suite Summary           \n";
        echo "========================================\n";
        echo "Total Tests: $total\n";
        echo "Passed: $passed\n";
        echo "Failed: $failed\n";
        echo "Success Rate: " . number_format($successRate, 1) . "%\n";
        echo "Total Duration: " . number_format($duration, 2) . "ms\n";
        echo "========================================\n\n";
        
        // Print individual file results
        echo "Individual Test Results:\n";
        echo "----------------------------------------\n";
        foreach ($this->results as $file => $result) {
            $status = $result['failed'] === 0 ? 'PASS' : 'FAIL';
            echo sprintf("%-25s: %s (%d/%d)\n", 
                basename($file, '.php'), 
                $status, 
                $result['passed'], 
                $result['total']
            );
        }
        echo "\n";
        
        // Print recommendations
        if ($failed > 0) {
            echo "Recommendations:\n";
            echo "- Review failed tests and fix underlying issues\n";
            echo "- Check framework configuration and dependencies\n";
            echo "- Ensure test environment is properly set up\n";
        } else {
            echo "All tests passed! The framework is working correctly.\n";
        }
    }
    
    /**
     * List available test files
     */
    public function listTests()
    {
        echo "Available Test Files:\n";
        echo "--------------------\n";
        
        $testFiles = glob(__DIR__ . DIRECTORY_SEPARATOR . '*.php');
        foreach ($testFiles as $file) {
            $file = basename($file);
            if (strpos($file, 'Test.php') !== false) {
                echo "- $file\n";
            }
        }
        echo "\n";
    }
}

// Command line interface
if (php_sapi_name() === 'cli') {
    $runner = new TestRunner();
    
    // Parse command line arguments
    $options = getopt('hl:t:', ['help', 'list', 'test:']);
    
    if (isset($options['h']) || isset($options['help'])) {
        echo "Gene Framework Test Runner\n";
        echo "Usage: php TestRunner.php [options]\n\n";
        echo "Options:\n";
        echo "  -h, --help          Show this help message\n";
        echo "  -l, --list          List available test files\n";
        echo "  -t, --test FILE     Run specific test file\n";
        echo "\n";
        echo "Examples:\n";
        echo "  php TestRunner.php                    # Run all tests\n";
        echo "  php TestRunner.php --list              # List test files\n";
        echo "  php TestRunner.php --test ApplicationTest.php  # Run specific test\n";
        exit(0);
    }
    
    if (isset($options['l']) || isset($options['list'])) {
        $runner->listTests();
        exit(0);
    }
    
    if (isset($options['t']) || isset($options['test'])) {
        $testFile = $options['t'] ?? $options['test'];
        exit($runner->runTest($testFile));
    }
    
    // Default: run all tests
    exit($runner->runAll());
} else {
    // Web interface
    $runner = new TestRunner();
    
    if (isset($_GET['test'])) {
        $testFile = $_GET['test'];
        $runner->runTest($testFile);
    } else {
        $runner->runAll();
    }
}
