<?php

namespace Controllers;

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

    /**
     * GET /healthz — 存活探针。
     * handleSwoole 入口自带 waitWorkerReady 阻塞，能返回 200 即说明
     * worker 已完成 bootstrap + workerReady。
     */
    public function healthz()
    {
        Response::json([
            'status' => 'ok',
            'env'    => \Gene\Application::getInstance()->getEnvironmentName(),
            'time'   => time(),
        ]);
    }

    /**
     * GET /metrics — 指标出口（gene_web 同名端口的 demo 版）。
     * 直接复用 Gene\Monitor::stats() 聚合，JSON 输出。
     */
    public function metrics()
    {
        Response::json(GeneMonitor::stats());
    }
}
