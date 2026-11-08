<?php

/**
 * Performance-tuning V3 regression test (plan/Performance-tuning-V3.closed.md)
 *
 *  - INI defaults of the V3 switches.
 *  - Validate: hand-written mobile/date matchers and the direct PCRE path
 *    agree with preg_match() on fuzzed + boundary input.
 *  - Route programs (gene.route_eval_compile): under `php-cgi -T N` one
 *    process serves N requests like an FPM worker, so the router file runs
 *    only once and closure routes / closure error handlers go through the
 *    eval path. Output must be byte-identical with the switch on and off;
 *    with opcache the compiled path replaces eval; without opcache, with an
 *    unwritable cache dir or a tampered cache file it falls back to eval.
 *  - Business-table zero copy (gene.memory_business_zero_copy): borrowed
 *    values survive delete/overwrite in the same request and across requests.
 *  - Cache fill counters (FPM/CLI: no coalescing, concurrency 1).
 */

use Gene\Cache\Cache;
use Gene\Validate;

echo "=== Performance V3 Regression Test ===\n\n";

$fail = 0;
function check($name, $cond, $detail = '') {
    global $fail;
    if ($cond) {
        echo "✓ $name\n";
    } else {
        $fail++;
        echo "✗ $name" . ($detail !== '' ? " -- $detail" : '') . "\n";
    }
}

function child_php_args() {
    $env = getenv('GENE_TEST_PHP_ARGS');
    if (is_string($env) && $env !== '') {
        return $env;
    }
    if (php_ini_loaded_file() !== false) {
        return '';
    }
    $args = ['-n'];
    foreach (get_loaded_extensions() as $ext) {
        try {
            $file = (new ReflectionExtension($ext))->getFileName();
        } catch (Throwable $e) {
            $file = false;
        }
        if (is_string($file) && $file !== '') {
            $args[] = '-d extension=' . $file;
        }
    }
    return implode(' ', array_map('escapeshellarg', $args));
}

function run_cmd($cmd, array $env = [], $cwd = null) {
    $pipes = [];
    $proc = proc_open($cmd, [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['redirect', 1]],
        $pipes, $cwd, $env ? array_merge(getenv(), $env) : null);
    if (!is_resource($proc)) {
        return [null, -1];
    }
    fclose($pipes[0]);
    $out = stream_get_contents($pipes[1]);
    fclose($pipes[1]);
    return [$out, proc_close($proc)];
}

function temp_script($code) {
    $f = tempnam(sys_get_temp_dir(), 'pv3');
    file_put_contents($f, "<?php\n" . $code);
    register_shutdown_function(function () use ($f) { @unlink($f); });
    return $f;
}

function rrmdir($dir) {
    if (!is_dir($dir)) {
        if (is_file($dir)) @unlink($dir);
        return;
    }
    foreach (scandir($dir) as $f) {
        if ($f === '.' || $f === '..') continue;
        $p = $dir . DIRECTORY_SEPARATOR . $f;
        is_dir($p) ? rrmdir($p) : @unlink($p);
    }
    @rmdir($dir);
}

/* ---------- INI defaults ---------- */
echo "Testing INI defaults:\n";
check("route_precompile defaults to 1", ini_get('gene.route_precompile') === '1');
check("route_eval_compile defaults to 0", ini_get('gene.route_eval_compile') === '0');
check("memory_business_zero_copy defaults to 0", ini_get('gene.memory_business_zero_copy') === '0');
check("cache_fill_wait_ms defaults to 0", ini_get('gene.cache_fill_wait_ms') === '0');
check("http_keepalive defaults to 0", ini_get('gene.http_keepalive') === '0');

$stats = (new Gene\Memory())->stats();
$keys = ['route_eval_calls', 'route_eval_bytes', 'route_eval_compiled', 'route_eval_files',
    'route_error_eval_calls', 'route_dispatch_direct', 'route_dispatch_closure',
    'memory_business_copy_calls', 'memory_business_copy_bytes', 'memory_business_borrow_calls',
    'memory_business_retired_items', 'validate_regex_calls', 'http_php_calls',
    'cache_fill_calls', 'cache_fill_concurrent_max', 'cache_fill_coalesced', 'cache_fill_inflight'];
$missing = array_diff($keys, array_keys($stats));
check("Memory::stats() exports V3 counters", !$missing, implode(',', $missing));
echo "\n";

/* ---------- Validate equivalence ---------- */
echo "Testing Validate matchers vs preg_match:\n";
const RE_MOBILE = '/^(13[0-9]|14[01456879]|15[0-35-9]|16[2567]|17[0-8]|18[0-9]|19[0-35-9])\d{8}$/';
const RE_DATE = '/^\d{4}[\/-]\d{1,2}[\/-]\d{1,2}$/';

