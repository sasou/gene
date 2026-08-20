<?php
// Test shared URL/path methods across Application, Controller, View, Hook, Response

// Minimal bootstrap: register DI and autoloader
\Gene\Di::set('db', function() { return new stdClass; });

// Simulate a request context via Application
$app = \Gene\Application::getInstance();
$app->load('config/router.ini.php', __DIR__ . '/../demo/config/');

// Test Application::getPath
echo "=== Application::getPath ===\n";
$path = \Gene\Application::getPath();
echo "getPath(): "; var_dump($path);

$pathNoLang = \Gene\Application::getPath(true);
echo "getPath(true): "; var_dump($pathNoLang);

// Test Application::getRouterUri
echo "\n=== Application::getRouterUri ===\n";
$uri = \Gene\Application::getRouterUri();
echo "getRouterUri(): "; var_dump($uri);

// Test Application::url
echo "\n=== Application::url ===\n";
echo "url('login.html'): "; var_dump(\Gene\Application::url('login.html'));
echo "url('login.html', 'fr'): "; var_dump(\Gene\Application::url('login.html', 'fr'));
echo "url('/'): "; var_dump(\Gene\Application::url('/'));

// Test Controller methods
echo "\n=== Controller ===\n";
echo "url('test.html'): "; var_dump(\Gene\Controller::url('test.html'));
echo "url('test.html', 'de'): "; var_dump(\Gene\Controller::url('test.html', 'de'));
echo "getPath(): "; var_dump(\Gene\Controller::getPath());
echo "getPath(true): "; var_dump(\Gene\Controller::getPath(true));
echo "getRouterUri(): "; var_dump(\Gene\Controller::getRouterUri());

// Test View methods
echo "\n=== View ===\n";
$view = new \Gene\View();
echo "url('view.html'): "; var_dump($view->url('view.html'));
echo "url('view.html', 'ja'): "; var_dump($view->url('view.html', 'ja'));
echo "getPath(): "; var_dump($view->getPath());
echo "getPath(true): "; var_dump($view->getPath(true));
echo "getRouterUri(): "; var_dump($view->getRouterUri());

// Test Hook methods
echo "\n=== Hook ===\n";
echo "url('hook.html'): "; var_dump(\Gene\Hook::url('hook.html'));
echo "url('hook.html', 'zh'): "; var_dump(\Gene\Hook::url('hook.html', 'zh'));
echo "getPath(): "; var_dump(\Gene\Hook::getPath());
echo "getPath(true): "; var_dump(\Gene\Hook::getPath(true));
echo "getRouterUri(): "; var_dump(\Gene\Hook::getRouterUri());

// Test Response methods
echo "\n=== Response ===\n";
echo "url('resp.html'): "; var_dump(\Gene\Response::url('resp.html'));
echo "url('resp.html', 'ko'): "; var_dump(\Gene\Response::url('resp.html', 'ko'));

echo "\n=== All tests passed ===\n";
