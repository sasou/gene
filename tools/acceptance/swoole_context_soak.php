<?php
declare(strict_types=1);

require __DIR__ . '/bootstrap.php';

if (!extension_loaded('swoole') || !extension_loaded('gene')) {
    fwrite(STDERR, "BLOCKED: requires loaded swoole and gene extensions.\n");
    exit(2);
}

$options = getopt('', ['coroutines::', 'concurrency::', 'omit-cleanup-rate::']);
$count = max(1, (int) ($options['coroutines'] ?? 10000));
$concurrency = max(1, min($count, (int) ($options['concurrency'] ?? 200)));
$omitRate = min(1.0, max(0.0, (float) ($options['omit-cleanup-rate'] ?? 0)));
$before = (new Gene\Memory())->stats();
$isolationFailures = 0;
Swoole\Coroutine\run(static function () use ($count, $concurrency, $omitRate, &$isolationFailures): void {
    for ($offset = 0; $offset < $count; $offset += $concurrency) {
        $batch = min($concurrency, $count - $offset);
        $wg = new Swoole\Coroutine\WaitGroup();
        for ($i = 0; $i < $batch; $i++) {
            $id = $offset + $i;
            $wg->add();
            go(static function () use ($id, $omitRate, &$isolationFailures, $wg): void {
                try {
                    Gene\Context::set('soak_id', $id);
                    Swoole\Coroutine::sleep(0.001);
                    if (Gene\Context::get('soak_id') !== $id) {
                        $isolationFailures++;
                    }
                    if (($id % 10000) >= (int) ($omitRate * 10000)) {
                        Gene\Application::cleanup();
                    }
                } finally {
                    $wg->done();
                }
            });
        }
        $wg->wait();
    }
});
$after = (new Gene\Memory())->stats();
$passed = $isolationFailures === 0
    && ($after['co_contexts_items'] ?? -1) === 0
    && ($after['ctx_pool_size'] ?? -1) <= ($after['ctx_pool_max'] ?? -1);
$result = compact('count', 'concurrency', 'omitRate', 'isolationFailures', 'before', 'after', 'passed');
echo json_encode($result, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE) . PHP_EOL;
exit($passed ? 0 : 1);
