<?php
/**
 * Repro: debug PHP fatals on arginfo/zpp mismatch for optional params.
 * Run: php -n -d extension_dir=... -d extension=php_gene.dll audit/repro/arginfo_zpp_selftest.php
 */
declare(strict_types=1);

$fail = 0;

function ok(string $label): void {
    echo "OK  $label\n";
}

function check(string $label, callable $fn): void {
    global $fail;
    try {
        $fn();
        ok($label);
    } catch (Throwable $e) {
        echo "FAIL $label: {$e->getMessage()}\n";
        $fail++;
    }
}

check('Request::header() no args', fn () => \Gene\Request::header());
check('Request::header(key)', fn () => \Gene\Request::header('Origin'));
check('Request::get() no args', fn () => \Gene\Request::get());
check('Service::getInstance() no args', fn () => \Gene\Service::getInstance());
check('Model::getInstance() no args', fn () => \Gene\Model::getInstance());
check('Session::__construct() no args', fn () => new \Gene\Session());
check('Session::__construct(config)', fn () => new \Gene\Session(['driver' => 'file']));
check('Memcached::__construct reflection', function () {
    $m = new ReflectionMethod(\Gene\Cache\Memcached::class, '__construct');
    if ($m->getNumberOfRequiredParameters() !== 1 || $m->getNumberOfParameters() !== 1) {
        throw new RuntimeException('unexpected Memcached __construct signature');
    }
});
check('Controller::__get reflection', function () {
    $m = new ReflectionMethod(\Gene\Controller::class, '__get');
    if ($m->getNumberOfRequiredParameters() !== 0) {
        throw new RuntimeException('unexpected required count: ' . $m->getNumberOfRequiredParameters());
    }
});

if ($fail) {
    echo "\n$fail failure(s)\n";
    exit(1);
}
echo "\nAll arginfo/zpp selftests passed.\n";
