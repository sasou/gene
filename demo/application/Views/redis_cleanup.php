<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Redis Cleanup - Gene Framework</title>
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
        .section h3.success { border-left-color:#10b981; color:#047857; }
        .code { display:inline-block; background:#f4f6fd; padding:3px 10px; margin:2px 0; border-radius:6px;
            font-family:"JetBrains Mono",Consolas,monospace; font-size:12.5px; color:#4f46e5; }
        .success { color:#10b981; }
        a { color:#4f46e5; text-decoration:none; }
        a:hover { color:#06b6d4; }
        .btn-row a { display:inline-block; padding:8px 18px; margin-right:10px; border-radius:8px; font-size:13.5px;
            color:#fff; background:linear-gradient(135deg,#4f46e5,#4a7cf7); box-shadow:0 4px 12px rgba(79,70,229,.3); transition:all .25s; }
        .btn-row a:hover { transform:translateY(-1px); box-shadow:0 6px 18px rgba(79,70,229,.42); color:#fff; }
        .btn-row a.ghost { color:#4f46e5; background:#fff; border:1px solid #d6dbec; box-shadow:none; }
        .btn-row a.ghost:hover { border-color:#4f46e5; }
        ul { margin:10px 0; padding-left:20px; line-height:2; font-size:13.5px; }
        .key-list { list-style:none; padding-left:0; }
        .key-list li { margin:4px 0; }
        .count-badge { display:inline-block; margin-top:10px; padding:6px 16px; border-radius:20px; font-size:13px;
            color:#fff; background:linear-gradient(90deg,#10b981,#06b6d4); box-shadow:0 4px 12px rgba(16,185,129,.3); }
    </style>
</head>
<body>
    <div class="header">
        <div class="header-inner">
            <h1>Redis 数据清理</h1>
            <p>演示数据一键清理工具</p>
        </div>
    </div>
    <div class="container">

        <div class="section">
            <h3 class="success">✓ 清理完成</h3>
            <p>已成功清理以下Redis键：</p>
            <ul class="key-list">
                <?php foreach ($cleaned_keys as $key): ?>
                    <li><span class="code"><?php echo htmlspecialchars($key); ?></span></li>
                <?php endforeach; ?>
            </ul>
            <span class="count-badge">共清理了 <?php echo count($cleaned_keys); ?> 个键</span>
        </div>

        <div class="section">
            <h3>清理说明</h3>
            <ul>
                <li>清理了所有演示数据（时间戳、消息、列表、哈希）</li>
                <li>清理了性能测试产生的临时数据</li>
                <li>连接池配置保持不变，可以继续使用</li>
            </ul>
        </div>

        <div class="section">
            <h3>返回链接</h3>
            <p class="btn-row">
                <a href="/redis-demo">返回演示首页</a>
                <a href="/redis-demo/performance" class="ghost">重新进行性能测试</a>
            </p>
        </div>
    </div>
</body>
</html>
