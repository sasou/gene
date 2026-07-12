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
    $spec = [1 => ['pipe', 'w'], 2 => ['pipe', 'w']];
    $process = proc_open($escaped, $spec, $pipes, $cwd);
    if (!is_resource($process)) {
        return ['exit_code' => 127, 'stdout' => '', 'stderr' => 'Unable to start process'];
    }
    $stdout = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    return ['exit_code' => proc_close($process), 'stdout' => $stdout, 'stderr' => $stderr];
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
