<?php
// 验证 url() 三种调用方式
// 用法: php -d extension=... url_test.php

// 模拟请求上下文需要先 bootstrap 一个 gene 应用
// 这里直接用反射或最小化启动来测试 gene_build_url 的行为

// gene 扩展需要在请求上下文中才能工作，我们通过 Application::url 静态调用测试
// 但需要先设置语言上下文。gene 的 url 在无请求上下文时 lang=NULL。

// 由于命令行没有请求上下文，ctx->lang 为空，所以：
// - url('login.html')       -> ctx->lang 空 -> 无前缀 -> '/login.html'
// - url('login.html','zh')  -> 显式 zh -> '/zh/login.html'
// - url('login.html','')    -> 显式空串 -> 无前缀 -> '/login.html'

echo "=== gene url() test (CLI, no request context) ===\n";

$r = \Gene\Application::url('login.html');
echo "url('login.html')       = " . var_export($r, true) . "\n";
echo "  expect: '/login.html' (no ctx lang in CLI)\n";

$r = \Gene\Application::url('login.html', 'zh');
echo "url('login.html','zh')  = " . var_export($r, true) . "\n";
echo "  expect: '/zh/login.html'\n";

$r = \Gene\Application::url('login.html', '');
echo "url('login.html','')    = " . var_export($r, true) . "\n";
echo "  expect: '/login.html' (empty string = no prefix)\n";

// 额外边界
$r = \Gene\Application::url('/');
echo "url('/')                = " . var_export($r, true) . "\n";
echo "  expect: '/'\n";

$r = \Gene\Application::url('/', 'en');
echo "url('/','en')           = " . var_export($r, true) . "\n";
echo "  expect: '/en/'\n";

$r = \Gene\Application::url('/', '');
echo "url('/','')             = " . var_export($r, true) . "\n";
echo "  expect: '/'\n";

echo "\n=== done ===\n";
