<?php
namespace Gene\Cache;

/**
 * Redis
 *
 * Redis 缓存封装，支持单节点和多节点集群（随机选节点），
 * 自动序列化/反序列化数组和对象，断线自动重连重试。
 *
 * 支持两种工作模式：
 *
 * 1. 直连模式（FPM / Swoole 均可）：
 *    $redis = new Gene\Cache\Redis([
 *        'host' => '127.0.0.1', 'port' => 6379, 'timeout' => 1.0,
 *        'password' => 'secret', 'serializer' => 1,
 *    ]);
 *
 * 2. 连接池模式（仅 Swoole 协程，runtime_type >= 2）：
 *    // onWorkerStart 中预先创建连接池：
 *    Gene\Cache\RedisPool::create('cache', 'redis', ['min' => 2, 'max' => 10]);
 *
 *    // 业务代码中使用（config 加入 'pool' 键）：
 *    $redis = new Gene\Cache\Redis([
 *        'host' => '127.0.0.1', 'port' => 6379, 'timeout' => 1.0,
 *        'pool' => 'cache',   // 指向已注册的 RedisPool 名称，自动从池中借连接
 *    ]);
 *    $result = $redis->get('key');
 *    $redis->release();  // 显式归还连接（可选，__destruct 自动归还）
 *
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.2
 */
class Redis
{
    /**
     * Redis 连接配置数组
     * @var array|null
     */
    public $config;

    /**
     * 底层 phpredis Redis 对象
     * @var \Redis|null
     */
    public $obj;

    /**
     * Gene 层序列化模式（0=不序列化, 1=JSON, 2=igbinary）
     * @var int
     */
    public $rehandler = 1;

    /**
     * 连接池模式下指向所属 Gene\Cache\RedisPool 实例的引用。
     * 非连接池模式下为 null。
     * @var RedisPool|null
     */
    public $pool = null;

    // -------------------------------------------------------------------------

    /**
     * __construct
     *
     * 直连模式：创建 phpredis 连接并存入 $obj。
     * 连接池模式：当 $config['pool'] 为已注册的 RedisPool 名称且
     *            runtime_type >= 2 时，自动从池中借出一个连接。
     *
     * @param array $config 配置数组，支持以下键：
     *
     *   连接参数：
     *   - host        string        Redis 主机（单节点模式）
     *   - port        int           Redis 端口（默认 6379）
     *   - timeout     float         连接超时（秒）
     *   - password    string|array  AUTH 密码；Redis 6 ACL 可传 [user, pass]
     *   - persistent  string        持久连接标识（仅非 Swoole 模式生效）
     *   - servers     array         多节点列表 [['host'=>..., 'port'=>...], ...]（随机选节点）
     *   - options     array         Redis::setOption 选项，数字键为 Redis::OPT_* 常量
     *   - serializer  int           Gene 层序列化模式（0=禁用, 1=JSON, 2=igbinary）
     *   - ttl         int           默认过期时间（秒，0=永不过期）
     *
     *   连接池参数（仅 Swoole 协程模式有效）：
     *   - pool        string        已注册的 Gene\Cache\RedisPool 名称；
     *                               设置此项后自动进入连接池模式，
     *                               连接从池中借出，host/port 等参数仅供
     *                               连接池内部连接创建时使用，此对象本身
     *                               不再直接创建连接。
     */
    public function __construct($config) {}

    // -------------------------------------------------------------------------

    /**
     * get
     *
     * 获取缓存值，自动反序列化。
     * 断线时自动重连（直连模式）或重新借出连接（连接池模式）后重试一次。
     *
     * @param string|array $key 缓存 key；传数组时批量获取（mGet），
     *                          返回以 key 为索引的关联数组
     * @return mixed 返回缓存值；不存在时返回 false；mGet 时返回关联数组
     */
    public function get($key) {}

    /**
     * set
     *
     * 设置缓存值，自动序列化数组/对象。
     * TTL > 0 时使用 setEx，否则使用 set（永不过期）。
     * 断线时自动重连后重试一次。
     *
     * @param string     $key   缓存 key
     * @param mixed      $value 缓存值
     * @param int|null   $ttl   过期时间（秒）；null 时使用 config['ttl'] 默认值
     * @return bool
     */
    public function set($key, $value, $ttl = null) {}

    /**
     * __call
     *
     * 动态代理：透传调用底层 phpredis Redis 对象的任意方法。
     * 断线时自动重连（直连模式）或重新借出连接（连接池模式）后重试一次。
     *
     * 示例：
     *   $redis->incr('counter');
     *   $redis->hSet('hash', 'field', 'value');
     *   $redis->expire('key', 60);
     *
     * @param string $method Redis 方法名（大小写不敏感）
     * @param array  $params 方法参数数组
     * @return mixed
     */
    public function __call($method, $params) {}

    // -------------------------------------------------------------------------

    /**
     * release
     *
     * 显式将借出的 Redis 连接归还给连接池。
     *
     * 适用场景：希望在对象仍存活期间提前释放连接，
     * 例如一个请求中只在开头使用 Redis，之后有大量耗时计算，
     * 不希望连接被长时间占用。
     *
     * 归还后 $obj 变为 null；若后续再次调用 get/set/__call，
     * 会自动重新从池中借出连接（仅连接池模式）。
     *
     * 非连接池模式下调用此方法无副作用。
     *
     * @return null
     */
    public function release() {}

    /**
     * free
     *
     * 释放当前 Redis 连接。
     *
     * 连接池模式：将连接归还给连接池（等同于 release()）。
     * 直连模式：  将 $obj 置 null，断开连接。
     *
     * @return null
     */
    public function free() {}

    /**
     * __destruct
     *
     * 析构时自动处理连接资源：
     *
     * 连接池模式：若 $obj 仍不为 null（未手动 release），自动归还连接池。
     *            池的 put() 会执行 PING 验活，死连接自动丢弃并递减计数。
     *
     * 直连模式：phpredis 对象随 GC 释放，连接自动关闭。
     */
    public function __destruct() {}
}
