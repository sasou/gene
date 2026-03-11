<?php
define('APP_ROOT', dirname(__dir__) . '/application');
define('CONF_DIR', dirname(__dir__) . '/config');

// 启用 Swoole 协程 Hook（SWOOLE_HOOK_ALL），将 PDO、Redis、Memcached 等原生阻塞
// 扩展替换为协程友好实现：I/O 等待时自动挂起当前协程，不阻塞 worker 进程。
// Gene 框架的 DI 层（runtime_type>=2）已配合此机制做了以下适配：
//   - instance:false 组件（如 db）每次请求得到新对象，隔离链式查询构建状态
//   - instance:true  组件（如 redis/memcache）作为 worker 级单例，Hook 保证协程安全
//   - Gene\Db\Mysql 在 runtime_type>=2 时自动注入 PDO::ATTR_PERSISTENT，复用 TCP 连接
if (class_exists('\Swoole\Runtime')) {
    \Swoole\Runtime::enableCoroutine(SWOOLE_HOOK_ALL);
}

$http = new \Swoole\Http\Server("0.0.0.0", 9501, SWOOLE_PROCESS);

$http->set([
    'worker_num'             => swoole_cpu_num(),
    'max_request'            => 10000,
    'dispatch_mode'          => 2,
    'enable_static_handler'  => true,
    'document_root'          => dirname(__dir__) . "/public/",
]);

$http->on("start", function ($server) {
    echo "Gene Swoole server started at http://0.0.0.0:9501\n";
});

// ── worker 启动：一次性初始化（每个 worker 进程执行一次） ──
$http->on("workerStart", function ($server, $workerId) {
    // 设置运行模式（仅需一次），加载路由和配置（持久缓存，跨请求复用）
    \Gene\Application::setRuntimeType('swoole');

    $app = new \Gene\Application();
    $app->autoload(APP_ROOT)
        ->load("router.ini.php", CONF_DIR)
        ->load("config.ini.php", CONF_DIR)
        ->setMode(1, 1);
});

// ── 请求处理 ──
$http->on("request", function (\Swoole\Http\Request $request, \Swoole\Http\Response $response) {
    // 1. 清理上一次请求的全局状态（method/path/module/controller/action 等）
    \Gene\Application::clearState();

    // 2. 注入本次请求数据
    \Gene\Request::init(
        $request->get, $request->post, $request->cookie,
        $request->server, null, $request->files
    );

    // 3. 将 Swoole Response 存入 DI，业务层可通过 $this->response 使用
    \Gene\Di::set("response", $response);

    // 4. 捕获输出并发送响应
    $method = strtolower($request->server['request_method'] ?? 'get');
    $uri    = $request->server['request_uri'] ?? '/';

    ob_start();
    try {
        \Gene\Application::getInstance()->run($method, $uri);
    } catch (\Throwable $e) {
        ob_end_clean();
        $response->status(500);
        $response->end("Internal Server Error: " . $e->getMessage());
        return;
    }
    $out = ob_get_clean();

    if (!$response->isWritable()) {
        return;
    }
    $response->header('Content-Type', 'text/html; charset=UTF-8');
    $response->end($out);
});

$http->start();