<?php
/* Front controller for PerfV3Test: run under `php-cgi -T N` so one process
 * serves N requests the way an FPM worker does (the router file is executed
 * only on the first one). The stats line is printed after each request. */
$root = __DIR__;
$app = \Gene\Application::getInstance();
$app->autoload($root . '/app')
    ->load('router.php', $root . '/config/');
$t0 = hrtime(true);
$app->run();
$runNs = hrtime(true) - $t0;
$s = (new \Gene\Memory())->stats();
echo "\nSTATS " . json_encode([
    'run_ns' => $runNs,
    'eval' => $s['route_eval_calls'],
    'compiled' => $s['route_eval_compiled'],
    'files' => $s['route_eval_files'],
    'error_eval' => $s['route_error_eval_calls'],
]) . "\n";
