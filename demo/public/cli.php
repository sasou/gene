<?php
define('APP_ROOT', dirname(__dir__) . '/application');

function geneHandler($e)
{
    \Gene\Router::display('error');
}

$path = '';
if (isset($_SERVER['argv'][1])) {
    $path = $_SERVER['argv'][1];
} else {
    exit('This script is run as CLI with no path?');
}

$app = \Gene\Application::getInstance();
$app
    ->autoload(APP_ROOT)
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->setMode(1, 1)
    ->run('get', $path);
