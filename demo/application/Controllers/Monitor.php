<?php

namespace Application\Controllers;

use Gene\Controller;
use Gene\Monitor as GeneMonitor;
use Gene\Response;

class Monitor extends Controller
{
    /**
     * F2 演示：聚合可观测出口
     * GET /monitor
     *
     * 返回 memory 分区（缓存/协程上下文/ctx pool/sweep 遥测）、
     * db_pools / redis_pools 命名连接池、请求计数与防御计数器。
     */
    public function index()
    {
        $started = microtime(true);
        $stats = GeneMonitor::stats();
        $stats['elapsed_ms'] = round((microtime(true) - $started) * 1000, 3);
        Response::json($stats);
    }
}
