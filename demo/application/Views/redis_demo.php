<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Redis Pool Demo - Gene Framework</title>
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
        .info { color:#06b6d4; }
        table { width:100%; border-collapse:collapse; margin:10px 0; }
        th, td { padding:10px 14px; text-align:left; border-bottom:1px solid #eef1f7; font-size:13.5px; }
        th { background:linear-gradient(90deg,#f6f8fd,#f2f6fc); color:#4a5578; font-weight:600; }
        tr:hover td { background:#f8faff; }
        a { color:#4f46e5; text-decoration:none; }
        a:hover { color:#06b6d4; }
        .btn-row a { display:inline-block; padding:8px 18px; margin-right:10px; border-radius:8px; font-size:13.5px;
            color:#fff; background:linear-gradient(135deg,#4f46e5,#4a7cf7); box-shadow:0 4px 12px rgba(79,70,229,.3); transition:all .25s; }
        .btn-row a:hover { transform:translateY(-1px); box-shadow:0 6px 18px rgba(79,70,229,.42); color:#fff; }
        .btn-row a.ghost { color:#4f46e5; background:#fff; border:1px solid #d6dbec; box-shadow:none; }
        .btn-row a.ghost:hover { border-color:#4f46e5; }
        ul { margin:8px 0 0; padding-left:20px; line-height:2; font-size:13.5px; }
        .tag { display:inline-block; padding:2px 10px; margin-left:6px; border-radius:10px; font-size:12px;
            color:#fff; background:linear-gradient(90deg,#4f46e5,#06b6d4); }
    </style>
</head>
<body>
    <div class="header">
        <div class="header-inner">
            <h1>Redis 连接池演示</h1>
            <p class="success">✓ Redis连接池已成功创建并运行</p>
        </div>
    </div>
    <div class="container">

        <div class="section">
            <h3>基础操作测试</h3>
            <table>
                <tr><th>操作</th><th>结果</th></tr>
                <tr>
                    <td>设置时间戳</td>
                    <td><span class="code"><?php echo date('Y-m-d H:i:s', $time); ?></span></td>
                </tr>
                <tr>
                    <td>获取消息</td>
                    <td><span class="code"><?php echo htmlspecialchars($message); ?></span></td>
                </tr>
                <tr>
                    <td>列表操作</td>
                    <td><span class="code"><?php echo implode(', ', $list); ?></span></td>
                </tr>
                <tr>
                    <td>哈希操作</td>
                    <td><span class="code">
                        <?php foreach ($hash as $k => $v): ?>
                            <?php echo htmlspecialchars($k); ?>: <?php echo htmlspecialchars($v); ?><br>
                        <?php endforeach; ?>
                    </span></td>
                </tr>
            </table>
        </div>

        <div class="section">
            <h3>连接池信息</h3>
            <table>
                <tr><th>配置项</th><th>值</th></tr>
                <?php foreach ($pool_info as $k => $v): ?>
                <tr>
                    <td><?php echo htmlspecialchars($k); ?></td>
                    <td><span class="code"><?php echo htmlspecialchars($v); ?></span></td>
                </tr>
                <?php endforeach; ?>
            </table>
        </div>

        <div class="section">
            <h3>测试链接</h3>
            <p class="btn-row">
                <a href="/redis-demo/performance">性能测试 (1000次操作)</a>
                <a href="/redis-demo/cleanup" class="ghost">清理测试数据</a>
            </p>
        </div>

        <div class="section">
            <h3>技术说明</h3>
            <ul>
                <li>使用 <code class="code">\Gene\Cache\RedisPool</code> 管理Redis连接</li>
                <li>每个Worker进程独立的连接池</li>
                <li>连接复用，避免频繁创建/销毁连接</li>
                <li>支持连接池大小控制、空闲超时等高级特性</li>
                <li>通过DI容器获取Redis实例：<code class="code">\Gene\Di::get('redis')</code></li>
                <li><strong>v5.4.3优化</strong><span class="tag">NEW</span>：默认最大连接数从10提升到64，显著提升高并发性能</li>
            </ul>
        </div>
    </div>
</body>
</html>
