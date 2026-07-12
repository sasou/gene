<?php
declare(strict_types=1);

require_once __DIR__ . '/bootstrap.php';

function acceptance_preflight(array $config): array
{
    $checks = [];
    $expect = $config['expect'] ?? [];
    $add = static function (string $name, bool $passed, string $detail = '') use (&$checks): void {
        $checks[] = compact('name', 'passed', 'detail');
    };

    $add('PHP version', PHP_VERSION_ID >= ($expect['php_version_id_min'] ?? 80000), PHP_VERSION);
    $add('Gene extension loaded', extension_loaded('gene'), (string) phpversion('gene'));
    $add('Gene version', !isset($expect['gene_version']) || phpversion('gene') === $expect['gene_version'], (string) phpversion('gene'));
    $add('Expected SAPI', !isset($expect['sapi']) || PHP_SAPI === $expect['sapi'], PHP_SAPI);
    foreach (($expect['extensions'] ?? []) as $extension) {
        $add("Extension {$extension}", extension_loaded((string) $extension), extension_loaded((string) $extension) ? 'loaded' : 'not loaded');
    }
    foreach (($expect['ini'] ?? []) as $key => $value) {
        $actual = ini_get((string) $key);
        $add("INI {$key}", (string) $actual === (string) $value, "actual={$actual}, expected={$value}");
    }

    $endpoint = $config['endpoint'] ?? null;
    if (is_string($endpoint) && $endpoint !== '') {
        $headers = @get_headers($endpoint);
        $add('Endpoint reachable', $headers !== false, $headers === false ? $endpoint : (string) $headers[0]);
    }
    $passed = !array_filter($checks, static fn(array $check): bool => !$check['passed']);
    return [
        'status' => $passed ? ACCEPTANCE_PASS : ACCEPTANCE_BLOCKED,
        'timestamp' => acceptance_now(),
        'php_version' => PHP_VERSION,
        'php_sapi' => PHP_SAPI,
        'gene_version' => phpversion('gene') ?: null,
        'checks' => $checks,
    ];
}

if (PHP_SAPI === 'cli' && realpath($_SERVER['SCRIPT_FILENAME'] ?? '') === __FILE__) {
    $configPath = $argv[1] ?? '';
    $result = acceptance_preflight(acceptance_load_config($configPath));
    echo json_encode($result, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE) . PHP_EOL;
    exit($result['status'] === ACCEPTANCE_PASS ? 0 : 2);
}
