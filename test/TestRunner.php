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
    'RestInvokeTest.php',
    'SwooleEntryTest.php',
    'DemoLoadTest.php'
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

    /**
     * [GENE_FIX:2026-09-23 S5] php args every child process gets.
     * GENE_TEST_PHP_ARGS wins. When the runner itself was started with -n
     * (no php.ini) a plain `php` child would read the default ini and load
     * the DEPLOYED php_gene.dll instead of the freshly built one — the
     * classic "18 phantom failures" trap. Forward -n plus every loaded
     * extension that came from a file, so children mirror this process.
     */
    private function childPhpArgs()
    {
        static $cached = null;
        if ($cached !== null) {
            return $cached;
        }
        $env = getenv('GENE_TEST_PHP_ARGS');
        if (is_string($env) && $env !== '') {
            return $cached = $env;
        }
        $cached = '';
        if (php_ini_loaded_file() === false) {
            // Started with -n: children need explicit -d extension args.
            // ReflectionExtension::getFileName() only exists on PHP >= 8.4,
            // so fall back to a file probe in ini extension_dir.
            static $builtin = ['Core' => 1, 'date' => 1, 'hash' => 1, 'json' => 1,
                'pcre' => 1, 'pdo' => 1, 'Reflection' => 1, 'SPL' => 1, 'standard' => 1];
            $args = ['-n'];
            $unmapped = [];
            $dir = ini_get('extension_dir');
            foreach (get_loaded_extensions() as $ext) {
                $file = false;
                if (method_exists('ReflectionExtension', 'getFileName')) {
                    $file = (new \ReflectionExtension($ext))->getFileName();
                }
                if ((!is_string($file) || $file === '') && is_string($dir) && $dir !== '') {
                    foreach (["php_{$ext}.dll", "{$ext}.dll", "{$ext}.so"] as $cand) {
                        if (is_file($dir . DIRECTORY_SEPARATOR . $cand)) {
                            $file = $dir . DIRECTORY_SEPARATOR . $cand;
                            break;
                        }
                    }
                }
                if (is_string($file) && $file !== '') {
                    $args[] = '-d extension=' . $file;
                } elseif (!isset($builtin[$ext])) {
                    $unmapped[] = $ext;
                }
            }
            $cached = implode(' ', array_map('escapeshellarg', $args));
            // Grandchildren (e.g. DemoLoadTest's own proc_open spawns)
            // inherit this env and forward the same args.
            putenv('GENE_TEST_PHP_ARGS=' . $cached);
            // Noise floor: most builds compile these statically — they are
            // absent from extension_dir but also don't need forwarding.
            static $needed = ['gene', 'pdo_sqlite', 'pdo_mysql', 'pdo_pgsql',
                'curl', 'openssl', 'redis', 'swoole', 'memcached'];
            $missing = array_intersect($unmapped, $needed);
            if ($missing) {
                fwrite(STDERR, "[TestRunner] WARNING: cannot derive dll path for "
                    . "needed extension(s) " . implode(', ', $missing) . " — "
                    . "child processes run WITHOUT them. Set GENE_TEST_PHP_ARGS "
                    . "with explicit '-d extension=<path>' entries.\n");
            }
        } elseif (extension_loaded('gene')) {
            fwrite(STDERR, "[TestRunner] WARNING: GENE_TEST_PHP_ARGS is empty and "
                . "php.ini is in use — child processes load php.ini's extension "
                . "list. If this runner was started with -d extension=<fresh "
                . "dll>, set GENE_TEST_PHP_ARGS='-d extension=<dll>'.\n");
        }
        return $cached;
    }

    /**
     * One-line banner with the child process's gene version + dll path, so a
     * mismatched extension is visible right above the test output. Probed
     * once (args are identical for every child).
     */
    private function childEnvBanner($phpArgs)
    {
        static $printed = false;
        if ($printed) {
            return '';
        }
        $printed = true;
        $cmd = escapeshellarg(PHP_BINARY) . ' ' . $phpArgs
            . ' -r ' . escapeshellarg(
                'echo \'[child-env] gene=\', '
                . '(extension_loaded(\'gene\') ? phpversion(\'gene\') : \'NOT LOADED\'), '
                . '\' php=\', PHP_VERSION, \' ext_dir=\', ini_get(\'extension_dir\'), PHP_EOL;'
            );
        $out = [];
        exec($cmd . ' 2>&1', $out);
        $line = implode("\n", $out);
        // The derived/guessed args may still resolve to a different dll than
        // the one this process loaded (e.g. a stale build in extension_dir).
        if (preg_match('/gene=([0-9][^ ]*)/', $line, $m)) {
            $mine = extension_loaded('gene') ? phpversion('gene') : null;
            if ($mine === null) {
                fwrite(STDERR, "[TestRunner] WARNING: children load gene "
                    . "{$m[1]} but this runner has NO gene loaded.\n");
            } elseif ($m[1] !== $mine) {
                fwrite(STDERR, "[TestRunner] WARNING: children load gene "
                    . "{$m[1]} but this runner has gene {$mine} — results "
                    . "reflect the WRONG build. Set GENE_TEST_PHP_ARGS with "
                    . "'-d extension=<fresh dll>'.\n");
            }
        } else {
            fwrite(STDERR, "[TestRunner] WARNING: gene not loaded in child "
                . "processes — suite will SKIP gene-dependent tests. Set "
                . "GENE_TEST_PHP_ARGS with '-d extension=<dll>'.\n");
        }
        return $line . "\n";
    }

    private function runIsolated($testPath)
    {
        $command = [escapeshellarg(PHP_BINARY)];
        $phpArgs = $this->childPhpArgs();
        if ($phpArgs !== '') {
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
        return [$this->childEnvBanner($phpArgs) . $output, proc_close($process)];
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
            'RestInvokeTest.php',
            'SwooleEntryTest.php',
            'DemoLoadTest.php'
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
