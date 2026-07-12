<?php
declare(strict_types=1);

require_once __DIR__ . '/bootstrap.php';

function acceptance_functional(array $config, string $root): array
{
    $commands = $config['functional_commands'] ?? [
        ['command' => ['php', 'TestRunner.php'], 'cwd' => 'test'],
        ['command' => ['php', 'tools/verify_5_6_6.php']],
    ];
    $results = [];
    foreach ($commands as $entry) {
        if (!is_array($entry) || $entry === []) {
            continue;
        }
        $command = isset($entry['command']) ? $entry['command'] : $entry;
        if (!is_array($command) || $command === []) {
            continue;
        }
        $cwd = isset($entry['cwd']) ? $root . DIRECTORY_SEPARATOR . $entry['cwd'] : $root;
        $run = acceptance_command(array_map('strval', $command), $cwd);
        $results[] = ['command' => $command, 'cwd' => $cwd] + $run;
    }
    $passed = !array_filter($results, static fn(array $result): bool => $result['exit_code'] !== 0);
    return [
        'status' => $passed ? ACCEPTANCE_PASS : ACCEPTANCE_FAIL,
        'timestamp' => acceptance_now(),
        'results' => $results,
    ];
}
