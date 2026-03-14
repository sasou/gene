<?php
date_default_timezone_set("Asia/Shanghai");
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');

if (class_exists('\Swoole\Runtime')) {
    \Swoole\Runtime::enableCoroutine(SWOOLE_HOOK_ALL & ~SWOOLE_HOOK_FILE);
}

$http = new \Swoole\Http\Server("0.0.0.0", 80, SWOOLE_PROCESS);

$http->set([
    'worker_num'             => swoole_cpu_num(),
    'max_request'            => 10000,
    'dispatch_mode'          => 2,
    'enable_static_handler'  => true,
    'document_root'          => dirname(__dir__) . "/public/",
]);

$http->on("start", function ($server) {
    echo "Gene Swoole server started at http://0.0.0.0:80\n";
});

$http->on("workerStart", function ($server, $workerId) {
    \Gene\Application::setRuntimeType('swoole');

    $app = new \Gene\Application();
    $app->autoload(APP_ROOT)
        ->load("router.ini.php", CONF_DIR)
        ->load("config.ini.php", CONF_DIR)
        ->setMode(1, 1);
});

$http->on("request", function ($request, $response) {
    \Gene\Request::init(
        $request->get, $request->post, $request->cookie,
        $request->server, null, $request->files
    );

    \Gene\Application::setResponse($response);

    $method = strtolower($request->server['REQUEST_METHOD'] ?? $request->server['request_method'] ?? 'get');
    $uri    = $request->server['REQUEST_URI'] ?? $request->server['request_uri'] ?? '/';

    ob_start();
    try {
        \Gene\Application::getInstance()->run($method, $uri);
    } catch (\Throwable $e) {
        ob_end_clean();
        $response->status(500);
        $response->end("Internal Server Error: " . $e->getMessage());
        return;
    } finally {
        \Gene\Application::clearState();
        \Gene\Application::destroyContext();
    }
    $out = ob_get_clean();

    if (!$response->isWritable()) {
        return;
    }
    $response->header('Content-Type', 'text/html; charset=UTF-8');
    $response->end($out);
});

$http->start();