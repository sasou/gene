<?php
header("Content-type: text/html; charset=UTF-8");
define('APP_ROOT', dirname(__dir__) . '/application');

if (class_exists('\Swoole\Runtime') && (getenv("GENE_ENABLE_COROUTINE_HOOK") ?: "1") === "1") {
    \Swoole\Runtime::enableCoroutine(true);
}

$http = new swoole_http_server("192.168.27.101", 9501, SWOOLE_PROCESS);

//配置
$http->set([
    'worker_num' => 1, 
    'max_request' => 10000,
    'dispatch_mode'=> 2,
    'enable_static_handler' => true,
    'document_root'=> dirname(__dir__) . "/public/",
]);

$http->on("start", function ($server) {
    echo "Swoole http server is started at http://192.168.27.101:9501\n";
});

$http->on("request", function ($request, $response) {
    $type = $request->server['request_method'] ?? "get";
    $url = $request->server['request_uri'] ?? "/";
    \Gene\Request::init($request->get, $request->post, $request->cookie, $request->server, null, $request->files);
    \Gene\Di::set("response", $response);
    
    ob_start();
    $app = \Gene\Application::getInstance();
    $app
    ->autoload(APP_ROOT)
    ->load("router.ini.php")
    ->load("config.ini.php")
    ->setRuntimeType(2)
    ->run($type, $url);
    
    $out = ob_get_contents();
    ob_end_clean();
    $out && $response->end($out);
});

$http->start();