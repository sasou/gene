<?php
$router = new \Gene\Router();
$router->clear()

    ->get("/", "\Controllers\Index@index","@clearAll")
    ->get("/doc.html", "\Controllers\Index@doc","@clearAll")
    ->get("/doc/:id.html", "\Controllers\Index@doc","@clearAll")
    ->get("/test.html", "\Controllers\Index@test","@clearAll")

    // admin 登录、控制台相关页面
    ->get("/admin.html", "Controllers\Admin\Index@run", "adminAuth@clearAll")
    ->get("/login.html", "Controllers\Admin\Index@login", "@clearAll")
    ->post("/login.action", "Controllers\Admin\Index@loginPost", "@clearBefore")
    ->get("/exit.action", "Controllers\Admin\Index@exits", "adminAuth@clearBefore") 
    ->get("/captcha.action", "Controllers\Admin\Index@captcha", "@clearBefore") 
    ->get("/welcome.html", "Controllers\Admin\Index@welcome", "adminAuth@clearAll")
    ->get("/set.html", "Controllers\Admin\User@set", "adminAuth@clearAll")
    ->post("/save.html", "Controllers\Admin\User@save", "adminAuth@clearBefore")

    /* 通用路由规则 */
    ->group("/:c")
    ->get(".html", "Controllers\Admin\:c@run", "adminAuth@clearAll")
    ->get("/:a", "Controllers\Admin\:c@:a", "adminAuth@clearBefore")
    ->get("/:a.html", "Controllers\Admin\:c@:a", "adminAuth@clearAll")
    ->get("/:a/:id", "Controllers\Admin\:c@:a", "adminAuth@clearBefore")
    ->get("/:a/:id.html", "Controllers\Admin\:c@:a", "adminAuth@clearAll")
    ->post("/:a", "Controllers\Admin\:c@:a", "adminAuth@clearBefore")
    ->group()
    
    ->group("/demo/admin")
        ->get("/", function ($a){echo ' admin '; }, "auth")
        ->get("/:name", function ($abc){var_dump($abc); },"@clearAll")
        ->get("/:name.:ext", function ($abc){var_dump($abc);},"@clearAll")
        ->get("/:name/sasou", function (){echo 'sasou'; }, "auth@clearAll")
        ->get("/blog/:ext/baidu", function (){
            echo 'baidu'; 
            return array('sdfasd' => 'baidu.edu.com'); 
        }, "auth@clearAll")
    ->group()
        
        
    ->error(404, function() {
        echo 404;
    })
    
    
    ->hook("auth", function (){echo " auth "; })
    ->hook("adminAuth", function() {
        $user = \Gene\Session::get('admin');
        \Gene\Di::set('user', $user);
        if(!isset($user['user_id'])) {
            \Gene\Response::redirect('/login.html', 301);
        }
        $id = \Services\Admin\Module::getInstance()->initPath(\Gene\Application::getRouterUri());
        if (!$id || strpos(",{$user['purview']},", ",{$id},") === false) {
            \Gene\Response::json(\Gene\Response::error("操作未授权"));
            return false;
        }
    })
    ->hook("before", function() {
        
    })
    ->hook("after", function($params) {
        $callback = \Gene\Request::get("callback");
        \gene\response::json($params, $callback);
    });
