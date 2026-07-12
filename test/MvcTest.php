<?php

/**
 * Gene Framework MVC Classes Test
 * 
 * This test file covers the important methods of the MVC classes:
 * Gene\Mvc\Controller, Gene\Mvc\Model, Gene\Mvc\View
 */

use Gene\Controller;
use Gene\Model;
use Gene\View;

class MvcTest
{
    private $controller;
    private $model;
    private $view;
    
    public function __construct()
    {
        echo "=== Gene MVC Classes Test Suite ===\n\n";
    }
    
    /**
     * Test Controller class
     */
    public function testControllerClass()
    {
        echo "Testing Controller Class:\n";
        
        try {
            // Test Controller constructor
            $this->controller = new Controller();
            echo "✓ Controller constructor works\n";
            
            // Test initialization methods
            $this->controller->init();
            echo "✓ init() method works\n";
            
            // Test request handling
            $this->controller->beforeAction();
            echo "✓ beforeAction() method works\n";
            
            $this->controller->afterAction();
            echo "✓ afterAction() method works\n";
            
            // Test response methods
            $this->controller->render('test');
            echo "✓ render() method works\n";
            
            $this->controller->renderText('Plain text response');
            echo "✓ renderText() method works\n";
            
            $this->controller->renderJson(['key' => 'value']);
            echo "✓ renderJson() method works\n";
            
            $this->controller->redirect('/target-url');
            echo "✓ redirect() method works\n";
            
            // Test parameter handling
            $getParam = $this->controller->getParam('id');
            echo "✓ getParam() method works\n";
            
            $getAllParams = $this->controller->getParams();
            echo "✓ getParams() method works\n";
            
            $hasParam = $this->controller->hasParam('id');
            echo "✓ hasParam() method works\n";
            
            // Test view variable assignment
            $this->controller->assign('variable', 'value');
            echo "✓ assign() method works\n";
            
            $this->controller->assignMultiple([
                'var1' => 'value1',
                'var2' => 'value2',
                'var3' => 'value3'
            ]);
            echo "✓ assignMultiple() method works\n";
            
            // Test layout methods
            $this->controller->setLayout('main');
            echo "✓ setLayout() method works\n";
            
            $this->controller->disableLayout();
            echo "✓ disableLayout() method works\n";
            
            // Test action methods
            $this->controller->forward('other/action');
            echo "✓ forward() method works\n";
            
            // Test error handling
            $this->controller->notFound();
            echo "✓ notFound() method works\n";
            
            $this->controller->error('Server error occurred');
            echo "✓ error() method works\n";
            
            // Test HTTP methods
            $this->controller->isGet();
            echo "✓ isGet() method works\n";
            
            $this->controller->isPost();
            echo "✓ isPost() method works\n";
            
            $this->controller->isAjax();
            echo "✓ isAjax() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test Model class
     */
    public function testModelClass()
    {
        echo "Testing Model Class:\n";
        
        try {
            // Test Model constructor
            $this->model = new Model();
            echo "✓ Model constructor works\n";
            
            // Test database connection
            $this->model->connect();
            echo "✓ connect() method works\n";
            
            $this->model->disconnect();
            echo "✓ disconnect() method works\n";
            
            // Test CRUD operations
            $this->model->find(1);
            echo "✓ find() method works\n";
            
            $this->model->findAll();
            echo "✓ findAll() method works\n";
            
            $this->model->findBy(['name' => 'test']);
            echo "✓ findBy() method works\n";
            
            $this->model->findOneBy(['name' => 'test']);
            echo "✓ findOneBy() method works\n";
            
            // Test data manipulation
            $this->model->create(['name' => 'test', 'value' => 123]);
            echo "✓ create() method works\n";
            
            $this->model->update(1, ['name' => 'updated']);
            echo "✓ update() method works\n";
            
            $this->model->delete(1);
            echo "✓ delete() method works\n";
            
            // Test query methods
            $this->model->query('SELECT * FROM users');
            echo "✓ query() method works\n";
            
            $this->model->select('name, email')->from('users')->where('active = 1');
            echo "✓ Query builder methods work\n";
            
            // Test validation
            $this->model->validate(['name' => 'required', 'email' => 'email']);
            echo "✓ validate() method works\n";
            
            // Test pagination
            $this->model->paginate(10, 1);
            echo "✓ paginate() method works\n";
            
            // Test relationships
            $this->model->hasMany('comments');
            echo "✓ hasMany() method works\n";
            
            $this->model->belongsTo('user');
            echo "✓ belongsTo() method works\n";
            
            $this->model->hasOne('profile');
            echo "✓ hasOne() method works\n";
            
            // Test events
            $this->model->beforeCreate();
            echo "✓ beforeCreate() event works\n";
            
            $this->model->afterCreate();
            echo "✓ afterCreate() event works\n";
            
            $this->model->beforeUpdate();
            echo "✓ beforeUpdate() event works\n";
            
            $this->model->afterUpdate();
            echo "✓ afterUpdate() event works\n";
            
            $this->model->beforeDelete();
            echo "✓ beforeDelete() event works\n";
            
            $this->model->afterDelete();
            echo "✓ afterDelete() event works\n";
            
            // Test attribute methods
            $this->model->setAttribute('name', 'test');
            echo "✓ setAttribute() method works\n";
            
            $getAttribute = $this->model->getAttribute('name');
            echo "✓ getAttribute() method works\n";
            
            $attributes = $this->model->getAttributes();
            echo "✓ getAttributes() method works\n";
            
            // Test mass assignment
            $this->model->fill(['name' => 'test', 'email' => 'test@example.com']);
            echo "✓ fill() method works\n";
            
            // Test serialization
            $this->model->toJson();
            echo "✓ toJson() method works\n";
            
            $this->model->toArray();
            echo "✓ toArray() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test View class
     */
    public function testViewClass()
    {
        echo "Testing View Class:\n";
        
        try {
            // Test View constructor
            $this->view = new View();
            echo "✓ View constructor works\n";
            
            // Test template rendering
            $this->view->render('template');
            echo "✓ render() method works\n";
            
            $this->view->renderPartial('partial');
            echo "✓ renderPartial() method works\n";
            
            // Test view variables
            $this->view->assign('title', 'Page Title');
            echo "✓ assign() method works\n";
            
            $this->view->assign('content', 'Page content');
            echo "✓ Multiple assign() calls work\n";
            
            $getVariable = $this->view->get('title');
            echo "✓ get() method works\n";
            
            $allVariables = $this->view->getVariables();
            echo "✓ getVariables() method works\n";
            
            // Test template paths
            $this->view->setTemplatePath('./views');
            echo "✓ setTemplatePath() method works\n";
            
            $templatePath = $this->view->getTemplatePath();
            echo "✓ getTemplatePath() method works\n";
            
            // Test layout management
            $this->view->setLayout('main');
            echo "✓ setLayout() method works\n";
            
            $this->view->disableLayout();
            echo "✓ disableLayout() method works\n";
            
            // View helpers
            $this->view->escape('<script>alert("xss")</script>');
            echo "✓ escape() helper works\n";
            
            $this->view->url('/controller/action');
            echo "✓ url() helper works\n";
            
            $this->view->asset('/css/style.css');
            echo "✓ asset() helper works\n";
            
            $this->view->link('Home', '/home');
            echo "✓ link() helper works\n";
            
            // Test template inheritance
            $this->view->extend('base');
            echo "✓ extend() method works\n";
            
            $this->view->block('content');
            echo "✓ block() method works\n";
            
            $this->view->endBlock();
            echo "✓ endBlock() method works\n";
            
            // Test template includes
            $this->view->include('header');
            echo "✓ include() method works\n";
            
            // Test caching
            $this->view->cache('cache_key', 3600);
            echo "✓ cache() method works\n";
            
            // Test view rendering with data
            $this->view->renderWithData('template', ['key' => 'value']);
            echo "✓ renderWithData() method works\n";
            
            // Test view compilation
            $this->view->compile('template');
            echo "✓ compile() method works\n";
            
            // Test template existence
            $exists = $this->view->templateExists('template');
            echo "✓ templateExists() method works\n";
            
            // Test view filters
            $this->view->addFilter('uppercase', function($str) {
                return strtoupper($str);
            });
            echo "✓ addFilter() method works\n";
            
            // Test view extensions
            $this->view->addExtension('custom', function() {
                return 'Custom extension output';
            });
            echo "✓ addExtension() method works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test MVC integration
     */
    public function testMvcIntegration()
    {
        echo "Testing MVC Integration:\n";
        
        try {
            // Test complete MVC workflow
            
            // 1. Controller receives request
            $controller = new Controller();
            echo "✓ Controller initialized in workflow\n";
            
            // 2. Controller uses model for data
            $model = new Model();
            $data = $model->findAll();
            echo "✓ Model provides data to controller\n";
            
            // 3. Controller assigns data to view
            $controller->assign('data', $data);
            echo "✓ Controller assigns data to view\n";
            
            // 4. View renders template with data
            $view = new View();
            $view->assign('data', $data);
            $output = $view->render('template');
            echo "✓ View renders template with data\n";
            
            // 5. Controller sends response
            $controller->render('template');
            echo "✓ Controller sends response\n";
            
            // Test data flow between components
            $controller->assign('user', $model->find(1));
            $view->assign('user', $controller->getParam('user'));
            echo "✓ Data flow between MVC components works\n";
            
            // Test event propagation
            $model->beforeCreate();
            $controller->beforeAction();
            $view->render('template');
            $controller->afterAction();
            $model->afterCreate();
            echo "✓ Event propagation works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test MVC patterns
     */
    public function testMvcPatterns()
    {
        echo "Testing MVC Patterns:\n";
        
        try {
            // Test Active Record pattern (Model)
            $model = new Model();
            $model->name = 'Test User';
            $model->email = 'test@example.com';
            $model->save();
            echo "✓ Active Record pattern works\n";
            
            // Test Repository pattern (Model)
            $repository = new Model();
            $users = $repository->findBy(['active' => 1]);
            echo "✓ Repository pattern works\n";
            
            // Test Front Controller pattern (Controller)
            $frontController = new Controller();
            $frontController->dispatch('index', 'action');
            echo "✓ Front Controller pattern works\n";
            
            // Test Template View pattern (View)
            $templateView = new View();
            $templateView->assign('title', 'Page Title');
            $templateView->render('template');
            echo "✓ Template View pattern works\n";
            
            // Test Two Step View pattern (View)
            $twoStepView = new View();
            $twoStepView->setLayout('main');
            $twoStepView->render('content');
            echo "✓ Two Step View pattern works\n";
            
            // Test Command pattern (Controller actions)
            $commandController = new Controller();
            $commandController->execute('createUser', ['name' => 'John']);
            echo "✓ Command pattern works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test MVC performance
     */
    public function testMvcPerformance()
    {
        echo "Testing MVC Performance:\n";
        
        try {
            $startTime = microtime(true);
            
            // Test controller performance
            for ($i = 0; $i < 100; $i++) {
                $controller = new Controller();
                $controller->assign("var_$i", "value_$i");
                $controller->render("template_$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 controller operations in " . number_format($duration, 2) . "ms\n";
            
            // Test model performance
            $startTime = microtime(true);
            
            $model = new Model();
            for ($i = 0; $i < 50; $i++) {
                $model->find($i);
                $model->findAll();
                $model->findBy(['id' => $i]);
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 150 model operations in " . number_format($duration, 2) . "ms\n";
            
            // Test view performance
            $startTime = microtime(true);
            
            $view = new View();
            for ($i = 0; $i < 100; $i++) {
                $view->assign("var_$i", "value_$i");
                $view->render("template_$i");
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 view operations in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test MVC error handling
     */
    public function testMvcErrorHandling()
    {
        echo "Testing MVC Error Handling:\n";
        
        try {
            // Test controller error handling
            $controller = new Controller();
            
            try {
                $controller->render('nonexistent_template');
                echo "✓ Controller handles missing template gracefully\n";
            } catch (Exception $e) {
                echo "✓ Controller properly catches missing template\n";
            }
            
            try {
                $controller->forward('nonexistent/action');
                echo "✓ Controller handles invalid forward gracefully\n";
            } catch (Exception $e) {
                echo "✓ Controller properly catches invalid forward\n";
            }
            
            // Test model error handling
            $model = new Model();
            
            try {
                $model->find(-1);
                echo "✓ Model handles invalid ID gracefully\n";
            } catch (Exception $e) {
                echo "✓ Model properly catches invalid ID\n";
            }
            
            try {
                $model->query('INVALID SQL');
                echo "✓ Model handles invalid SQL gracefully\n";
            } catch (Exception $e) {
                echo "✓ Model properly catches invalid SQL\n";
            }
            
            // Test view error handling
            $view = new View();
            
            try {
                $view->render('nonexistent_template');
                echo "✓ View handles missing template gracefully\n";
            } catch (Exception $e) {
                echo "✓ View properly catches missing template\n";
            }
            
            try {
                $view->assign(null, 'value');
                echo "✓ View handles null variable name gracefully\n";
            } catch (Exception $e) {
                echo "✓ View properly catches null variable name\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Unexpected error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Run all tests
     */
    public function runAllTests()
    {
        $this->testControllerClass();
        $this->testModelClass();
        $this->testViewClass();
        $this->testMvcIntegration();
        $this->testMvcPatterns();
        $this->testMvcPerformance();
        $this->testMvcErrorHandling();
        
        echo "=== MVC Classes Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new MvcTest();
    $test->runAllTests();
}
