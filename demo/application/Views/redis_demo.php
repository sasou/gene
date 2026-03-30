<!DOCTYPE html>
<html>
<head>
    <title>Redis Pool Demo - Gene Framework</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .section h3 { margin-top: 0; color: #333; }
        .code { background: #f5f5f5; padding: 10px; border-radius: 3px; font-family: monospace; }
        .success { color: #28a745; }
        .info { color: #17a2b8; }
        table { width: 100%; border-collapse: collapse; margin: 10px 0; }
        th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background-color: #f8f9fa; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Redis 连接池演示</h1>
        <p class="success">✓ Redis连接池已成功创建并运行</p>
        
        <div class="section">
            <h3>基础操作测试</h3>
            <table>
                <tr><th>操作</th><th>结果</th></tr>
                <tr>
                    <td>设置时间戳</td>
                    <td class="code"><?php echo date('Y-m-d H:i:s', $time); ?></td>
                </tr>
                <tr>
                    <td>获取消息</td>
                    <td class="code"><?php echo htmlspecialchars($message); ?></td>
                </tr>
                <tr>
                    <td>列表操作</td>
                    <td class="code"><?php echo implode(', ', $list); ?></td>
                </tr>
                <tr>
                    <td>哈希操作</td>
                    <td class="code">
                        <?php foreach ($hash as $k => $v): ?>
                            <?php echo htmlspecialchars($k); ?>: <?php echo htmlspecialchars($v); ?><br>
                        <?php endforeach; ?>
                    </td>
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
                    <td class="code"><?php echo htmlspecialchars($v); ?></td>
                </tr>
                <?php endforeach; ?>
            </table>
        </div>
        
        <div class="section">
            <h3>测试链接</h3>
            <p>
                <a href="/redis-demo/performance">性能测试 (1000次操作)</a> |
                <a href="/redis-demo/cleanup">清理测试数据</a>
            </p>
        </div>
        
        <div class="section info">
            <h3>技术说明</h3>
            <ul>
                <li>使用 <code>\Gene\Cache\RedisPool</code> 管理Redis连接</li>
                <li>每个Worker进程独立的连接池</li>
                <li>连接复用，避免频繁创建/销毁连接</li>
                <li>支持连接池大小控制、空闲超时等高级特性</li>
                <li>通过DI容器获取Redis实例：<code>\Gene\Di::get('redis')</code></li>
                <li><strong>v5.4.3优化</strong>：默认最大连接数从10提升到64，显著提升高并发性能</li>
            </ul>
        </div>
    </div>
</body>
</html>
