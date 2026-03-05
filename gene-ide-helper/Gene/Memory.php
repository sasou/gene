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
 * @version  5.1.5
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
     * clean
     * 销毁并重新初始化整个共享内存 HashTable
     *
     * @return bool
     */
    public function clean() {}
}
