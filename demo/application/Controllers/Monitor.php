<?php

namespace Application\Controllers;

use Gene\Controller;
use Gene\Monitor as GeneMonitor;
use Gene\Response;

class Monitor extends Controller
{
    private $started;

    /**
     * F3 演示：Yaf 风格生命周期钩子。
     * 路由直派实例化控制器后、调用 action 前由框架自动调用一次。
     */
    public function init()
    {
        $this->started = microtime(true);
    }

    /**
     * F2 演示：聚合可观测出口
     * GET /monitor
     *
     * 返回 memory 分区（缓存/协程上下文/ctx pool/sweep 遥测）、
     * db_pools / redis_pools 命名连接池、请求计数与防御计数器。
     */
    public function index()
    {
        $stats = GeneMonitor::stats();
        $stats['elapsed_ms'] = round((microtime(true) - $this->started) * 1000, 3);
        Response::json($stats);
    }
}
