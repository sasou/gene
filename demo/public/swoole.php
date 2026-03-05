<?php
header("Content-type: text/html; charset=UTF-8");
define('APP_ROOT', dirname(__dir__) . '/application');

// 启用 Swoole 协程 Hook（SWOOLE_HOOK_ALL），将 PDO、Redis、Memcached 等原生阻塞
// 扩展替换为协程友好实现：I/O 等待时自动挂起当前协程，不阻塞 worker 进程。
// 环境变量 GENE_ENABLE_COROUTINE_HOOK=0 可单独关闭（用于调试或不需要协程的场景）。
// Gene 框架的 DI 层（runtime_type>=2）已配合此机制做了以下适配：
//   - instance:false 组件（如 db）每次请求得到新对象，隔离链式查询构建状态
//   - instance:true  组件（如 redis/memcache）作为 worker 级单例，Hook 保证协程安全
//   - Gene\Db\Mysql 在 runtime_type>=2 时自动注入 PDO::ATTR_PERSISTENT，复用 TCP 连接
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