<?php
namespace Gene;

/**
 * Gene\Pool - Swoole协程连接池
 *
 * 在Swoole协程模式下管理PDO数据库连接，减少重复连接带来的资源消耗，提高并发性能。
 * 支持最小/最大连接数、空闲回收、自动重连、进程启动初始化、进程关闭释放。
 *
 * @author  sasou<admin@php-gene.com>
 * @version 1.0.0
 */
class Pool
{
    /**
     * 命名池实例注册表（worker进程级别）
     * @var array<string, self>
     */
    private static $instances = [];

    /**
     * @var \Swoole\Coroutine\Channel
     */
    private $channel;

    /**
     * 连接配置
     * @var array
     */
    private $config;

    /**
     * 最小连接数
     * @var int
     */
    private $min;

    /**
     * 最大连接数
     * @var int
     */
    private $max;

    /**
     * 空闲超时时间（秒）
     * @var int
     */
    private $idleTimeout;

    /**
     * 当前总连接数（空闲 + 借出）
     * @var int
     */
    private $currentCount = 0;

    /**
     * 空闲回收定时器ID
     * @var int|null
     */
    private $timerId = null;

    /**
     * 获取连接的等待超时（秒）
     * @var float
     */
    private $waitTimeout;

    /**
     * 池是否已关闭
     * @var bool
     */
    private $closed = false;

    /**
     * 构造连接池
     *
     * @param array $config 配置参数：
     *   - dsn:         string  PDO DSN
     *   - username:    string  数据库用户名
     *   - password:    string  数据库密码
     *   - options:     array   PDO选项（可选）
     *   - min:         int     最小连接数（默认1）
     *   - max:         int     最大连接数（默认10）
     *   - idleTimeout: int     空闲超时秒数（默认60）
     *   - waitTimeout: float   获取连接等待超时秒数（默认3.0）
     * @throws \RuntimeException 在非Swoole模式下运行时抛出异常
     */
    public function __construct(array $config)
    {
        // 检查运行时模式：只在Swoole模式下允许使用连接池
        if (!class_exists('\Swoole\Coroutine\Channel')) {
            throw new \RuntimeException('Gene\\Pool can only be used in Swoole coroutine mode');
        }
        
        $this->min = max(0, (int)($config['min'] ?? 1));
        $this->max = max(1, (int)($config['max'] ?? 10));
        if ($this->min > $this->max) {
            $this->min = $this->max;
        }
        $this->idleTimeout = max(0, (int)($config['idleTimeout'] ?? 60));
        $this->waitTimeout = (float)($config['waitTimeout'] ?? 3.0);
        $this->config = $config;

        $this->channel = new \Swoole\Coroutine\Channel($this->max);

        $this->fill();

        if ($this->idleTimeout > 0) {
            $this->startIdleRecycler();
        }
    }

    /**
     * 创建命名连接池并注册到全局注册表
     *
     * @param string $name  池名称
     * @param array  $config 配置参数（同构造函数）
     * @return self
     * @throws \RuntimeException 在非Swoole模式下运行时抛出异常
     */
    public static function create(string $name, array $config): self
    {
        // 检查运行时模式：只在Swoole模式下允许使用连接池
        if (!defined('SWOOLE_RUNTIME') && !class_exists('\Swoole\Coroutine\Channel')) {
            throw new \RuntimeException('Gene\\Pool can only be used in Swoole coroutine mode');
        }
        
        if (isset(self::$instances[$name])) {
            self::$instances[$name]->close();
        }
        $pool = new self($config);
        self::$instances[$name] = $pool;
        return $pool;
    }

    /**
     * 获取命名连接池实例
     *
     * @param string $name 池名称
     * @return self|null
     */
    public static function getInstance(string $name): ?self
    {
        return self::$instances[$name] ?? null;
    }

    /**
     * 从池中获取一个PDO连接
     *
     * 优先从空闲队列取出，若空闲队列为空且未达最大连接数则创建新连接，
     * 若已达最大连接数则等待其他协程归还连接（带超时）。
     * 取出的连接会进行健康检查，失效连接自动丢弃并重试。
     *
     * @return \PDO|null 成功返回PDO对象，超时或失败返回null
     */
    public function get(): ?\PDO
    {
        if ($this->closed) {
            return null;
        }

        $retries = 0;
        $maxRetries = $this->max + 2;

        while ($retries < $maxRetries) {
            // 1. 尝试从空闲队列取
            if (!$this->channel->isEmpty()) {
                $item = $this->channel->pop(0.001);
                if ($item === false) {
                    // Channel竞争失败，进入创建逻辑
                } else {
                    if ($this->isAlive($item['conn'])) {
                        return $item['conn'];
                    }
                    // 失效连接，丢弃
                    $this->currentCount--;
                    $retries++;
                    continue;
                }
            }

            // 2. 未达上限，创建新连接
            if ($this->currentCount < $this->max) {
                $conn = $this->createConnection();
                if ($conn !== null) {
                    return $conn;
                }
                // 创建失败，fallthrough等待
            }

            // 3. 已达上限，等待归还
            $item = $this->channel->pop($this->waitTimeout);
            if ($item === false) {
                return null; // 超时
            }
            if ($this->isAlive($item['conn'])) {
                return $item['conn'];
            }
            // 失效连接，丢弃并重试
            $this->currentCount--;
            $retries++;
        }

        return null;
    }

