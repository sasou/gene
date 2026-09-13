<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Redis Pool Performance Test - Gene Framework</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body { font-family:"Inter","PingFang SC","Microsoft YaHei","Helvetica Neue",Arial,sans-serif; background:#eef1f7; color:#3d4a70; -webkit-font-smoothing:antialiased; }
        .header { position:relative; padding:38px 0 60px; overflow:hidden;
            background:radial-gradient(60% 90% at 15% 10%,rgba(56,189,248,.35) 0%,transparent 60%),radial-gradient(55% 85% at 85% 20%,rgba(129,140,248,.38) 0%,transparent 60%),linear-gradient(150deg,#171f38 0%,#22305c 50%,#16405c 100%); }
        .header-inner { max-width:860px; margin:0 auto; padding:0 20px; }
        .header h1 { font-size:26px; font-weight:800; letter-spacing:1px;
            background:linear-gradient(90deg,#7dd3fc,#a5b4fc 55%,#5eead4);
            -webkit-background-clip:text; background-clip:text; -webkit-text-fill-color:transparent; }
        .header p { margin-top:8px; font-size:13.5px; color:rgba(226,232,255,.8); }
        .container { max-width:860px; margin:-34px auto 0; padding:0 20px 44px; position:relative; z-index:2; }
        .section { margin:16px 0; padding:20px 22px; background:#fff; border-radius:12px;
            box-shadow:0 1px 3px rgba(15,23,42,.06),0 8px 24px rgba(15,23,42,.06); }
        .section h3 { margin:0 0 12px; font-size:15.5px; color:#1f2b4d;
            padding-left:12px; border-left:3px solid #4f46e5; line-height:1.3; }
        .code { background:#f4f6fd; padding:3px 10px; border-radius:6px; font-family:"JetBrains Mono",Consolas,monospace; font-size:12.5px; color:#4f46e5; }
        .success { color:#10b981; }
        .warning { color:#f59e0b; }
        .info { color:#06b6d4; }
        table { width:100%; border-collapse:collapse; margin:10px 0; }
        th, td { padding:10px 14px; text-align:left; border-bottom:1px solid #eef1f7; font-size:13.5px; }
        th { background:linear-gradient(90deg,#f6f8fd,#f2f6fc); color:#4a5578; font-weight:600; }
        tr:hover td { background:#f8faff; }
        a { color:#4f46e5; text-decoration:none; }
        a:hover { color:#06b6d4; }
        .metric { font-size:1.15em; font-weight:700; color:#1f2b4d; font-variant-numeric:tabular-nums; }
        .metric.success { color:#10b981; }
        .metric.info { color:#06b6d4; }
        .metric-cards { display:flex; flex-wrap:wrap; gap:14px; }
        .metric-card { flex:1; min-width:170px; padding:16px 18px; border-radius:12px; background:#f8faff;
            border-top:3px solid #4f46e5; }
        .metric-card:nth-child(2){border-top-color:#06b6d4;}
        .metric-card:nth-child(3){border-top-color:#10b981;}
        .metric-card:nth-child(4){border-top-color:#f59e0b;}
        .metric-card .mv { display:block; font-size:22px; font-weight:800; color:#1f2b4d; font-variant-numeric:tabular-nums; }
        .metric-card .mv small { font-size:12px; font-weight:500; color:#7a86a8; margin-left:2px; }
        .metric-card .ml { display:block; margin-top:4px; font-size:12.5px; color:#7a86a8; }
        .btn-row a { display:inline-block; padding:8px 18px; margin-right:10px; border-radius:8px; font-size:13.5px;
            color:#fff; background:linear-gradient(135deg,#4f46e5,#4a7cf7); box-shadow:0 4px 12px rgba(79,70,229,.3); transition:all .25s; }
        .btn-row a:hover { transform:translateY(-1px); box-shadow:0 6px 18px rgba(79,70,229,.42); color:#fff; }
        .btn-row a.ghost { color:#4f46e5; background:#fff; border:1px solid #d6dbec; box-shadow:none; }
        .btn-row a.ghost:hover { border-color:#4f46e5; }
        ul { margin:8px 0 0; padding-left:20px; line-height:2; font-size:13.5px; }
        .verdict { padding:12px 16px; border-radius:10px; font-size:14px; margin-bottom:6px; }
        .verdict.ok { background:#ecfdf5; color:#047857; border:1px solid #a7f3d0; }
        .verdict.warn { background:#fffbeb; color:#b45309; border:1px solid #fde68a; }
    </style>
</head>
<body>
    <div class="header">
        <div class="header-inner">
            <h1>Redis 连接池性能测试</h1>
            <p>1000 次 set/get/del 循环 · 连接复用压测</p>
        </div>
    </div>
    <div class="container">

        <div class="section">
            <h3>测试结果</h3>
            <div class="metric-cards">
                <div class="metric-card">
                    <span class="mv"><?php echo number_format($result['iterations']); ?><small>次</small></span>
                    <span class="ml">总操作次数</span>
                </div>
                <div class="metric-card">
                    <span class="mv"><?php echo $result['duration_ms']; ?><small>毫秒</small></span>
                    <span class="ml">总耗时</span>
                </div>
                <div class="metric-card">
                    <span class="mv"><?php echo number_format($result['ops_per_second']); ?><small>ops/sec</small></span>
                    <span class="ml">每秒操作数</span>
                </div>
                <div class="metric-card">
                    <span class="mv"><?php echo $result['avg_time_per_op']; ?><small>毫秒</small></span>
                    <span class="ml">平均每次操作耗时</span>
                </div>
            </div>
        </div>

        <div class="section">
            <h3>性能分析</h3>
            <?php if ($result['ops_per_second'] > 10000): ?>
                <p class="verdict ok">✓ <strong>优秀性能</strong> - 连接池运行良好，Redis响应迅速</p>
            <?php elseif ($result['ops_per_second'] > 5000): ?>
                <p class="verdict warn">⚠ <strong>性能一般</strong> - 考虑检查网络延迟或Redis服务器负载</p>
            <?php else: ?>
                <p class="verdict warn">⚠ <strong>性能较低</strong> - 建议优化配置或检查Redis服务器状态</p>
            <?php endif; ?>

            <ul>
                <li>测试包含 1000 次 set/get/del 操作循环</li>
                <li>使用连接池复用连接，避免频繁建立连接</li>
                <li>每个Worker进程独立管理连接池</li>
            </ul>
        </div>

        <div class="section">
            <h3>优化建议</h3>
            <ul>
                <li><strong>连接池大小</strong>：根据并发量调整 max 参数（v5.4.3默认64）</li>
                <li><strong>空闲超时</strong>：适当设置 idleTimeout 避免连接积压</li>
                <li><strong>等待超时</strong>：设置合理的 waitTimeout 防止阻塞</li>
                <li><strong>Redis配置</strong>：确保Redis服务器有足够的内存和CPU</li>
                <li><strong>v5.4.3改进</strong>：默认连接池容量提升6倍，适合更高并发场景</li>
            </ul>
        </div>

        <div class="section">
            <h3>测试链接</h3>
            <p class="btn-row">
                <a href="/redis-demo">返回演示首页</a>
                <a href="javascript:location.reload()">重新测试</a>
                <a href="/redis-demo/cleanup" class="ghost">清理测试数据</a>
            </p>
        </div>
    </div>
</body>
</html>
