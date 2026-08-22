<?php
namespace Gene;

/**
 * Memory
 *
 * 进程级共享内存缓存，基于 Zend 持久化 HashTable 实现。
 * 数据跨请求持续存活（Worker 进程生命周期），不依赖外部存储，
 * 适合高频读取、低频更新的数据（如配置、路由、权限表等）。
 *
 * 注意：每个 Worker 进程拥有独立的内存空间，数据不在进程间共享。
 *
 * 配置示例（config.ini.php）：
 *   $config->set("memory", [
 *       'class'    => '\Gene\Memory',
 *       'params'   => [['myapp']],
 *       'instance' => true
 *   ]);
 *
 * @author  sasou<admin@php-gene.com>
 * @version  5.4.3
 */
class Memory
{
    /**
     * __construct
     *
     * @param string|null $safe 命名空间前缀（默认使用 app_key 或应用目录路径）
     */
    public function __construct($safe = null) {}

    /**
     * set
     * 将值存入共享内存，key 自动加命名空间前缀
     *
     * @param string $key 缓存 key
     * @param mixed $value 值（支持 string/array/标量）
     * @param int $ttl 过期时间（秒，0=永不过期）
     * @return bool
     */
    public function set($key, $value, $ttl = 0) {}

    /**
     * get
     * 从共享内存读取值，key 不存在时返回 null
     *
     * @param string $key 缓存 key
     * @return mixed
     */
    public function get($key) {}

    /**
     * getTime
     * 获取某 key 的写入时间戳
     *
     * @param string $key 缓存 key
     * @return int|null
     */
    public function getTime($key) {}

    /**
     * exists
     * 检查 key 是否存在于共享内存
     *
     * @param string $key 缓存 key
     * @return bool
     */
    public function exists($key) {}

    /**
     * del
     * 从共享内存删除指定 key（正确处理持久化内存的 free）
     *
     * @param string $key 缓存 key
     * @return bool
     */
    public function del($key) {}

    /**
     * incr
     * 原子自增（读-改-写全程在写锁内完成）。key 不存在时以 $step 为初始值创建；
     * 已有值为非整数时返回 false。与 set 一样受 Swoole workerReady 冻结约束。
     *
     * @param string $key 缓存 key
     * @param int $step 步长（默认 1）
     * @return int|false 自增后的值
     */
    public function incr($key, $step = 1) {}

    /**
     * decr
     * 原子自减，语义同 incr
     *
     * @param string $key 缓存 key
     * @param int $step 步长（默认 1）
     * @return int|false 自减后的值
     */
    public function decr($key, $step = 1) {}

    /**
     * Single-process / single-worker rate limit (not shared across FPM/Swoole workers).
     * After Swoole workerReady() Memory is frozen — use Redis::rateLimit.
     *
     * @param string $key
     * @param int $max
     * @param int $windowSec
     * @return bool
     */
    public function rateLimit($key, $max, $windowSec) {}

    /**
     * Process-local lock: SET NX EX. Returns token or false.
     *
     * @param string $key
     * @param int $ttlSec
     * @return string|false
     */
    public function lock($key, $ttlSec) {}

    /**
     * Compare-and-del unlock.
     *
     * @param string $key
     * @param string $token
     * @return bool
     */
    public function unlock($key, $token) {}

    /**
     * clean
     * 销毁并重新初始化整个共享内存 HashTable
     *
     * @return bool
     */
    public function clean() {}

    /**
     * stats
     * 获取共享内存统计信息
     *
     * @return array{
     *     cache_items: int,        // 主缓存项数量
     *     cache_easy_items: int,   // 简单缓存项数量
     *     fn_cache_items: int,     // 闭包路由分发缓存项数量
     *     co_contexts_items: int,  // 存活的 Swoole 协程上下文数量
     *     co_contexts_max: int,    // 配置的软上限
     *     ctx_pool_size: int,      // 回收的上下文结构池大小
     *     ctx_pool_max: int       // 池容量
     * }
     */
    public function stats() {}

    /**
     * mget
     * Get multiple cache values by their keys at once.
     *
     * @param array $keys list of cache keys
     * @return array associative array of found key=>value pairs
     */
    public function mget(array $keys) {}

    /**
     * mset
     * Set multiple cache key=>value pairs at once with the same TTL.
     *
     * @param array $items associative array of key=>value pairs
     * @param int $ttl time-to-live in seconds (0 = no expiry)
     * @return bool
     */
    public function mset(array $items, int $ttl = 0) {}
}
