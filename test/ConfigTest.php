<?php

/**
 * Gene Framework Config Class Test
 *
 * Covers Gene\Config dotted-key write/read semantics, with emphasis on
 * overwrite behaviour between scalar leaves and nested directories.
 */

use Gene\Config;

class ConfigTest
{
    private $config;

    public function __construct()
    {
        echo "=== Gene\Config Test Suite ===\n\n";
    }

    private function config()
    {
        if (!$this->config) {
            $this->config = new Config();
        }
        return $this->config;
    }

    /**
     * Basic scalar / array round-trip on dotted keys.
     */
    public function testBasicSetGet()
    {
        echo "Testing Config Basic Set/Get:\n";

        try {
            $c = $this->config();

            $c->set('cfgtest.basic.host', '127.0.0.1');
            $got = $c->get('cfgtest.basic.host');
            if ($got === '127.0.0.1') {
                echo "✓ scalar round-trip on dotted key\n";
            } else {
                echo "✗ scalar round-trip returned " . var_export($got, true) . "\n";
            }

            $c->set('cfgtest.basic.pool', ['size' => 8, 'idle' => 2]);
            $got = $c->get('cfgtest.basic.pool');
            if (is_array($got) && ($got['size'] ?? null) === 8) {
                echo "✓ array round-trip on dotted key\n";
            } else {
                echo "✗ array round-trip returned " . var_export($got, true) . "\n";
            }

            $got = $c->get('cfgtest.basic');
            if (is_array($got) && isset($got['host'], $got['pool'])) {
                echo "✓ intermediate node reads back as a directory\n";
            } else {
                echo "✗ intermediate node returned " . var_export($got, true) . "\n";
            }
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Overwrite semantics. A later set() always wins, whatever the type
     * transition is — scalar→scalar, scalar→array, and (the case a router
     * fix once broke) array→scalar, which must not be silently dropped.
     */
    public function testOverwriteSemantics()
    {
        echo "Testing Config Overwrite Semantics:\n";

        try {
            $c = $this->config();

            $c->set('cfgtest.ow.scalar', 'first');
            $c->set('cfgtest.ow.scalar', 'second');
            $got = $c->get('cfgtest.ow.scalar');
            if ($got === 'second') {
                echo "✓ scalar overwritten by scalar\n";
            } else {
                echo "✗ scalar overwrite returned " . var_export($got, true) . "\n";
            }

            $c->set('cfgtest.ow.promote', 'leaf');
            $c->set('cfgtest.ow.promote', ['a' => 1]);
            $got = $c->get('cfgtest.ow.promote');
            if (is_array($got) && ($got['a'] ?? null) === 1) {
                echo "✓ scalar overwritten by array\n";
            } else {
                echo "✗ scalar→array overwrite returned " . var_export($got, true) . "\n";
            }

            // Directory collapsed back to a scalar: the whole subtree goes away.
            $c->set('cfgtest.ow.demote.host', '127.0.0.1');
            $c->set('cfgtest.ow.demote', 'disabled');
            $got = $c->get('cfgtest.ow.demote');
            if ($got === 'disabled') {
                echo "✓ array directory overwritten by scalar\n";
            } else {
                echo "✗ array→scalar overwrite silently dropped, got " . var_export($got, true) . "\n";
            }

            // Same transition through the nested-write path rather than a
            // top-level key, since these take different branches internally.
            $c->set('cfgtest.ow.deep.db.master.host', 'a');
            $c->set('cfgtest.ow.deep.db.master', 'b');
            $got = $c->get('cfgtest.ow.deep.db.master');
            if ($got === 'b') {
                echo "✓ nested array directory overwritten by scalar\n";
            } else {
                echo "✗ nested array→scalar overwrite dropped, got " . var_export($got, true) . "\n";
            }
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Writing under a key that currently holds a scalar must promote it to a
     * directory instead of dereferencing a non-array.
     */
    public function testLeafPromotion()
    {
        echo "Testing Config Leaf Promotion:\n";

        try {
            $c = $this->config();

            $c->set('cfgtest.promo.node', 'scalar');
            $c->set('cfgtest.promo.node.child', 'value');

            $got = $c->get('cfgtest.promo.node.child');
            if ($got === 'value') {
                echo "✓ scalar leaf promoted to directory on nested write\n";
            } else {
                echo "✗ nested write under scalar leaf returned " . var_export($got, true) . "\n";
            }

            $got = $c->get('cfgtest.promo.node');
            if (is_array($got) && ($got['child'] ?? null) === 'value') {
                echo "✓ promoted node reads back as a directory\n";
            } else {
                echo "✗ promoted node returned " . var_export($got, true) . "\n";
            }
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Missing keys must report as null rather than leaking a neighbour.
     */
    public function testMissingKey()
    {
        echo "Testing Config Missing Key:\n";

        try {
            $c = $this->config();

            $got = $c->get('cfgtest.nope.nothing.here');
            if ($got === null) {
                echo "✓ missing key returns null\n";
            } else {
                echo "✗ missing key returned " . var_export($got, true) . "\n";
            }
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }

        echo "\n";
    }

    /**
     * Run all tests
     */
    public function runAllTests()
    {
        $this->testBasicSetGet();
        $this->testOverwriteSemantics();
        $this->testLeafPromotion();
        $this->testMissingKey();

        echo "=== Config Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new ConfigTest();
    $test->runAllTests();
}
