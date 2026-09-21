<?php
declare(strict_types=1);

require __DIR__ . '/bootstrap.php';

if (!extension_loaded('swoole') || !extension_loaded('gene')) {
    fwrite(STDERR, "BLOCKED: requires loaded swoole and gene extensions.\n");
    exit(2);
}
if (Gene\Application::getRuntimeType() < 2) {
    fwrite(STDERR, "BLOCKED: run with gene.runtime_type=2 or 3.\n");
    exit(2);
}

$options = getopt('', ['pool::', 'pool-max::', 'workers::', 'iterations::']);
$poolType = ($options['pool'] ?? 'db') === 'redis' ? 'redis' : 'db';
$poolMax = max(2, (int) ($options['pool-max'] ?? 4));
$workers = max(2, (int) ($options['workers'] ?? 32));
$iterations = max(10, (int) ($options['iterations'] ?? 100));

function lifecycle_pool(string $type, int $max, float $waitTimeout = 1.0): object
{
    if ($type === 'redis') {
        $config = [
            'host' => getenv('GENE_REDIS_HOST') ?: '127.0.0.1',
            'port' => (int) (getenv('GENE_REDIS_PORT') ?: 6379),
            'timeout' => (float) (getenv('GENE_REDIS_TIMEOUT') ?: 3),
            'min' => $max,
            'max' => $max,
            'idleTimeout' => 3600,
            'waitTimeout' => $waitTimeout,
        ];
        $password = getenv('GENE_REDIS_PASS');
        if (is_string($password) && $password !== '') {
            $config['password'] = $password;
        }
        return new Gene\Cache\RedisPool($config);
    }

    $dsn = getenv('GENE_MYSQL_DSN');
    $user = getenv('GENE_MYSQL_USER');
    if (!is_string($dsn) || $dsn === '' || !is_string($user) || $user === '') {
        throw new RuntimeException('set GENE_MYSQL_DSN and GENE_MYSQL_USER');
    }
    return new Gene\Pool([
        'dsn' => $dsn,
        'username' => $user,
        'password' => getenv('GENE_MYSQL_PASS') ?: '',
        'min' => $max,
        'max' => $max,
        'idleTimeout' => 3600,
        'waitTimeout' => $waitTimeout,
    ]);
}

function lifecycle_probe(string $type, object $connection): bool
{
    try {
        if ($type === 'redis') {
            $pong = $connection->ping();
            return $pong === true || $pong === 'PONG' || $pong === '+PONG';
        }
        return $connection->query('SELECT 1') !== false;
    } catch (Throwable $e) {
        return false;
    }
}

function lifecycle_settled(array $stats): bool
{
    return ($stats['closed'] ?? false) === false
        && ($stats['using'] ?? -1) === 0
        && ($stats['idle'] ?? -1) === ($stats['total'] ?? -2);
}

$results = [];
$blocked = null;
Swoole\Coroutine\run(static function () use ($poolType, $poolMax, $workers, $iterations, &$results, &$blocked): void {
    try {
        $pool = lifecycle_pool($poolType, $poolMax, 1.0);
        $held = [];
        for ($i = 0; $i < $poolMax; $i++) {
            $held[] = $pool->get();
        }
        if (count(array_filter($held, 'is_object')) !== $poolMax) {
            throw new RuntimeException('unable to fill pool to max');
        }
        $waited = 0.0;
        $waiterOk = false;
        $done = new Swoole\Coroutine\Channel(1);
        go(static function () use ($pool, $poolType, &$waited, &$waiterOk, $done): void {
            $start = microtime(true);
            $connection = $pool->get();
            $waited = microtime(true) - $start;
            $waiterOk = is_object($connection) && lifecycle_probe($poolType, $connection);
            if (is_object($connection)) {
                $pool->put($connection);
            }
            $done->push(true);
        });
        Swoole\Coroutine::sleep(0.2);
        $pool->put(array_pop($held));
        $done->pop(2.0);
        foreach ($held as $connection) {
            $pool->put($connection);
        }
        $stats = $pool->stats();
        $results['fullQueue'] = [
            'passed' => $waiterOk && $waited >= 0.15 && ($stats['overflow'] ?? -1) === 0 && lifecycle_settled($stats),
            'waitSeconds' => $waited,
            'stats' => $stats,
        ];
        $pool->close();

        $pool = lifecycle_pool($poolType, $poolMax, 1.0);
        $failures = 0;
        $wg = new Swoole\Coroutine\WaitGroup();
        for ($i = 0; $i < $workers; $i++) {
            $wg->add();
            go(static function () use ($pool, $poolType, $iterations, &$failures, $wg): void {
                try {
                    for ($j = 0; $j < $iterations; $j++) {
                        $connection = $pool->get();
                        if (!is_object($connection) || !lifecycle_probe($poolType, $connection)) {
                            $failures++;
                            if (is_object($connection)) {
                                $pool->remove();
                            }
                            continue;
                        }
                        $pool->put($connection);
                    }
                } finally {
                    $wg->done();
                }
            });
        }
        $wg->add();
        go(static function () use ($pool, $iterations, $wg): void {
            try {
                for ($i = 0; $i < $iterations; $i++) {
                    $pool->recycleIdle();
                    Swoole\Coroutine::sleep(0.001);
                }
            } finally {
                $wg->done();
            }
        });
        $wg->wait();
        $stats = $pool->stats();
        $results['recycleInterleave'] = [
            'passed' => $failures === 0 && lifecycle_settled($stats),
            'failures' => $failures,
            'stats' => $stats,
        ];
        $pool->close();

        $pool = lifecycle_pool($poolType, $poolMax, 2.0);
        $held = [];
        for ($i = 0; $i < $poolMax; $i++) {
            $held[] = $pool->get();
        }
        $waiterResult = 'pending';
        $done = new Swoole\Coroutine\Channel(1);
        go(static function () use ($pool, &$waiterResult, $done): void {
            $connection = $pool->get();
            $waiterResult = is_object($connection) ? 'connection' : 'null';
            if (is_object($connection)) {
                $pool->put($connection);
            }
            $done->push(true);
        });
        Swoole\Coroutine::sleep(0.1);
        go(static function () use ($pool): void {
            $pool->close();
        });
        Swoole\Coroutine::sleep(0.1);
        foreach ($held as $connection) {
            $pool->put($connection);
        }
        $done->pop(3.0);
        Swoole\Coroutine::sleep(0.1);
        $stats = $pool->stats();
        $results['closeInterleave'] = [
            'passed' => $waiterResult === 'null' && ($stats['closed'] ?? false) === true && ($stats['total'] ?? -1) === 0,
            'waiterResult' => $waiterResult,
            'stats' => $stats,
        ];
    } catch (Throwable $e) {
        $blocked = $e->getMessage();
    }
});

if (is_string($blocked)) {
    fwrite(STDERR, "BLOCKED: {$blocked}.\n");
    exit(2);
}
$passed = count($results) === 3 && !in_array(false, array_column($results, 'passed'), true);
echo json_encode([
    'pool' => $poolType,
    'poolMax' => $poolMax,
    'workers' => $workers,
    'iterations' => $iterations,
    'scenarios' => $results,
    'passed' => $passed,
], JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . PHP_EOL;
exit($passed ? 0 : 1);
