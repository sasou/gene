<?php

/**
 * Demo application smoke test
 *
 * [GENE_FIX:2026-09-23 R12] Loads the repository demo through the real
 * bootstrap path in an isolated child process (GENE_DEMO_LOCAL=1 → sqlite
 * file + Ext\LocalStore, no external MySQL/Redis/Memcached):
 *
 *   1. demo/database/init_sqlite.php — idempotent schema/seed init
 *   2. demo/public/cli.php /healthz — full route dispatch through
 *      bootstrap → router → controller → Response::json
 *
 * Child processes inherit GENE_TEST_PHP_ARGS the same way TestRunner spawns
 * them, so the freshly built extension and pdo_sqlite are picked up.
 */

class DemoLoadTest
{
    private $passed = 0;
    private $failed = 0;

    public function __construct()
    {
        echo "=== Demo Load Smoke Test ===\n\n";
    }

    private function ok($msg)
    {
        echo "✓ $msg\n";
        $this->passed++;
    }

    private function fail($msg)
    {
        echo "✗ $msg\n";
        $this->failed++;
    }

    /**
     * Spawn a child PHP process mirroring TestRunner::runIsolated().
     * Returns [stdout, exitCode] or [null, -1] on spawn failure.
     */
    private function spawn(array $argv, $cwd)
    {
        $phpArgs = getenv('GENE_TEST_PHP_ARGS');
        $command = [escapeshellarg(PHP_BINARY)];
        if (is_string($phpArgs) && $phpArgs !== '') {
            $command[] = $phpArgs;
        }
        foreach ($argv as $a) {
            $command[] = escapeshellarg($a);
        }
        $pipes = [];
        $process = proc_open(implode(' ', $command), [
            0 => ['pipe', 'r'],
            1 => ['pipe', 'w'],
            2 => ['redirect', 1],
        ], $pipes, $cwd);
        if (!is_resource($process)) {
            return [null, -1];
        }
        fclose($pipes[0]);
        $output = stream_get_contents($pipes[1]);
        fclose($pipes[1]);
        return [$output, proc_close($process)];
    }

    public function testSqliteInit()
    {
        echo "Testing demo sqlite init (idempotent):\n";

        $demo = dirname(__DIR__) . DIRECTORY_SEPARATOR . 'demo';
        $init = $demo . DIRECTORY_SEPARATOR . 'database' . DIRECTORY_SEPARATOR . 'init_sqlite.php';
        $dbFile = $demo . DIRECTORY_SEPARATOR . 'database' . DIRECTORY_SEPARATOR . 'gene_demo.db';
        if (!is_file($init)) {
            $this->fail('init_sqlite.php missing');
            return;
        }

        putenv('GENE_DEMO_LOCAL=1');
        [$out, $code] = $this->spawn([$init, $dbFile], $demo);
        if ($code === 0 && is_file($dbFile)) {
            $this->ok('init_sqlite.php exit=0, db file present');
        } else {
            $this->fail("init_sqlite.php exit=$code out=" . trim((string)$out));
        }

        // Second run must also succeed (CREATE IF NOT EXISTS + conditional seed).
        [$out2, $code2] = $this->spawn([$init, $dbFile], $demo);
        putenv('GENE_DEMO_LOCAL');
        if ($code2 === 0) {
            $this->ok('init_sqlite.php re-run idempotent');
        } else {
            $this->fail("init_sqlite.php re-run exit=$code2 out=" . trim((string)$out2));
        }
        echo "\n";
    }

    public function testCliDispatch()
    {
        echo "Testing demo CLI dispatch (GENE_DEMO_LOCAL=1):\n";

        $demo = dirname(__DIR__) . DIRECTORY_SEPARATOR . 'demo';
        $cli = $demo . DIRECTORY_SEPARATOR . 'public' . DIRECTORY_SEPARATOR . 'cli.php';
        if (!is_file($cli)) {
            $this->fail('demo/public/cli.php missing');
            return;
        }

        putenv('GENE_DEMO_LOCAL=1');
        [$out, $code] = $this->spawn([$cli, '/healthz'], $demo . DIRECTORY_SEPARATOR . 'public');
        putenv('GENE_DEMO_LOCAL');
        $out = (string)$out;
        if ($code === 0 && strpos($out, '"status":"ok"') !== false) {
            $this->ok('cli /healthz returns {"status":"ok"}');
        } else {
            $this->fail("cli /healthz exit=$code out=" . substr(trim($out), 0, 400));
        }

        // 404 hook path — proves error hook wiring did not regress.
        putenv('GENE_DEMO_LOCAL=1');
        [$out2, $code2] = $this->spawn([$cli, '/no-such-route-xyz'], $demo . DIRECTORY_SEPARATOR . 'public');
        putenv('GENE_DEMO_LOCAL');
        if ($code2 === 0) {
            $this->ok('cli unknown route handled by error hook (exit=0)');
        } else {
            $this->fail("cli unknown route exit=$code2 out=" . substr(trim((string)$out2), 0, 400));
        }
        echo "\n";
    }

    public function run()
    {
        $this->testSqliteInit();
        $this->testCliDispatch();
        echo "--- DemoLoad results: {$this->passed} passed, {$this->failed} failed ---\n";
        return $this->failed === 0;
    }

    public function runAllTests()
    {
        $this->run();
    }
}

if (php_sapi_name() === 'cli' && realpath($_SERVER['SCRIPT_FILENAME'] ?? '') === __FILE__) {
    $t = new DemoLoadTest();
    exit($t->run() ? 0 : 1);
}
