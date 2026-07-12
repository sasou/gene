<?php
declare(strict_types=1);

require __DIR__ . '/bootstrap.php';

function acceptance_compare_results(string $baselinePath, string $candidatePath, array $thresholds): array
{
    $baseline = json_decode((string) file_get_contents($baselinePath), true, 512, JSON_THROW_ON_ERROR);
    $candidate = json_decode((string) file_get_contents($candidatePath), true, 512, JSON_THROW_ON_ERROR);
    $metrics = [];
    foreach (($thresholds['metrics'] ?? []) as $metric => $rule) {
        $before = $baseline['metrics'][$metric] ?? null;
        $after = $candidate['metrics'][$metric] ?? null;
        if (!is_numeric($before) || !is_numeric($after) || (float) $before == 0.0) {
            $metrics[$metric] = ['status' => ACCEPTANCE_BLOCKED, 'reason' => 'missing or zero baseline'];
            continue;
        }
        $change = ((float) $after - (float) $before) / (float) $before;
        $maxRegression = (float) ($rule['max_regression'] ?? 0.03);
        $metrics[$metric] = [
            'baseline' => (float) $before,
            'candidate' => (float) $after,
            'change' => $change,
            'status' => $change <= $maxRegression ? ACCEPTANCE_PASS : ACCEPTANCE_FAIL,
        ];
    }
    $failed = array_filter($metrics, static fn(array $metric): bool => $metric['status'] === ACCEPTANCE_FAIL);
    return ['status' => $failed ? ACCEPTANCE_FAIL : ACCEPTANCE_PASS, 'metrics' => $metrics];
}
