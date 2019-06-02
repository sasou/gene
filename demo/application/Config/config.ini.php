<?php
$config = new \Gene\Config();
$config->clear();

//视图类注入配置
$config->set("view", [
    'class' => '\Gene\View'
]);

//数据库类注入配置
$config->set("db", [
    'class' => '\Gene\Db\Mysql',
    'params' => [[
    'dsn' => 'mysql:dbname=gene_demo;host=127.0.0.1;port=3306;charset=utf8',
    'username' => 'root',
    'password' => ''
        ]],
    'instance' => true
]);

//缓存类注入配置
$config->set("memcache", [
    'class' => '\Ext\Cache\Memcache',
    'params' => [[
    'servers' => [['host' => '127.0.0.1', 'port' => 11211]],
    'persistent' => true,
        ]],
    'instance' => true
]);

//缓存类注入配置
$config->set("redis", [
    'class' => '\Ext\Cache\Redis',
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

//Rest调用模块注入配置
$config->set("rest", [
    'class' => '\Ext\Services\Rest',
    'params' => [[
    'registry' => 'memcache',
    'sign' => 'apiStore:',
        ]],
    'instance' => true
]);

//文件类注入配置
$config->set("mogilefs", [
    'class' => '\Ext\Mogilefs',
    'params' => [[
    'host' => '127.0.0.1', 
    'port' => 7001,
    'domain' => 'file'
        ]],
    'instance' => true
]);

//mongodb类注入配置
$config->set("mongodb", [
    'class' => '\Ext\Mongo',
    'params' => [[
    'server' => 'mongodb://127.0.0.1:27017', 
    'options' => ['connect' => true ]
        ]],
    'instance' => true
]);

//httpsqs队列类注入配置
$config->set("httpsqs", [
    'class' => '\Ext\Queue\Httpsqs',
    'params' => [[
    'host' => '127.0.0.1', 
    'port' => 1212,
    'name' => 'email'
        ]],
    'instance' => true
]);

//redis队列类注入配置
$config->set("redisQueue", [
    'class' => '\Ext\Queue\Redis',
    'params' => [[
    'host' => '192.168.27.101', 
    'port' => 6379,
    'name' => 'email'
        ]],
    'instance' => true
]);

//redis队列类注入配置
$config->set("log", [
    'class' => '\Ext\Log\File',
    'params' => [[
    'file' => '/tmp/log.txt', 
        ]],
    'instance' => true
]);

