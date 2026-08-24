<?php
/**
 * Repro: Swoole workerStart abort while loading
 *   $router->head("/", fn(){}) and $router->options("/", fn(){})
 *
 * Debug PHP used to hit:
 *   zend_hash.c: ht is being destroyed
 *   gene_memory_set_val / gene_memory_set_by_router
 *
 * Usage: php audit/repro/router_head_options.php
 */
$r = new \Gene\Router();
$r->clear()
    ->get("/", "Controllers\\Index@index")
    ->get("/admin/:c/:a", "Controllers\\Admin\\:c@:a")
    ->head("/", function () {})
    ->options("/", function () {});
echo "OK head+options root routes\n";
