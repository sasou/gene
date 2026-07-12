<?php
declare(strict_types=1);

require __DIR__ . '/bootstrap.php';

if (!extension_loaded('swoole') || !extension_loaded('gene')) {
    fwrite(STDERR, "BLOCKED: requires loaded swoole and gene extensions.\n");
    exit(2);
}

$options = getopt('', ['coroutines::', 'omit-cleanup-rate::']);
$count = max(1, (int) ($options['coroutines'] ?? 10000));
$omitRate = min(1.0, max(0.0, (float) ($options['omit-cleanup-rate'] ?? 0)));
$before = (new Gene\Memory())->stats();
Swoole\Coroutine\run(static function () use ($count, $omitRate): void {
    for ($i = 0; $i < $count; $i++) {
        go(static function () use ($i, $omitRate): void {
            Gene\Application::getInstance();
            if (($i % 10000) >= (int) ($omitRate * 10000)) {
                Gene\Application::cleanup(true);
            }
        });
    }
});
$after = (new Gene\Memory())->stats();
$result = compact('count', 'omitRate', 'before', 'after');
echo json_encode($result, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE) . PHP_EOL;
