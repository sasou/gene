<?php

/**
 * Gene Framework Application Class Test
 * 
 * This test file covers the important methods of the Gene\Application class
 */

use Gene\Application;

class ApplicationTest
{
    private $app;
    
    public function __construct()
    {
        echo "=== Gene\Application Test Suite ===\n\n";
    }
    
    /**
     * Test Application constructor and getInstance
     */
    public function testConstructorAndGetInstance()
    {
        echo "Testing Application Constructor and getInstance():\n";
        
        try {
            // Test constructor with safe mode
            $app1 = new Gene\Application(true);
            echo "✓ Application constructor with safe mode works\n";
            
            // Test constructor without parameters
            $app2 = new Gene\Application();
            echo "✓ Application constructor without parameters works\n";
            
            // Test getInstance
            $app3 = Gene\Application::getInstance();
            echo "✓ Static getInstance() works\n";
            
            // Test getInstance with safe mode
            $app4 = Gene\Application::getInstance(true);
            echo "✓ Static getInstance() with safe mode works\n";
            
            $this->app = $app1;
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test environment and runtime methods
     */
    public function testEnvironmentAndRuntime()
    {
        echo "Testing Environment and Runtime Methods:\n";
        
        try {
            if ($this->app) {
                // Test getEnvironment
                $env = $this->app->getEnvironment();
                echo "✓ getEnvironment() returns: " . $env . "\n";
                
                // Test getEnvironmentName
                $envName = $this->app->getEnvironmentName();
                echo "✓ getEnvironmentName() returns: " . $envName . "\n";
                
                // Test getRuntimeType
                $runtimeType = $this->app->getRuntimeType();
                echo "✓ getRuntimeType() returns: " . $runtimeType . "\n";
                
                // Test getRuntimeTypeName
                $runtimeName = $this->app->getRuntimeTypeName();
                echo "✓ getRuntimeTypeName() returns: " . $runtimeName . "\n";
                
                // Test setEnvironment
                $this->app->setEnvironment(1); // Development
                echo "✓ setEnvironment(1) works\n";
                
                // Test setRuntimeType
                $this->app->setRuntimeType(1); // FPM
                echo "✓ setRuntimeType(1) works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test request information methods
     */
    public function testRequestInfo()
    {
        echo "Testing Request Information Methods:\n";
        
        try {
            if ($this->app) {
                // These methods may return null if not in a web context
                $method = $this->app->getMethod();
                echo "✓ getMethod() returns: " . ($method ?? 'null') . "\n";
                
                $path = $this->app->getPath();
                echo "✓ getPath() returns: " . ($path ?? 'null') . "\n";
                
                $module = $this->app->getModule();
                echo "✓ getModule() returns: " . ($module ?? 'null') . "\n";
                
                $controller = $this->app->getController();
                echo "✓ getController() returns: " . ($controller ?? 'null') . "\n";
                
                $action = $this->app->getAction();
                echo "✓ getAction() returns: " . ($action ?? 'null') . "\n";
                
                $lang = $this->app->getLang();
                echo "✓ getLang() returns: " . ($lang ?? 'null') . "\n";
                
                $routerUri = $this->app->getRouterUri();
                echo "✓ getRouterUri() returns: " . ($routerUri ?? 'null') . "\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test parameter handling
     */
    public function testParameters()
    {
        echo "Testing Parameter Methods:\n";
        
        try {
            if ($this->app) {
                // Test params method
                $param = $this->app->params('test');
                echo "✓ params('test') returns: " . ($param ?? 'null') . "\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test state management
     */
    public function testStateManagement()
    {
        echo "Testing State Management Methods:\n";
        
        try {
            if ($this->app) {
                // Test clearState
                $this->app->clearState();
                echo "✓ clearState() works\n";
                
                // Test destroyContext
                $this->app->destroyContext();
                echo "✓ destroyContext() works\n";
                
                // Test cleanup
                $this->app->cleanup();
                echo "✓ cleanup() works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test configuration and autoloading
     */
    public function testConfigAndAutoload()
    {
        echo "Testing Configuration and Autoload Methods:\n";
        
        try {
            if ($this->app) {
                // Test config
                $config = $this->app->config();
                echo "✓ config() works\n";
                
                // Test autoload
                $this->app->autoload();
                echo "✓ autoload() works\n";
                
                // Test autoload with parameters
                $this->app->autoload('/test', 'test.php');
                echo "✓ autoload() with parameters works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test error and exception handling
     */
    public function testErrorHandling()
    {
        echo "Testing Error and Exception Handling:\n";
        
        try {
            if ($this->app) {
                // Test error handler setup
                $this->app->error(E_ALL, function($errno, $errstr, $errfile, $errline) {
                    echo "Custom error handler called: $errstr\n";
                });
                echo "✓ error() method works\n";
                
                // Test exception handler setup
                $this->app->exception(E_ALL, function($exception) {
                    echo "Custom exception handler called: " . $exception->getMessage() . "\n";
                });
                echo "✓ exception() method works\n";
                
                // Test setMode
                $this->app->setMode(E_ALL, E_ALL);
                echo "✓ setMode() works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test view configuration
     */
    public function testViewConfiguration()
    {
        echo "Testing View Configuration:\n";
        
        try {
            if ($this->app) {
                // Test setView
                $this->app->setView('php', '.php');
                echo "✓ setView() works\n";
                
                $this->app->setView();
                echo "✓ setView() with defaults works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test webscan configuration
     */
    public function testWebscan()
    {
        echo "Testing Webscan Configuration:\n";
        
        try {
            if ($this->app) {
                // Test webscan
                $this->app->webscan(function($data) {
                    echo "Webscan callback triggered\n";
                });
                echo "✓ webscan() works\n";
            }
            
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
            if ($this->app) {
                // Test __set
                $this->app->testProperty = 'testValue';
                echo "✓ __set() works\n";
                
                // Test __get
                $value = $this->app->testProperty;
                echo "✓ __get() returns: " . ($value ?? 'null') . "\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test load method
     */
    public function testLoad()
    {
        echo "Testing Load Method:\n";
        
        try {
            if ($this->app) {
                // Test load method
                $this->app->load('test', 3600);
                echo "✓ load() method works\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test response handling
     */
    public function testResponseHandling()
    {
        echo "Testing Response Handling:\n";
        
        try {
            if ($this->app) {
                // Create a mock response object
                $mockResponse = new stdClass();
                $mockResponse->status = 200;
                
                // Test setResponse
                $this->app->setResponse($mockResponse);
                echo "✓ setResponse() works\n";
            }
            
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
        $this->testConstructorAndGetInstance();
        $this->testEnvironmentAndRuntime();
        $this->testRequestInfo();
        $this->testParameters();
        $this->testStateManagement();
        $this->testConfigAndAutoload();
        $this->testErrorHandling();
        $this->testViewConfiguration();
        $this->testWebscan();
        $this->testMagicMethods();
        $this->testLoad();
        $this->testResponseHandling();
        
        echo "=== Application Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new ApplicationTest();
    $test->runAllTests();
}
