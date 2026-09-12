<?php
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');

$app = \Gene\Application::getInstance();
// bootstrap 收口 autoload → load(router) → load(config) → setMode；
// config 名支持 {env} 占位符（本 demo 只有 config.ini.php，用固定名）。
// debug_envs 命中才注册异常处理器：旧写法 setMode(1,1) 恒开，
// 等价语义为非 prod 环境全列（扩展内置 env：dev/test/gray/prod）。
// requestId/webscan/run 属应用策略，按计划保持显式配置。
$app->bootstrap(APP_ROOT, CONF_DIR, [
        'router'     => 'router.ini.php',
        'config'     => 'config.ini.php',
        'mode'       => 1,
        'debug_envs' => ['dev', 'test', 'gray'],
    ])
    ->requestId(['header' => 'X-Request-Id', 'bytes' => 8, 'trust' => true, 'max_length' => 128])
    ->webscan(1, 'admin', function () {
        if (\Gene\Request::isAjax()) {
            return json_encode(\Gene\Response::error("Illegal access"));
        }
        return "Illegal access";
    })
    ->run();