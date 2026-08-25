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

$options = getopt('', ['pool::', 'pool-max::', 'coroutines::', 'iterations::']);
$poolType = ($options['pool'] ?? 'db') === 'redis' ? 'redis' : 'db';
$poolClass = $poolType === 'redis' ? Gene\Cache\RedisPool::class : Gene\Pool::class;
$coroutines = max(1, (int) ($options['coroutines'] ?? 100));
$iterations = max(1, (int) ($options['iterations'] ?? 100));
$poolMax = max(2, (int) ($options['pool-max'] ?? min(32, $coroutines)));
// One-shot Coroutine\run() must not leave idle-recycler timers running; otherwise
// Swoole keeps the event loop alive after work finishes (stopTimers() below run never runs).
$poolOptions = ['min' => 2, 'max' => $poolMax, 'idleTimeout' => 0];

$config = new Gene\Config();
if ($poolType === 'db') {
    $dsn = getenv('GENE_MYSQL_DSN');
    $user = getenv('GENE_MYSQL_USER');
    if (!is_string($dsn) || $dsn === '' || !is_string($user) || $user === '') {
        fwrite(STDERR, "BLOCKED: set GENE_MYSQL_DSN and GENE_MYSQL_USER.\n");
        exit(2);
    }
    $config->set('acceptance_db', [
        'class' => Gene\Db\Mysql::class,
        'params' => [[
            $dsn,
            $user,
            getenv('GENE_MYSQL_PASS') ?: '',
            [2 => 5], // PDO::ATTR_TIMEOUT (1002 is MYSQL_ATTR_INIT_COMMAND)
        ]],
    ]);
} else {
    $redisParams = [
        'host' => getenv('GENE_REDIS_HOST') ?: '127.0.0.1',
        'port' => (int) (getenv('GENE_REDIS_PORT') ?: 6379),
        'timeout' => (float) (getenv('GENE_REDIS_TIMEOUT') ?: 3),
    ];
    // RedisPool issues AUTH whenever the key is present, so an empty password
    // would kill every connection on a password-less Redis.
    $redisPass = getenv('GENE_REDIS_PASS');
    if (is_string($redisPass) && $redisPass !== '') {
        $redisParams['password'] = $redisPass;
    }
    $config->set('acceptance_redis', [
        'class' => Gene\Cache\Redis::class,
        'params' => [$redisParams],
    ]);
}

/**
 * Explain why a pool handed out no connection: either the persistent config
 * never reached the pool (missing dsn/host) or the backend refused the
 * connection. Both look identical from get() === null.
 */
function pool_diagnose(object $pool): string
{
    $resolved = [];
    try {
        $property = new ReflectionProperty($pool, 'config');
        $property->setAccessible(true);
        $value = $property->getValue($pool);
        if (is_array($value)) {
            foreach (['dsn', 'username', 'host', 'port'] as $key) {
                if (isset($value[$key])) {
                    $resolved[$key] = $key === 'username' ? '<set>' : $value[$key];
                }
            }
            if (isset($value['password'])) {
                $resolved['password'] = $value['password'] === '' ? '<empty>' : '<set>';
            }
        }
    } catch (Throwable $e) {
        return 'unable to read pool config: ' . $e->getMessage();
    }

    if ($resolved === []) {
        return 'pool config carries no connection parameters — the config key was not resolved';
    }
    $summary = 'pool config = ' . json_encode($resolved, JSON_UNESCAPED_SLASHES);

    if (isset($resolved['dsn'])) {
        try {
            new PDO(
                (string) $resolved['dsn'],
                (string) (getenv('GENE_MYSQL_USER') ?: ''),
                (string) (getenv('GENE_MYSQL_PASS') ?: ''),
                [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_PERSISTENT => false]
            );
            $summary .= '; a direct PDO connect with the same dsn SUCCEEDED, so the pool path is at fault';
        } catch (Throwable $e) {
            $summary .= '; direct PDO connect failed too: ' . $e->getMessage();
        }
    }
    return $summary;
}

$failures = 0;
$stats = [];
$passed = false;
$blocked = null;
Swoole\Coroutine\run(static function () use (
    $poolType, $poolClass, $poolOptions, $coroutines, $iterations, &$failures, &$stats, &$passed, &$blocked
): void {
    if (!$poolClass::getInstance('acceptance')) {
        $configKey = $poolType === 'db' ? 'acceptance_db' : 'acceptance_redis';
        $poolClass::create('acceptance', $configKey, $poolOptions);
    }

    $pool = $poolClass::getInstance('acceptance');
    if (!$pool) {
        $blocked = "failed to create named pool 'acceptance'";
        return;
    }

    $probe = $pool->get();
    if ($probe === false || $probe === null) {
        $blocked = 'failed to borrow a connection from pool: ' . pool_diagnose($pool);
        return;
    }
    $pool->put($probe);

    $wg = new Swoole\Coroutine\WaitGroup();
    for ($i = 0; $i < $coroutines; $i++) {
        $wg->add();
        go(static function () use ($pool, $iterations, &$failures, $wg): void {
            try {
                for ($j = 0; $j < $iterations; $j++) {
                    $connection = $pool->get();
                    if ($connection === false || $connection === null) {
                        $failures++;
                        continue;
                    }
                    $pool->put($connection);
                }
            } finally {
                $wg->done();
            }
        });
    }
    $wg->wait();
    $poolClass::stopTimers();
    $stats = $pool->stats();
    $passed = $failures === 0
        && ($stats['using'] ?? -1) === 0
        && ($stats['idle'] ?? -1) === ($stats['total'] ?? -2);
});

if (is_string($blocked)) {
    fwrite(STDERR, "BLOCKED: {$blocked}.\n");
    $poolClass::closeAll();
    $poolClass::stopTimers();
    exit(2);
}
echo json_encode([
    'class' => $poolClass,
    'coroutines' => $coroutines,
    'iterations' => $iterations,
    'poolMax' => $poolMax,
    'failures' => $failures,
    'stats' => $stats,
    'passed' => $passed,
], JSON_PRETTY_PRINT) . PHP_EOL;
$poolClass::closeAll();
$poolClass::stopTimers();
exit($passed ? 0 : 1);
