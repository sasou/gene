<?php
declare(strict_types=1);

require_once __DIR__ . '/bootstrap.php';

function acceptance_swoole_benchmark(array $config, string $root): array
{
    $php = $config['php_binary'] ?? 'php';
    $script = $config['swoole_verify_script'] ?? 'tools/verify_5_6_6_swoole.php';
    $matrix = [
        ['gene.swoole_getcid_capi=0', 'gene.route_precompile=0'],
        ['gene.swoole_getcid_capi=1', 'gene.route_precompile=0'],
        ['gene.swoole_getcid_capi=0', 'gene.route_precompile=1'],
        ['gene.swoole_getcid_capi=1', 'gene.route_precompile=1'],
    ];
    $results = [];
    $digests = [];
    foreach ($matrix as $ini) {
        $command = [$php, '-d', $ini[0], '-d', $ini[1], $script];
        $run = acceptance_command($command, $root);
        preg_match('/RESULT-DIGEST=([a-f0-9]+)/i', $run['stdout'], $match);
        $digest = $match[1] ?? null;
        if ($digest !== null) {
            $digests[] = $digest;
        }
        $results[] = ['ini' => $ini, 'digest' => $digest] + $run;
    }
    $consistent = count($digests) === 4 && count(array_unique($digests)) === 1;
    $passed = $consistent && !array_filter($results, static fn(array $r): bool => $r['exit_code'] !== 0);
    return ['status' => $passed ? ACCEPTANCE_PASS : ACCEPTANCE_FAIL, 'results' => $results, 'digest_consistent' => $consistent];
}
