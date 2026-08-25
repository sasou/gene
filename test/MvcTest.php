<?php

/**
 * Gene Framework MVC Classes Test
 *
 * Covers the public Gene 6.1 Controller, Model, and View APIs.
 */

use Gene\Controller;
use Gene\Di;
use Gene\Model;
use Gene\Request;
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

    public function testControllerClass()
    {
        echo "Testing Controller Class:\n";
        try {
            Request::init(['id' => '7'], ['name' => 'gene'], [], [], [], []);
            $this->controller = new Controller();
            if ($this->controller->get('id') !== '7' || $this->controller->post('name') !== 'gene') {
                throw new RuntimeException('Controller request accessors returned unexpected values');
            }
            echo "✓ Controller request accessors work\n";

            $success = $this->controller->success('ok');
            $error = $this->controller->error('bad');
            $data = $this->controller->data(['id' => 7], 1, 'ok');
            if (($success['code'] ?? null) !== 2000 || ($error['code'] ?? null) !== 4000 || ($data['count'] ?? null) !== 1) {
                throw new RuntimeException('Controller response helpers returned unexpected payloads');
            }
            echo "✓ Controller response helpers work\n";

            $this->controller->assign('title', 'Gene');
            if ($this->controller->url('docs', '') !== '/docs') {
                throw new RuntimeException('Controller URL helper returned unexpected path');
            }
            echo "✓ Controller view and URL helpers work\n";

            foreach (['isGet', 'isPost', 'isPut', 'isDelete', 'isHead', 'isOptions', 'isAjax', 'isCli'] as $method) {
                if (!is_bool($this->controller->{$method}())) {
                    throw new RuntimeException("Controller::$method must return bool");
                }
            }
            echo "✓ Controller method predicates return bool\n";
        } catch (Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        echo "\n";
    }

    public function testModelClass()
    {
        echo "Testing Model Class:\n";
        try {
            $this->model = new Model();
            $this->model->value = 'stored';
            if ($this->model->value !== 'stored') {
                throw new RuntimeException('Model magic property round-trip failed');
            }
            echo "✓ Model magic properties work\n";

            $success = $this->model->success('ok');
            $error = $this->model->error('bad');
            $data = $this->model->data(['value' => 1], 1, 'ok');
            if (($success['code'] ?? null) !== 2000 || ($error['code'] ?? null) !== 4000 || ($data['count'] ?? null) !== 1) {
                throw new RuntimeException('Model response helpers returned unexpected payloads');
            }
            echo "✓ Model response helpers work\n";

            if (Model::getInstance() !== Model::getInstance()) {
                throw new RuntimeException('Model::getInstance did not reuse the request instance');
            }
            echo "✓ Model request-scoped instance works\n";
        } catch (Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        echo "\n";
    }

    public function testViewClass()
    {
        echo "Testing View Class:\n";
        try {
            $this->view = new View();
            if ($this->view->assign('title', 'Gene') !== null) {
                throw new RuntimeException('View::assign returned an unexpected value');
            }
            echo "✓ View assign works\n";

            if ($this->view->url('guide', '') !== '/guide') {
                throw new RuntimeException('View URL helper returned unexpected path');
            }
            if ($this->view->getPath() !== null || $this->view->getRouterUri() !== null) {
                throw new RuntimeException('Unset view request paths must be null');
            }
            echo "✓ View request helpers work\n";

            if ($this->view->clearAssign() !== true) {
                throw new RuntimeException('View::clearAssign returned false');
            }
            echo "✓ View clearAssign works\n";
        } catch (Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        echo "\n";
    }

    public function testMvcIntegration()
    {
        echo "Testing MVC Integration:\n";
        try {
            $dependency = (object)['name' => 'mvc'];
            Di::set('mvcDependency', $dependency);
            $controller = new Controller();
            $model = new Model();
            $view = new View();
            if ($controller->mvcDependency !== $dependency || $model->mvcDependency !== $dependency) {
                throw new RuntimeException('Controller/Model DI resolution failed');
            }
            $view->dependency = $controller->mvcDependency;
            if ($view->dependency !== $dependency) {
                throw new RuntimeException('MVC DI data flow failed');
            }
            echo "✓ Controller and Model resolve request-scoped DI\n";
            echo "✓ Controller to View data flow works\n";
        } catch (Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        echo "\n";
    }

    public function testMvcPerformance()
    {
        echo "Testing MVC Performance:\n";
        try {
            $controller = new Controller();
            $model = new Model();
            $view = new View();
            $start = hrtime(true);
            for ($i = 0; $i < 1000; $i++) {
                $controller->success('ok');
                $model->data($i, 1, 'ok');
                $view->assign('value', $i);
            }
            $duration = (hrtime(true) - $start) / 1000000;
            echo "✓ 3000 MVC helper operations in " . number_format($duration, 2) . "ms\n";
        } catch (Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        echo "\n";
    }

    public function testMvcErrorHandling()
    {
        echo "Testing MVC Error Handling:\n";
        try {
            $warnings = [];
            set_error_handler(function ($type, $message) use (&$warnings) {
                $warnings[] = $message;
                return true;
            });
            try {
                $result = (new View())->render('../forbidden');
            } finally {
                restore_error_handler();
            }
            if ($result !== '' || $warnings === []) {
                throw new RuntimeException('View traversal rejection did not fail safely');
            }
            echo "✓ View rejects traversal paths without throwing\n";
        } catch (Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        echo "\n";
    }

    public function runAllTests()
    {
        $this->testControllerClass();
        $this->testModelClass();
        $this->testViewClass();
        $this->testMvcIntegration();
        $this->testMvcPerformance();
        $this->testMvcErrorHandling();
        echo "=== MVC Test Suite Complete ===\n";
    }
}

if (basename(__FILE__) === basename($_SERVER['SCRIPT_FILENAME'] ?? '')) {
    (new MvcTest())->runAllTests();
}
