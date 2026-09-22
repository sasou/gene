<?php
$router = new \Gene\Router();
$router->clear()
    // Web 页面路由
    ->get("/", "\Controllers\Index@index","@clearAll")
    ->get("/doc.html", "\Controllers\Index@doc","@clearAll")
    ->get("/doc/:id.html", "\Controllers\Index@doc","@clearAll")
    ->get("/test.html", "\Controllers\Index@test","@clearAll")
    
    // Redis Pool 演示路由
    ->group("/redis-demo")
    ->get("/", "\Controllers\RedisDemo@index", "@clearAfter")
    ->get("/performance", "\Controllers\RedisDemo@performance", "@clearAfter")
    ->get("/cleanup", "\Controllers\RedisDemo@cleanup", "@clearAfter")
    ->group()

    // Monitor 聚合可观测出口演示（F2）
    ->get("/monitor", "\Controllers\Monitor@index", "@clearAfter")

    // 健康检查与指标出口（验收脚本 wait_for_demo_web 依赖）
    ->get("/healthz", "\Controllers\Monitor@healthz", "@clearAfter")
    ->get("/metrics", "\Controllers\Monitor@metrics", "@clearAfter")

    // Admin 登录、控制台相关页面 静态匹配具体类的方法
    ->get("/admin.html", "Controllers\Admin\Index@run", "adminAuth@clearAfter")
    ->get("/login.html", "Controllers\Admin\Index@login", "@clearAfter")
    ->post("/login.action", "Controllers\Admin\Index@loginPost", "@")
    ->get("/exit.action", "Controllers\Admin\Index@exits", "adminAuth@") 
    ->get("/captcha.action", "Controllers\Admin\Index@captcha", "@clearAfter") 
    ->get("/welcome.html", "Controllers\Admin\Index@welcome", "adminAuth@clearAfter")
    ->get("/set.html", "Controllers\Admin\User@set", "adminAuth@clearAfter")
    ->post("/save.html", "Controllers\Admin\User@save", "adminAuth@")

    // 后台验证、CORS 与全局前后置钩子使用类 Hook，走 C 层直接分发
    ->hook("adminAuth", "Hooks\AdminAuth@handle")
    ->hook("cors", "Hooks\Cors@handle")
    ->hook("before", "Hooks\BeforeHook@handle")
    ->hook("after", "Hooks\AfterHook@handle")

    // Admin模块路由规则 动态匹配类和方法
    ->group("/:c")->through(["adminAuth"])
    ->get(".html", "Controllers\Admin\:c@run", "@clearAfter")
    ->get("/:a", "Controllers\Admin\:c@:a", "@")
    ->get("/:a.html", "Controllers\Admin\:c@:a", "@clearAfter")
    ->get("/:a/:id", "Controllers\Admin\:c@:a", "@")
    ->get("/:a/:id.html", "Controllers\Admin\:c@:a", "@clearAfter")
    ->post("/:a", "Controllers\Admin\:c@:a", "@")
    ->group()
    
    // Doc 模块路由 动态匹配方法
    ->group("/mark")->through(["adminAuth"])
    ->get(".html", "Controllers\Doc\Mark@run", "@clearAfter")
    ->get("/:a", "Controllers\Doc\Mark@:a", "@")
    ->get("/:a.html", "Controllers\Doc\Mark@:a", "@clearAfter")
    ->get("/:a/:id", "Controllers\Doc\Mark@:a", "@")
    ->get("/:a/:id.html", "Controllers\Doc\Mark@:a", "@clearAfter")
    ->post("/:a", "Controllers\Doc\Mark@:a", "@")
    ->group()
        
    ->error(404, "Hooks\ErrorHook@handle");
