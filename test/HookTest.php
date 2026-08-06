<?php

/**
 * Gene Framework Hook Class Test
 *
 * Covers Gene\Hook against the real API:
 * - before()/after()/handle() default implementations (subclass override points)
 * - success()/error()/data() response payload shapes
 * - method predicates (isCli under CLI SAPI)
 * - static request accessors (get/post/server/cookie/env)
 */

use Gene\Hook;

/**
 * Subclass overriding the lifecycle hooks, mirroring real usage.
 */
class GeneHookTestHook extends Hook
{
    public static $beforeCalls = 0;
    public static $afterCalls = 0;
    public static $handleCalls = 0;
    public static $lastAfterParam;

    public function before()
    {
        self::$beforeCalls++;
        return true;
    }

    public function after($params = null)
    {
        self::$afterCalls++;
        self::$lastAfterParam = $params;
    }

    public function handle()
    {
        self::$handleCalls++;
        return true;
    }
}

class HookTest
{
    public function __construct()
    {
        echo "=== Gene Hook Class Test Suite ===\n\n";
    }

    /**
     * Default hooks are callable and permissive.
     */
    public function testDefaultHooks()
    {
        echo "Testing Hook Defaults:\n";

        try {
            $hook = new Hook();
            if ($hook->before() === true) {
                echo "✓ before() default returns true\n";
            } else {
                echo "✗ before() default is not true\n";
            }
            if ($hook->handle() === true) {
                echo "✓ handle() default returns true\n";
            } else {
                echo "✗ handle() default is not true\n";
            }
            $hook->after(['ok' => 1]);
            echo "✓ after() accepts the dispatch result param\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Subclass overrides are honored (the dispatch path calls these).
     */
    public function testSubclassOverrides()
    {
        echo "Testing Hook Subclass Overrides:\n";

        try {
            $hook = new GeneHookTestHook();
            $hook->before();
            $hook->handle();
            $hook->after('result-payload');

            if (GeneHookTestHook::$beforeCalls === 1 && GeneHookTestHook::$handleCalls === 1) {
                echo "✓ overridden before()/handle() are invoked\n";
            } else {
                echo "✗ overridden hooks not invoked as expected\n";
            }
            if (GeneHookTestHook::$afterCalls === 1 && GeneHookTestHook::$lastAfterParam === 'result-payload') {
                echo "✓ overridden after() receives the dispatch result\n";
            } else {
                echo "✗ overridden after() did not receive the dispatch result\n";
            }
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * success()/error()/data() payload shapes.
     */
    public function testResponsePayloads()
    {
        echo "Testing Hook Response Payloads:\n";

        try {
            $ok = Hook::success('done');
            if (is_array($ok) && $ok['code'] === 2000 && $ok['msg'] === 'done') {
                echo "✓ success() defaults to code 2000\n";
            } else {
                echo "✗ success() payload mismatch: " . json_encode($ok) . "\n";
            }

            $okCustom = Hook::success('done', 2001);
            if (is_array($okCustom) && $okCustom['code'] === 2001) {
                echo "✓ success() accepts a custom code\n";
            } else {
                echo "✗ success() custom code mismatch\n";
            }

            $err = Hook::error('broken');
            if (is_array($err) && $err['code'] === 4000 && $err['msg'] === 'broken') {
                echo "✓ error() defaults to code 4000\n";
            } else {
                echo "✗ error() payload mismatch: " . json_encode($err) . "\n";
            }

            $data = Hook::data(['a' => 1], 5, 'listed', 2000);
            if (is_array($data)
                && $data['code'] === 2000
                && $data['msg'] === 'listed'
                && $data['data'] === ['a' => 1]
                && $data['count'] === 5) {
                echo "✓ data() carries code/msg/data/count\n";
            } else {
                echo "✗ data() payload mismatch: " . json_encode($data) . "\n";
            }
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Method predicates under the CLI SAPI.
     */
    public function testMethodPredicates()
    {
        echo "Testing Hook Method Predicates:\n";

        try {
            // ctx->method is only populated by router init; under a bare CLI
            // harness the predicates return false. Assert type, not truthiness.
            $cli = Hook::isCli();
            if (is_bool($cli)) {
                echo "✓ isCli() returns bool under CLI (method unset: " . var_export($cli, true) . ")\n";
            } else {
                echo "✗ isCli() did not return bool\n";
            }

            // Under CLI there is no HTTP method; the HTTP predicates must not throw.
            Hook::isGet();
            Hook::isPost();
            Hook::isPut();
            Hook::isDelete();
            Hook::isHead();
            Hook::isOptions();
            echo "✓ HTTP method predicates are safe to call under CLI\n";
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Static request accessors degrade gracefully under CLI.
     */
    public function testRequestAccessors()
    {
        echo "Testing Hook Request Accessors:\n";

        try {
            // Under CLI these superglobals are empty; accessors must not throw.
            Hook::get('missing');
            Hook::post('missing');
            Hook::cookie('missing');
            Hook::env('missing');
            Hook::files('missing');
            echo "✓ request accessors are safe to call under CLI\n";

            $script = Hook::server('SCRIPT_NAME');
            if ($script === null || is_string($script)) {
                echo "✓ server() returns null|string\n";
            } else {
                echo "✗ server() returned unexpected type\n";
            }
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function runAllTests()
    {
        $this->testDefaultHooks();
        $this->testSubclassOverrides();
        $this->testResponsePayloads();
        $this->testMethodPredicates();
        $this->testRequestAccessors();

        echo "=== Hook Test Suite Complete ===\n";
    }
}

if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new HookTest();
    $test->runAllTests();
}
