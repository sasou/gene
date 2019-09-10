<?php
header("Content-type: text/html; charset=UTF-8");
define('APP_ROOT', dirname(__dir__) . '/application');

$http = new swoole_http_server("192.168.27.101", 9501);

//配置
$http->set([
    'enable_static_handler'=>true,
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
    ->run($type, $url);
    
    $out = ob_get_contents();
    ob_end_clean();
    $out && $response->write($out);
});

$http->start();