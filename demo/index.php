<?php
date_default_timezone_set('PRC');
header("Content-type: text/html; charset=UTF-8");
define('APP_ROOT', __dir__ . '/app/');

function geneHandler($e) {
	\Gene\Router::displayExt('error');
}

$app = new Gene\Application();
$app
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->setMode(1,1)
    ->run();
