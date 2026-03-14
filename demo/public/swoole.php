<?php
date_default_timezone_set("Asia/Shanghai");
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');
define('SWOOLE_HOST', getenv('GENE_SWOOLE_HOST') ?: '0.0.0.0');
define('SWOOLE_PORT', (int) (getenv('GENE_SWOOLE_PORT') ?: 9501));

if (!extension_loaded('swoole') && !extension_loaded('openswoole')) {
    fwrite(STDERR, "Swoole or OpenSwoole extension is required.\n");
    exit(1);
}

if (class_exists('\Swoole\Runtime')) {
    \Swoole\Runtime::enableCoroutine(SWOOLE_HOOK_ALL & ~SWOOLE_HOOK_FILE);
}

$serverClass = class_exists('\Swoole\Http\Server') ? '\Swoole\Http\Server' : '\OpenSwoole\Http\Server';
$http = new $serverClass(SWOOLE_HOST, SWOOLE_PORT, SWOOLE_PROCESS);

$http->set([
    'worker_num'             => function_exists('swoole_cpu_num') ? swoole_cpu_num() : 1,
    'max_request'            => 10000,
    'dispatch_mode'          => 2,
    'enable_static_handler'  => true,
    'document_root'          => dirname(__dir__) . "/public/",
]);

$http->on("start", function ($server) {
    echo "Gene Swoole server started at http://" . SWOOLE_HOST . ":" . SWOOLE_PORT . "\n";
});

$http->on("workerStart", function ($server, $workerId) {
    \Gene\Application::setRuntimeType('swoole');

    $app = new \Gene\Application();
    $app->autoload(APP_ROOT)
        ->load("router.ini.php", CONF_DIR)
        ->load("config.ini.php", CONF_DIR)
        ->setMode(1, 1);

    \Gene\Application::destroyContext();
});

$http->on("request", function ($request, $response) {
    \Gene\Request::init(
        $request->get, $request->post, $request->cookie,
        $request->server, null, $request->files
    );

    \Gene\Application::setResponse($response);
    $method = strtolower($request->server['request_method'] ?? 'get');
    $uri    = $request->server['request_uri'] ?? '/';

    ob_start();
    $error = null;
    try {
        \Gene\Application::getInstance()->run($method, $uri);
    } catch (\Throwable $e) {
        $error = $e;
    }
    $out = ob_get_clean();

    \Gene\Application::clearState();
    \Gene\Application::destroyContext();

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