<?php
/** Scan Gene extension C sources for arginfo vs zend_parse_parameters required-count mismatches. */

$src = realpath(__DIR__ . '/../../src');

function zpp_required(string $fmt): int {
    $p = strpos($fmt, '|');
    return $p === false ? strlen($fmt) : $p;
}

function arginfo_total(string $body): int {
    return preg_match_all('/ZEND_ARG_(TYPE_)?INFO/', $body);
}

$allIssues = [];

$iter = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($src));
foreach ($iter as $file) {
    if ($file->getExtension() !== 'c') {
        continue;
    }
    $path = $file->getPathname();
    $content = file_get_contents($path);

    $arginfos = [];
    if (preg_match_all(
        '/ZEND_BEGIN_ARG_INFO_EX\((\w+),\s*0,\s*0,\s*(\d+)\)(.*?)ZEND_END_ARG_INFO\(\)/s',
        $content,
        $matches,
        PREG_SET_ORDER
    )) {
        foreach ($matches as $m) {
            $arginfos[$m[1]] = [
                'required' => (int)$m[2],
                'total' => arginfo_total($m[3]),
            ];
        }
    }

    $meMap = [];
    if (preg_match_all('/PHP_ME\((\w+),\s*(\w+),\s*(\w+)/', $content, $matches, PREG_SET_ORDER)) {
        foreach ($matches as $m) {
            $meMap[$m[1] . '::' . $m[2]] = $m[3];
        }
    }

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
        $ai = $arginfos[$arginfoName];

        if (preg_match('/zend_parse_parameters\(ZEND_NUM_ARGS\(\),\s*"([^"]+)"/', $part, $zm)) {
            $zpp = $zm[1];
            $req = zpp_required($zpp);
        } elseif (strpos($part, 'zend_parse_parameters_none()') !== false) {
            $zpp = '(none)';
            $req = 0;
        } else {
            continue;
        }

        if ($req !== $ai['required']) {
            $allIssues[] = compact('path', 'key', 'arginfoName', 'zpp', 'req') + [
                'arginfoRequired' => $ai['required'],
            ];
        }
    }

    if (preg_match_all('/GENE_REQUEST_METHOD\((\w+),\s*(\w+),\s*\w+\)/', $content, $matches, PREG_SET_ORDER)) {
        foreach ($matches as $m) {
            $key = $m[1] . '::' . $m[2];
            $arginfoName = $meMap[$key] ?? null;
            if (!$arginfoName || !isset($arginfos[$arginfoName])) {
                continue;
            }
            $ai = $arginfos[$arginfoName];
            $zpp = '|sz';
            $req = zpp_required($zpp);
            if ($req !== $ai['required']) {
                $allIssues[] = [
                    'path' => $path,
                    'key' => $key,
                    'arginfoName' => $arginfoName,
                    'zpp' => $zpp,
                    'req' => $req,
                    'arginfoRequired' => $ai['required'],
                    'via' => 'GENE_REQUEST_METHOD',
                ];
            }
        }
    }
}

if (!$allIssues) {
    echo "No arginfo/zpp required-count mismatches found.\n";
    exit(0);
}

echo 'Found ' . count($allIssues) . " mismatch(es):\n\n";
foreach ($allIssues as $i) {
    $via = isset($i['via']) ? " [{$i['via']}]" : '';
    echo "{$i['path']}: {$i['key']}(){$via}\n";
    echo "  arginfo {$i['arginfoName']}: required={$i['arginfoRequired']}\n";
    echo "  zpp \"{$i['zpp']}\": required={$i['req']}\n\n";
}
exit(1);
