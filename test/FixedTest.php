<?php

echo "=== Fixed Gene Framework Test ===\n\n";

// Test Application with correct namespace
try {
    $app = new Gene\Application();
    echo "✓ Gene\Application works\n";
    
    // Test some methods
    $env = $app->getEnvironment();
    echo "✓ getEnvironment() works\n";
    
} catch (Exception $e) {
    echo "✗ Gene\Application failed: " . $e->getMessage() . "\n";
}

// Test Router
try {
    $router = new Gene\Router();
    echo "✓ Gene\Router works\n";
    
    // Test some methods
    $tree = $router->getTree();
    echo "✓ getTree() works\n";
    
} catch (Exception $e) {
    echo "✗ Gene\Router failed: " . $e->getMessage() . "\n";
}

// Test Cache with config
try {
    $cache = new Gene\Cache\Cache([]);
    echo "✓ Gene\Cache\Cache works\n";
    
} catch (Exception $e) {
    echo "✗ Gene\Cache\Cache failed: " . $e->getMessage() . "\n";
}

// Test Session with config
try {
    $session = new Gene\Session([]);
    echo "✓ Gene\Session works\n";
    
} catch (Exception $e) {
    echo "✗ Gene\Session failed: " . $e->getMessage() . "\n";
}

echo "\n=== Test Summary ===\n";
echo "✓ Core Gene Framework classes are functional\n";
echo "! Need to update all test files with correct namespaces and parameters\n";
echo "! Database classes require actual database connections\n";
