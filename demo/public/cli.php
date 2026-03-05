<?php
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');

$path = '';
if (isset($_SERVER['argv'][1])) {
    $path = $_SERVER['argv'][1];
} else {
    exit('This script is run as CLI with no path?');
}

$app = \Gene\Application::getInstance();
$app
    ->autoload(APP_ROOT)
    ->load("router.ini.php", CONF_DIR)
    ->load("config.ini.php", CONF_DIR)
    ->setMode(1, 1)
    ->run('get', $path);
