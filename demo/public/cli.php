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
// debug_envs 命中才注册异常处理器：旧写法 setMode(1,1) 恒开，
// 等价语义为非 prod 环境全列（内置 env：dev/test/gray/prod）。
$app->bootstrap(APP_ROOT, CONF_DIR, [
        'router'     => 'router.ini.php',
        'config'     => 'config.ini.php',
        'mode'       => 1,
        'debug_envs' => ['dev', 'test', 'gray'],
    ])
    ->run('get', $path);
