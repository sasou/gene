<?php
/**
 * CLI: php rest_invoke.php
 * Local Rest::call → Api\Ping::pong (no Redis, no HTTP if class exists).
 */
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');

$app = \Gene\Application::getInstance();
// debug_envs 命中才注册异常处理器（内置 env：dev/test/gray/prod），非 prod 全开。
$app->bootstrap(APP_ROOT, CONF_DIR, ['config' => 'config.ini.php', 'mode' => 1, 'debug_envs' => ['dev', 'test', 'gray']]);

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
