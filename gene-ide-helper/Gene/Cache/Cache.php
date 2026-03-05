<?php
namespace Gene\Cache;

/**
 * Cache
 *
 * 透明缓存装饰器，代理调用任意 [class, method] 并将返回值缓存到外部存储（Redis/Memcached 等）
 * 或本地 APCu 进程缓存，支持版本号控制的缓存失效策略。
 *
 * 配置示例（config.ini.php）：
 *   $config->set("cache", [
 *       'class'  => '\Gene\Cache\Cache',
 *       'params' => [['hook' => 'redis', 'sign' => 'myapp:', 'versionSign' => 'db:']],
 *       'instance' => false
 *   ]);
 *
 * @author  sasou<admin@php-gene.com>
 * @version  5.2.0
 */
class Cache
{
    public $config;

    /**
     * __construct
     *
     * @param array $configs 配置数组，支持以下键：
     *   - hook         string  DI 中注册的外部缓存组件名（如 "redis"/"memcache"）
     *   - sign         string  缓存 key 前缀
     *   - versionSign  string  版本号 key 前缀
     */
    public function __construct($configs) {}

    /**
     * cached
     * 代理调用对象方法并将结果缓存到外部缓存（未命中时执行调用）
     *
     * @param array $obj 被代理的可调用，格式 [$instance_or_class, 'methodName']
     * @param array $args 传递给被代理方法的参数数组
     * @param int|null $ttl 缓存时间（秒），null 表示使用 hook 组件默认值
     * @return mixed
     */
    public function cached($obj, $args, $ttl = null) {}

    /**
     * localCached
     * 代理调用对象方法并将结果缓存到本地进程 APCu（未命中时执行调用）
     *
     * @param array $obj 被代理的可调用，格式 [$instance_or_class, 'methodName']
     * @param array $args 传递给被代理方法的参数数组
     * @param int|null $ttl 缓存时间（秒）
     * @return mixed
     */
    public function localCached($obj, $args, $ttl = null) {}

    /**
     * unsetCached
     * 删除外部缓存中对应的缓存项
     *
     * @param array $obj 被代理的可调用，格式 [$instance_or_class, 'methodName']
     * @param array $args 传递给被代理方法的参数数组（用于计算缓存 key）
     * @param int|null $ttl 忽略，保留参数兼容性
     * @return mixed
     */
    public function unsetCached($obj, $args, $ttl = null) {}

    /**
     * unsetLocalCached
     * 删除 APCu 本地缓存中对应的缓存项
     *
     * @param array $obj 被代理的可调用，格式 [$instance_or_class, 'methodName']
     * @param array $args 传递给被代理方法的参数数组（用于计算缓存 key）
     * @param int|null $ttl 忽略，保留参数兼容性
     * @return mixed
     */
    public function unsetLocalCached($obj, $args, $ttl = null) {}

    /**
     * cachedVersion
     * 带版本号控制的外部缓存；当版本号变更时自动失效并重新调用
     *
     * @param array $obj 被代理的可调用，格式 [$instance_or_class, 'methodName']
     * @param array $args 传递给被代理方法的参数数组
     * @param array $versionField 关联的版本字段数组（如 ['user', 'group']）
     * @param int|null $ttl 缓存时间（秒）
     * @param mixed|null $mode 保留参数
     * @return mixed
     */
    public function cachedVersion($obj, $args, $versionField, $ttl = null, $mode = null) {}

    /**
     * localCachedVersion
     * 带版本号控制的本地 APCu 缓存；版本从外部缓存读取
     *
     * @param array $obj 被代理的可调用，格式 [$instance_or_class, 'methodName']
     * @param array $args 传递给被代理方法的参数数组
     * @param array $versionField 关联的版本字段数组
     * @param int|null $ttl 缓存时间（秒）
     * @param mixed|null $mode 保留参数
     * @return mixed
     */
    public function localCachedVersion($obj, $args, $versionField, $ttl = null, $mode = null) {}

    /**
     * getVersion
     * 从外部缓存读取指定版本字段的当前版本值
     *
     * @param string|array $version 版本字段名或字段名数组
     * @return mixed
     */
    public function getVersion($version) {}

    /**
     * updateVersion
     * 对指定版本字段执行 incr，使关联的版本号缓存失效
     *
     * @param string|array $version 版本字段名或字段名数组
     * @return mixed
     */
    public function updateVersion($version) {}
}
