<?php
/**
 * Router configuration using Gene\Hook class-based hooks.
 * This avoids eval() for hook execution, improving performance.
 *
 * Compare with router.ini.php which uses closure-based hooks.
 * Class-based hooks use the direct dispatch path (C-level),
 * while closure hooks fall back to eval().
 */
$router = new \Gene\Router();
$router->clear()
    // Web 页面路由
    ->get("/", "\Controllers\Index@index","@clearAll")
    ->get("/doc.html", "\Controllers\Index@doc","@clearAll")
    ->get("/doc/:id.html", "\Controllers\Index@doc","@clearAll")
    ->get("/test.html", "\Controllers\Index@test","@clearAll")

    // Admin 登录、控制台相关页面
    ->get("/admin.html", "Controllers\Admin\Index@run", "adminAuth@clearAfter")
    ->get("/login.html", "Controllers\Admin\Index@login", "@clearAfter")
    ->post("/login.action", "Controllers\Admin\Index@loginPost", "@")
    ->get("/exit.action", "Controllers\Admin\Index@exits", "adminAuth@") 
    ->get("/captcha.action", "Controllers\Admin\Index@captcha", "@clearAfter") 
    ->get("/welcome.html", "Controllers\Admin\Index@welcome", "adminAuth@clearAfter")
    ->get("/set.html", "Controllers\Admin\User@set", "adminAuth@clearAfter")
    ->post("/save.html", "Controllers\Admin\User@save", "adminAuth@")

    // Admin模块路由规则
    ->group("/:c")
    ->get(".html", "Controllers\Admin\:c@run", "adminAuth@clearAfter")
    ->get("/:a", "Controllers\Admin\:c@:a", "adminAuth@")
    ->get("/:a.html", "Controllers\Admin\:c@:a", "adminAuth@clearAfter")
    ->get("/:a/:id", "Controllers\Admin\:c@:a", "adminAuth@")
    ->get("/:a/:id.html", "Controllers\Admin\:c@:a", "adminAuth@clearAfter")
    ->post("/:a", "Controllers\Admin\:c@:a", "adminAuth@")
    ->group()
    
    // Doc 模块路由
    ->group("/mark")
    ->get(".html", "Controllers\Doc\Mark@run", "adminAuth@clearAfter")
    ->get("/:a", "Controllers\Doc\Mark@:a", "adminAuth@")
    ->get("/:a.html", "Controllers\Doc\Mark@:a", "adminAuth@clearAfter")
    ->get("/:a/:id", "Controllers\Doc\Mark@:a", "adminAuth@")
    ->get("/:a/:id.html", "Controllers\Doc\Mark@:a", "adminAuth@clearAfter")
    ->post("/:a", "Controllers\Doc\Mark@:a", "adminAuth@")
    ->group()
        
    ->error(404, "Hooks\AdminAuth@handle")

    // 使用 Gene\Hook 子类注册钩子 (避免 eval，走C直接调用)
    ->hook("adminAuth", "Hooks\AdminAuth@handle")
    ->hook("before", "Hooks\BeforeHook@handle")
    ->hook("after", "Hooks\AfterHook@handle");
