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

//http响应类注入配置
$config->set("session", [
    'class' => '\Ext\Session',
    'params' => [[
    'driver' => "memcache"
        ]],
]);

//数据库类注入配置
$config->set("db", [
    'class' => '\Gene\Db\Mysql',
    'params' => [[
    'dsn' => 'mysql:dbname=gene_demo;host=10.5.5.11;port=3306;charset=utf8',
    'username' => 'dev',
    'password' => 'dev123'
        ]],
    'instance' => true
]);

//缓存类注入配置
$config->set("memcache", [
    'class' => '\Gene\Cache\Memcached',
    'params' => [[
    'servers' => [['host' => '10.5.5.13', 'port' => 11211]],
    'persistent' => true,
        ]],
    'instance' => true
]);

//缓存类注入配置
$config->set("redis", [
    'class' => '\Gene\Cache\Redis',
    'params' => [[
    'persistent' => true,
    'host' => '192.168.27.101',
    'port' => 6379,
    'timeout' => 3,
    'ttl' => 0,
        ]],
    'instance' => false
]);

//框架方法级缓存模块注入配置
$config->set("cache", [
    'class' => '\Gene\Cache\Cache',
    'params' => [[
    'hook' => 'memcache',
    'sign' => 'web:',
    'versionSign' => 'database:',
        ]],
    'instance' => false
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

