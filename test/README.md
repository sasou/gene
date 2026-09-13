# Gene Framework Test Suite

This directory contains the regression test suite for the Gene extension. `TestRunner.php` executes each `*Test.php` file below in an isolated PHP child process and aggregates pass/fail counts.

Related verification assets outside this directory:

- `audit/repro/` — one-shot reproduction / leak-probe scripts backing individual audit findings (see `audit/README.md`).
- `tools/acceptance/` — FPM/Swoole acceptance harness, benchmark and Linux release gate (see `tools/acceptance/README.md`).

## Test Files (run by TestRunner)

### Core Framework

| File | Coverage |
|------|----------|
| `ApplicationTest.php` | `Gene\Application`: getInstance, environment/runtime config, request info, state management, config & autoloading, error/exception handling, view config, magic methods |
| `CacheTest.php` | `Gene\Cache`: configurations, basic caching, versioned cache, invalidation, TTL, complex objects, performance, error handling |
| `ConfigTest.php` | `Gene\Config`: dotted-key write/read, scalar-leaf vs nested-directory overwrite semantics |
| `RouterTest.php` | `Gene\Router`: registration & matching, groups/prefixes, dispatch, params, language routes, HTTP verb magic methods, template rendering |
| `SessionTest.php` | `Gene\Session`: data ops, lifecycle, session-id management, lifetime, cookies, data types, performance |
| `LogTest.php` | `Gene\Log`: debug/info/warning/error, exception logging, level management, file config, message types, performance |
| `LanguageTest.php` | `Gene\Language`: switching, retrieval, `__call`/`__get`, fallback, parameter substitution, nested translations |
| `ServiceTest.php` | `Gene\Service`: magic methods, success/error responses, property & state management, response consistency |
| `BenchmarkTest.php` | `Gene\Benchmark`: timing, memory tracking, multiple cycles, accuracy, nested operations |
| `ExecuteTest.php` | `Gene\Execute`: opcode generation, string code execution, error handling, PHP feature support, security |
| `DiTest.php` | `Gene\Di`: static set/get/has/del registry, magic accessors via singleton, verbatim value semantics |
| `HookTest.php` | `Gene\Hook`: before/after/handle override points, success/error/data payload shapes, method predicates, static request accessors |

### HTTP Layer

| File | Coverage |
|------|----------|
| `HttpTest.php` | `Request`/`Response`/`Validate`: method detection, params, headers, files, cookies; status codes & redirects; string/numeric/email/url/file validation; workflow integration |
| `HttpClientTest.php` | `Gene\Http::request()`: curl backend against a local `php -S` echo fixture; Swoole coroutine client when `runtime_type>=2` — explicit SKIP without the env, no fake pass |
| `LifecycleTest.php` | `Context` / `Json` / `Request::json` / SSE `write` / `Crypto` / `Memory` rateLimit+lock; Redis paths SKIP without env. Leak probe: `audit/repro/lifecycle_leak_probe.php` |
| `RestInvokeTest.php` | `Gene\Invoke` local dispatch and `Gene\Rest` immutable proxy semantics |
| `SwooleEntryTest.php` | `Request::initSwoole` / `Application::handleSwoole` / `bootstrap()` / `pools()` lifecycle via duck-typed Swoole doubles — ext-swoole not required |

### MVC Layer

| File | Coverage |
|------|----------|
| `MvcTest.php` | `Controller`/`Model`/`View`: action handling, rendering, redirects, parameter management, integration patterns |
| `OrmTest.php` | `Gene\Orm\Model` & `Query`: class surface always asserted; SQLite in-memory CRUD when available via DI |

### Database Layer

| File | Coverage |
|------|----------|
| `DatabaseTest.php` | `Mysql`/`Pgsql`/`Sqlite`/`Pdo`/`Pool`: connections, CRUD, transactions, prepared statements, query builder, migrations; real-server sections degrade to reported items when unreachable |

## Standalone Helpers (not run by TestRunner)

| File | Purpose |
|------|---------|
| `FixedTest.php` | Minimal smoke checks for Application/Router instantiation |
| `SimpleTest.php` | Instantiation smoke test across core classes |
| `url_methods_test.php` | URL/path helper probe across Application/Controller/View/Hook/Response — prints values, no assertions |
| `fixtures/http_echo.php` | Echo endpoint served via `php -S`, used by `HttpClientTest` and `RestInvokeTest` |

## Usage

### Running All Tests

```bash
php TestRunner.php
```

### Running Specific Test

```bash
php TestRunner.php --test ApplicationTest.php
# or
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

## Running Against a Locally Built Extension (no install)

`TestRunner.php` spawns each test through `PHP_BINARY` in a child process and forwards the contents of the `GENE_TEST_PHP_ARGS` environment variable as extra php arguments. To exercise a freshly built DLL/SO without installing it (e.g. while WampServer still locks the deployed copy), pass the same `-n` / `-d extension=...` arguments to both the runner and its children:

```bat
rem cmd.exe — Windows, testing the no-deploy DLL
set GENE_TEST_PHP_ARGS=-n -d extension_dir=D:\wampServer-php8.1_x64_nts\php_ext -d extension=pdo_sqlite -d extension=F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll
D:\wampServer-php8.1_x64_nts\bin\php.exe %GENE_TEST_PHP_ARGS% test\TestRunner.php
```

```powershell
# PowerShell equivalent
$env:GENE_TEST_PHP_ARGS = '-n -d extension=pdo_sqlite -d extension=F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll'
D:\wampServer-php8.1_x64_nts\bin\php.exe -n -d extension=pdo_sqlite -d extension=F:\php_src\php-8.1.30-src\x64\Release\php_gene.dll test\TestRunner.php
```

```bash
# Linux/macOS equivalent
export GENE_TEST_PHP_ARGS='-n -d extension=pdo_sqlite -d extension=/path/to/gene.so'
php $GENE_TEST_PHP_ARGS test/TestRunner.php
```

- Without `GENE_TEST_PHP_ARGS`, child processes load the default `php.ini` (and possibly an older installed `gene` extension), producing false failures.
- Some tests need extra extensions in these args — e.g. `openssl` for the `LifecycleTest` Crypto cases; missing extensions surface as environmental failures/SKIPs, not regressions.
- On Windows, redirected console output may appear UTF-16 encoded — that is a PowerShell encoding quirk, not a test failure.

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

1. Follow the existing naming conventions (`*Test.php`) and register the file in `TestRunner.php`'s `$testFiles` list — otherwise it is never executed by the suite
2. Include both positive and negative test cases
3. Add performance tests for critical operations
4. Test error conditions and edge cases
5. Document any special requirements or setup needed

This comprehensive test suite helps ensure the reliability and robustness of the Gene Framework across all its major components.
