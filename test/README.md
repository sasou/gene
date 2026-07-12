# Gene Framework Test Suite

This directory contains comprehensive test files for all major classes in the Gene Framework. Each test file covers the important methods and functionality of its corresponding class.

## Test Files

### Core Framework Classes

1. **ApplicationTest.php** - Tests for `Gene\Application` class
   - Constructor and getInstance methods
   - Environment and runtime configuration
   - Request information methods
   - State management
   - Configuration and autoloading
   - Error and exception handling
   - View configuration
   - Magic methods

2. **CacheTest.php** - Tests for `Gene\Cache` class
   - Constructor with different configurations
   - Basic caching functionality
   - Version-based caching
   - Cache invalidation
   - TTL (Time To Live) management
   - Complex object caching
   - Performance testing
   - Error handling

3. **RouterTest.php** - Tests for `Gene\Router` class
   - Constructor and basic routing
   - Route registration and matching
   - Template rendering
   - Dispatch functionality
   - Parameter handling
   - Route groups and prefixes
   - Language support
   - Magic methods for HTTP verbs

### Utility Classes

4. **SessionTest.php** - Tests for `Gene\Session` class
   - Constructor with configuration
   - Session data operations (get, set, delete)
   - Session lifecycle management
   - Session ID management
   - Lifetime configuration
   - Cookie operations
   - Data type handling
   - Performance testing

5. **LogTest.php** - Tests for `Gene\Log` class
   - Basic logging methods (debug, info, warning, error)
   - Exception logging
   - Log level management
   - File configuration
   - Message type handling
   - Performance testing
   - Error handling

6. **LanguageTest.php** - Tests for `Gene\Language` class
   - Constructor and configuration
   - Language switching
   - Translation retrieval
   - Magic methods (__call, __get)
   - Fallback behavior
   - Parameter substitution
   - Nested translations
   - Performance testing

7. **ServiceTest.php** - Tests for `Gene\Service` class
   - Constructor and magic methods
   - Success and error responses
   - Property management
   - Response formats
   - State management
   - Data container functionality
   - Response consistency

8. **BenchmarkTest.php** - Tests for `Gene\Benchmark` class
   - Constructor and timing methods
   - Memory usage tracking
   - Multiple benchmark cycles
   - Different operation types
   - Accuracy and precision
   - Nested operations
   - Performance testing

9. **ExecuteTest.php** - Tests for `Gene\Execute` class
   - Constructor and configuration
   - Opcode generation
   - String code execution
   - Complex code execution
   - Error handling
   - PHP features support
   - Security considerations
   - Return type handling

### HTTP Layer

10. **HttpTest.php** - Tests for HTTP classes
    - **Request class**: Method detection, parameter handling, headers, files, cookies
    - **Response class**: Content setting, status codes, headers, redirects
    - **Validate class**: String, numeric, email, URL, file validation
    - HTTP workflow integration
    - Security features
    - Performance testing

### MVC Layer

11. **MvcTest.php** - Tests for MVC classes
    - **Controller class**: Action handling, rendering, redirection, parameter management
    - **Model class**: CRUD operations, query building, relationships, events
    - **View class**: Template rendering, variable assignment, helpers, caching
    - MVC integration and patterns
    - Performance testing
    - Error handling

### Database Layer

12. **DatabaseTest.php** - Tests for Database classes
    - **MySQL class**: Connection, CRUD operations, transactions, prepared statements
    - **PostgreSQL class**: Specific features, JSON/Array support, parameter binding
    - **SQLite class**: In-memory database, specific features, PRAGMA settings
    - **PDO class**: Generic database access, different DSN formats
    - **Pool class**: Connection pooling, management, statistics
    - Query builder, migrations, security, performance

## Usage

### Running All Tests

```bash
php TestRunner.php
```

### Running Specific Test

```bash
php TestRunner.php --test ApplicationTest.php
```

or

```bash
php TestRunner.php -t ApplicationTest.php
```

### Listing Available Tests

```bash
php TestRunner.php --list
```

### Running Individual Test Files

```bash
php ApplicationTest.php
php CacheTest.php
php RouterTest.php
# ... etc
```

## Test Structure

Each test file follows a consistent structure:

1. **Constructor** - Sets up the test environment
2. **Individual Test Methods** - Test specific functionality
3. **Error Handling Tests** - Test edge cases and error conditions
4. **Performance Tests** - Test performance with multiple operations
5. **Integration Tests** - Test interaction with other components
6. **runAllTests()** - Executes all test methods in the file

## Test Output

Tests use visual indicators:
- ✓ indicates a successful test
- ✗ indicates a failed test
- Each test method provides descriptive output

## Coverage

The test suite covers:

- **Public Methods**: All major public methods of each class
- **Constructor Variations**: Different ways to instantiate classes
- **Parameter Combinations**: Various parameter combinations and types
- **Error Conditions**: How classes handle errors and edge cases
- **Performance**: Basic performance testing with multiple operations
- **Integration**: How classes work together
- **Security**: Security-related functionality

## Requirements

- PHP 8.0 or higher (the extension build configuration requires PHP 8.0+)
- Gene Framework extension loaded
- Appropriate permissions for file operations
- Database access for database tests (optional)

## Notes

- Some tests may require actual database connections to fully test functionality
- Tests are designed to be run in isolation but can also be run as a complete suite
- Performance tests provide relative measurements and may vary based on system resources
- Error handling tests verify graceful degradation rather than complete failure

## Contributing

When adding new tests:

1. Follow the existing naming conventions
2. Include both positive and negative test cases
3. Add performance tests for critical operations
4. Test error conditions and edge cases
5. Document any special requirements or setup needed

This comprehensive test suite helps ensure the reliability and robustness of the Gene Framework across all its major components.
