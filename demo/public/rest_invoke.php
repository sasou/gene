<?php
/**
 * CLI: php rest_invoke.php
 * Local Rest::call → Api\Ping::pong (no Redis, no HTTP if class exists).
 */
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');

$app = \Gene\Application::getInstance();
$app->autoload(APP_ROOT)->load('config.ini.php', CONF_DIR)->setMode(1, 1);

\Gene\Request::init([], ['from' => 'cli'], [], [], [], []);
$rest = new \Gene\Rest([
    'services' => [
        'demo' => [
            'base_url' => 'http://127.0.0.1:8081',
            'local' => 'Api\\',
        ],
    ],
]);
$result = $rest->use('demo')->call('Api\\Ping', 'pong', ['name' => 'demo']);
var_export($result);
echo PHP_EOL;
