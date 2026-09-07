<?php
/**
 * Repro / verification for PERFORMANCE_OPTIMIZATION.md §1.1:
 *   Router::clear() used to leave GENE_G(route_pc) untouched, so precompiled
 *   dispatch descriptors kept borrowed pointers into the freed route tree and
 *   into fn_cache closures released by clear() — a dispatch that hit the same
 *   leaf HashTable address afterwards read freed memory.
 *
 * Expected after the fix ([GENE_FIX:2026-09-07 PC-GEN]):
 *   - every dispatch runs the *current* handler (H2 after the rebuild, never H1);
 *   - route_pc_generation advances on each clear()/delTree()/delEvent();
 *   - no crash / ASAN report under repeated clear() + dispatch.
 *
 * Usage (route_precompile is opt-in and Swoole+workerReady only):
 *   php -d gene.route_precompile=1 audit/repro/route_pc_clear_invalidate.php
 */
$app = \Gene\Application::getInstance();
$app->setRuntimeType(2);   /* route_pc requires runtime_type >= 2 */

function gen() {
    static $mem = null;
    if ($mem === null) $mem = new \Gene\Memory('pcgen');
    $s = $mem->stats();
    return [$s['route_pc_generation'] ?? -1, $s['route_pc_items'] ?? -1, $s['route_pc_retired'] ?? -1];
}

function report($label) {
    list($g, $i, $r) = gen();
    printf("%-28s generation=%d items=%d retired=%d\n", $label, $g, $i, $r);
}

$router = new \Gene\Router();
$router->clear()
    ->get('/hello', function () { echo "  handler=H1\n"; })
    ->get('/plain', function () { echo "  handler=P1\n"; });

$app->workerReady();
report('after workerReady');

echo "dispatch /hello x3 (expect H1 each time, memoized after the 1st)\n";
$app->run('get', '/hello');
$app->run('get', '/hello');
$app->run('get', '/hello');
report('after first dispatches');

/* Post-workerReady the process cache is frozen, so clear() cannot actually
 * rewrite the tree and emits warnings — expected, and irrelevant here: what
 * matters is that the descriptors are invalidated either way. Same output with
 * gene.route_precompile=0, which is the parity check. */
error_reporting(E_ALL & ~E_WARNING);

/* Rebuild the tree under the descriptor cache's feet. */
$router = new \Gene\Router();
$router->clear()
    ->get('/hello', function () { echo "  handler=H2\n"; })
    ->get('/plain', function () { echo "  handler=P2\n"; });
report('after clear + re-register');

echo "dispatch /hello (expect H2, not H1)\n";
$app->run('get', '/hello');
echo "dispatch /plain (expect P2, not P1)\n";
$app->run('get', '/plain');
report('after rebuild dispatch');

/* Hammer it: interleave clear() and dispatch so stale descriptors are hit
 * repeatedly and the retire list is exercised. */
for ($i = 0; $i < 200; $i++) {
    $r = new \Gene\Router();
    $r->clear()->get('/hello', function () {})->get('/plain', function () {});
    $app->run('get', '/hello');
    $app->run('get', '/plain');
}
report('after 200 clear+dispatch');

echo "DONE\n";
