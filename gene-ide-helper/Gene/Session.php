<?php
namespace Gene;

/**
 * Session
 *
 * Session 管理类，支持多种存储驱动（通过 DI 注入的 driver 组件实现）。
 * 驱动对象需实现 get(id)、set(id, data)、delete(id) 三个方法。
 *
 * @author  sasou<admin@php-gene.com>
 * @version  5.1.5
 */
class Session
{
    /**
     * __construct
     *
     * @param array $config 配置数组，支持以下键：
     *   - driver    string  DI 中注册的存储驱动组件名（如 "memcache"/"redis"）
     *   - prefix    string  SessionId Cookie 名称前缀
     *   - name      string  Cookie 名称（默认 "PHPSESSID"）
     *   - domain    string  Cookie 作用域
     *   - path      string  Cookie 路径（默认 "/"）
     *   - secure    bool    是否仅 HTTPS
     *   - httponly  bool    是否禁止 JS 访问
     *   - ttl       int     Session 数据过期时间（秒）
     *   - uttl      int     Cookie 的 Max-Age（秒，0=浏览器会话）
     */
    public function __construct(array $config) {}

    /**
     * load
     * 从存储驱动中加载 Session 数据
     *
     * @return static
     */
    public function load() {}

    /**
     * save
     * 将 Session 数据持久化到存储驱动并刷新 Cookie
     *
     * @return static
     */
    public function save() {}

    /**
     * get
     * 获取 Session 值，支持点号路径（如 "user.id"）
     *
     * @param string|null $name 键名，不传则返回全部数据
     * @return mixed
     */
    public function get($name = null) {}

    /**
     * set
     * 设置 Session 值，支持点号路径（如 "user.id"），自动持久化
     *
     * @param string $name 键名（支持点号路径）
     * @param mixed $value 值
     * @return bool
     */
    public function set($name, $value) {}

    /**
     * del
     * 删除指定 Session 值，支持点号路径
     *
     * @param string $name 键名（支持点号路径）
     * @return bool
     */
    public function del($name) {}

    /**
     * has
     * 判断 Session 键是否存在
     *
     * @param string $name 键名
     * @return bool
     */
    public function has($name) {}

    /**
     * destroy
     * 清除所有 Session 数据并重新生成 SessionId
     *
     * @return bool
     */
    public function destroy() {}

    /**
     * cookie
     * 使用 Session 的 path/domain 等配置直接设置 Cookie
     *
     * @param mixed $name Cookie 名称
     * @param mixed $value Cookie 值
     * @param mixed $time 过期时间（时间戳或秒数）
     * @return static
     */
    public function cookie($name, $value, $time) {}

    /**
     * getSessionId
     * 获取当前 SessionId（不含前缀的 Cookie 值）
     *
     * @return string|null
     */
    public function getSessionId() {}

    /**
     * setSessionId
     * 手动设置 SessionId（Swoole 等场景下替代 Cookie 读取）
     *
     * @param mixed $cookie SessionId 值
     * @return static
     */
    public function setSessionId($cookie) {}

    /**
     * setLifeTime
     * 设置 Cookie 的生存时长（秒）
     *
     * @param mixed $time 时长（秒）
     * @return bool
     */
    public function setLifeTime($time) {}

    /**
     * __get
     *
     * @param string $name 键名
     * @return mixed
     */
    public function __get($name) {}

    /**
     * __set
     *
     * @param string $name 键名
     * @param mixed $value 值
     * @return bool
     */
    public function __set($name, $value) {}

    /**
     * __isset
     *
     * @param string $name 键名
     * @return bool
     */
    public function __isset($name) {}

    /**
     * __unset
     *
     * @param string $name 键名
     * @return bool
     */
    public function __unset($name) {}
}
