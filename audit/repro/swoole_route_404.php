<?php
// 复现线上 swoole 模式路由 404：使用与 gene_web 一致的路由注册顺序
$router = new \Gene\Router();
$router->clear()
    ->lang('zh,en')
    ->get("/", "\Controllers\Index@index", "@clearAll")
    ->get("/doc.html", "\Controllers\Index@doc", "@clearAll")
    ->get("/doc/:slug.html", "\Controllers\Index@doc", "@clearAll")
    ->get("/test.html", "\Controllers\Index@test", "@clearAll")
    ->group("/admin/")
    ->get("/", "Controllers\Admin\Index@panel", "adminAuth@clearAfter")
    ->get(".html", "Controllers\Admin\Index@panel", "adminAuth@clearAfter")
    ->get("/:c.html", "Controllers\Admin\:c@run", "adminAuth@clearAfter")
    ->group()
    ->head("/", function () {})
    ->options("/", function () {})
    ->get("/favicon.ico", function () {}, "@clearAll")
    ->error("404", function () {
        echo "[404 hook]\n";
    });

foreach (['/test.html', '/doc.html', '/favicon.ico', '/en/test.html', '/en/doc.html', '/admin.html', '/admin/', '/'] as $p) {
    $hit = $router->match('GET', $p);
    printf("match GET %-16s => %s\n", $p, $hit === false ? 'MISS' : 'HIT');
}
