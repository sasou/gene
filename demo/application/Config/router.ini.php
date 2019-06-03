<?php
$router = new \Gene\Router();
$router->clear()
    // Web 页面路由
    ->get("/", "\Controllers\Index@index","@clearAll")
    ->get("/doc.html", "\Controllers\Index@doc","@clearAll")
    ->get("/doc/:id.html", "\Controllers\Index@doc","@clearAll")
    ->get("/test.html", "\Controllers\Index@test","@clearAll")

    // Admin 登录、控制台相关页面 静态匹配具体类的方法
    ->get("/admin.html", "Controllers\Admin\Index@run", "adminAuth@clearAfter")
    ->get("/login.html", "Controllers\Admin\Index@login", "@clearAfter")
    ->post("/login.action", "Controllers\Admin\Index@loginPost", "@")
    ->get("/exit.action", "Controllers\Admin\Index@exits", "adminAuth@") 
    ->get("/captcha.action", "Controllers\Admin\Index@captcha") 
    ->get("/welcome.html", "Controllers\Admin\Index@welcome", "adminAuth@clearAfter")
    ->get("/set.html", "Controllers\Admin\User@set", "adminAuth@clearAfter")
    ->post("/save.html", "Controllers\Admin\User@save", "adminAuth@")

    // Admin模块路由规则 动态匹配类和方法
    ->group("/:c")
    ->get(".html", "Controllers\Admin\:c@run", "adminAuth@clearAfter")
    ->get("/:a", "Controllers\Admin\:c@:a", "adminAuth@")
    ->get("/:a.html", "Controllers\Admin\:c@:a", "adminAuth@clearAfter")
    ->get("/:a/:id", "Controllers\Admin\:c@:a", "adminAuth@")
    ->get("/:a/:id.html", "Controllers\Admin\:c@:a", "adminAuth@clearAfter")
    ->post("/:a", "Controllers\Admin\:c@:a", "adminAuth@")
    ->group()
    
    // Doc 模块路由 动态匹配方法
    ->group("/mark")
    ->get(".html", "Controllers\Doc\Mark@run", "adminAuth@clearAfter")
    ->get("/:a", "Controllers\Doc\Mark@:a", "adminAuth@")
    ->get("/:a.html", "Controllers\Doc\Mark@:a", "adminAuth@clearAfter")
    ->get("/:a/:id", "Controllers\Doc\Mark@:a", "adminAuth@")
    ->get("/:a/:id.html", "Controllers\Doc\Mark@:a", "adminAuth@clearAfter")
    ->post("/:a", "Controllers\Doc\Mark@:a", "adminAuth@")
    ->group()
        
        
    ->error(404, function() {
        echo 404;
    })
    
    // 后台验证钩子
    ->hook("adminAuth", function() {
        if(!isset($this->user['user_id'])) {
            \Gene\Response::redirect('/login.html', 301);
        }
        $id = \Services\Admin\Module::getInstance()->initPath(\Gene\Application::getRouterUri());
        if (!$id || strpos(",{$this->user['purview']},", ",{$id},") === false) {
            \Gene\Response::json(\Gene\Response::error("操作未授权"));
            return false;
        }
    })
    // 全局前置钩子
    ->hook("before", function() {
        $user = \Gene\Session::get('admin');
        \Gene\Di::set('user', $user); 
    })
    // 全局后置钩子
    ->hook("after", function($params) {
        $callback = \Gene\Request::get("callback");
        \gene\response::json($params, $callback);
    });
