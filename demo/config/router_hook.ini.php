<?php
/**
 * Alternative minimal router configuration using Gene\Hook class-based hooks.
 * The primary router.ini.php uses the same C-level direct dispatch pattern.
 */
$router = new \Gene\Router();
$router->clear()
    ->hook("adminAuth", "Hooks\AdminAuth@handle")
    ->hook("cors", "Hooks\Cors@handle")
    ->hook("before", "Hooks\BeforeHook@handle")
    ->hook("after", "Hooks\AfterHook@handle")

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
    ->group("/:c")->through(["adminAuth"])
    ->get(".html", "Controllers\Admin\:c@run", "@clearAfter")
    ->get("/:a", "Controllers\Admin\:c@:a", "@")
    ->get("/:a.html", "Controllers\Admin\:c@:a", "@clearAfter")
    ->get("/:a/:id", "Controllers\Admin\:c@:a", "@")
    ->get("/:a/:id.html", "Controllers\Admin\:c@:a", "@clearAfter")
    ->post("/:a", "Controllers\Admin\:c@:a", "@")
    ->group()
    
    // Doc 模块路由
    ->group("/mark")->through(["adminAuth"])
    ->get(".html", "Controllers\Doc\Mark@run", "@clearAfter")
    ->get("/:a", "Controllers\Doc\Mark@:a", "@")
    ->get("/:a.html", "Controllers\Doc\Mark@:a", "@clearAfter")
    ->get("/:a/:id", "Controllers\Doc\Mark@:a", "@")
    ->get("/:a/:id.html", "Controllers\Doc\Mark@:a", "@clearAfter")
    ->post("/:a", "Controllers\Doc\Mark@:a", "@")
    ->group()
        
    ->error(404, "Hooks\ErrorHook@handle");
