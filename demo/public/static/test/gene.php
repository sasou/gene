框架入口：
<?php
define('APP_ROOT', __dir__ . '/app/');

$app = \Gene\Application::getInstance();
$app
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->run();

路由定义：
<?php
$router = new \Gene\Router();
$router->clear()
       ->get("/test","\Controllers\Index@test");

控制器定义:
<?php
namespace Controllers;
class Index extends \Gene\Controller 
{
	function test()
	{
		echo 'test';
	}  
}