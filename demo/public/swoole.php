<?php
date_default_timezone_set("Asia/Shanghai");
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');
define('WWW_ROOT', dirname(__dir__) . '/public');


\Swoole\Runtime::enableCoroutine(SWOOLE_HOOK_ALL);
$http = new \Swoole\Http\Server("0.0.0.0", 80, SWOOLE_PROCESS);

$http->set([
    'reactor_num'            => 4,
    'worker_num'             => 4,
    'max_request'            => 10000,
    'dispatch_mode'          => 2,
    'enable_static_handler'  => true,
    'document_root'          => WWW_ROOT
]);

$http->on("start", function ($server) {
    echo "Gene Swoole server started at http://0.0.0.0:80\n";
});

$http->on("workerStart", function ($server, $workerId) {
    \Gene\Application::getInstance()->autoload(APP_ROOT)
        ->load("router.ini.php", CONF_DIR)
        ->load("config.ini.php", CONF_DIR)
        ->setMode(1, 1)
        ->setRuntimeType('swoole');

    // 创建数据库连接池（每个Worker进程独立）
    // 第二个参数 'db' 对应 config.ini.php 中 $config->set("db", ...) 的配置key，自动从持久化配置缓存中读取 dsn/username/password
    // 第三个参数可选，不传则使用默认连接池参数。
    \Gene\Pool::create('dbPool', 'db', [
        'min'         => 2,    // 最小连接数（默认1）
        'max'         => 10,   // 最大连接数（默认10）
        'idleTimeout' => 60,   // 空闲超时（秒），超时回收，保持最小连接数（默认60）
        'waitTimeout' => 1.5,  // 获取连接等待超时（秒）（默认3.0）
    ]);
});

$http->on("workerExit", function ($server, $workerId) {
    // 清除连接池定时器，让事件循环可以正常退出
    \Gene\Pool::stopTimers();
});

$http->on("workerStop", function ($server, $workerId) {
    // 关闭所有连接池，释放资源
    \Gene\Pool::closeAll();
});

$http->on("request", function ($request, $response) {
    \Gene\Request::init(
        $request->get, $request->post, $request->cookie,
        $request->server, null, $request->files
    );
    \Gene\Application::setResponse($response);

    ob_start();
    $error = null;
    try {
        \Gene\Application::getInstance()->run();
    } catch (\Throwable $e) {
        $error = $e;
    }
    $out = ob_get_clean();

    \Gene\Application::cleanup();

    if ($error) {
        $response->status(500);
        $response->end("Internal Server Error: " . $error->getMessage());
        return;
    }

    if (!$response->isWritable()) {
        return;
    }
    $response->header('Content-Type', 'text/html; charset=UTF-8');
    $response->end($out);
});

$http->start();