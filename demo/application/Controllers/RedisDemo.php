<?php

namespace Application\Controllers;

use Gene\Controller;
use Gene\Di;

class RedisDemo extends Controller
{
    /**
     * Redis连接池演示
     * GET /redis-demo
     */
    public function index()
    {
        $redis = Di::get('redis');
        
        // 设置一个键值对
        $redis->set('gene:demo:time', time());
        $redis->set('gene:demo:message', 'Hello from Redis Pool!');
        
        // 获取数据
        $time = $redis->get('gene:demo:time');
        $message = $redis->get('gene:demo:message');
        
        // 使用列表
        $redis->lpush('gene:demo:list', 'item1', 'item2', 'item3');
        $list = $redis->lrange('gene:demo:list', 0, -1);
        
        // 使用哈希
        $redis->hmset('gene:demo:hash', [
            'name' => 'Gene Framework',
            'version' => '5.4.3',
            'pool' => 'RedisPool'
        ]);
        $hash = $redis->hgetall('gene:demo:hash');
        
        // 获取连接池信息
        $poolInfo = [
            'pool_name' => 'redisPool',
            'config_key' => 'redis',
            'using_pool' => true
        ];
        
        $this->assign('time', $time);
        $this->assign('message', $message);
        $this->assign('list', $list);
        $this->assign('hash', $hash);
        $this->assign('pool_info', $poolInfo);
        
        $this->display('redis_demo');
    }
    
    /**
     * 性能测试
     * GET /redis-demo/performance
     */
    public function performance()
    {
        $redis = Di::get('redis');
        $iterations = 1000;
        
        $start = microtime(true);
        
        // 执行1000次Redis操作
        for ($i = 0; $i < $iterations; $i++) {
            $key = "gene:perf:test:{$i}";
            $redis->set($key, "value_{$i}");
            $value = $redis->get($key);
            $redis->del($key);
        }
        
        $end = microtime(true);
        $duration = ($end - $start) * 1000; // 转换为毫秒
        
        $result = [
            'iterations' => $iterations,
            'duration_ms' => round($duration, 2),
            'ops_per_second' => round($iterations / ($duration / 1000), 0),
            'avg_time_per_op' => round($duration / $iterations, 3)
        ];
        
        $this->assign('result', $result);
        $this->display('redis_performance');
    }
    
    /**
     * 清理测试数据
     * GET /redis-demo/cleanup
     */
    public function cleanup()
    {
        $redis = Di::get('redis');
        
        // 清理演示数据
        $keys = [
            'gene:demo:time',
            'gene:demo:message',
            'gene:demo:list',
            'gene:demo:hash'
        ];
        
        foreach ($keys as $key) {
            $redis->del($key);
        }
        
        // 清理性能测试数据
        $perfKeys = $redis->keys('gene:perf:test:*');
        if ($perfKeys) {
            foreach ($perfKeys as $key) {
                $redis->del($key);
            }
        }
        
        $this->assign('cleaned_keys', array_merge($keys, $perfKeys ?: []));
        $this->display('redis_cleanup');
    }
}
