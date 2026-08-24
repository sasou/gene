<?php
/** Report arginfo names shared by methods with different zpp signatures. */

$src = realpath(__DIR__ . '/../../src');

function zpp_total(string $fmt): int {
    return strlen(str_replace('|', '', $fmt));
}

function zpp_required(string $fmt): int {
    $p = strpos($fmt, '|');
    return $p === false ? strlen($fmt) : $p;
}

function arginfo_total(string $body): int {
    return preg_match_all('/ZEND_ARG_(TYPE_)?INFO/', $body);
}

$byArginfo = [];

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
        if (preg_match('/zend_parse_parameters\(ZEND_NUM_ARGS\(\),\s*"([^"]+)"/', $part, $zm)) {
            $zpp = $zm[1];
        } elseif (strpos($part, 'zend_parse_parameters_none()') !== false) {
            $zpp = '';
        } elseif (preg_match('/ZEND_PARSE_PARAMETERS_START\((\d+),\s*(\d+)\)/', $part, $zm)) {
            $zpp = 'START(' . $zm[1] . ',' . $zm[2] . ')';
        } else {
            continue;
        }
        $byArginfo[$arginfoName][$key] = [
            'zpp' => $zpp,
            'req' => zpp_required($zpp),
            'total' => zpp_total($zpp),
            'file' => $path,
        ];
    }
}

foreach ($byArginfo as $name => $methods) {
    $sigs = [];
    foreach ($methods as $m => $info) {
        $sig = $info['req'] . '/' . $info['total'] . ':' . $info['zpp'];
        $sigs[$sig][$m] = $info;
    }
    if (count($sigs) > 1) {
        echo "CONFLICT $name:\n";
        foreach ($sigs as $sig => $ms) {
            echo "  $sig -> " . implode(', ', array_keys($ms)) . "\n";
        }
        echo "\n";
    }
}
