<?php
define('APP_ROOT', dirname(__dir__) . '/application');

$path = '';
if (isset($_SERVER['argv'][1])) {
    $path = $_SERVER['argv'][1];
} else {
    exit('This script is run as CLI with no path?');
}

function doException($e) {
    echo $e->getMessage();
}

$app = \Gene\Application::getInstance();
$app
    ->autoload(APP_ROOT)
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->setMode(1, 1, "doException")
    ->run('get', $path);
