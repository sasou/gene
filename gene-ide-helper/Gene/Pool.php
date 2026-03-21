<?php
namespace Gene;

/**
 * Pool - Swoole协程连接池
 *
 * 在Swoole协程模式下管理PDO数据库连接，减少重复连接带来的资源消耗，提高并发性能。
 * 支持最小/最大连接数、空闲回收、自动重连、进程启动初始化、进程关闭释放。
 *
 * FPM模式下不可用，仅在Swoole协程环境中生效。
 *
 * @author  sasou<admin@php-gene.com>
 * @version 1.0.0
 */
class Pool
{
    /**
     * 创建命名连接池并注册到全局注册表
     *
     * @param string $name   池名称（与db config中的pool参数对应）
     * @param array  $config 配置参数：
     *   - dsn:         string  PDO DSN
     *   - username:    string  数据库用户名
     *   - password:    string  数据库密码
     *   - options:     array   PDO选项（可选）
     *   - min:         int     最小连接数（默认1）
     *   - max:         int     最大连接数（默认10）
     *   - idleTimeout: int     空闲超时秒数（默认60）
     *   - waitTimeout: float   获取连接等待超时秒数（默认3.0）
     * @return self
     */
    public static function create(string $name, array $config): self {
        return new self($config);
    }

    /**
     * 获取命名连接池实例
     *
     * @param string $name 池名称
     * @return self|null
     */
    public static function getInstance(string $name): ?self {
        return null;
    }

    /**
     * 从池中获取一个PDO连接
     *
     * @return \PDO|null
     */
    public function get(): ?\PDO {
        return null;
    }

    /**
     * 归还PDO连接到池中
     *
     * @param \PDO $pdo
     * @return void
     */
    public function put(\PDO $pdo): void {
    }

    /**
     * 通知池一个借出的连接已失效（不归还）
     *
     * @return void
     */
    public function remove(): void {
    }

    /**
     * 关闭连接池，释放所有空闲连接
     *
     * @return void
     */
    public function close(): void {
    }

    /**
     * 关闭所有命名连接池
     *
     * @return void
     */
    public static function closeAll(): void {
    }

    /**
     * 获取连接池状态
     *
     * @return array{total: int, idle: int, using: int, min: int, max: int, closed: bool}
     */
    public function stats(): array {
        return [];
    }
}
