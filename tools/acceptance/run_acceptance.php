<?php
declare(strict_types=1);

require_once __DIR__ . '/bootstrap.php';
require __DIR__ . '/preflight.php';
require __DIR__ . '/functional.php';
require __DIR__ . '/fpm_benchmark.php';
require __DIR__ . '/swoole_benchmark.php';
require __DIR__ . '/collect_process_metrics.php';

$options = getopt('', ['profile:', 'config:', 'output:']);
if (!isset($options['profile'], $options['config'], $options['output'])) {
    fwrite(STDERR, "Usage: php tools/acceptance/run_acceptance.php --profile=fpm|swoole --config=FILE --output=DIR\n");
    exit(64);
}

$root = dirname(__DIR__, 2);
$output = $options['output'];
acceptance_mkdir($output);
$config = acceptance_load_config($options['config']);
$environment = [
    'timestamp' => acceptance_now(),
    'php_version' => PHP_VERSION,
    'php_sapi' => PHP_SAPI,
    'gene_version' => phpversion('gene') ?: null,
    'profile' => $options['profile'],
    'config' => $options['config'],
];
acceptance_json($output . '/environment.json', $environment);

$preflight = acceptance_preflight($config);
acceptance_json($output . '/preflight.json', $preflight);
if ($preflight['status'] !== ACCEPTANCE_PASS) {
    acceptance_json($output . '/acceptance.json', ['status' => ACCEPTANCE_BLOCKED, 'preflight' => $preflight]);
    exit(2);
}

$functional = acceptance_functional($config, $root);
acceptance_json($output . '/functional.json', $functional);
if ($functional['status'] !== ACCEPTANCE_PASS) {
    acceptance_json($output . '/acceptance.json', ['status' => ACCEPTANCE_FAIL, 'functional' => $functional]);
    exit(1);
}

$benchmark = $options['profile'] === 'swoole'
    ? acceptance_swoole_benchmark($config, $root)
    : acceptance_fpm_benchmark($config, $root);
acceptance_json($output . '/benchmark.json', $benchmark);

$metrics = isset($config['metrics']) ? acceptance_collect_process_metrics($config['metrics'], $output) : null;
if ($metrics !== null) {
    acceptance_json($output . '/metrics.json', $metrics);
}
$status = $benchmark['status'] === ACCEPTANCE_PASS ? ACCEPTANCE_PASS : ACCEPTANCE_FAIL;
acceptance_json($output . '/acceptance.json', compact('status', 'preflight', 'functional', 'benchmark', 'metrics'));
exit($status === ACCEPTANCE_PASS ? 0 : 1);
