<?php
declare(strict_types=1);

require_once __DIR__ . '/bootstrap.php';

/**
 * Converts profiler evidence into an explicit implement/defer decision.
 * Input is a hand-collected JSON file:
 * {"cpu_samples":{"route_pc":3.2},"allocation_share":{"config_path":5.4},
 *  "p99_spikes":["sql_builder"],"rss_monotonic_growth":[]}
 */
function acceptance_profile_gate(array $profile): array
{
    $cpu = $profile['cpu_samples'] ?? [];
    $alloc = $profile['allocation_share'] ?? [];
    $p99 = array_flip($profile['p99_spikes'] ?? []);
    $rss = array_flip($profile['rss_monotonic_growth'] ?? []);
    $candidates = [
        'route_pc_prewarm' => ['signal' => 'route_pc', 'change' => 'workerReady route descriptor prewarm'],
        'config_path_cache' => ['signal' => 'config_path', 'change' => 'frozen-config segment cache'],
        'sql_builder' => ['signal' => 'sql_builder', 'change' => 'property-slot and smart_str capacity optimization'],
        'route_key_builder' => ['signal' => 'route_registration', 'change' => 'single-allocation route key builder'],
    ];
    $decisions = [];
    foreach ($candidates as $id => $candidate) {
        $signal = $candidate['signal'];
        $eligible = (float) ($cpu[$signal] ?? 0) >= 3.0
            || (float) ($alloc[$signal] ?? 0) >= 5.0
            || isset($p99[$signal])
            || isset($rss[$signal]);
        $decisions[$id] = [
            'status' => $eligible ? 'IMPLEMENT_IN_INDEPENDENT_PR' : 'DEFER',
            'reason' => $eligible ? 'profile threshold reached' : 'no CPU/allocation/p99/RSS threshold reached',
            'change' => $candidate['change'],
        ];
    }
    return ['timestamp' => acceptance_now(), 'decisions' => $decisions];
}

if (PHP_SAPI === 'cli' && realpath($_SERVER['SCRIPT_FILENAME'] ?? '') === __FILE__) {
    $input = $argv[1] ?? '';
    if ($input === '' || !is_file($input)) {
        fwrite(STDERR, "Usage: php tools/acceptance/profile_gate.php profile.json\n");
        exit(64);
    }
    $profile = json_decode((string) file_get_contents($input), true, 512, JSON_THROW_ON_ERROR);
    echo json_encode(acceptance_profile_gate($profile), JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE) . PHP_EOL;
}
