<?php
/** Scan Gene extension C sources for arginfo total/required vs zend_parse_parameters mismatches. */

$src = realpath(__DIR__ . '/../../src');

function zpp_count(string $fmt): int {
    $count = 0;
    $len = strlen($fmt);
    for ($i = 0; $i < $len; $i++) {
        $c = $fmt[$i];
        if ($c === '|') {
            continue;
        }
        $count++;
        while ($i + 1 < $len && str_contains('!/+*&', $fmt[$i + 1])) {
            $i++;
        }
    }
    return $count;
}

function zpp_required(string $fmt): int {
    $p = strpos($fmt, '|');
    return $p === false ? zpp_count($fmt) : zpp_count(substr($fmt, 0, $p));
}

function zpp_total(string $fmt): int {
    return zpp_count($fmt);
}

function arginfo_total(string $body): int {
    return preg_match_all('/ZEND_ARG_(TYPE_|ARRAY_|OBJ_|VARIADIC_TYPE_|CALLABLE_)?INFO/', $body);
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
            $total = zpp_total($zpp);
        } elseif (strpos($part, 'zend_parse_parameters_none()') !== false) {
            $zpp = '(none)';
            $req = 0;
            $total = 0;
        } elseif (preg_match('/ZEND_PARSE_PARAMETERS_START\((\d+),\s*(\d+)\)/', $part, $zm)) {
            $req = (int)$zm[1];
            $total = (int)$zm[2];
            $zpp = "START($req,$total)";
        } else {
            continue;
        }

        if ($req !== $ai['required'] || $total !== $ai['total']) {
            $allIssues[] = compact('path', 'key', 'arginfoName', 'zpp', 'req', 'total') + [
                'arginfoRequired' => $ai['required'],
                'arginfoTotal' => $ai['total'],
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
            $total = zpp_total($zpp);
            if ($req !== $ai['required'] || $total !== $ai['total']) {
                $allIssues[] = [
                    'path' => $path,
                    'key' => $key,
                    'arginfoName' => $arginfoName,
                    'zpp' => $zpp,
                    'req' => $req,
                    'total' => $total,
                    'arginfoRequired' => $ai['required'],
                    'arginfoTotal' => $ai['total'],
                    'via' => 'GENE_REQUEST_METHOD',
                ];
            }
        }
    }
}

if (!$allIssues) {
    echo "No arginfo/zpp total/required mismatches found.\n";
    exit(0);
}

echo 'Found ' . count($allIssues) . " mismatch(es):\n\n";
foreach ($allIssues as $i) {
    $via = isset($i['via']) ? " [{$i['via']}]" : '';
    echo "{$i['path']}: {$i['key']}(){$via}\n";
    echo "  arginfo {$i['arginfoName']}: required={$i['arginfoRequired']} total={$i['arginfoTotal']}\n";
    echo "  zpp \"{$i['zpp']}\": required={$i['req']} total={$i['total']}\n\n";
}
exit(1);
