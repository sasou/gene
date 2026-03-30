<!DOCTYPE html>
<html>
<head>
    <title>Redis Cleanup - Gene Framework</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .section h3 { margin-top: 0; color: #333; }
        .code { background: #f5f5f5; padding: 10px; border-radius: 3px; font-family: monospace; }
        .success { color: #28a745; }
        .info { color: #17a2b8; }
        ul { margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Redis 数据清理</h1>
        
        <div class="section">
            <h3 class="success">✓ 清理完成</h3>
            <p>已成功清理以下Redis键：</p>
            <ul>
                <?php foreach ($cleaned_keys as $key): ?>
                    <li class="code"><?php echo htmlspecialchars($key); ?></li>
                <?php endforeach; ?>
            </ul>
            <p><strong>共清理了 <?php echo count($cleaned_keys); ?> 个键</strong></p>
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
            <p>
                <a href="/redis-demo">返回演示首页</a> |
                <a href="/redis-demo/performance">重新进行性能测试</a>
            </p>
        </div>
    </div>
</body>
</html>
