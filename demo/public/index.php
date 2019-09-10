<?php
define('APP_ROOT', dirname(__dir__) . '/application');

function geneHandler($e)
{
    \Gene\Router::display('error');
}

$app = \Gene\Application::getInstance();
$app
    ->autoload(APP_ROOT)
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->setMode(1, 1)
    ->run();
