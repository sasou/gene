<?php
$r = new \Gene\Router();
// no routes registered => every match() misses => exercise the no-match branch
function probe($label, callable $fn, $iters = 20000) {
    for ($i = 0; $i < 100; $i++) $fn();
    gc_collect_cycles();
    $u0 = memory_get_usage();
    for ($i = 0; $i < $iters; $i++) $fn();
    gc_collect_cycles();
    $d = memory_get_usage() - $u0;
    printf("%-40s %+10d B (%.2f B/call)\n", $label, $d, $d / $iters);
}
var_dump($r->match('GET', '/no/such/route'));
probe('Router::match() miss', function () use ($r) { $r->match('GET', '/no/such/route'); });
echo "DONE\n";
