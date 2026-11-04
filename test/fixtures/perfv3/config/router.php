<?php
$router = new \Gene\Router();
$router->clear()
    ->hook("before", function () {
        echo "[before]";
    })
    ->hook("after", function () {
        echo "[after]";
    })
    ->hook("guard", function () {
        echo "[guard]";
    })
    ->get("/c/:id", function ($params) {
        $id = is_array($params) ? ($params['id'] ?? '') : '';
        echo "closure:" . $id;
    })
    ->get("/g", function () {
        echo "guarded";
    }, "guard@")
    ->get("/s", "\PerfV3\Home@index", "@clearAll")
    ->error(404, function () {
        echo "not-found";
    });
