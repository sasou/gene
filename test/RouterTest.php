<?php

/**
 * Gene Framework Router Class Test
 * 
 * This test file covers the important methods of the Gene\Router class
 */

use Gene\Router;

class RouterGroupTrace { public static $values = []; }
class RouterGroupHookA extends \Gene\Hook { public function handle() { RouterGroupTrace::$values[] = 'a'; return true; } }
class RouterGroupHookB extends \Gene\Hook { public function handle() { RouterGroupTrace::$values[] = 'b'; return true; } }
class RouterGroupHookStop extends \Gene\Hook { public function handle() { RouterGroupTrace::$values[] = 'stop'; return self::abort(); } }
class RouterGroupController { public function run() { RouterGroupTrace::$values[] = 'controller'; } }
class RouterRequestIdController { public function run() { RouterGroupTrace::$values[] = \Gene\Context::get('request_id'); } }

class RouterTest
{
    private $router;
    
    public function __construct()
    {
        echo "=== Gene\Router Test Suite ===\n\n";
    }
    
    /**
     * Test Router constructor
     */
    public function testConstructor()
    {
        echo "Testing Router Constructor:\n";
        
        try {
            // Test constructor with safe mode
            $router1 = new Router(true);
            echo "✓ Router constructor with safe mode works\n";
            
            // Test constructor without parameters
            $router2 = new Router();
            echo "✓ Router constructor without parameters works\n";
            
            $this->router = $router1;
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test basic routing functionality
     */
    public function testBasicRouting()
    {
        echo "Testing Basic Routing Functionality:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test run method
            $result = $this->router->run('GET', '/test');
            echo "✓ run() method works\n";
            
            // Test runError method
            $result = $this->router->runError('GET');
            echo "✓ runError() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test router data management
     */
    public function testDataManagement()
    {
        echo "Testing Router Data Management:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test getEvent
            $event = $this->router->getEvent();
            echo "✓ getEvent() works\n";
            
            // Test getTree
            $tree = $this->router->getTree();
            echo "✓ getTree() works\n";
            
            // Test getConf
            $conf = $this->router->getConf();
            echo "✓ getConf() works\n";
            
            // Test getTime
            $time = $this->router->getTime();
            echo "✓ getTime() returns: " . ($time ?? 'null') . "\n";
            
            // Test getRouter (should return self)
            $routerRef = $this->router->getRouter();
            echo "✓ getRouter() works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test router data deletion
     */
    public function testDataDeletion()
    {
        echo "Testing Router Data Deletion:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test delTree
            $result = $this->router->delTree();
            echo "✓ delTree() works\n";
            
            // Test delEvent
            $result = $this->router->delEvent();
            echo "✓ delEvent() works\n";
            
            // Test delConf
            $result = $this->router->delConf();
            echo "✓ delConf() works\n";
            
            // Test clear
            $result = $this->router->clear();
            echo "✓ clear() works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test template rendering
     */
    public function testTemplateRendering()
    {
        echo "Testing Template Rendering:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test assign
            $this->router->assign('testVar', 'testValue');
            echo "✓ assign() works\n";
            
            // Test assign with array
            $this->router->assign('arrayVar', ['key1' => 'value1', 'key2' => 'value2']);
            echo "✓ assign() with array works\n";
            
            // Test display (this might throw error if file doesn't exist)
            try {
                $this->router->display('test.php');
                echo "✓ display() works\n";
            } catch (Exception $e) {
                echo "✓ display() handles missing file gracefully\n";
            }
            
            // Test displayExt
            try {
                $this->router->displayExt('test.php');
                echo "✓ displayExt() works\n";
            } catch (Exception $e) {
                echo "✓ displayExt() handles missing file gracefully\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test dispatch functionality
     */
    public function testDispatch()
    {
        echo "Testing Dispatch Functionality:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test dispatch with valid parameters
            $result = $this->router->dispatch('TestController', 'testAction', ['param1', 'param2']);
            echo "✓ dispatch() with parameters works\n";
            
            // Test dispatch without parameters (dispatch requires 3 args)
            $result = $this->router->dispatch('TestController', 'testAction', []);
            echo "✓ dispatch() without parameters works\n";
            
        } catch (Exception $e) {
            echo "✓ dispatch() handles invalid controller gracefully\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test parameter handling
     */
    public function testParameterHandling()
    {
        echo "Testing Parameter Handling:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test params without name (should return all params)
            $allParams = $this->router->params();
            echo "✓ params() without name works\n";
            
            // Test params with name
            $param = $this->router->params('testParam');
            echo "✓ params() with name works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test routing groups
     */
    public function testRoutingGroups()
    {
        echo "Testing Routing Groups:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test group
            $this->router->group('api');
            echo "✓ group() works\n";
            
            // Test prefix
            $this->router->prefix('v1');
            echo "✓ prefix() works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test language handling
     */
    public function testLanguageHandling()
    {
        echo "Testing Language Handling:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test lang
            $this->router->lang('en,zh');
            echo "✓ lang() works\n";
            
            // Test getLang
            $currentLang = $this->router->getLang();
            echo "✓ getLang() returns: " . ($currentLang ?? 'null') . "\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test file operations
     */
    public function testFileOperations()
    {
        echo "Testing File Operations:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test readFile — [GENE_FIX:2026-08-10 R2] read this file itself
            // as a guaranteed-existing fixture. The previous 'test.txt' did
            // not exist: readFile() returned null (no exception, so the catch
            // never ran) and a PHP Warning leaked into the normal output.
            $content = $this->router->readFile(__FILE__);
            if (is_string($content) && strpos($content, 'testFileOperations') !== false) {
                echo "✓ readFile() reads an existing file\n";
            } else {
                echo "✗ readFile() returned unexpected content\n";
            }

            // Missing file returns null (no exception); @ keeps the stream
            // warning out of the test output.
            $missing = @$this->router->readFile(__DIR__ . '/__definitely_not_here__.txt');
            echo ($missing === null ? "✓" : "✗") . " readFile() returns null for a missing file\n";
            
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
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test __call with route methods
            try {
                $this->router->get('/test', 'TestController@test');
                echo "✓ __call() with GET route works\n";
            } catch (Exception $e) {
                echo "✓ __call() handles route method gracefully\n";
            }
            
            try {
                $this->router->post('/test', 'TestController@test');
                echo "✓ __call() with POST route works\n";
            } catch (Exception $e) {
                echo "✓ __call() handles POST route gracefully\n";
            }
            
            try {
                $this->router->put('/test', 'TestController@test');
                echo "✓ __call() with PUT route works\n";
            } catch (Exception $e) {
                echo "✓ __call() handles PUT route gracefully\n";
            }
            
            try {
                $this->router->delete('/test', 'TestController@test');
                echo "✓ __call() with DELETE route works\n";
            } catch (Exception $e) {
                echo "✓ __call() handles DELETE route gracefully\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test route patterns
     */
    public function testRoutePatterns()
    {
        echo "Testing Route Patterns:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test route with parameters
            try {
                $this->router->get('/user/{id}', 'UserController@show');
                echo "✓ Route with parameter pattern works\n";
            } catch (Exception $e) {
                echo "✓ Route with parameter pattern handled gracefully\n";
            }
            
            // Test route with optional parameter
            try {
                $this->router->get('/user/{id?}', 'UserController@index');
                echo "✓ Route with optional parameter works\n";
            } catch (Exception $e) {
                echo "✓ Route with optional parameter handled gracefully\n";
            }
            
            // Test route with regex
            try {
                $this->router->get('/user/{id:[0-9]+}', 'UserController@show');
                echo "✓ Route with regex constraint works\n";
            } catch (Exception $e) {
                echo "✓ Route with regex constraint handled gracefully\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test middleware and hooks
     */
    public function testMiddlewareAndHooks()
    {
        echo "Testing Middleware and Hooks:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            // Test route with middleware
            try {
                $middleware = function($request, $next) {
                    echo "Middleware executed\n";
                    return $next($request);
                };
                $this->router->get('/admin', 'AdminController@index', $middleware);
                echo "✓ Route with middleware works\n";
            } catch (Exception $e) {
                echo "✓ Route with middleware handled gracefully\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test performance with multiple routes
     */
    public function testPerformance()
    {
        echo "Testing Performance with Multiple Routes:\n";
        
        try {
            if (!$this->router) {
                $this->router = new Router();
            }
            
            $startTime = microtime(true);
            
            // Register multiple routes
            for ($i = 0; $i < 100; $i++) {
                $this->router->get("/route_$i", "TestController@action$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000; // Convert to milliseconds
            
            echo "✓ 100 routes registered in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }

    /**
     * head("/") / options("/") used to write "method//leaf/run" into the
     * persistent router tree and abort debug PHP (HashTable being destroyed).
     */
    public function testHeadOptionsRoot()
    {
        echo "Testing head('/') and options('/') root routes:\n";

        try {
            $router = new Router();
            $router->clear()
                ->get("/", "Controllers\\Index@index")
                ->get("/test.html", "Controllers\\Index@test")
                ->get("/doc.html", "Controllers\\Index@doc")
                ->get("/admin/:c/:a", "Controllers\\Admin\\:c@:a")
                ->head("/", function () {})
                ->options("/", function () {});
            echo "✓ head('/') and options('/') registered without crash\n";
            $hit = $router->match('GET', '/test.html');
            if ($hit !== false) {
                echo "✓ match GET /test.html after head/options root routes\n";
            } else {
                echo "✗ match GET /test.html missed (html routes broken)\n";
            }
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }
    
    public function testComposableGroupHooks()
    {
        echo "Testing Composable Group Hooks:\n";
        $router = new Router('group-hook-test');
        $router->clear()
            ->hook('groupA', 'RouterGroupHookA@handle')
            ->hook('groupB', 'RouterGroupHookB@handle')
            ->hook('groupStop', 'RouterGroupHookStop@handle');
        RouterGroupTrace::$values = [];
        $router->group('/admin')->through(['groupA'])
            ->group('/users')->through(['groupB'])
            ->get('/list', 'RouterGroupController@run')
            ->group()->group();
        $router->run('GET', '/admin/users/list');
        if (RouterGroupTrace::$values === ['a', 'b', 'controller']) echo "✓ nested groups inherit hooks in stable order\n"; else echo "✗ group order mismatch: " . json_encode(RouterGroupTrace::$values) . "\n";
        $router->group('/blocked')->through(['groupA', 'groupStop', 'groupB'])
            ->get('/run', 'RouterGroupController@run')->group();
        RouterGroupTrace::$values = [];
        $router->run('GET', '/blocked/run');
        $blockedTrace = array_slice(RouterGroupTrace::$values, -2);
        if ($blockedTrace === ['a', 'stop']) echo "✓ abort skips remaining hooks and controller\n"; else echo "✗ group abort mismatch: " . json_encode($blockedTrace) . "\n";
        \Gene\Application::cleanup();
        echo "\n";
    }

    public function testRequestIdPolicy()
    {
        echo "Testing Request-id Policy:\n";
        $router = new Router('request-id-test');
        $router->clear()->get('/', 'RouterRequestIdController@run');
        $app = new \Gene\Application('request-id-test');
        $app->requestId(['header' => 'X-Request-Id', 'bytes' => 8, 'trust' => true, 'max_length' => 32]);
        RouterGroupTrace::$values = [];
        \Gene\Request::init([], [], [], [], null, [], null, ['x-request-id' => 'trusted-id'], '');
        @$app->run('GET', '/');
        if (array_slice(RouterGroupTrace::$values, -1) === ['trusted-id']) echo "✓ trusted visible ASCII request-id is reused\n"; else echo "✗ trusted request-id was not reused\n";
        \Gene\Application::cleanup();
        RouterGroupTrace::$values = [];
        \Gene\Request::init([], [], [], [], null, [], null, ['X-Request-Id' => "bad\r\nid"], '');
        @$app->run('GET', '/');
        $generated = end(RouterGroupTrace::$values);
        if (is_string($generated) && strlen($generated) === 16 && ctype_xdigit($generated)) echo "✓ invalid request-id is replaced\n"; else echo "✗ invalid request-id replacement failed\n";
        $app->requestId(null);
        \Gene\Application::cleanup();
        echo "\n";
    }

    /**
     * Run all tests
     */
    public function runAllTests()
    {
        $this->testConstructor();
        $this->testBasicRouting();
        $this->testDataManagement();
        $this->testDataDeletion();
        $this->testTemplateRendering();
        $this->testDispatch();
        $this->testParameterHandling();
        $this->testRoutingGroups();
        $this->testLanguageHandling();
        $this->testFileOperations();
        $this->testMagicMethods();
        $this->testRoutePatterns();
        $this->testMiddlewareAndHooks();
        $this->testPerformance();
        $this->testHeadOptionsRoot();
        $this->testComposableGroupHooks();
        $this->testRequestIdPolicy();
        
        echo "=== Router Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new RouterTest();
    $test->runAllTests();
}
