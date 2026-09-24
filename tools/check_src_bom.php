<?php

/**
 * [GENE_FIX:2026-09-23 S8] src recursive *.c and *.h encoding check.
 *
 * AGENTS.md: Windows builds require UTF-8 with BOM — MSVC emits C4819
 * (file contains characters that cannot be represented in the current
 * code page) for any source with non-ASCII bytes and no BOM. Pure-ASCII
 * files without BOM are fine and are allowed.
 *
 * Rule enforced here: a file containing bytes > 0x7F MUST start with
 * EF BB BF. Run from the repo root or pass a src dir:
 *
 *   php tools/check_src_bom.php [src-dir]
 *
 * Exit 0 = clean, 1 = violations found.
 */

$root = $argv[1] ?? (dirname(__DIR__) . DIRECTORY_SEPARATOR . 'src');
if (!is_dir($root)) {
    fwrite(STDERR, "src dir not found: $root\n");
    exit(1);
}

$bad = [];
$checked = 0;
$it = new \RecursiveIteratorIterator(
    new \RecursiveDirectoryIterator($root, \FilesystemIterator::SKIP_DOTS)
);
foreach ($it as $file) {
    $ext = strtolower($file->getExtension());
    if ($ext !== 'c' && $ext !== 'h') {
        continue;
    }
    $checked++;
    $head = file_get_contents($file->getPathname(), false, null, 0, 3);
    $bom = $head !== false && strlen($head) === 3
        && $head === "\xEF\xBB\xBF";
    if ($bom) {
        continue;
    }
    $data = file_get_contents($file->getPathname());
    if ($data !== false && preg_match('/[\x80-\xFF]/', $data)) {
        $bad[] = $file->getPathname();
    }
}

foreach ($bad as $f) {
    echo "NO-BOM  $f\n";
}
printf("%d files checked, %d non-ASCII without BOM\n", $checked, count($bad));
exit($bad ? 1 : 0);
