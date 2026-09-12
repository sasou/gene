<?php
// 复现 [GENE_FIX:2026-09-12]：Router->__call 仅在首参 IS_STRING 时取作事件名，
// ->error(404, ...)（README/demo 的文档形式，int）被静默丢成空名，注册成
// "error:"/"fcl:" 而派发查找 "error:404" → 404 处理器永不触发。
// 修复后：标量首参字符串化，error(404) 与 error("404") 等价。
error_reporting(E_ALL);

$app = \Gene\Application::getInstance();
$router = new \Gene\Router();
$router->clear()
    ->get('/echo', function () { echo "R:echo"; })
    ->error(404, function () { echo "R:404-int"; })
    ->error("500", function () { echo "R:500-str"; })
    ->hook("myHook", function () { echo "R:hook"; });

$fail = 0;
$check = function (string $label, callable $cb, string $expect) use (&$fail) {
    ob_start();
    $cb();
    $out = ob_get_clean();
    $ok = ($out === $expect);
    printf("  [%s] %-28s got='%s'\n", $ok ? 'PASS' : 'FAIL', $label, $out);
    if (!$ok) $fail++;
};

$check('dispatch /echo',        function () use ($app) { $app->run('GET', '/echo'); },          'R:echo');
$check('dispatch unknown → 404', function () use ($app) { $app->run('GET', '/no/such/route'); }, 'R:404-int');
$check("runError('404')",       function () { \Gene\Router::runError('404'); },                 'R:404-int');
$check("runError('500')",       function () { \Gene\Router::runError('500'); },                 'R:500-str');

exit($fail ? 1 : 0);
