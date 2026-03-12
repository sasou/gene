<?php
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');

$app = \Gene\Application::getInstance();
$app
    ->autoload(APP_ROOT)
    ->load("router.ini.php", CONF_DIR)
    ->load("config.ini.php", CONF_DIR)
    ->setMode(1, 1)
    ->setRuntimeType(1)
    ->webscan(1, 'cms', function () {
        if (\Gene\Request::isAjax()) {
            return json_encode(\Gene\Response::error("Illegal access"));
        }
        return "Illegal access";
    })
    ->run();