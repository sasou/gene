<?php
// Gene 框架版性能测试示例：与左侧原生 test.php 对比，
// 框架路径为 index.php 入口 → config/router 装载 → 控制器方法。

// public/index.php
$app = \Gene\Application::getInstance();
$app->bootstrap(APP_ROOT, CONF_DIR, [
        'router' => 'router.ini.php',
        'config' => 'config.ini.php',
    ])
    ->run();

// config/router.ini.php
$router->get("/test", "\Controllers\Index@test");

// application/Controllers/Index.php
class Index extends \Gene\Controller
{
    public function test()
    {
        echo 'test';
    }
}