function v_rule($rule, $value) {
    $v = new Validate();
    $v->init(['f' => $value]);
    return $v->name('f')->$rule();
}

mt_srand(20261104);
$alpha = "0123456789/-\n a\xB2\xD9";
$mobileBad = 0; $dateBad = 0; $firstBad = '';
$cases = ['13800138000', "13800138000\n", "13800138000\n\n", '1380013800', '138001380000',
    '14200000000', '14700000000', '15400000000', '15500000000', '16100000000', '16200000000',
    '17900000000', '17800000000', '19400000000', '19900000000', '12000000000', '1380013800a',
    "1380013800\xB2", '', "\n", '2024-01-01', '2024/1/1', "2024-1-1\n", '2024-1-1-', '2024-123-1',
    '24-01-01', '2024-01-011', '2024--01', '2024-1/1', "2024-01-01\n\n", "2024-01-0\xD9"];
for ($i = 0; $i < 20000; $i++) {
    if ($i % 2) {
        $s = '1' . mt_rand(0, 9) . mt_rand(0, 9) . str_pad((string)mt_rand(0, 99999999), 8, '0', STR_PAD_LEFT);
        $r = mt_rand(0, 9);
        if ($r === 0) $s .= "\n";
        elseif ($r === 1) $s = substr($s, 0, -1);
        elseif ($r === 2) $s[mt_rand(0, 10)] = $alpha[mt_rand(0, strlen($alpha) - 1)];
    } else {
        $s = '';
        $len = mt_rand(0, 12);
        for ($j = 0; $j < $len; $j++) $s .= $alpha[mt_rand(0, 13)];
        if (mt_rand(0, 3) === 0) $s = sprintf('%04d%s%d%s%d', mt_rand(0, 9999), mt_rand(0, 1) ? '-' : '/', mt_rand(0, 120), mt_rand(0, 1) ? '-' : '/', mt_rand(0, 120));
    }
    $cases[] = $s;
}
foreach ($cases as $s) {
    if (v_rule('rule_mobile', $s) !== (preg_match(RE_MOBILE, $s) === 1)) {
        $mobileBad++;
        if ($firstBad === '') $firstBad = 'mobile:' . bin2hex($s);
    }
    if (v_rule('rule_date', $s) !== (preg_match(RE_DATE, $s) === 1)) {
        $dateBad++;
        if ($firstBad === '') $firstBad = 'date:' . bin2hex($s);
    }
}
check("rule_mobile == preg_match over " . count($cases) . " inputs", $mobileBad === 0, $firstBad);
check("rule_date == preg_match over " . count($cases) . " inputs", $dateBad === 0, $firstBad);
check("rule_mobile non-string (int) keeps preg_match semantics", v_rule('rule_mobile', 13800138000) === true);
check("rule_date non-string (false) is false", v_rule('rule_date', false) === false);

$v = new Validate();
$v->init(['f' => 'Hello World']);
check("rule_match direct PCRE hit", $v->name('f')->rule_match('/world/i') === true);
check("rule_match direct PCRE miss", $v->name('f')->rule_match('/^world/') === false);
$warn = '';
set_error_handler(function ($no, $str) use (&$warn) { $warn = $str; return true; });
$bad = $v->name('f')->rule_match('/(/');
restore_error_handler();
check("rule_match invalid pattern is false + warning", $bad === false && stripos($warn, 'compilation failed') !== false, $warn);
$v->init(['f' => 12345]);
check("rule_match int subject uses legacy coercion", $v->name('f')->rule_match('/^\d+$/') === true);
$before = (new Gene\Memory())->stats()['validate_regex_calls'];
$v->init(['f' => '13800138000']);
$v->name('f')->rule_mobile();
check("hand-written mobile skips PCRE", (new Gene\Memory())->stats()['validate_regex_calls'] === $before);
echo "\n";

/* ---------- Cache fill counters + zero copy (in a child) ---------- */
echo "Testing business-table zero copy and fill counters:\n";
$php = escapeshellarg(PHP_BINARY);
$args = child_php_args();
$zc = <<<'PHP'
class H { public static $n = 0; function p($k) { self::$n++; return ['k' => $k, 'list' => range(1, 5), 's' => str_repeat('x', 40)]; } }
$c = new Gene\Cache\Cache(['sign' => 'pv3:']); $h = new H;
$a = $c->processCached([$h, 'p'], ['a']);
$b = $c->processCached([$h, 'p'], ['a']);
$st = (new Gene\Memory())->stats();
$r = ['same' => $a === $b, 'n' => H::$n, 'borrow' => $st['memory_business_borrow_calls'], 'copy' => $st['memory_business_copy_calls'],
      'fill' => $st['cache_fill_calls'], 'max' => $st['cache_fill_concurrent_max'], 'coalesced' => $st['cache_fill_coalesced']];
