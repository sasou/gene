<!DOCTYPE html>
<html>
<head>
    <title>Redis Pool Performance Test - Gene Framework</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .section h3 { margin-top: 0; color: #333; }
        .code { background: #f5f5f5; padding: 10px; border-radius: 3px; font-family: monospace; }
        .success { color: #28a745; }
        .warning { color: #ffc107; }
        .info { color: #17a2b8; }
        table { width: 100%; border-collapse: collapse; margin: 10px 0; }
        th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background-color: #f8f9fa; }
        .metric { font-size: 1.2em; font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Redis 连接池性能测试</h1>
        
        <div class="section">
            <h3>测试结果</h3>
            <table>
                <tr><th>指标</th><th>数值</th></tr>
                <tr>
                    <td>总操作次数</td>
                    <td class="metric"><?php echo number_format($result['iterations']); ?> 次</td>
                </tr>
                <tr>
                    <td>总耗时</td>
                    <td class="metric"><?php echo $result['duration_ms']; ?> 毫秒</td>
                </tr>
                <tr>
                    <td>每秒操作数</td>
                    <td class="metric success"><?php echo number_format($result['ops_per_second']); ?> ops/sec</td>
                </tr>
                <tr>
                    <td>平均每次操作耗时</td>
                    <td class="metric info"><?php echo $result['avg_time_per_op']; ?> 毫秒</td>
                </tr>
            </table>
        </div>
        
        <div class="section">
            <h3>性能分析</h3>
            <?php if ($result['ops_per_second'] > 10000): ?>
                <p class="success">✓ <strong>优秀性能</strong> - 连接池运行良好，Redis响应迅速</p>
            <?php elseif ($result['ops_per_second'] > 5000): ?>
                <p class="warning">⚠ <strong>性能一般</strong> - 考虑检查网络延迟或Redis服务器负载</p>
            <?php else: ?>
                <p class="warning">⚠ <strong>性能较低</strong> - 建议优化配置或检查Redis服务器状态</p>
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
            <p>
                <a href="/redis-demo">返回演示首页</a> |
                <a href="/redis-demo/cleanup">清理测试数据</a> |
                <a href="javascript:location.reload()">重新测试</a>
            </p>
        </div>
    </div>
</body>
</html>
