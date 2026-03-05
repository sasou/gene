<?php
$config = new \Gene\Config();
$config->clear();

//视图类注入配置
$config->set("view", [
    'class' => '\Gene\View'
]);

//http请求类注入配置
$config->set("request", [
    'class' => '\Gene\Request'
]);

//http请求类注入配置
$config->set("response", [
    'class' => '\Gene\Response'
]);

//http验证类注入配置
$config->set("validate", [
    'class' => '\Gene\Validate'
]);

//http响应类注入配置
$config->set("session", [
    'class' => '\Ext\Session',
    'params' => [[
    'driver' => "memcache"
        ]],
]);

//数据库类注入配置
// instance:false — FPM 下同一请求内复用（DI 按名称缓存），Swoole/协程下每次请求
// 得到全新对象（DI 跳过名称缓存），隔离链式查询构建状态；连接复用由
// PDO::ATTR_PERSISTENT 实现（框架在 runtime_type>=2 时自动注入）。
$config->set("db", [
    'class' => '\Gene\Db\Mysql',
    'params' => [[
    'dsn' => 'mysql:dbname=gene_demo;host=10.5.5.11;port=3306;charset=utf8',
    'username' => 'dev',
    'password' => 'dev123'
        ]],
    'instance' => false
]);

//缓存类注入配置
// instance:true — Memcached 操作单次完整调用、无链式状态，Swoole 协程 Hook
// 保证底层 socket 按协程隔离，worker 级单例可安全共享，避免重复 pconnect 开销。
$config->set("memcache", [
    'class' => '\Gene\Cache\Memcached',
    'params' => [[
    'servers' => [['host' => '10.5.5.13', 'port' => 11211]],
    'persistent' => true,
    'serializer' => 2
        ]],
    'instance' => true
]);

//缓存类注入配置
// instance:true — Redis 操作单次完整调用、无链式状态，Swoole 协程 Hook
// 保证底层 socket 按协程隔离，worker 级单例可安全共享，避免重复 pconnect 开销。
$config->set("redis", [
    'class' => '\Gene\Cache\Redis',
    'params' => [[
    'persistent' => true,
    'host' => '10.5.5.13',
    'port' => 6379,
    'timeout' => 3,
    'ttl' => 0,
    'serializer' => 1
        ]],
    'instance' => true
]);

//框架方法级缓存模块注入配置
$config->set("cache", [
    'class' => '\Gene\Cache\Cache',
    'params' => [[
    'hook' => 'memcache',
    'sign' => 'demo:',
    'versionSign' => 'database:',
        ]],
    'instance' => false
]);

// Gene\Memory 进程级共享内存缓存（跨请求存活，不依赖外部服务）
// 适合高频读取、低频更新的数据（如配置、权限、路由预热等）
// 注意：每个 Worker 进程独立内存空间，多 Worker 模式下数据不互通
$config->set("memory", [
    'class'    => '\Gene\Memory',
    'params'   => [['demo']],
    'instance' => true
]);

//自定义httpsqs队列类注入配置
$config->set("httpsqs", [
    'class' => '\Ext\Queue\Httpsqs',
    'params' => [[
    'host' => '10.5.5.14', 
    'port' => 1212,
    'name' => 'email'
        ]],
    'instance' => true
]);

//自定义redis队列类注入配置
$config->set("redisQueue", [
    'class' => '\Ext\Queue\Redis',
    'params' => [[
    'host' => '10.5.5.13', 
    'port' => 6379,
    'name' => 'email'
        ]],
    'instance' => true
]);

