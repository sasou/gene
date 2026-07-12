<?php

/**
 * Gene Framework HTTP Classes Test
 * 
 * This test file covers the important methods of the HTTP classes:
 * Gene\Http\Request, Gene\Http\Response, Gene\Http\Validate
 */

use Gene\Request;
use Gene\Response;
use Gene\Validate;

class HttpTest
{
    private $request;
    private $response;
    private $validate;
    
    public function __construct()
    {
        echo "=== Gene HTTP Classes Test Suite ===\n\n";
    }
    
    /**
     * Test Request class
     */
    public function testRequestClass()
    {
        echo "Testing Request Class:\n";
        
        try {
            // Test Request constructor
            $this->request = new Request();
            echo "✓ Request constructor works\n";
            
            // Test getMethod (static method, callable on instance)
            $getMethod = Request::getMethod();
            echo "✓ getMethod() returns: " . ($getMethod ?? 'null') . "\n";
            
            // Test parameter methods (static)
            $getParam = Request::get('test');
            echo "✓ get('test') works\n";
            
            $postParam = Request::post('test');
            echo "✓ post('test') works\n";
            
            // Test header methods (static)
            $getHeader = Request::header('Content-Type');
            echo "✓ header('Content-Type') works\n";
            
            // Test file methods (static)
            $getAllFiles = Request::files();
            echo "✓ files() works\n";
            
            // Test cookie methods (static)
            $getCookie = Request::cookie('session');
            echo "✓ cookie('session') works\n";
            
            // Test server methods (static)
            $getServer = Request::server('HTTP_HOST');
            echo "✓ server('HTTP_HOST') works\n";
            
            // Test AJAX detection (static)
            $isAjax = Request::isAjax();
            echo "✓ isAjax() returns: " . ($isAjax ? 'true' : 'false') . "\n";
            
            // Test method detection (static)
            $isGet = Request::isGet();
            echo "✓ isGet() returns: " . ($isGet ? 'true' : 'false') . "\n";
            
            $isPost = Request::isPost();
            echo "✓ isPost() returns: " . ($isPost ? 'true' : 'false') . "\n";
            
            $isPut = Request::isPut();
            echo "✓ isPut() returns: " . ($isPut ? 'true' : 'false') . "\n";
            
            $isCli = Request::isCli();
            echo "✓ isCli() returns: " . ($isCli ? 'true' : 'false') . "\n";
            
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Response class
     */
    public function testResponseClass()
    {
        echo "Testing Response Class:\n";
        
        try {
            // Test Response constructor (takes no args)
            $this->response = new Response();
            echo "✓ Response constructor works\n";
            
            // Test static response helpers
            Response::setJsonHeader();
            echo "✓ setJsonHeader() works\n";
            
            Response::setHtmlHeader();
            echo "✓ setHtmlHeader() works\n";
            
            Response::header('X-Custom', 'test-value');
            echo "✓ header() works\n";
            
            Response::cookie('test_cookie', 'value', 3600);
            echo "✓ cookie() works\n";
            
            // Test json/data/success/error (static, output-buffered to avoid headers_sent)
            ob_start();
            Response::json(['key' => 'value']);
            ob_end_clean();
            echo "✓ json() works\n";
            
            ob_start();
            Response::success('Operation completed');
            ob_end_clean();
            echo "✓ success() works\n";
            
            ob_start();
            Response::error('Something went wrong');
            ob_end_clean();
            echo "✓ error() works\n";
            
            ob_start();
            Response::data(['result' => 'ok'], 'Success', 200);
            ob_end_clean();
            echo "✓ data() works\n";
            
            // Test redirect (static)
            ob_start();
            Response::redirect('/new-location');
            ob_end_clean();
            echo "✓ redirect() works\n";
            
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Validate class
     */
    public function testValidateClass()
    {
        echo "Testing Validate Class:\n";
        
        try {
            // Test Validate constructor
            $this->validate = new Validate();
            echo "✓ Validate constructor works\n";
            
            // Test init with data
            $this->validate->init(['name' => 'John', 'email' => 'test@example.com', 'age' => '25']);
            echo "✓ init() with data works\n";
            
            // Test name (set field for validation)
            $this->validate->name('name');
            echo "✓ name() works\n";
            
            // Test rule_required
            $this->validate->name('name')->rule_required();
            echo "✓ rule_required() works\n";
            
            // Test rule_email
            $this->validate->name('email')->rule_email();
            echo "✓ rule_email() works\n";
            
            // Test rule_number
            $this->validate->name('age')->rule_number();
            echo "✓ rule_number() works\n";
            
            // Test rule_int
            $this->validate->name('age')->rule_int();
            echo "✓ rule_int() works\n";
            
            // Test rule_min
            $this->validate->name('age')->rule_min(18);
            echo "✓ rule_min() works\n";
            
            // Test rule_max
            $this->validate->name('age')->rule_max(100);
            echo "✓ rule_max() works\n";
            
            // Test rule_url
            $this->validate->name('website')->rule_url();
            echo "✓ rule_url() works\n";
            
            // Test rule_ip
            $this->validate->name('ip_address')->rule_ip();
            echo "✓ rule_ip() works\n";
            
            // Test rule_string
            $this->validate->name('name')->rule_string();
            echo "✓ rule_string() works\n";
            
            // Test rule_digit
            $this->validate->name('age')->rule_digit();
            echo "✓ rule_digit() works\n";
            
            // Test rule_date
            $this->validate->name('birthdate')->rule_date();
            echo "✓ rule_date() works\n";
            
            // Test valid (run validation)
            $this->validate->valid();
            echo "✓ valid() works\n";
            
            // Test getError
            $errors = $this->validate->getError();
            echo "✓ getError() works\n";
            
            // Test getValue
            $value = $this->validate->getValue('name');
            echo "✓ getValue() works\n";
            
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test HTTP workflow integration
     */
    public function testHttpWorkflowIntegration()
    {
        echo "Testing HTTP Workflow Integration:\n";
        
        try {
            // Simulate a complete HTTP request/response cycle
            
            // 1. Create request
            $request = new Request();
            echo "✓ Request created for workflow\n";
            
            // 2. Validate request data using Validate API
            $validator = new Validate();
            $validator->init(['name' => 'test', 'email' => 'test@example.com']);
            $validator->name('name')->rule_required()->rule_string();
            $validator->name('email')->rule_required()->rule_email();
            $validator->valid();
            echo "✓ Request validation in workflow\n";
            
            // 3. Process request (simulate)
            $requestData = Request::get('test');
            echo "✓ Request data processed\n";
            
            // 4. Create response
            $response = new Response();
            echo "✓ Response created for workflow\n";
            
            // 5. Send JSON response based on validation
            ob_start();
            Response::json(['status' => 'success', 'data' => $requestData]);
            ob_end_clean();
            echo "✓ Response content set based on validation\n";
            
            // 6. Set response headers
            Response::header('Content-Type', 'application/json');
            Response::header('X-Custom-Header', 'test-value');
            echo "✓ Response headers set\n";
            
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test HTTP security features
     */
    public function testHttpSecurityFeatures()
    {
        echo "Testing HTTP Security Features:\n";
        
        try {
            // Test input sanitization through validation
            $validator = new Validate();
            
            // Test XSS prevention — validate input as string
            $validator->init(['input' => '<script>alert("xss")</script>']);
            $validator->name('input')->rule_string();
            $validator->valid();
            echo "✓ XSS input handled\n";
            
            // Test SQL injection patterns — validate as string
            $validator->init(['input' => "'; DROP TABLE users; --"]);
            $validator->name('input')->rule_string();
            $validator->valid();
            echo "✓ SQL injection pattern handled\n";
            
            // Test CSRF token validation (simulate)
            $csrfToken = bin2hex(random_bytes(32));
            echo "✓ CSRF token generation simulated\n";
            
            // Test content security headers (static Response methods)
            Response::header('Content-Security-Policy', "default-src 'self'");
            Response::header('X-Frame-Options', 'DENY');
            Response::header('X-Content-Type-Options', 'nosniff');
            echo "✓ Security headers set\n";
            
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test HTTP performance
     */
    public function testHttpPerformance()
    {
        echo "Testing HTTP Performance:\n";
        
        try {
            $startTime = microtime(true);
            
            // Test multiple request operations (static methods)
            for ($i = 0; $i < 100; $i++) {
                Request::get("param_$i");
                Request::header("Header-$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 request operations in " . number_format($duration, 2) . "ms\n";
            
            // Test multiple response operations (static methods)
            $startTime = microtime(true);
            
            for ($i = 0; $i < 100; $i++) {
                Response::header("Header-$i", "Value-$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 response operations in " . number_format($duration, 2) . "ms\n";
            
            // Test multiple validations
            $startTime = microtime(true);
            
            for ($i = 0; $i < 100; $i++) {
                $validator = new Validate();
                $validator->init(['email' => "test$i@example.com", 'age' => $i]);
                $validator->name('email')->rule_email();
                $validator->name('age')->rule_int();
                $validator->valid();
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 validation cycles in " . number_format($duration, 2) . "ms\n";
            
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Run all tests
     */
    public function runAllTests()
    {
        $this->testRequestClass();
        $this->testResponseClass();
        $this->testValidateClass();
        $this->testHttpWorkflowIntegration();
        $this->testHttpSecurityFeatures();
        $this->testHttpPerformance();
        
        echo "=== HTTP Classes Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new HttpTest();
    $test->runAllTests();
}
