<?php
declare(strict_types=1);

const ACCEPTANCE_PASS = 'PASS';
const ACCEPTANCE_FAIL = 'FAIL';
const ACCEPTANCE_BLOCKED = 'BLOCKED';
const ACCEPTANCE_PARTIAL = 'PARTIAL';

function acceptance_json(string $path, array $data): void
{
    file_put_contents(
        $path,
        json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE) . PHP_EOL
    );
}

function acceptance_command(array $command, ?string $cwd = null): array
{
    $escaped = implode(' ', array_map('escapeshellarg', $command));
    $stdoutFile = tempnam(sys_get_temp_dir(), 'acc_out_');
    $stderrFile = tempnam(sys_get_temp_dir(), 'acc_err_');
    if ($stdoutFile === false || $stderrFile === false) {
        return ['exit_code' => 127, 'stdout' => '', 'stderr' => 'Unable to create temp files'];
    }
    // File-backed capture avoids Windows pipe-buffer deadlocks when children
    // (e.g. TestRunner LogTest) emit large mixed stdout/stderr.
    $spec = [
        0 => ['pipe', 'r'],
        1 => ['file', $stdoutFile, 'w'],
        2 => ['file', $stderrFile, 'w'],
    ];
    $process = proc_open($escaped, $spec, $pipes, $cwd);
    if (!is_resource($process)) {
        @unlink($stdoutFile);
        @unlink($stderrFile);
        return ['exit_code' => 127, 'stdout' => '', 'stderr' => 'Unable to start process'];
    }
    fclose($pipes[0]);
    $exitCode = proc_close($process);
    $stdout = (string) @file_get_contents($stdoutFile);
    $stderr = (string) @file_get_contents($stderrFile);
    @unlink($stdoutFile);
    @unlink($stderrFile);
    return ['exit_code' => $exitCode, 'stdout' => $stdout, 'stderr' => $stderr];
}

function acceptance_mkdir(string $path): void
{
    if (!is_dir($path) && !mkdir($path, 0777, true) && !is_dir($path)) {
        throw new RuntimeException("Cannot create output directory: {$path}");
    }
}

function acceptance_load_config(string $path): array
{
    if (!is_file($path)) {
        throw new InvalidArgumentException("Config does not exist: {$path}");
    }
    $config = json_decode((string) file_get_contents($path), true);
    if (!is_array($config)) {
        throw new InvalidArgumentException("Config is not valid JSON: {$path}");
    }
    return $config;
}

function acceptance_now(): string
{
    return gmdate('c');
}
