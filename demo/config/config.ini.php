<?php
$config = new \Gene\Config();
$config->clear();

// GENE_DEMO_LOCAL=1：完全本地化模式（验收/压测用，不依赖任何外部服务）——
//   db      → sqlite 文件 demo/database/gene_demo.db（init_sqlite.php 幂等初始化）
//   session → localStore（Ext\LocalStore：Gene\Memory 适配 get/set/delete 句柄契约）
//   cache   → hook 改用 localStore（cachedVersion 的数组 key 走 mget，数据键走 set/incr）
// 同时 swoole.php 入口在此模式下只声明 dbPool（sqlite 连接池），不建 redisPool。
$demoLocal = (bool)getenv('GENE_DEMO_LOCAL');
$demoDbFile = dirname(__DIR__) . '/database/gene_demo.db';

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
// hash_mode: SessionId 哈希算法选择（可选）
//   - 0 (默认): MD5 - 32位十六进制字符串，兼容性最好
//   - 1: Fast (FNV-1a 64位) - 16位十六进制字符串，速度最快
//   - 2: xxHash64 - 16位十六进制字符串，高性能非加密哈希
//   - 3: FarmHash64 - 16位十六进制字符串，Google优化哈希
//   - 4: MurmurHash3 - 8位十六进制字符串，通用哈希算法
//   - 5: TurboHash32 - 8位十六进制字符串，Gene优化高性能哈希
$config->set("session", [
    'class' => '\Gene\Session',
    'params' => [[
    'driver' => $demoLocal ? 'localStore' : 'memcache',
    'prefix' => 'memc.sess.key.',
    'name' => 'SSID',
    'domain' => '',
    'path' => '/',
    'hash_mode' => 1,  // 可选 0-5，1-5 提升性能
        ]],
    'instance' => false
]);

//Redis类注入配置
$config->set("redis", [
    'class' => '\Gene\Cache\Redis',
    'params' => [[
    'host' => '127.0.0.1',
    'port' => 6379,
    'password' => '',
    'database' => 0,
    'pool' => 'redisPool'
        ]],
    'instance' => true
]);

//数据库类注入配置
// instance:true — 请求/协程内按类名单例；同一上下文复用 Db 对象，不跨请求或协程共享。
// 配置 pool 后 Db 自动按需借还 PDO，业务代码仍使用普通链式 API。
// PDO::ATTR_PERSISTENT 可保留用于 FPM；Swoole/coroutine 模式下扩展会自动改为 false。
// pool: 连接池名称（可选），在Swoole协程模式下启用连接池。需在workerStart中通过
//   Gene\Pool::create('dbPool', 'db') 预先创建，自动读取此处的dsn/username/password。
//   FPM模式下此参数被忽略，行为不变。
$config->set("db", $demoLocal ? [
    'class' => '\Gene\Db\Sqlite',
    'params' => [[
    'dsn' => 'sqlite:' . $demoDbFile,
    'pool' => 'dbPool'
        ]],
    'instance' => true
] : [
    'class' => '\Gene\Db\Mysql',
    'params' => [[
    'dsn' => 'mysql:dbname=gene_demo;host=127.0.0.1;port=3306;charset=utf8',
    'username' => 'root',
    'password' => '123456',
    'pool' => 'dbPool'
        ]],
    'instance' => true
]);

//缓存类注入配置
// instance:true — 请求/协程内按类名单例；生命周期仍由当前请求上下文管理，不跨请求共享。
$config->set("memcache", [
    'class' => '\Gene\Cache\Memcached',
    'params' => [[
    'servers' => [['host' => '127.0.0.1', 'port' => 11211]],
    'persistent' => true,
    'serializer' => 2
        ]],
    'instance' => true
]);

//直连 Redis 示例；主业务使用上方带 redisPool 的 redis 组件。
// instance:true — 请求/协程内按类名单例；生命周期仍由当前请求上下文管理，不跨请求共享。
$config->set("redisDirect", [
    'class' => '\Gene\Cache\Redis',
    'params' => [[
    'persistent' => true,
    'host' => '127.0.0.1',
    'port' => 6379,
    'timeout' => 3,
    'ttl' => 0,
    'pass' => 'rds2024',
    'serializer' => 1
        ]],
    'instance' => true
]);

//框架方法级缓存模块注入配置
// hash_mode: 缓存键哈希算法选择（可选）
//   - 0 (默认): MD5 - 32位十六进制字符串，兼容性最好
//   - 1: Fast (FNV-1a 64位) - 16位十六进制字符串，速度最快
//   - 2: xxHash64 - 16位十六进制字符串，高性能非加密哈希
//   - 3: FarmHash64 - 16位十六进制字符串，Google优化哈希
//   - 4: MurmurHash3 - 8位十六进制字符串，通用哈希算法
//   - 5: TurboHash32 - 8位十六进制字符串，Gene优化高性能哈希
$config->set("cache", [
    'class' => '\Gene\Cache\Cache',
    'params' => [[
    'hook' => $demoLocal ? 'localStore' : 'memcache',
    'sign' => 'demo:',
    'versionSign' => 'database:',
    'hash_mode' => 0,  // 可选 0-5，1-5 提升性能
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

//本地存储适配器：包一层 Gene\Memory，补齐 cache hook（数组 key→mget）与
// session 句柄（delete）契约；GENE_DEMO_LOCAL 时作为 session.driver 与 cache.hook
$config->set("localStore", [
    'class'    => '\Ext\LocalStore',
    'instance' => true
]);

//自定义httpsqs队列类注入配置
$config->set("httpsqs", [
    'class' => '\Ext\Queue\Httpsqs',
    'params' => [[
    'host' => '127.0.0.1', 
    'port' => 1212,
    'name' => 'email'
        ]],
    'instance' => true
]);

$config->set('rest', [
    'class' => '\Gene\Rest',
    'params' => [[
        'timeout' => 5,
        'connect_timeout' => 2,
        'ssl_verify' => true,
        'keep_alive' => true,
        'headers' => ['Accept' => 'application/json'],
        'pass_request_id' => true,
        'services' => [
            'demo' => [
                'base_url' => 'http://127.0.0.1:8081',
                'local' => 'Api\\',
            ],
        ],
    ]],
    'instance' => true,
]);

//自定义redis队列类注入配置
$config->set("redisQueue", [
    'class' => '\Ext\Queue\Redis',
    'params' => [[
    'host' => '127.0.0.1', 
    'port' => 6379,
    'name' => 'email'
        ]],
    'instance' => true
]);

