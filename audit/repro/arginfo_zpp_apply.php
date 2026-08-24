<?php
/** Auto-fix ZEND_BEGIN_ARG_INFO_EX required counts to match zend_parse_parameters. */

$src = realpath(__DIR__ . '/../../src');

function zpp_required(string $fmt): int {
    $p = strpos($fmt, '|');
    return $p === false ? strlen($fmt) : $p;
}

$wanted = [];

$iter = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($src));
foreach ($iter as $file) {
    if ($file->getExtension() !== 'c') {
        continue;
    }
    $path = $file->getPathname();
    $content = file_get_contents($path);

    $arginfos = [];
    if (preg_match_all('/ZEND_BEGIN_ARG_INFO_EX\((\w+),\s*0,\s*0,\s*(\d+)\)/', $content, $matches, PREG_SET_ORDER)) {
        foreach ($matches as $m) {
            $arginfos[$m[1]] = (int)$m[2];
        }
    }

    $meMap = [];
    if (preg_match_all('/PHP_ME\((\w+),\s*(\w+),\s*(\w+)/', $content, $matches, PREG_SET_ORDER)) {
        foreach ($matches as $m) {
            $meMap[$m[1] . '::' . $m[2]] = $m[3];
        }
    }

    $setWanted = function (string $name, int $req) use (&$wanted) {
        if (isset($wanted[$name]) && $wanted[$name] !== $req) {
            fwrite(STDERR, "CONFLICT for $name: {$wanted[$name]} vs $req\n");
            exit(1);
        }
        $wanted[$name] = $req;
    };

    $parts = preg_split('/(?=PHP_METHOD\()/', $content);
    foreach ($parts as $part) {
        if (!preg_match('/^PHP_METHOD\((\w+),\s*(\w+)\)/', $part, $mm)) {
            continue;
        }
        $key = $mm[1] . '::' . $mm[2];
        $arginfoName = $meMap[$key] ?? null;
        if (!$arginfoName || !isset($arginfos[$arginfoName])) {
            continue;
        }
        if (preg_match('/zend_parse_parameters\(ZEND_NUM_ARGS\(\),\s*"([^"]+)"/', $part, $zm)) {
            $req = zpp_required($zm[1]);
        } elseif (strpos($part, 'zend_parse_parameters_none()') !== false) {
            $req = 0;
        } else {
            continue;
        }
        if ($req !== $arginfos[$arginfoName]) {
            $setWanted($arginfoName, $req);
        }
    }

    if (preg_match_all('/GENE_REQUEST_METHOD\((\w+),\s*(\w+),\s*\w+\)/', $content, $matches, PREG_SET_ORDER)) {
        foreach ($matches as $m) {
            $key = $m[1] . '::' . $m[2];
            $arginfoName = $meMap[$key] ?? null;
            if (!$arginfoName || !isset($arginfos[$arginfoName])) {
                continue;
            }
            $req = zpp_required('|sz');
            if ($req !== $arginfos[$arginfoName]) {
                $setWanted($arginfoName, $req);
            }
        }
    }
}

$changedFiles = 0;
$changedArginfos = 0;

foreach (new RecursiveIteratorIterator(new RecursiveDirectoryIterator($src)) as $file) {
    if ($file->getExtension() !== 'c') {
        continue;
    }
    $path = $file->getPathname();
    $content = file_get_contents($path);
    $orig = $content;

    foreach ($wanted as $name => $req) {
        $content = preg_replace_callback(
            '/ZEND_BEGIN_ARG_INFO_EX\(' . preg_quote($name, '/') . ',\s*0,\s*0,\s*(\d+)\)/',
            function ($m) use ($name, $req, &$changedArginfos) {
                if ((int)$m[1] !== $req) {
                    $changedArginfos++;
                    return "ZEND_BEGIN_ARG_INFO_EX($name, 0, 0, $req)";
                }
                return $m[0];
            },
            $content
        );
    }

    if ($content !== $orig) {
        file_put_contents($path, $content);
        $changedFiles++;
        echo "patched: $path\n";
    }
}

echo "\nPatched $changedArginfos arginfo definition(s) across $changedFiles file(s).\n";

// verify
passthru('"' . PHP_BINARY . '" ' . escapeshellarg(__DIR__ . '/arginfo_zpp_audit.php'), $code);
exit($code);
