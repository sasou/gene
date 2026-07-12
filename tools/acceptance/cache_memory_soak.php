<?php
declare(strict_types=1);

require __DIR__ . '/bootstrap.php';

$options = getopt('', ['keys::']);
$keys = max(1, (int) ($options['keys'] ?? 5000));
if (!extension_loaded('gene')) {
    fwrite(STDERR, "BLOCKED: Gene extension is not loaded.\n");
    exit(2);
}
$memory = new Gene\Memory();
$before = $memory->stats();
$cache = new Gene\Cache\Cache(['sign' => 'acceptance:']);
$producer = new class {
    public function value(string $key): string { return $key; }
};
for ($i = 0; $i < $keys; $i++) {
    $cache->processCached([$producer, 'value'], ["key-{$i}"]);
}
$after = $memory->stats();
$cap = (int) ini_get('gene.cache_max_items');
$delta = ($after['cache_business_items'] ?? $after['cache_items']) - ($before['cache_business_items'] ?? $before['cache_items']);
$passed = $cap <= 0 || $delta <= $cap;
$result = compact('keys', 'cap', 'before', 'after', 'delta', 'passed');
echo json_encode($result, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE) . PHP_EOL;
exit($passed ? 0 : 1);
