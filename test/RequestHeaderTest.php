<?php

/**
 * Request::header() FPM fallback regression test
 *
 * The HEADER bag (request_attr index 7) used to be populated only by
 * Request::init()/initSwoole(); under php-cgi/FPM no caller filled it, so
 * Request::header() silently returned NULL for every header. The bag is now
 * materialized lazily: keys from the request's server bag (or the real
 * $_SERVER when none was injected) are normalized the same way Swoole/PSR
 * callers do — HTTP_* loses its prefix, underscores become dashes, names are
 * lowercased; CONTENT_TYPE and CONTENT_LENGTH (no HTTP_ prefix) are mapped.
 *
 * CLI note: writing $_SERVER from userland separates the array, so the C side
 * keeps seeing the pre-write copy. This file therefore exercises the injected
 * server-bag path (identical code path as FPM's materialize) plus an optional
 * real-$_SERVER check driven by a preset HTTP_X_CLI_TEST env var.
 */

use Gene\Request;

echo "=== Request::header() FPM Fallback Test ===\n\n";

$fail = 0;
function check($name, $cond) {
    global $fail;
    if ($cond) {
        echo "✓ $name\n";
    } else {
        $fail++;
        echo "✗ $name\n";
    }
}

// Injected server bag (init without header arg) -> header() materializes from
// it on first access. This is the same lazy path FPM takes against $_SERVER.
$req = new Request();
$req->init(
    null, null, null,
    [
        'REQUEST_METHOD' => 'GET',
        'REQUEST_URI' => '/',
        'HTTP_X_REGRESSION_HDR' => 'regv',
        'HTTP_SEC_FETCH_SITE' => 'cross-site',
        'CONTENT_TYPE' => 'application/x-test',
    ]
);

check("lowercase lookup resolves HTTP_* key", Request::header('x-regression-hdr') === 'regv');
check("uppercase lookup resolves via lowercase fallback", Request::header('X-REGRESSION-HDR') === 'regv');
check("dashed name maps underscored server key", Request::header('sec-fetch-site') === 'cross-site');
check("CONTENT_TYPE mapped without HTTP_ prefix", Request::header('content-type') === 'application/x-test');
check("missing header returns default", Request::header('x-absent-hdr', 'dflt') === 'dflt');
check("missing header returns null", Request::header('x-absent-hdr') === null);

$all = Request::header();
check("header() without name returns whole bag", is_array($all) && ($all['x-regression-hdr'] ?? null) === 'regv');
check("non HTTP_/CONTENT_ server keys excluded", !isset($all['request_method']) && !isset($all['request_uri']));

// An explicit init() header bag replaces the materialized one.
$req->init(null, null, null, null, null, null, null, ['x-init-hdr' => 'initv']);
check("init() header bag is authoritative", Request::header('x-init-hdr') === 'initv');
check("init() bag replaces materialized bag", Request::header('x-regression-hdr') === null);

// Optional real-$_SERVER coverage: launch the file with env var
// HTTP_X_CLI_TEST=<v> (CLI $_SERVER contains env vars at SAPI init) to verify
// the materialize path against the true superglobal, as under FPM.
if (isset($_SERVER['HTTP_X_CLI_TEST'])) {
    // Fresh request context not available in-process; assert via a child run.
    $bin = PHP_BINARY;
    $args = getenv('GENE_TEST_PHP_ARGS') ?: '';
    $script = 'var_dump((new Gene\\Request())->header("x-cli-test"));';
    $out = shell_exec("$bin $args -r " . escapeshellarg($script));
    check("real \$_SERVER path (HTTP_X_CLI_TEST env)", strpos((string) $out, $_SERVER['HTTP_X_CLI_TEST']) !== false);
} else {
    echo "- SKIP real-\$_SERVER check (set env HTTP_X_CLI_TEST to enable)\n";
}

echo "\n=== " . ($fail ? "$fail FAIL" : "all PASS") . " ===\n";
exit($fail ? 1 : 0);
