<?php
date_default_timezone_set("Asia/Shanghai");
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');
define('WWW_ROOT', dirname(__dir__) . '/public');

\Gene\Application::setRuntimeType('swoole');

\Swoole\Runtime::enableCoroutine(SWOOLE_HOOK_ALL);

$http = new \Swoole\Http\Server("0.0.0.0", 80, SWOOLE_PROCESS);

$http->set([
    'reactor_num'            => 1,
    'worker_num'             => 2,
    'max_request'            => 10000,
    'dispatch_mode'          => 2,
    'enable_static_handler'  => true,
    'document_root'          => WWW_ROOT
]);

$http->on("start", function ($server) {
    echo "Gene Swoole server started at http://0.0.0.0:80\n";
});

$app = \Gene\Application::getInstance();

$http->on("workerStart", function ($server, $workerId) use ($app) {
    // 共享装载收口：autoload → load(router) → load(config) → setMode
    // （mode=1 注册错误处理器；debug_envs 命中才注册异常处理器，
    // 旧写法 setMode(1,1) 恒开 → 等价语义为非 prod 环境全列，
    // 内置 env：dev/test/gray/prod）。
    // config 名支持 {env} 占位符展开为 getEnvironmentName()，本 demo 用固定文件。
    $app->bootstrap(APP_ROOT, CONF_DIR, [
        'router'     => 'router.ini.php',
        'config'     => 'config.ini.php',
        'mode'       => 1,
        'debug_envs' => ['dev', 'test', 'gray'],
    ]);

    // 显式声明连接池（driver 仅 db/redis；component 为 config.ini.php
    // 中 $config->set(...) 的键名；params 可选池参数 min/max/idleTimeout/
    // waitTimeout，不传则默认 max=64）。FPM 下 startPools 明确返回 false。
    $app->pools([
        'dbPool'    => ['driver' => 'db',    'component' => 'db'],
        'redisPool' => ['driver' => 'redis', 'component' => 'redis'],
    ]);
    $app->startPools();

    // 标记Worker已就绪，handleSwoole 入口会先阻塞等待此标记
    $app->workerReady();
});

$http->on("workerExit", function ($server, $workerId) use ($app) {
    // 清除已声明池的定时器，让事件循环可以正常退出（幂等）
    $app->stopPoolTimers();
});

$http->on("workerStop", function ($server, $workerId) use ($app) {
    // 关闭已声明池，释放资源（幂等，可安全重入）
    $app->closePools();
    gc_collect_cycles();
});

$http->on("request", function ($request, $response) use ($app) {
    // 一行收口：waitWorkerReady → Request::initSwoole → setResponse → run()
    // → Throwable 边界（Log::exception + 最小 500）→ 未结束则 end(输出)
    // → cleanup。不覆盖业务 status/header/Content-Type，不二次 end。
    $app->handleSwoole($request, $response);
    // 可选 options：['cleanup_gc' => false, 'catch' => function (\Throwable $e, $request, $response) { ... }]
});

$http->start();