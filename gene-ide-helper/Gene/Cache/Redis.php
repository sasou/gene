<?php
namespace Gene\Cache;

/**
 * Redis
 *
 * Redis 缓存封装，支持单节点和多节点集群（随机选节点），
 * 自动序列化/反序列化数组和对象，断线自动重连重试。
 *
 * 配置示例（config.ini.php）：
 *   $config->set("redis", [
 *       'class'  => '\Gene\Cache\Redis',
 *       'params' => [['persistent' => true, 'host' => '127.0.0.1', 'port' => 6379,
 *                     'timeout' => 3, 'ttl' => 0, 'serializer' => 1]],
 *       'instance' => true
 *   ]);
 *
 * @author  sasou<admin@php-gene.com>
 * @version  5.2.0
 */
class Redis
{
    public $config;
    public $obj;
    public $rehandler;

    /**
     * __construct
     *
     * @param array $config 配置数组，支持以下键：
     *   - host        string  Redis 主机（单节点模式）
     *   - port        int     Redis 端口（默认 6379）
     *   - servers     array   多节点配置 [['host'=>..., 'port'=>...], ...]（随机选节点）
     *   - timeout     int     连接超时（秒）
     *   - persistent  bool    是否持久连接
     *   - password    string  认证密码
     *   - options     array   Redis::setOption 选项键值对
     *   - serializer  int     序列化模式（0=不序列化, 1=JSON, 2=igbinary）
     *   - ttl         int     默认过期时间（秒，0=永不过期）
     */
    public function __construct($config) {}

    /**
     * get
     * 获取缓存，自动反序列化，断线时自动重连重试
     *
     * @param string|array $key 缓存 key；传数组时批量获取（mGet）
     * @return mixed
     */
    public function get($key) {}

    /**
     * set
     * 设置缓存，有 TTL 时用 setEx，否则用 set；自动序列化数组/对象，支持断线重连
     *
     * @param string $key 缓存 key
     * @param mixed $value 缓存值
     * @param int|null $ttl 过期时间（秒），null 时使用配置中的默认值
     * @return bool
     */
    public function set($key, $value, $ttl = null) {}

    /**
     * __call
     * 动态代理，透传调用底层 Redis 对象的任意方法，支持断线重连
     *
     * @param string $method 方法名
     * @param array $params 参数数组
     * @return mixed
     */
    public function __call($method, $params) {}
}
