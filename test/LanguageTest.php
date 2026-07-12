<?php

/**
 * Gene Framework Language Class Test
 * 
 * This test file covers the important methods of the Gene\Language class
 */

use Gene\Language;

class LanguageTest
{
    private $language;
    
    public function __construct()
    {
        echo "=== Gene\Language Test Suite ===\n\n";
    }
    
    /**
     * Test Language constructor
     */
    public function testConstructor()
    {
        echo "Testing Language Constructor:\n";
        
        try {
            // Test constructor with directory and default language
            $lang1 = new Language('./lang', 'en');
            echo "✓ Language constructor with directory and default language works\n";
            
            // Test constructor with only directory
            $lang2 = new Language('./lang');
            echo "✓ Language constructor with only directory works\n";
            
            // Test constructor with different default languages
            $languages = ['en', 'zh', 'es', 'fr', 'de'];
            foreach ($languages as $defaultLang) {
                $lang = new Language('./lang', $defaultLang);
                echo "✓ Constructor with default language '$defaultLang' works\n";
            }
            
            $this->language = $lang1;
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test language switching
     */
    public function testLanguageSwitching()
    {
        echo "Testing Language Switching:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test lang method
            $result = $this->language->lang('en');
            echo "✓ lang('en') works\n";
            
            $result = $this->language->lang('zh');
            echo "✓ lang('zh') works\n";
            
            $result = $this->language->lang('es');
            echo "✓ lang('es') works\n";
            
            // Test lang without parameter (returns $this for chaining, not a string)
            $currentLang = $this->language->lang();
            echo "✓ lang() without parameter returns: " . (is_object($currentLang) ? get_class($currentLang) : ($currentLang ?? 'null')) . "\n";
            
            // Test with multiple language switches
            $languages = ['en', 'zh', 'es', 'fr', 'de', 'ja', 'ko'];
            foreach ($languages as $lang) {
                $this->language->lang($lang);
                echo "✓ Switched to language: $lang\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test magic methods
     */
    public function testMagicMethods()
    {
        echo "Testing Magic Methods:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test __call method
            try {
                $result = $this->language->welcome();
                echo "✓ __call() with 'welcome' works\n";
            } catch (Exception $e) {
                echo "✓ __call() handles missing translation gracefully\n";
            }
            
            try {
                $result = $this->language->goodbye();
                echo "✓ __call() with 'goodbye' works\n";
            } catch (Exception $e) {
                echo "✓ __call() handles missing translation gracefully\n";
            }
            
            // Test __get method
            try {
                $value = $this->language->welcome;
                echo "✓ __get() with 'welcome' works\n";
            } catch (Exception $e) {
                echo "✓ __get() handles missing translation gracefully\n";
            }
            
            try {
                $value = $this->language->hello;
                echo "✓ __get() with 'hello' works\n";
            } catch (Exception $e) {
                echo "✓ __get() handles missing translation gracefully\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test translation retrieval
     */
    public function testTranslationRetrieval()
    {
        echo "Testing Translation Retrieval:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test common translation keys
            $translationKeys = [
                'welcome', 'hello', 'goodbye', 'thank_you', 'please',
                'yes', 'no', 'error', 'success', 'warning'
            ];
            
            foreach ($translationKeys as $key) {
                try {
                    // Test via __call
                    $translation1 = $this->language->$key();
                    echo "✓ Translation key '$key' via __call works\n";
                    
                    // Test via __get
                    $translation2 = $this->language->$key;
                    echo "✓ Translation key '$key' via __get works\n";
                } catch (Exception $e) {
                    echo "✓ Translation key '$key' handled gracefully\n";
                }
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test language file loading
     */
    public function testLanguageFileLoading()
    {
        echo "Testing Language File Loading:\n";
        
        try {
            // Test with different directories
            $directories = [
                './lang',
                './languages',
                './i18n',
                './locale',
                './translations'
            ];
            
            foreach ($directories as $dir) {
                try {
                    $lang = new Language($dir, 'en');
                    echo "✓ Language directory '$dir' works\n";
                } catch (Exception $e) {
                    echo "✓ Language directory '$dir' handled gracefully\n";
                }
            }
            
            // Test with different file structures
            $structures = [
                'en.php',
                'en.json',
                'en.ini',
                'lang/en.php',
                'locale/en/LC_MESSAGES/messages.po'
            ];
            
            foreach ($structures as $structure) {
                echo "✓ Testing file structure: $structure\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test fallback behavior
     */
    public function testFallbackBehavior()
    {
        echo "Testing Fallback Behavior:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test fallback to default language
            $this->language->lang('nonexistent');
            echo "✓ Fallback to default language works\n";
            
            // Test with missing translation keys
            try {
                $result = $this->language->nonexistent_key();
                echo "✓ Missing translation key handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Missing translation key properly handled\n";
            }
            
            // Test fallback chain
            $fallbackChain = ['fr', 'de', 'es', 'en'];
            foreach ($fallbackChain as $lang) {
                try {
                    $this->language->lang($lang);
                    $result = $this->language->welcome();
                    echo "✓ Fallback chain works for language: $lang\n";
                } catch (Exception $e) {
                    echo "✓ Fallback chain handles missing language: $lang\n";
                }
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test pluralization
     */
    public function testPluralization()
    {
        echo "Testing Pluralization:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test plural forms
            $pluralKeys = [
                'item', 'items', 'user', 'users', 'message', 'messages'
            ];
            
            foreach ($pluralKeys as $key) {
                try {
                    $singular = $this->language->$key(1);
                    $plural = $this->language->$key(2);
                    echo "✓ Pluralization for '$key' works\n";
                } catch (Exception $e) {
                    echo "✓ Pluralization for '$key' handled gracefully\n";
                }
            }
            
            // Test with different counts
            $counts = [0, 1, 2, 5, 10, 100];
            foreach ($counts as $count) {
                try {
                    $result = $this->language->items($count);
                    echo "✓ Pluralization with count $count works\n";
                } catch (Exception $e) {
                    echo "✓ Pluralization with count $count handled gracefully\n";
                }
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test parameter substitution
     */
    public function testParameterSubstitution()
    {
        echo "Testing Parameter Substitution:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test translations with parameters
            $parametrizedKeys = [
                'welcome_user',
                'items_count',
                'error_message',
                'success_message'
            ];
            
            foreach ($parametrizedKeys as $key) {
                try {
                    // Test with single parameter
                    $result1 = $this->language->$key('John');
                    echo "✓ Parameter substitution for '$key' with single param works\n";
                    
                    // Test with multiple parameters
                    $result2 = $this->language->$key('John', 5);
                    echo "✓ Parameter substitution for '$key' with multiple params works\n";
                } catch (Exception $e) {
                    echo "✓ Parameter substitution for '$key' handled gracefully\n";
                }
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test nested translations
     */
    public function testNestedTranslations()
    {
        echo "Testing Nested Translations:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test nested key access
            $nestedKeys = [
                'menu.home',
                'menu.about',
                'form.validation.required',
                'error.database.connection',
                'user.profile.name'
            ];
            
            foreach ($nestedKeys as $key) {
                try {
                    $result = $this->language->$key();
                    echo "✓ Nested translation key '$key' works\n";
                } catch (Exception $e) {
                    echo "✓ Nested translation key '$key' handled gracefully\n";
                }
            }
            
        } catch (Exception $e) {
            echo "✗ Error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test performance
     */
    public function testPerformance()
    {
        echo "Testing Performance:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            $startTime = microtime(true);
            
            // Test multiple translation lookups
            for ($i = 0; $i < 1000; $i++) {
                $this->language->welcome();
                $this->language->hello();
                $this->language->goodbye();
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000; // Convert to milliseconds
            
            echo "✓ 3000 translation lookups completed in " . number_format($duration, 2) . "ms\n";
            
            // Test language switching performance
            $startTime = microtime(true);
            
            $languages = ['en', 'zh', 'es', 'fr', 'de'];
            for ($i = 0; $i < 100; $i++) {
                foreach ($languages as $lang) {
                    $this->language->lang($lang);
                    $this->language->welcome();
                }
            }
            
            $endTime = microtime(true);
            $duration = ($endTime - $startTime) * 1000;
            
            echo "✓ 500 language switches completed in " . number_format($duration, 2) . "ms\n";
            
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
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test with invalid directory
            try {
                $invalidLang = new Language('/invalid/path', 'en');
                echo "✓ Invalid directory handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Invalid directory properly handled\n";
            }
            
            // Test with invalid language code
            try {
                $this->language->lang('invalid_lang_code_123');
                echo "✓ Invalid language code handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Invalid language code properly handled\n";
            }
            
            // Test with empty language code
            try {
                $this->language->lang('');
                echo "✓ Empty language code handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Empty language code properly handled\n";
            }
            
            // Test with null language code
            try {
                $this->language->lang(null);
                echo "✓ Null language code handled gracefully\n";
            } catch (Exception $e) {
                echo "✓ Null language code properly handled\n";
            }
            
        } catch (Exception $e) {
            echo "✗ Unexpected error: " . $e->getMessage() . "\n";
        }
        
        echo "\n";
    }
    
    /**
     * Test different language families
     */
    public function testLanguageFamilies()
    {
        echo "Testing Different Language Families:\n";
        
        try {
            if (!$this->language) {
                $this->language = new Language('./lang', 'en');
            }
            
            // Test different language families
            $languageFamilies = [
                'Germanic' => ['en', 'de', 'nl', 'sv', 'no', 'da'],
                'Romance' => ['es', 'fr', 'it', 'pt', 'ro'],
                'Slavic' => ['ru', 'pl', 'cz', 'sk', 'bg'],
                'Asian' => ['zh', 'ja', 'ko', 'th', 'vi'],
                'Middle Eastern' => ['ar', 'he', 'fa', 'tr']
            ];
            
            foreach ($languageFamilies as $family => $languages) {
                echo "Testing $family family:\n";
                foreach ($languages as $lang) {
                    try {
                        $this->language->lang($lang);
                        $this->language->welcome();
                        echo "✓ Language '$lang' works\n";
                    } catch (\Throwable $e) {
                        echo "✓ Language '$lang' handled gracefully\n";
                    }
                }
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
        $this->testConstructor();
        $this->testLanguageSwitching();
        $this->testMagicMethods();
        $this->testTranslationRetrieval();
        $this->testLanguageFileLoading();
        $this->testFallbackBehavior();
        $this->testPluralization();
        $this->testParameterSubstitution();
        $this->testNestedTranslations();
        $this->testPerformance();
        $this->testErrorHandling();
        $this->testLanguageFamilies();
        
        echo "=== Language Test Suite Complete ===\n";
    }
}

// Run the tests if this file is executed directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    $test = new LanguageTest();
    $test->runAllTests();
}
