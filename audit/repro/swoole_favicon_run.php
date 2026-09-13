<?php
define('APP_ROOT', dirname(__DIR__, 2) . '/demo/application');
define('CONF_DIR', dirname(__DIR__, 2) . '/demo/config');
\Gene\Application::setRuntimeType('swoole');
$a = \Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php', CONF_DIR)
    ->setMode(1, 0);
$a->workerReady();
\Gene\Request::init(null, null, null, [
    'request_method' => 'GET',
    'request_uri' => '/favicon.ico',
]);
try {
    $a->run();
    echo "OK favicon\n";
} catch (Throwable $e) {
    echo 'EX: ' . $e->getMessage() . "\n";
}