$c->unsetProcessCached([$h, 'p'], ['a']);
$r['intact'] = $b['list'] === [1, 2, 3, 4, 5] && strlen($b['s']) === 40;
$r['retired'] = (new Gene\Memory())->stats()['memory_business_retired_items'];
$d = $c->processCached([$h, 'p'], ['a']);
$e = $c->processCached([$h, 'p'], ['a']);
$e['list'][] = 6;
$r['cow'] = count($d['list']) === 5 && count($e['list']) === 6;
(new Gene\Memory())->clean();
$r['after_clean'] = $e['k'] === 'a' && $d['s'] === str_repeat('x', 40);
echo json_encode($r);
PHP;
foreach ([0, 1] as $z) {
    [$out] = run_cmd("$php $args -d gene.memory_business_zero_copy=$z " . escapeshellarg(temp_script($zc)));
    $r = json_decode(trim((string)$out), true);
    if (!is_array($r)) {
        check("zero_copy=$z child run", false, substr((string)$out, 0, 300));
        continue;
    }
    check("zero_copy=$z hit returns equal value, producer ran once", $r['same'] && $r['n'] === 1);
    if ($z) {
        check("zero_copy=1 hit is borrowed, not copied", $r['borrow'] === 1 && $r['copy'] === 0, json_encode($r));
        check("zero_copy=1 borrowed value intact after delete (retired)", $r['intact'] && $r['retired'] >= 1, json_encode($r));
    } else {
        check("zero_copy=0 hit is copied", $r['borrow'] === 0 && $r['copy'] === 1, json_encode($r));
        check("zero_copy=0 value intact after delete", $r['intact']);
    }
    check("zero_copy=$z copy-on-write isolation", $r['cow']);
    check("zero_copy=$z values intact after Memory::clean()", $r['after_clean']);
    check("zero_copy=$z fill counted, concurrency 1, no coalescing", $r['fill'] === 1 && $r['max'] === 1 && $r['coalesced'] === 0, json_encode($r));
}
echo "\n";

/* ---------- Route programs under php-cgi -T (FPM-like worker) ---------- */
echo "Testing route program compile (php-cgi -T):\n";
$cgi = dirname(PHP_BINARY) . DIRECTORY_SEPARATOR . (DIRECTORY_SEPARATOR === '\\' ? 'php-cgi.exe' : 'php-cgi');
$fixture = __DIR__ . DIRECTORY_SEPARATOR . 'fixtures' . DIRECTORY_SEPARATOR . 'perfv3';
$cacheDir = $fixture . DIRECTORY_SEPARATOR . 'app' . DIRECTORY_SEPARATOR . 'Cache';
$routerDir = $cacheDir . DIRECTORY_SEPARATOR . 'Router';

