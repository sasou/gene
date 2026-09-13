<?php
namespace Ext;

/**
 * LocalStore — 基于 Gene\Memory 的进程级 KV 适配器。
 *
 * 同时满足两套存储契约，使 GENE_DEMO_LOCAL=1 本地模式完全去除外部服务：
 *  - Session 存储句柄：get(string) / set(k, v, ttl) / delete(k)
 *  - Cache 版本钩子：get(string|array)（数组走 mget）/ set / incr / del
 */
class LocalStore extends \Gene\Service
{
    /**
     * @var \Gene\Memory
     */
    private $mem;

    public function __construct()
    {
        $this->mem = \Gene\Di::get('memory');
    }

    /**
     * get — 字符串 key 单取；数组 key 走 mget（cachedVersion 版本键批量查询）
     * @param string|array $key
     * @return mixed
     */
    public function get($key)
    {
        return is_array($key) ? $this->mem->mget($key) : $this->mem->get($key);
    }

    /**
     * set
     * @param string $key
     * @param mixed $value
     * @param int $ttl
     * @return bool
     */
    public function set($key, $value, $ttl = 0)
    {
        return $this->mem->set($key, $value, (int)$ttl);
    }

    /**
     * incr — updateVersion 的版本号自增
     * @param string $key
     * @param int $step
     * @return int|false
     */
    public function incr($key, $step = 1)
    {
        return $this->mem->incr($key, (int)$step);
    }

    /**
     * del
     * @param string $key
     * @return bool
     */
    public function del($key)
    {
        return $this->mem->del($key);
    }

    /**
     * delete — Session 句柄契约方法名（Memory 侧为 del）
     * @param string $key
     * @return bool
     */
    public function delete($key)
    {
        return $this->mem->del($key);
    }
}
