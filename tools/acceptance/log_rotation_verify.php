<?php
declare(strict_types=1);

require __DIR__ . '/bootstrap.php';

if (!extension_loaded('swoole') || !extension_loaded('gene')) {
    fwrite(STDERR, "BLOCKED: requires loaded swoole and gene extensions.\n");
    exit(2);
}
if (Gene\Application::getRuntimeType() < 2 || !filter_var(ini_get('gene.log_keep_open'), FILTER_VALIDATE_BOOL)) {
    fwrite(STDERR, "BLOCKED: run with gene.runtime_type=2 and gene.log_keep_open=1.\n");
    exit(2);
}

$options = getopt('', ['output:', 'lines::']);
$output = $options['output'] ?? null;
$lines = max(10, (int) ($options['lines'] ?? 100));
if (!is_string($output) || $output === '') {
    fwrite(STDERR, "Usage: log_rotation_verify.php --output=PATH [--lines=100]\n");
    exit(64);
}
if (!is_dir($output) && !mkdir($output, 0777, true) && !is_dir($output)) {
    fwrite(STDERR, "Unable to create output directory: {$output}\n");
    exit(2);
}

$path = rtrim($output, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . 'keep-open.log';
$rotated = $path . '.1';
@unlink($path);
@unlink($rotated);
Gene\Log::setLevel(Gene\Log::LEVEL_DEBUG);
Gene\Log::setFile($path);

Gene\Log::info('RENAME-BEFORE');
$renameOk = rename($path, $rotated);
sleep(2);
Gene\Log::info('RENAME-AFTER');
$rotatedBody = is_file($rotated) ? (string) file_get_contents($rotated) : '';
$currentBody = is_file($path) ? (string) file_get_contents($path) : '';
$renamePassed = $renameOk
    && str_contains($rotatedBody, 'RENAME-BEFORE')
    && !str_contains($rotatedBody, 'RENAME-AFTER')
    && str_contains($currentBody, 'RENAME-AFTER');

Gene\Log::info('TRUNCATE-BEFORE');
$truncateOk = file_put_contents($path, '') !== false;
sleep(2);
Gene\Log::info('TRUNCATE-AFTER');
$truncatedBody = is_file($path) ? (string) file_get_contents($path) : '';
$truncatePassed = $truncateOk
    && !str_contains($truncatedBody, 'TRUNCATE-BEFORE')
    && str_contains($truncatedBody, 'TRUNCATE-AFTER');

$crashPath = rtrim($output, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . 'abnormal-exit.log';
@unlink($crashPath);
$process = new Swoole\Process(static function (Swoole\Process $worker) use ($crashPath, $lines): void {
    Gene\Log::setLevel(Gene\Log::LEVEL_DEBUG);
    Gene\Log::setFile($crashPath);
    for ($i = 0; $i < $lines; $i++) {
        Gene\Log::info(sprintf('ABNORMAL-LINE-%04d', $i));
    }
    $worker->write("ready\n");
    while (true) {
        sleep(1);
    }
}, true, SOCK_STREAM, true);
$pid = $process->start();
$ready = trim((string) $process->read()) === 'ready';
if ($pid > 0) {
    Swoole\Process::kill($pid, 9);
    Swoole\Process::wait(true);
}
$crashBody = is_file($crashPath) ? (string) file_get_contents($crashPath) : '';
$abnormalLines = substr_count($crashBody, 'ABNORMAL-LINE-');
$abnormalPassed = $ready && $abnormalLines === $lines;

$result = [
    'rename' => ['passed' => $renamePassed, 'rotated' => $rotated, 'current' => $path],
    'copytruncate' => ['passed' => $truncatePassed, 'path' => $path],
    'abnormalExit' => ['passed' => $abnormalPassed, 'expectedLines' => $lines, 'actualLines' => $abnormalLines, 'path' => $crashPath],
];
$passed = !in_array(false, array_column($result, 'passed'), true);
echo json_encode(['scenarios' => $result, 'passed' => $passed], JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . PHP_EOL;
exit($passed ? 0 : 1);
