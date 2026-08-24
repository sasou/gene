<?php
// 模拟 swoole worker：workerStart 注册路由 → 连续多次请求（Request::init + run + cleanup）
define('APP_ROOT', 'F:/codeup/gene_web/application');
define('CONF_DIR', 'F:/codeup/gene_web/config');

\Gene\Application::setRuntimeType('swoole');
$app = \Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php', CONF_DIR)
    ->setMode(1, 0);
$app->workerReady();

$urls = ['/', '/test.html', '/favicon.ico', '/doc.html', '/en/doc.html', '/en/test.html', '/favicon.ico', '/doc.html'];

foreach ($urls as $i => $u) {
    \Gene\Request::init(null, null, null, [
        'request_method' => 'GET',
        'request_uri'    => $u,
    ]);
    ob_start();
    $err = null;
    set_error_handler(function ($no, $msg) use (&$err) { $err = $msg; return true; });
    try {
        $app->run();
    } catch (\Throwable $e) {
        $err = 'EX:' . $e->getMessage();
    }
    restore_error_handler();
    ob_get_clean();
    \Gene\Application::cleanup(true);
    printf("#%d %-16s => %s\n", $i, $u, $err === null ? 'OK' : $err);
}
