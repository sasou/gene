<?php
declare(strict_types=1);

require __DIR__ . '/bootstrap.php';

if (!extension_loaded('swoole') || !extension_loaded('gene')) {
    fwrite(STDERR, "BLOCKED: requires loaded swoole and gene extensions.\n");
    exit(2);
}

$options = getopt('', ['pool::', 'coroutines::', 'iterations::']);
$poolClass = ($options['pool'] ?? 'db') === 'redis' ? Gene\Cache\RedisPool::class : Gene\Pool::class;
$coroutines = max(1, (int) ($options['coroutines'] ?? 100));
$iterations = max(1, (int) ($options['iterations'] ?? 100));
if (!$poolClass::getInstance('acceptance')) {
    fwrite(STDERR, "BLOCKED: named pool 'acceptance' was not created by the prepared Swoole bootstrap.\n");
    exit(2);
}
$pool = $poolClass::getInstance('acceptance');
Swoole\Coroutine\run(static function () use ($pool, $coroutines, $iterations): void {
    for ($i = 0; $i < $coroutines; $i++) {
        go(static function () use ($pool, $iterations): void {
            for ($j = 0; $j < $iterations; $j++) {
                $connection = $pool->get();
                if ($connection !== false && $connection !== null) {
                    $pool->put($connection);
                }
            }
        });
    }
});
$stats = $pool->stats();
$passed = ($stats['using'] ?? -1) === 0;
echo json_encode(['class' => $poolClass, 'stats' => $stats, 'passed' => $passed], JSON_PRETTY_PRINT) . PHP_EOL;
exit($passed ? 0 : 1);
