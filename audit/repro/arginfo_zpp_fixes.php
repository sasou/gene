<?php
/** Output unique arginfo required-count fixes grouped by arginfo name. */

$src = realpath(__DIR__ . '/../../src');

function zpp_required(string $fmt): int {
    $p = strpos($fmt, '|');
    return $p === false ? strlen($fmt) : $p;
}

$arginfoFixes = [];

$iter = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($src));
foreach ($iter as $file) {
    if ($file->getExtension() !== 'c') {
        continue;
    }
    $path = $file->getPathname();
    $content = file_get_contents($path);

    $arginfos = [];
    if (preg_match_all(
        '/ZEND_BEGIN_ARG_INFO_EX\((\w+),\s*0,\s*0,\s*(\d+)\)/',
        $content,
        $matches,
        PREG_SET_ORDER
    )) {
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

    $record = function (string $arginfoName, int $req, string $path, string $method) use (&$arginfoFixes, $arginfos) {
        if (!isset($arginfoFixes[$arginfoName])) {
            $arginfoFixes[$arginfoName] = [
                'current' => $arginfos[$arginfoName] ?? null,
                'wanted' => $req,
                'file' => $path,
                'examples' => [],
            ];
        }
        if ($arginfoFixes[$arginfoName]['wanted'] !== $req) {
            echo "CONFLICT: $arginfoName wanted $req and {$arginfoFixes[$arginfoName]['wanted']}\n";
        }
        $arginfoFixes[$arginfoName]['examples'][$method] = true;
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
            $record($arginfoName, $req, $path, $key);
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
                $record($arginfoName, $req, $path, $key);
            }
        }
    }
}

ksort($arginfoFixes);
foreach ($arginfoFixes as $name => $info) {
    $examples = implode(', ', array_slice(array_keys($info['examples']), 0, 3));
    echo "$name: {$info['current']} -> {$info['wanted']}  ($examples)  [{$info['file']}]\n";
}
echo "\nTotal unique arginfo fixes: " . count($arginfoFixes) . "\n";
