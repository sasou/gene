<?php
define('APP_ROOT', 'F:/codeup/gene_web/application');
define('CONF_DIR', 'F:/codeup/gene_web/config');
require __DIR__ . '/swoole_route_probe.php';

\Gene\Application::setRuntimeType('swoole');
$app = \Gene\Application::getInstance()
    ->autoload(APP_ROOT)
    ->load('router.ini.php', CONF_DIR)
    ->setMode(1, 0);

gene_probe('A-after-load');
$app->workerReady();
gene_probe('B-after-workerReady');

\Gene\Request::init(null, null, null, ['request_method' => 'GET', 'request_uri' => '/doc.html']);
gene_probe('C-req1-before');
ob_start();
try { $app->run(); } catch (\Throwable $e) {}
ob_get_clean();
\Gene\Application::cleanup(true);
gene_probe('C-req1-after');
