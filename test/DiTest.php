<?php

/**
 * Gene Framework Di Class Test
 *
 * Covers Gene\Di against the real API:
 * - static set/get/has/del registry
 * - magic __set/__get via the singleton instance
 * - getInstance() singleton identity
 * - verbatim value semantics (arrays/closures stored as-is, overwrite on re-set)
 */

use Gene\Di;

class DiTest
{
    public function __construct()
    {
        echo "=== Gene Di Class Test Suite ===\n\n";
    }

    /**
     * Static registry: set / has / get / del round-trip.
     */
    public function testStaticRegistry()
    {
        echo "Testing Di Static Registry:\n";

        try {
            Di::set('di_test_scalar', 42);
            echo "✓ Di::set() scalar works\n";

            if (Di::has('di_test_scalar')) {
                echo "✓ Di::has() reports existing key\n";
            } else {
                echo "✗ Di::has() missing registered key\n";
            }

            if (Di::get('di_test_scalar') === 42) {
                echo "✓ Di::get() returns the stored value\n";
            } else {
                echo "✗ Di::get() returned unexpected value\n";
            }

            $obj = new \stdClass();
            $obj->tag = 'di';
            Di::set('di_test_object', $obj);
            if (Di::get('di_test_object') === $obj) {
                echo "✓ Di::get() returns identical object instance\n";
            } else {
                echo "✗ Di::get() did not preserve object identity\n";
            }

            Di::del('di_test_scalar');
            if (!Di::has('di_test_scalar') && Di::get('di_test_scalar') === null) {
                echo "✓ Di::del() removes the entry\n";
            } else {
                echo "✗ Di::del() did not remove the entry\n";
            }
            Di::del('di_test_object');
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * getInstance() singleton + magic accessors.
     */
    public function testInstanceAndMagicAccess()
    {
        echo "Testing Di Singleton And Magic Access:\n";

        try {
            $a = Di::getInstance();
            $b = Di::getInstance();
            if ($a instanceof Di && $a === $b) {
                echo "✓ Di::getInstance() returns the same singleton\n";
            } else {
                echo "✗ Di::getInstance() identity mismatch\n";
            }

            $a->magicKey = 'magic-value';
            if (Di::get('magicKey') === 'magic-value') {
                echo "✓ magic __set() registers into the container\n";
            } else {
                echo "✗ magic __set() did not register the value\n";
            }
            if ($a->magicKey === 'magic-value') {
                echo "✓ magic __get() reads from the container\n";
            } else {
                echo "✗ magic __get() returned unexpected value\n";
            }
            Di::del('magicKey');
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Registry stores values verbatim (arrays, closures) and overwrites on re-set.
     * Note: class/params config resolution only applies to entries coming from
     * the persistent config cache (Gene\Config), not to Di::set() values.
     */
    public function testValueSemantics()
    {
        echo "Testing Di Value Semantics:\n";

        try {
            $arr = ['class' => 'GeneDiTestService', 'params' => ['alpha', 7]];
            Di::set('di_test_service', $arr);
            if (Di::get('di_test_service') === $arr) {
                echo "✓ Di::set() stores arrays verbatim (no implicit resolution)\n";
            } else {
                echo "✗ Di::get() did not return the stored array verbatim\n";
            }

            Di::set('di_test_service', 'overwritten');
            if (Di::get('di_test_service') === 'overwritten') {
                echo "✓ Di::set() overwrites an existing key\n";
            } else {
                echo "✗ Di::set() did not overwrite the existing key\n";
            }

            $fn = function () { return 'fn-result'; };
            Di::set('di_test_closure', $fn);
            $got = Di::get('di_test_closure');
            if ($got instanceof \Closure && $got() === 'fn-result') {
                echo "✓ closures round-trip through the registry\n";
            } else {
                echo "✗ closure round-trip failed\n";
            }

            Di::del('di_test_service');
            Di::del('di_test_closure');
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Unknown keys resolve to null without warnings.
     */
    public function testMissingKeys()
    {
        echo "Testing Di Missing Keys:\n";

        try {
            if (Di::has('di_test_no_such_key') === false) {
                echo "✓ Di::has() is false for unknown keys\n";
            } else {
                echo "✗ Di::has() true for unknown key\n";
            }
            if (Di::get('di_test_no_such_key') === null) {
                echo "✓ Di::get() returns null for unknown keys\n";
            } else {
                echo "✗ Di::get() non-null for unknown key\n";
            }
        } catch (\Throwable $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * [GENE_FEATURE:2026-08-07] Test Di::alias()
     */
    public function testAlias()
    {
        echo "Testing Di alias():\n";

        try {
            $di = new \Gene\Di();
            // Register a service then alias it
            $di->set("db", function() { return new \stdClass(); });
            $di->alias("database", "db");
            echo "✓ alias('database', 'db') works\n";

            // Resolve via alias
            $instance = $di->instance("database");
            if ($instance instanceof \stdClass) {
                echo "✓ instance('database') resolved alias to target service\n";
            } else {
                echo "✗ instance('database') did not resolve to stdClass\n";
            }
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    public function runAllTests()
    {
        $this->testStaticRegistry();
        $this->testInstanceAndMagicAccess();
        $this->testValueSemantics();
        $this->testMissingKeys();
        $this->testAlias();

        echo "=== Di Test Suite Complete ===\n";
    }
}

if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new DiTest();
    $test->runAllTests();
}