if (!is_file($cgi)) {
    echo "- SKIP route program tests (php-cgi not found next to " . PHP_BINARY . ")\n\n";
} else {
    $probeFile = temp_script('echo function_exists("opcache_get_status") ? "Y" : "N";');
    $hasOpcache = false;
    foreach (['', 'opcache', 'php_opcache.dll'] as $so) {
        $opcache = ($so === '' ? '' : "-d zend_extension=$so ") . '-d opcache.enable=1';
        [$probe] = run_cmd(escapeshellarg($cgi) . " -q $args $opcache " . escapeshellarg($probeFile),
            ['SCRIPT_FILENAME' => $probeFile, 'REDIRECT_STATUS' => '200', 'REQUEST_METHOD' => 'GET']);
        if (substr(trim((string)$probe), -1) === 'Y') {
            $hasOpcache = true;
            break;
        }
    }

    $serve = function ($uri, $extra, $n = 3) use ($cgi, $args, $fixture) {
        $env = ['REQUEST_METHOD' => 'GET', 'REQUEST_URI' => $uri, 'REDIRECT_STATUS' => '200',
            'SCRIPT_FILENAME' => $fixture . DIRECTORY_SEPARATOR . 'index.php'];
        [$out] = run_cmd(escapeshellarg($cgi) . " -q $args $extra -T $n index.php", $env, $fixture);
        $out = preg_replace('/\r\n/', "\n", (string)$out);
        $out = preg_replace('/^Elapsed time:.*$/m', '', $out);
        $bodies = [];
        $stats = [];
        foreach (explode("\nSTATS ", "\n" . $out) as $i => $chunk) {
            if ($i === 0) { $bodies[] = $chunk; continue; }
            $nl = strpos($chunk, "\n");
            $stats[] = json_decode(substr($chunk, 0, $nl === false ? strlen($chunk) : $nl), true);
            $bodies[] = $nl === false ? '' : substr($chunk, $nl + 1);
        }
        return [array_slice(array_map(function ($b) {
            return trim(preg_replace('/^(X-Powered-By|Content-type):.*$/mi', '', $b));
        }, $bodies), 0, $n), $stats];
    };

    $uris = ['/c/7' => '[before]closure:7[after]', '/g' => '[before][guard]guarded[after]',
        '/s' => 'string-route', '/nope' => 'not-found'];

    rrmdir($cacheDir);
    foreach ($uris as $uri => $expect) {
        [$b0, $s0] = $serve($uri, '-d gene.route_eval_compile=0 ' . ($hasOpcache ? $opcache : ''));
        check("compile=0 $uri output", $b0 === [$expect, $expect, $expect], json_encode($b0));
        if ($uri !== '/s') {
            check("compile=0 $uri requests 2..3 use eval", ($s0[2]['eval'] ?? 0) === 2 && ($s0[2]['compiled'] ?? -1) === 0, json_encode($s0));
        }
        if (!$hasOpcache) {
            continue;
        }
        [$b1, $s1] = $serve($uri, "-d gene.route_eval_compile=1 $opcache");
        check("compile=1 $uri output identical", $b1 === $b0, json_encode($b1));
        if ($uri !== '/s') {
            check("compile=1 $uri requests 2..3 compiled, no eval", ($s1[2]['eval'] ?? -1) === 0 && ($s1[2]['compiled'] ?? 0) === 2 && ($s1[2]['files'] ?? 0) === 1, json_encode($s1));
        }
        if ($uri === '/nope') {
            check("compile=1 error handler counted as error eval", ($s1[2]['error_eval'] ?? 0) === 2, json_encode($s1));
        }
    }
    if (!$hasOpcache) {
        echo "- SKIP compiled-path checks (opcache not loadable)\n";
    } else {
        $files = glob($routerDir . DIRECTORY_SEPARATOR . '*.php') ?: [];
        check("compiled route files written (3 programs)", count($files) === 3, implode(',', array_map('basename', $files)));
        check("no temp files left behind", !(glob($routerDir . DIRECTORY_SEPARATOR . '*.tmp') ?: []));

        [$bn, $sn] = $serve('/c/7', '-d gene.route_eval_compile=1 -d opcache.enable=0');
        check("compile=1 without opcache falls back to eval", $bn[2] === $uris['/c/7'] && ($sn[2]['eval'] ?? 0) === 2 && ($sn[2]['compiled'] ?? -1) === 0, json_encode($sn));

        foreach ($files as $f) {
            file_put_contents($f, "<?php\necho 'TAMPERED';\n");
        }
        [$bt, $stt] = $serve('/c/7', "-d gene.route_eval_compile=1 $opcache");
        check("tampered cache file never executed", strpos(implode('', $bt), 'TAMPERED') === false && $bt[2] === $uris['/c/7'], json_encode($bt));
        /* A mismatching file is left alone (opcache may still hold the old
         * script under that name) and the program runs through eval. */
        check("tampered cache file falls back to eval", ($stt[2]['eval'] ?? 0) === 2 && ($stt[2]['compiled'] ?? -1) === 0, json_encode($stt));

        rrmdir($cacheDir);
        @mkdir($cacheDir, 0777, true);
        file_put_contents($routerDir, 'not a directory');
        [$bu, $su] = $serve('/c/7', "-d gene.route_eval_compile=1 $opcache");
        check("unwritable cache dir falls back to eval", $bu[2] === $uris['/c/7'] && ($su[2]['eval'] ?? 0) === 2 && ($su[2]['compiled'] ?? -1) === 0, json_encode($su));
    }

    [$bz] = $serve('/c/7', '-d gene.memory_business_zero_copy=1 ' . ($hasOpcache ? "-d gene.route_eval_compile=1 $opcache" : ''), 5);
    check("zero_copy=1 across 5 worker requests", $bz === array_fill(0, 5, $uris['/c/7']), json_encode($bz));
    rrmdir($cacheDir);
}

echo "\n=== " . ($fail ? "$fail FAIL" : "all PASS") . " ===\n";
exit($fail ? 1 : 0);
