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

if (!$poolClass::getInstance('acceptance')) {
    if ($poolType === 'db') {
        $dsn = getenv('GENE_MYSQL_DSN');
        $user = getenv('GENE_MYSQL_USER');
        if (!is_string($dsn) || $dsn === '' || !is_string($user) || $user === '') {
            fwrite(STDERR, "BLOCKED: set GENE_MYSQL_DSN and GENE_MYSQL_USER.\n");
            exit(2);
        }
        Gene\Config::set('acceptance_db', [[
            'dsn' => $dsn,
            'username' => $user,
            'password' => getenv('GENE_MYSQL_PASS') ?: '',
        ]]);
        Gene\Pool::create('acceptance', 'acceptance_db', ['min' => 2, 'max' => $poolMax]);
    } else {
        Gene\Config::set('acceptance_redis', [[
            'host' => getenv('GENE_REDIS_HOST') ?: '127.0.0.1',
            'port' => (int) (getenv('GENE_REDIS_PORT') ?: 6379),
            'timeout' => (float) (getenv('GENE_REDIS_TIMEOUT') ?: 3),
            'password' => getenv('GENE_REDIS_PASS') ?: '',
            'database' => (int) (getenv('GENE_REDIS_DB') ?: 0),
        ]]);
        Gene\Cache\RedisPool::create('acceptance', 'acceptance_redis', ['min' => 2, 'max' => $poolMax]);
    }
}

$pool = $poolClass::getInstance('acceptance');
if (!$pool) {
    fwrite(STDERR, "BLOCKED: failed to create named pool 'acceptance'.\n");
    exit(2);
}
$failures = 0;
Swoole\Coroutine\run(static function () use ($pool, $coroutines, $iterations, &$failures): void {
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
});
$stats = $pool->stats();
$passed = $failures === 0 && ($stats['using'] ?? -1) === 0 && ($stats['idle'] ?? -1) === ($stats['total'] ?? -2);
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
