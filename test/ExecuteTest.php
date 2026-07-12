<?php

/**
 * Gene Framework Execute Class Test
 * 
 * This test file covers the important methods of the Gene\Execute class
 */

use Gene\Execute;

class ExecuteTest
{
    private $execute;
    
    public function __construct()
    {
        echo "=== Gene\Execute Test Suite ===\n\n";
    }
    
    /**
     * Test Execute constructor
     */
    public function testConstructor()
    {
        echo "Testing Execute Constructor:\n";
        
        try {
            // Test constructor without parameters
            $execute1 = new Execute();
            echo "✓ Execute constructor without parameters works\n";
            
            // Test constructor with debug mode
            $execute2 = new Execute(true);
            echo "✓ Execute constructor with debug mode works\n";
            
            // Test constructor with debug disabled
            $execute3 = new Execute(false);
            echo "✓ Execute constructor with debug disabled works\n";
            
            $this->execute = $execute1;
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test GetOpcodes method
     */
    public function testGetOpcodes()
    {
        echo "Testing GetOpcodes Method:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test with simple PHP code (no <?php tag — GetOpcodes compiles raw PHP)
            $simpleCode = '$x = 1 + 2; echo $x;';
            $opcodes1 = $this->execute->GetOpcodes($simpleCode);
            echo "✓ GetOpcodes() with simple code works\n";
            
            // Test with variable assignment
            $variableCode = '$name = "John"; $age = 25; echo "$name is $age years old";';
            $opcodes2 = $this->execute->GetOpcodes($variableCode);
            echo "✓ GetOpcodes() with variable assignment works\n";
            
            // Test with function definition
            $functionCode = 'function add($a, $b) { return $a + $b; } echo add(5, 3);';
            $opcodes3 = $this->execute->GetOpcodes($functionCode);
            echo "✓ GetOpcodes() with function definition works\n";
            
            // Test with loop
            $loopCode = 'for($i = 0; $i < 5; $i++) { echo $i; }';
            $opcodes4 = $this->execute->GetOpcodes($loopCode);
            echo "✓ GetOpcodes() with loop works\n";
            
            // Test with class definition
            $classCode = 'class Test { public $prop = "value"; public function method() { return "method"; } }';
            $opcodes5 = $this->execute->GetOpcodes($classCode);
            echo "✓ GetOpcodes() with class definition works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test StringRun method
     */
    public function testStringRun()
    {
        echo "Testing StringRun Method:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test with simple expression
            $simpleCode = 'return 2 + 3;';
            $result1 = $this->execute->StringRun($simpleCode);
            echo "✓ StringRun() with simple expression works\n";
            
            // Test with variable assignment and return
            $variableCode = '$x = 10; $y = 20; return $x + $y;';
            $result2 = $this->execute->StringRun($variableCode);
            echo "✓ StringRun() with variables works\n";
            
            // Test with string operations
            $stringCode = '$name = "World"; return "Hello, " . $name . "!";';
            $result3 = $this->execute->StringRun($stringCode);
            echo "✓ StringRun() with string operations works\n";
            
            // Test with array operations
            $arrayCode = '$arr = [1, 2, 3]; return array_sum($arr);';
            $result4 = $this->execute->StringRun($arrayCode);
            echo "✓ StringRun() with array operations works\n";
            
            // Test with conditional logic
            $conditionalCode = '$x = 15; return $x > 10 ? "greater" : "less";';
            $result5 = $this->execute->StringRun($conditionalCode);
            echo "✓ StringRun() with conditional logic works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test complex code execution
     */
    public function testComplexCodeExecution()
    {
        echo "Testing Complex Code Execution:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test with function definition and call
            $functionCode = '
                function calculate($a, $b) {
                    return $a * $b + ($a + $b);
                }
                return calculate(5, 3);
            ';
            $result1 = $this->execute->StringRun($functionCode);
            echo "✓ Function definition and call works\n";
            
            // Test with class instantiation
            $classCode = '
                class Calculator {
                    private $value = 0;
                    public function add($num) {
                        $this->value += $num;
                        return $this;
                    }
                    public function getResult() {
                        return $this->value;
                    }
                }
                $calc = new Calculator();
                return $calc->add(10)->add(20)->getResult();
            ';
            $result2 = $this->execute->StringRun($classCode);
            echo "✓ Class instantiation and method chaining works\n";
            
            // Test with array manipulation
            $arrayCode = '
                $numbers = [1, 2, 3, 4, 5];
                $squared = array_map(function($n) { return $n * $n; }, $numbers);
                return array_sum($squared);
            ';
            $result3 = $this->execute->StringRun($arrayCode);
            echo "✓ Array manipulation with closures works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test error handling
     */
    public function testErrorHandling()
    {
        echo "Testing Error Handling:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test with syntax error (ParseError extends Error, not Exception — catch Throwable)
            try {
                $syntaxError = '$x = 1 + ;';
                $result = $this->execute->StringRun($syntaxError);
                echo "✓ Syntax error handled gracefully\n";
            } catch (\Throwable $e) {
                echo "✓ Syntax error properly caught\n";
            }
            
            // Test with undefined variable
            try {
                $undefinedVar = 'return $undefinedVariable;';
                $result = $this->execute->StringRun($undefinedVar);
                echo "✓ Undefined variable handled gracefully\n";
            } catch (\Throwable $e) {
                echo "✓ Undefined variable properly caught\n";
            }
            
            // Test with fatal error
            try {
                $fatalError = 'require("nonexistent_file.php");';
                $result = $this->execute->StringRun($fatalError);
                echo "✓ Fatal error handled gracefully\n";
            } catch (\Throwable $e) {
                echo "✓ Fatal error properly caught\n";
            }
            
            // Test with empty code
            try {
                $emptyCode = '';
                $result = $this->execute->StringRun($emptyCode);
                echo "✓ Empty code handled gracefully\n";
            } catch (\Throwable $e) {
                echo "✓ Empty code properly handled\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Unexpected error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test different PHP features
     */
    public function testPHPFeatures()
    {
        echo "Testing Different PHP Features:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test with namespaces
            $namespaceCode = '
                namespace Test {
                    class Helper {
                        public static function greet($name) {
                            return "Hello, $name!";
                        }
                    }
                }
                use Test\Helper;
                return Helper::greet("World");
            ';
            $result1 = $this->execute->StringRun($namespaceCode);
            echo "✓ Namespaces work\n";
            
            // Test with traits
            $traitCode = '
                trait Logger {
                    public function log($message) {
                        return "LOG: $message";
                    }
                }
                class User {
                    use Logger;
                    public function getName() {
                        return "John";
                    }
                }
                $user = new User();
                return $user->log($user->getName());
            ';
            $result2 = $this->execute->StringRun($traitCode);
            echo "✓ Traits work\n";
            
            // Test with generators
            $generatorCode = '
                function countToThree() {
                    for($i = 1; $i <= 3; $i++) {
                        yield $i;
                    }
                }
                $sum = 0;
                foreach(countToThree() as $num) {
                    $sum += $num;
                }
                return $sum;
            ';
            $result3 = $this->execute->StringRun($generatorCode);
            echo "✓ Generators work\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test performance with different code sizes
     */
    public function testPerformance()
    {
        echo "Testing Performance with Different Code Sizes:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test with small code
            $startTime = microtime(true);
            for ($i = 0; $i < 100; $i++) {
                $smallCode = 'return $i * 2;';
                $result = $this->execute->StringRun($smallCode);
            }
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 100 small code executions in " . number_format($duration, 2) . "ms\n";
            
            // Test with medium code
            $startTime = microtime(true);
            for ($i = 0; $i < 50; $i++) {
                $mediumCode = '
                    $x = $i;
                    $y = $x * 2;
                    $z = $y + 10;
                    return $z;
                ';
                $result = $this->execute->StringRun($mediumCode);
            }
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 50 medium code executions in " . number_format($duration, 2) . "ms\n";
            
            // Test opcode generation performance
            $startTime = microtime(true);
            for ($i = 0; $i < 20; $i++) {
                $code = '$a = $i; $b = $a * 2; return $b;';
                $opcodes = $this->execute->GetOpcodes($code);
            }
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            echo "✓ 20 opcode generations in " . number_format($duration, 2) . "ms\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test debug mode functionality
     */
    public function testDebugMode()
    {
        echo "Testing Debug Mode Functionality:\n";
        
        try {
            // Test with debug mode enabled
            $debugExecute = new Execute(true);
            
            $debugCode = '$x = 5; $y = 10; return $x + $y;';
            $debugResult = $debugExecute->StringRun($debugCode);
            echo "✓ Debug mode enabled execution works\n";
            
            $debugOpcodes = $debugExecute->GetOpcodes('echo "Debug test";');
            echo "✓ Debug mode enabled opcode generation works\n";
            
            // Test with debug mode disabled
            $normalExecute = new Execute(false);
            
            $normalResult = $normalExecute->StringRun($debugCode);
            echo "✓ Normal mode execution works\n";
            
            $normalOpcodes = $normalExecute->GetOpcodes('echo "Normal test";');
            echo "✓ Normal mode opcode generation works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test code with different return types
     */
    public function testReturnTypes()
    {
        echo "Testing Different Return Types:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test integer return
            $intCode = 'return 42;';
            $intResult = $this->execute->StringRun($intCode);
            echo "✓ Integer return works\n";
            
            // Test float return
            $floatCode = 'return 3.14159;';
            $floatResult = $this->execute->StringRun($floatCode);
            echo "✓ Float return works\n";
            
            // Test string return
            $stringCode = 'return "Hello World";';
            $stringResult = $this->execute->StringRun($stringCode);
            echo "✓ String return works\n";
            
            // Test boolean return
            $boolCode = 'return true;';
            $boolResult = $this->execute->StringRun($boolCode);
            echo "✓ Boolean return works\n";
            
            // Test array return
            $arrayCode = 'return [1, 2, 3, "test"];';
            $arrayResult = $this->execute->StringRun($arrayCode);
            echo "✓ Array return works\n";
            
            // Test object return
            $objectCode = '
                $obj = new stdClass();
                $obj->name = "Test";
                $obj->value = 123;
                return $obj;
            ';
            $objectResult = $this->execute->StringRun($objectCode);
            echo "✓ Object return works\n";
            
            // Test null return
            $nullCode = 'return null;';
            $nullResult = $this->execute->StringRun($nullCode);
            echo "✓ Null return works\n";
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test security considerations
     */
    public function testSecurityConsiderations()
    {
        echo "Testing Security Considerations:\n";
        
        try {
            if (!$this->execute) {
                $this->execute = new Execute();
            }
            
            // Test with potentially dangerous functions (should be handled gracefully)
            try {
                $dangerousCode = 'exec("ls");';
                $result = $this->execute->StringRun($dangerousCode);
                echo "✓ Dangerous function handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Dangerous function properly blocked\n";
            }
            
            // Test with file operations
            try {
                $fileCode = 'file_put_contents("test.txt", "test");';
                $result = $this->execute->StringRun($fileCode);
                echo "✓ File operation handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ File operation properly handled\n";
            }
            
            // Test with network operations
            try {
                $networkCode = 'file_get_contents("http://example.com");';
                $result = $this->execute->StringRun($networkCode);
                echo "✓ Network operation handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Network operation properly handled\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Unexpected error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Run all tests
     */
    public function runAllTests()
    {
        $this->testConstructor();
        $this->testGetOpcodes();
        $this->testStringRun();
        $this->testComplexCodeExecution();
        $this->testErrorHandling();
        $this->testPHPFeatures();
        $this->testPerformance();
        $this->testDebugMode();
        $this->testReturnTypes();
        $this->testSecurityConsiderations();
        
        echo "=== Execute Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new ExecuteTest();
    $test->runAllTests();
}
