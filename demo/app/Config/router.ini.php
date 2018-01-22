<?php
$router = new \gene\router();
$router->clear()
->get("/",function (){
        (new \Controllers\Index)->run();
    })

->group("/admin")
    ->get("/",function(){
        echo 'admin';
    },"auth")
    ->get("/:name/",function($params){
        var_dump($params);
    },"auth@")
    ->get("/:name.:ext",function($params){
        var_dump($params);
    })
    ->get("/:name/sasoud",function(){
        echo 'dd';
    },"name")
    ->get("/blog/:ext/baidu",function(){
        echo 'baidu';
        return array('sdfasd'=>'baidu.edu.com');
    },"auth@clearAll")
->group()

->get("/index",function(){
    echo 'index';
},"@clearAll")

->get("/:baidu",function($params){
    echo 'baidu';
},"@clearAll")

->get("/:c/:a.html","Controllers\\\:c@:a","@clearAll")

->error(404,function(){echo " 404 "; })

->hook("auth",function(){echo " auth-hook ";})
->hook("before", function(){
    echo " before-hook ";
})
->hook("after", function($params){
    echo " after-hook ";
    if(is_array($params))var_dump($params);
});
