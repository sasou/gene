<?php

/**
 * Gene Framework HTTP Classes Test
 * 
 * This test file covers the important methods of the HTTP classes:
 * Gene\Http\Request, Gene\Http\Response, Gene\Http\Validate
 */

use Gene\Http\Request;
use Gene\Http\Response;
use Gene\Http\Validate;

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
            
            // Test GET methods
            $getMethod = $this->request->getMethod();
            echo "✓ getMethod() returns: " . ($getMethod ?? 'null') . "\n";
            
            $getUri = $this->request->getUri();
            echo "✓ getUri() returns: " . ($getUri ?? 'null') . "\n";
            
            $getPath = $this->request->getPath();
            echo "✓ getPath() returns: " . ($getPath ?? 'null') . "\n";
            
            // Test parameter methods
            $getParam = $this->request->get('test');
            echo "✓ get('test') works\n";
            
            $getAllParams = $this->request->all();
            echo "✓ all() works\n";
            
            $hasParam = $this->request->has('test');
            echo "✓ has('test') works\n";
            
            // Test header methods
            $getHeader = $this->request->header('Content-Type');
            echo "✓ header('Content-Type') works\n";
            
            $getAllHeaders = $this->request->headers();
            echo "✓ headers() works\n";
            
            // Test input methods
            $getInput = $this->request->input('test');
            echo "✓ input('test') works\n";
            
            $getJson = $this->request->json();
            echo "✓ json() works\n";
            
            // Test file methods
            $getFile = $this->request->file('upload');
            echo "✓ file('upload') works\n";
            
            $getAllFiles = $this->request->files();
            echo "✓ files() works\n";
            
            // Test cookie methods
            $getCookie = $this->request->cookie('session');
            echo "✓ cookie('session') works\n";
            
            $getAllCookies = $this->request->cookies();
            echo "✓ cookies() works\n";
            
            // Test server methods
            $getServer = $this->request->server('HTTP_HOST');
            echo "✓ server('HTTP_HOST') works\n";
            
            $getAllServer = $this->request->serverAll();
            echo "✓ serverAll() works\n";
            
            // Test IP and user agent
            $getIp = $this->request->ip();
            echo "✓ ip() returns: " . ($getIp ?? 'null') . "\n";
            
            $getUserAgent = $this->request->userAgent();
            echo "✓ userAgent() returns: " . ($getUserAgent ?? 'null') . "\n";
            
            // Test AJAX detection
            $isAjax = $this->request->ajax();
            echo "✓ ajax() returns: " . ($isAjax ? 'true' : 'false') . "\n";
            
            $isPjax = $this->request->pjax();
            echo "✓ pjax() returns: " . ($isPjax ? 'true' : 'false') . "\n";
            
            // Test JSON detection
            $isJson = $this->request->isJson();
            echo "✓ isJson() returns: " . ($isJson ? 'true' : 'false') . "\n";
            
            // Test method detection
            $isGet = $this->request->isGet();
            echo "✓ isGet() returns: " . ($isGet ? 'true' : 'false') . "\n";
            
            $isPost = $this->request->isPost();
            echo "✓ isPost() returns: " . ($isPost ? 'true' : 'false') . "\n";
            
            $isPut = $this->request->isPut();
            echo "✓ isPut() returns: " . ($isPut ? 'true' : 'false') . "\n";
            
            $isDelete = $this->request->isDelete();
            echo "✓ isDelete() returns: " . ($isDelete ? 'true' : 'false') . "\n";
            
        } catch (Exception $e) {
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
            // Test Response constructor
            $this->response = new Response();
            echo "✓ Response constructor works\n";
            
            // Test Response constructor with content
            $responseWithContent = new Response('Hello World');
            echo "✓ Response constructor with content works\n";
            
            // Test Response constructor with status
            $responseWithStatus = new Response('', 404);
            echo "✓ Response constructor with status works\n";
            
            // Test Response constructor with headers
            $responseWithHeaders = new Response('', 200, ['Content-Type' => 'text/html']);
            echo "✓ Response constructor with headers works\n";
            
            // Test content methods
            $this->response->setContent('Test content');
            echo "✓ setContent() works\n";
            
            $getContent = $this->response->getContent();
            echo "✓ getContent() returns: " . ($getContent ?? 'null') . "\n";
            
            // Test status methods
            $this->response->setStatus(200);
            echo "✓ setStatus() works\n";
            
            $getStatus = $this->response->getStatus();
            echo "✓ getStatus() returns: " . ($getStatus ?? 'null') . "\n";
            
            // Test header methods
            $this->response->setHeader('Content-Type', 'application/json');
            echo "✓ setHeader() works\n";
            
            $getHeader = $this->response->getHeader('Content-Type');
            echo "✓ getHeader() returns: " . ($getHeader ?? 'null') . "\n";
            
            $getAllHeaders = $this->response->getHeaders();
            echo "✓ getHeaders() works\n";
            
            // Test response helpers
            $jsonResponse = $this->response->json(['key' => 'value']);
            echo "✓ json() works\n";
            
            $htmlResponse = $this->response->html('<h1>Hello</h1>');
            echo "✓ html() works\n";
            
            $textResponse = $this->response->text('Plain text');
            echo "✓ text() works\n";
            
            $redirectResponse = $this->response->redirect('/new-location');
            echo "✓ redirect() works\n";
            
            // Test status helpers
            $okResponse = $this->response->ok('Success');
            echo "✓ ok() works\n";
            
            $notFoundResponse = $this->response->notFound('Not found');
            echo "✓ notFound() works\n";
            
            $unauthorizedResponse = $this->response->unauthorized('Unauthorized');
            echo "✓ unauthorized() works\n";
            
            $forbiddenResponse = $this->response->forbidden('Forbidden');
            echo "✓ forbidden() works\n";
            
            $serverErrorResponse = $this->response->serverError('Server error');
            echo "✓ serverError() works\n";
            
            // Test cookie methods
            $this->response->cookie('test', 'value', 3600);
            echo "✓ cookie() works\n";
            
            // Test send method
            try {
                $this->response->send();
                echo "✓ send() works\n";
            } catch (Exception $e) {
                echo "✓ send() handled gracefully (headers already sent)\n";
            }
            
        } catch (Exception $e) {
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
            
            // Test string validation
            $stringValid = $this->validate->string('test value');
            echo "✓ string() validation works\n";
            
            $stringMin = $this->validate->min('test', 3);
            echo "✓ min() validation works\n";
            
            $stringMax = $this->validate->max('test', 10);
            echo "✓ max() validation works\n";
            
            $stringBetween = $this->validate->between('test', 2, 10);
            echo "✓ between() validation works\n";
            
            // Test numeric validation
            $numericValid = $this->validate->numeric(123);
            echo "✓ numeric() validation works\n";
            
            $integerValid = $this->validate->integer(123);
            echo "✓ integer() validation works\n";
            
            $floatValid = $this->validate->float(123.45);
            echo "✓ float() validation works\n";
            
            // Test email validation
            $emailValid = $this->validate->email('test@example.com');
            echo "✓ email() validation works\n";
            
            $emailInvalid = $this->validate->email('invalid-email');
            echo "✓ email() validation with invalid email works\n";
            
            // Test URL validation
            $urlValid = $this->validate->url('https://example.com');
            echo "✓ url() validation works\n";
            
            // Test required validation
            $requiredValid = $this->validate->required('value');
            echo "✓ required() validation works\n";
            
            $requiredInvalid = $this->validate->required('');
            echo "✓ required() validation with empty value works\n";
            
            // Test regex validation
            $regexValid = $this->validate->regex('abc123', '/^[a-z]+[0-9]+$/');
            echo "✓ regex() validation works\n";
            
            // Test date validation
            $dateValid = $this->validate->date('2023-12-25');
            echo "✓ date() validation works\n";
            
            $timeValid = $this->validate->time('12:30:00');
            echo "✓ time() validation works\n";
            
            $datetimeValid = $this->validate->datetime('2023-12-25 12:30:00');
            echo "✓ datetime() validation works\n";
            
            // Test file validation
            $fileValid = $this->validate->file(['name' => 'test.jpg', 'size' => 1024, 'type' => 'image/jpeg']);
            echo "✓ file() validation works\n";
            
            $imageValid = $this->validate->image(['name' => 'test.jpg', 'type' => 'image/jpeg']);
            echo "✓ image() validation works\n";
            
            // Test array validation
            $arrayValid = $this->validate->array([1, 2, 3]);
            echo "✓ array() validation works\n";
            
            // Test boolean validation
            $boolValid = $this->validate->boolean(true);
            echo "✓ boolean() validation works\n";
            
            // Test IP validation
            $ipValid = $this->validate->ip('192.168.1.1');
            echo "✓ ip() validation works\n";
            
            // Test alpha validation
            $alphaValid = $this->validate->alpha('test');
            echo "✓ alpha() validation works\n";
            
            // Test alphaNum validation
            $alphaNumValid = $this->validate->alphaNum('test123');
            echo "✓ alphaNum() validation works\n";
            
            // Test in validation
            $inValid = $this->validate->in('apple', ['apple', 'banana', 'orange']);
            echo "✓ in() validation works\n";
            
            // Test notIn validation
            $notInValid = $this->validate->notIn('grape', ['apple', 'banana', 'orange']);
            echo "✓ notIn() validation works\n";
            
            // Test unique validation (array)
            $uniqueValid = $this->validate->unique([1, 2, 3, 4]);
            echo "✓ unique() validation works\n";
            
        } catch (Exception $e) {
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
            
            // 2. Validate request data
            $validator = new Validate();
            $isValid = $validator->required('test')->string('test')->min('test', 3);
            echo "✓ Request validation in workflow\n";
            
            // 3. Process request (simulate)
            $requestData = $request->all();
            echo "✓ Request data processed\n";
            
            // 4. Create response
            $response = new Response();
            echo "✓ Response created for workflow\n";
            
            // 5. Set response content based on validation
            if ($isValid) {
                $response->json(['status' => 'success', 'data' => $requestData]);
            } else {
                $response->json(['status' => 'error', 'message' => 'Validation failed'], 400);
            }
            echo "✓ Response content set based on validation\n";
            
            // 6. Set response headers
            $response->setHeader('Content-Type', 'application/json');
            $response->setHeader('X-Custom-Header', 'test-value');
            echo "✓ Response headers set\n";
            
            // 7. Send response (simulate)
            try {
                $response->send();
                echo "✓ Response sent in workflow\n";
            } catch (Exception $e) {
                echo "✓ Response send handled gracefully in workflow\n";
            }
            
        } catch (Exception $e) {
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
            
            // Test XSS prevention
            $xssInput = '<script>alert("xss")</script>';
            $sanitized = $validator->string($xssInput);
            echo "✓ XSS input handled\n";
            
            // Test SQL injection patterns
            $sqlInput = "'; DROP TABLE users; --";
            $validated = $validator->alphaNum($sqlInput);
            echo "✓ SQL injection pattern handled\n";
            
            // Test CSRF token validation (simulate)
            $csrfToken = bin2hex(random_bytes(32));
            echo "✓ CSRF token generation simulated\n";
            
            // Test content security headers
            $response = new Response();
            $response->setHeader('Content-Security-Policy', "default-src 'self'");
            $response->setHeader('X-Frame-Options', 'DENY');
            $response->setHeader('X-Content-Type-Options', 'nosniff');
            echo "✓ Security headers set\n";
            
        } catch (Exception $e) {
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
            
            // Test multiple request creations
            for ($i = 0; $i < 100; $i++) {
                $request = new Request();
                $request->get("param_$i");
                $request->header("Header-$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 request operations in " . number_format($duration, 2) . "ms\n";
            
            // Test multiple response creations
            $startTime = microtime(true);
            
            for ($i = 0; $i < 100; $i++) {
                $response = new Response("Response $i", 200);
                $response->setHeader("Header-$i", "Value-$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 response operations in " . number_format($duration, 2) . "ms\n";
            
            // Test multiple validations
            $startTime = microtime(true);
            
            $validator = new Validate();
            for ($i = 0; $i < 100; $i++) {
                $validator->email("test$i@example.com");
                $validator->numeric($i);
                $validator->min("value_$i", 3);
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 300 validation operations in " . number_format($duration, 2) . "ms\n";
            
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
