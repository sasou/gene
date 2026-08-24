<?php
/**
 * Swoole 路由丢失定位探针。
 *
 * 用法：把 gene_probe() 这个函数体贴进 gene_web/public/swoole.php，
 * 然后在三个位置各调一次（见下方 A/B/C 注释），重启 swoole 并抓 stderr。
 *
 * Router::match() 走的是和 Application::run() 完全相同的 trie 查找与 safe
 * 前缀解析（router.c PHP_METHOD(gene_router, match) 里显式对齐过），所以它
 * 是一个可信的探针：match 命中 = 树里有这条路由。
 *
 * 关键是对比三个时间点：
 *   A 注册刚结束、workerReady() 之前
 *   B workerReady() 之后
 *   C 第一个请求内 / 第二个请求内
 * 树在哪一步从 HIT 变 MISS，就锁定了是注册没写进去、还是冻结时丢的、
 * 还是请求生命周期里被清掉的。
 */

function gene_probe($tag)
{
    static $probes = ['/', '/doc.html', '/test.html', '/favicon.ico', '/en/doc.html', '/admin/login.html'];

    // 探针必须用和 router.ini.php 里同样的方式构造 Router（同样的 safe 前缀）
    $r = new \Gene\Router();
    $out = [];
    foreach ($probes as $p) {
        $out[] = $p . '=' . ($r->match('GET', $p) === false ? 'MISS' : 'HIT');
    }

    $m = new \Gene\Memory();
    $stats = $m->stats();

    fwrite(STDERR, sprintf(
        "[GENE-PROBE %s] pid=%d cache_items=%d fn_cache=%d refused=%s | %s\n",
        $tag,
        getmypid(),
        $stats['cache_items'] ?? -1,
        $stats['fn_cache_items'] ?? -1,
        $stats['cache_insert_refused'] ?? 'n/a',
        implode(' ', $out)
    ));
}

/*
 * 贴进 swoole.php 的位置：
 *
 * $http->on("workerStart", function ($server, $workerId) {
 *     \Gene\Application::getInstance()
 *         ->autoload(APP_ROOT)
 *         ->load("router.ini.php", CONF_DIR)
 *         ->load("config.ini.prod.php", CONF_DIR)
 *         ->setMode(1, 0);
 *
 *     gene_probe('A-after-load');                 // <== A
 *
 *     \Gene\Pool::create('dbPool', 'db');
 *     \Gene\Cache\RedisPool::create('redisPool', 'redis');
 *
 *     gene_probe('A2-after-pools');               // <== A2：确认建池没把树写坏
 *
 *     \Gene\Application::getInstance()->workerReady();
 *
 *     gene_probe('B-after-workerReady');          // <== B
 * });
 *
 * $http->on("request", function ($request, $response) {
 *     \Gene\Application::waitWorkerReady();
 *     static $n = 0;
 *     if (++$n <= 3) { gene_probe("C-req$n-before"); }   // <== C
 *     \Gene\Request::init(...);
 *     ...
 * });
 *
 * 另外请一并提供：
 *   1) php -i | grep -i '^gene\.'      —— 线上 gene.* 全部 ini 值
 *   2) php -v 与 php -m（确认 opcache/JIT 是否开、swoole 版本）
 *   3) workerStart 阶段 stderr 里有没有
 *      "Gene memory cache is frozen after workerReady()" 这条 warning
 */
