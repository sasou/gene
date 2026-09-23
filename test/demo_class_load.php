<?php

/**
 * [GENE_FIX:2026-09-23 S3] Demo class-load smoke probe — spawned by
 * DemoLoadTest in a child process. Registers the same PSR-4 rule the demo
 * bootstrap uses (namespace == path under application/) and loads EVERY
 * class file in the six class dirs. `php -l` cannot catch inheritance-time
 * fatals (e.g. a typed static property redeclaring an untyped parent
 * property), so classes are really loaded via class_exists($fqcn, true).
 *
 * Contract: exit 0 and print NOTHING on success; any load failure,
 * E_WARNING, E_NOTICE or E_DEPRECATED prints a line and exits 1.
 */

$appRoot = dirname(__DIR__) . DIRECTORY_SEPARATOR . 'demo'
    . DIRECTORY_SEPARATOR . 'application';

$errors = [];
set_error_handler(function ($no, $str, $file, $line) use (&$errors) {
    $errors[] = "$str in $file:$line";
    return true;
});

spl_autoload_register(function ($class) use ($appRoot) {
    $file = $appRoot . DIRECTORY_SEPARATOR
        . str_replace('\\', DIRECTORY_SEPARATOR, $class) . '.php';
    if (is_file($file)) {
        require $file;
    }
});

$dirs = ['Api', 'Controllers', 'Ext', 'Hooks', 'Models', 'Services'];
$loaded = 0;
$missing = [];

foreach ($dirs as $dir) {
    $base = $appRoot . DIRECTORY_SEPARATOR . $dir;
    if (!is_dir($base)) {
        $missing[] = $dir;
        continue;
    }
    $it = new \RecursiveIteratorIterator(
        new \RecursiveDirectoryIterator($base, \FilesystemIterator::SKIP_DOTS)
    );
    foreach ($it as $file) {
        if ($file->getExtension() !== 'php') {
            continue;
        }
        $rel = substr($file->getPathname(), strlen($appRoot) + 1);
        $fqcn = str_replace(DIRECTORY_SEPARATOR, '\\', substr($rel, 0, -4));
        if (!class_exists($fqcn, true)) {
            $errors[] = "class_exists failed: $fqcn";
            continue;
        }
        $loaded++;
    }
}

foreach ($missing as $dir) {
    $errors[] = "missing dir: $dir";
}
foreach ($errors as $e) {
    echo "LOAD-FAIL $e\n";
}
exit($errors ? 1 : 0);