    /**
     * 归还PDO连接到池中
     *
     * @param \PDO $pdo PDO连接对象
     * @return void
     */
    public function put(\PDO $pdo): void
    {
        if ($this->closed) {
            $this->currentCount--;
            return;
        }

        if ($this->channel->isFull()) {
            // 池已满，丢弃多余连接
            $this->currentCount--;
            return;
        }

        $this->channel->push([
            'conn' => $pdo,
            'lastUsed' => time(),
        ]);
    }

    /**
     * 通知池一个借出的连接已失效（不归还）
     *
     * 当连接在使用中断开（如MySQL server has gone away），调用此方法
     * 让池减少计数，以便后续能创建新连接填补。
     *
     * @return void
     */
    public function remove(): void
    {
        $this->currentCount--;
        if ($this->currentCount < 0) {
            $this->currentCount = 0;
        }
    }

    /**
     * 关闭连接池，释放所有空闲连接，停止定时器
     *
     * @return void
     */
    public function close(): void
    {
        if ($this->closed) {
            return;
        }
        $this->closed = true;

        if ($this->timerId !== null) {
            \Swoole\Timer::clear($this->timerId);
            $this->timerId = null;
        }

        while (!$this->channel->isEmpty()) {
            $item = $this->channel->pop(0.001);
            if ($item !== false) {
                $this->currentCount--;
            }
        }

        $this->channel->close();
    }

    /**
     * 关闭所有命名连接池
     *
     * @return void
     */
    public static function closeAll(): void
    {
        foreach (self::$instances as $pool) {
            $pool->close();
        }
        self::$instances = [];
    }

    /**
     * 获取连接池状态
     *
     * @return array{total: int, idle: int, using: int, min: int, max: int, closed: bool}
     */
    public function stats(): array
    {
        return [
            'total'  => $this->currentCount,
            'idle'   => $this->channel->length(),
            'using'  => $this->currentCount - $this->channel->length(),
            'min'    => $this->min,
            'max'    => $this->max,
            'closed' => $this->closed,
        ];
    }

    /**
     * 预填充最小连接数
     *
     * @return void
     */
    private function fill(): void
    {
        for ($i = 0; $i < $this->min; $i++) {
            $conn = $this->createConnection();
            if ($conn !== null) {
                $this->channel->push([
                    'conn' => $conn,
                    'lastUsed' => time(),
                ]);
            }
        }
    }

    /**
     * 创建一个新的PDO连接
     *
     * @return \PDO|null
     */
    private function createConnection(): ?\PDO
    {
        try {
            $options = $this->config['options'] ?? [];
            $options[\PDO::ATTR_ERRMODE] = \PDO::ERRMODE_EXCEPTION;
            $options[\PDO::ATTR_EMULATE_PREPARES] = false;

            $pdo = new \PDO(
                $this->config['dsn'],
                $this->config['username'] ?? '',
                $this->config['password'] ?? '',
                $options
            );
            $this->currentCount++;
            return $pdo;
        } catch (\PDOException $e) {
            return null;
        }
    }

    /**
     * 检查PDO连接是否存活
     *
     * @param \PDO $pdo
     * @return bool
     */
    private function isAlive(\PDO $pdo): bool
    {
        try {
            $pdo->getAttribute(\PDO::ATTR_SERVER_INFO);
            return true;
        } catch (\PDOException $e) {
            return false;
        }
    }

    /**
     * 启动空闲连接回收定时器
     *
     * @return void
     */
    private function startIdleRecycler(): void
    {
        $interval = max((int)($this->idleTimeout / 2), 1) * 1000;
        $this->timerId = \Swoole\Timer::tick($interval, function () {
            $this->recycleIdle();
        });
    }

    /**
     * 回收超时空闲连接，保持最小连接数
     *
     * @return void
     */
    private function recycleIdle(): void
    {
        if ($this->closed) {
            return;
        }

        $now = time();
        $size = $this->channel->length();
        $kept = [];

        for ($i = 0; $i < $size; $i++) {
            $item = $this->channel->pop(0.001);
            if ($item === false) {
                break;
            }

            if ($this->currentCount > $this->min
                && ($now - $item['lastUsed']) > $this->idleTimeout) {
                // 丢弃超时空闲连接
                $this->currentCount--;
                continue;
            }

            $kept[] = $item;
        }

        // 放回保留的连接
        foreach ($kept as $item) {
            $this->channel->push($item);
        }

        // 补充到最小连接数
        while ($this->currentCount < $this->min && !$this->channel->isFull()) {
            $conn = $this->createConnection();
            if ($conn !== null) {
                $this->channel->push([
                    'conn' => $conn,
                    'lastUsed' => time(),
                ]);
            } else {
                break;
            }
        }
    }

    public function __destruct()
    {
        $this->close();
    }
}
