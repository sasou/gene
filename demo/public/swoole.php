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

$http->on("workerStart", function ($server, $workerId) {
    \Gene\Application::getInstance()
        ->autoload(APP_ROOT)
        ->load("router.ini.php", CONF_DIR)
        ->load("config.ini.php", CONF_DIR)
        ->setMode(1, 1);

    // 创建数据库连接池（每个Worker进程独立）
    // 第二个参数 'db' 对应 config.ini.php 中 $config->set("db", ...) 的配置key，自动从持久化配置缓存中读取 dsn/username/password
    // 第三个参数可选，不传则使用默认连接池参数（v5.4.3起默认max=64）
    \Gene\Pool::create('dbPool', 'db');

    // 创建Redis连接池（每个Worker进程独立）
    // 第二个参数 'redis' 对应 config.ini.php 中 $config->set("redis", ...) 的配置key，自动从持久化配置缓存中读取连接参数
    // 第三个参数可选，不传则使用默认连接池参数（v5.4.3起默认max=64）
    \Gene\Cache\RedisPool::create('redisPool', 'redis');

    // 标记Worker已就绪，request 回调开头会先阻塞等待此标记
    \Gene\Application::getInstance()->workerReady();
});

$http->on("workerExit", function ($server, $workerId) {
    // 清除连接池定时器，让事件循环可以正常退出
    \Gene\Pool::stopTimers();
    \Gene\Cache\RedisPool::stopTimers();
});

$http->on("workerStop", function ($server, $workerId) {
    // 关闭所有连接池，释放资源
    \Gene\Pool::closeAll();
    \Gene\Cache\RedisPool::closeAll();
    gc_collect_cycles();
});

$http->on("request", function ($request, $response) {
    // 先等待 workerStart 初始化完成，避免首批请求过早进入 run() 导致异常
    \Gene\Application::waitWorkerReady();
    \Gene\Request::init($request->get, $request->post, $request->cookie, $request->server, null, $request->files, null, $request->header,  $request->rawContent());
    \Gene\Application::setResponse($response);

    ob_start();
    $error = false;
    try {
        \Gene\Application::getInstance()->run();
    } catch (\Throwable $e) {
        $error = true;
        \Gene\Log::exception($e);
    } finally {
        $out = ob_get_clean();
        \Gene\Application::cleanup(true);
    }

    if ($error) {
        $response->redirect('/50x.html');
        return;
    }

    if (!$response->isWritable()) {
        return;
    }
    $response->header('Content-Type', 'text/html; charset=utf-8');
    $response->end($out);
});

$http->start();