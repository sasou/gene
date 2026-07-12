<?php
declare(strict_types=1);

require_once __DIR__ . '/bootstrap.php';

function acceptance_fpm_benchmark(array $config, string $root): array
{
    $command = $config['benchmark_command'] ?? null;
    if (!is_array($command) || $command === []) {
        return ['status' => ACCEPTANCE_BLOCKED, 'reason' => 'benchmark_command must be provided by the prepared environment'];
    }
    $rounds = max(1, (int) ($config['rounds'] ?? 5));
    $warmup = max(0, (int) ($config['warmup_seconds'] ?? 60));
    if ($warmup > 0) {
        sleep($warmup);
    }
    $results = [];
    for ($round = 1; $round <= $rounds; $round++) {
        $results[] = ['round' => $round] + acceptance_command(array_map('strval', $command), $root);
    }
    return [
        'status' => !array_filter($results, static fn(array $r): bool => $r['exit_code'] !== 0) ? ACCEPTANCE_PASS : ACCEPTANCE_FAIL,
        'timestamp' => acceptance_now(),
        'results' => $results,
    ];
}
