<?php
date_default_timezone_set('PRC');
header("Content-type: text/html; charset=UTF-8");
define('APP_ROOT', __dir__ . '/app/');

$path = '';
if (isset($_SERVER['argv'][1])) {
    $path = $_SERVER['argv'][1];
} else {
    exit('This script is run as CLI');
}

$app = new Gene\Application();
$app
	->autoload(APP_ROOT)
	->load("router.ini.php")
	->load("config.ini.php")
	->run('get', $path);
    
