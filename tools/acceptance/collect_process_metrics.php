<?php
declare(strict_types=1);

require_once __DIR__ . '/bootstrap.php';

function acceptance_collect_process_metrics(array $config, string $output): array
{
    $pid = (int) ($config['pid'] ?? 0);
    if ($pid <= 0) {
        return ['status' => ACCEPTANCE_BLOCKED, 'reason' => 'config.pid is required'];
    }
    $samples = max(1, (int) ($config['samples'] ?? 1));
    $interval = max(1, (int) ($config['interval_seconds'] ?? 10));
    $rows = [];
    for ($i = 0; $i < $samples; $i++) {
        $row = ['timestamp' => acceptance_now(), 'pid' => $pid];
        if (DIRECTORY_SEPARATOR === '/' && is_readable("/proc/{$pid}/status")) {
            $status = (string) file_get_contents("/proc/{$pid}/status");
            preg_match('/VmRSS:\s+(\d+)\s+kB/', $status, $rss);
            preg_match('/VmHWM:\s+(\d+)\s+kB/', $status, $hwm);
            $row['rss_kb'] = isset($rss[1]) ? (int) $rss[1] : null;
            $row['hwm_kb'] = isset($hwm[1]) ? (int) $hwm[1] : null;
        } else {
            $row['rss_kb'] = null;
            $row['hwm_kb'] = null;
            $row['note'] = 'RSS collection requires Linux /proc';
        }
        $rows[] = $row;
        if ($i + 1 < $samples) {
            sleep($interval);
        }
    }
    $file = $output . '/memory.csv';
    $fp = fopen($file, 'wb');
    fputcsv($fp, array_keys($rows[0]));
    foreach ($rows as $row) {
        fputcsv($fp, $row);
    }
    fclose($fp);
    return ['status' => ACCEPTANCE_PASS, 'samples' => count($rows), 'file' => $file];
}
