# Gene PHP Extension v5.2.1 - Comprehensive Code Analysis Report

## 1. Architecture Overview

The Gene extension is a full-stack PHP MVC framework implemented as a C extension. It consists of:

| Module | Files | Purpose |
|--------|-------|---------|
| **app** | application.c/h | Application bootstrap, routing init, environment management |
| **cache** | memory.c/h, redis.c/h, memcached.c/h, cache.c/h | Persistent in-memory cache, Redis/Memcached wrappers |
| **common** | common.c/h | String utilities, serialization, PHP function wrappers |
| **config** | configs.c/h | Configuration management via persistent cache |
| **db** | mysql.c/h, mssql.c/h, pgsql.c/h, sqlite.c/h, pdo.c/h | Query builder + PDO wrappers |
| **di** | di.c/h | Dependency injection container (singleton) |
| **exception** | exception.c/h | Error/exception handling with HTML renderer |
| **factory** | load.c/h, factory.c/h | Autoloader (spl_autoload_register), class factory |
| **http** | request.c/h, response.c/h, validate.c/h | HTTP request/response, validation |
| **mvc** | controller.c/h, model.c/h, view.c/h | MVC base classes with template engine |
| **router** | router.c/h | Tree-based URL router with hooks |
| **service** | service.c/h | Service layer base class |
| **session** | session.c/h | Session management with cookie support |
| **tool** | execute.c/h, benchmark.c/h, language.c/h | Code execution, benchmarking, i18n |

**Runtime modes:** FPM (1), Swoole/resident (2), Coroutine (3) - controlled via `gene.runtime_type` INI or `setRuntimeType()`.

---

## 2. Critical Bugs

### 2.1 Memory Leak in `getRouterUri` (application.c:375-387)

```c
path = str_init(GENE_G(router_path));       // emalloc'd
path = strreplace2(path, ":m", GENE_G(module));  // may realloc
path = strreplace2(path, ":c", GENE_G(controller));
path = strreplace2(path, ":a", GENE_G(action));
strtolower(path);
RETVAL_STRING(path);  // copies into zval, but `path` is never freed
```

**Impact:** Leaks memory on every call to `getRouterUri()`.
**Fix:** Add `efree(path)` after `RETVAL_STRING(path)`.

### 2.2 `add_assoc_str_ex` Steals Ownership of Borrowed `zend_string` (multiple files)

In `response.c`, `controller.c`, `model.c`, `service.c`, the `success()` and `error()` methods do:

```c
zend_string *text;
zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &text, &code);  // borrowed ref
add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), text);  // takes ownership!
```

`add_assoc_str_ex` takes ownership of the `zend_string` (increments refcount internally in newer PHP, but the semantics assume the caller is giving up its reference). With `zend_parse_parameters "S"`, `text` is a borrowed reference from the argument stack. This can lead to **use-after-free** or premature string destruction.

**Impact:** Potential crash or corruption.
**Fix:** Use `zend_string_copy(text)` to add a reference: `add_assoc_str_ex(return_value, ZEND_STRL(...), zend_string_copy(text))`.

**Affected files:**
- `http/response.c:208,224,242`
- `mvc/controller.c:379,395,413`
- `mvc/model.c:141,157,175`
- `service/service.c:141,157,175`

### 2.3 Potential Crash in `gene_application_instance` (application.c:224-248)

```c
zval *instance = zend_read_static_property(...);  // returns IS_NULL zval
if (Z_TYPE_P(instance) == IS_OBJECT) return instance;
if (this_ptr) {
    instance = this_ptr;
} else {
    object_init_ex(instance, gene_application_ce);  // modifying static property zval directly!
```

When `this_ptr` is NULL (called from `getInstance()`), `instance` points to the internal static property zval. Calling `object_init_ex` on it modifies the static property zval in place, which may cause issues with GC/refcounting.

**Fix:** Use a local zval, initialize it, then update the static property.

### 2.4 `GetOpcodes` Iterates `cache_size` Instead of `last` (execute.c:105)

```c
for (i = 0; i < op_array->cache_size; i++) {
    zend_op op = op_array->opcodes[i];  // out-of-bounds!
```

`cache_size` is the size of the runtime cache (in bytes), not the opcode count. The correct field is `op_array->last` (number of opcodes).

**Impact:** Out-of-bounds read, potential crash/segfault.
**Fix:** Change to `op_array->last`.

### 2.5 `parser_templates` Checks Wrong Variable (view.c:310)

```c
CacheStream = php_stream_open_wrapper(compile_path, "wb", ...);
php_stream_write_string(CacheStream, result);
if (stream == NULL) {  // should check CacheStream, not stream!
```

**Impact:** If `CacheStream` fails to open, NULL dereference on `php_stream_write_string`. The error check on `stream` (the input parameter) is meaningless here.
**Fix:** Check `CacheStream == NULL` before writing.

### 2.6 Global `struct timeval` Variables in DB Modules (mysql.c:37-38, etc.)

```c
struct timeval db_start, db_end;           // file-scope globals
zend_long db_mysql_memory_start = 0, ...;  // file-scope globals
```

All four DB modules (`mysql.c`, `mssql.c`, `pgsql.c`, `sqlite.c`) plus `benchmark.c` declare **file-scope global** timing variables. These are shared across all threads in ZTS builds and all coroutines.

**Impact:** Data races in multi-threaded/coroutine environments. Benchmark data corruption.
**Fix:** Move to per-object properties or use GENE_G() module globals.

---

## 3. PHP 8.2/8.3/8.4 Compatibility Issues

### 3.1 Dynamic Properties Deprecation (PHP 8.2+)

PHP 8.2 deprecated dynamic properties on classes. Internal classes that want to allow dynamic properties must set `ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES`. The Gene extension uses `__get`/`__set` handlers on multiple classes to proxy to the DI container, which means dynamic property access won't trigger the deprecation directly (since `__get`/`__set` intercept). However, if any user code inherits from Gene classes and uses dynamic properties, they will get deprecation warnings.

**Classes with `__get`/`__set` (safe):** `Gene\Controller`, `Gene\Model`, `Gene\Service`, `Gene\View`, `Gene\Application`, `Gene\Di`, `Gene\Session`, `Gene\Request`

**Classes with `__call` (safe):** `Gene\Router`, `Gene\Validate`, `Gene\Redis`, `Gene\Memcached`, `Gene\Language`

**Recommendation:** Add `ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES` to base classes that users extend (`Controller`, `Model`, `Service`, `View`) for safety.

### 3.2 Redundant `INIT_CLASS_ENTRY` Calls

Three files call `INIT_CLASS_ENTRY` directly before `GENE_INIT_CLASS_ENTRY` (which also calls it):

- `tool/language.c:240-241`
- `session/session.c:691-692`
- `http/validate.c:1553-1554`

**Impact:** Harmless (second call overwrites first), but wasteful. Remove the first bare `INIT_CLASS_ENTRY` call.

### 3.3 Dead Code in `gene_get_exception_base` (exception.c:270-283)

```c
#if can_handle_soft_dependency_on_SPL && defined(HAVE_SPL) && ...
```

`can_handle_soft_dependency_on_SPL` is never defined anywhere, so this block is always dead code. The function always returns `zend_exception_get_default()`.

### 3.4 `php_pcre_replace` API Changes

`view.c:277-305` already handles PHP 7.2/7.3 differences. The current code is compatible with PHP 8.x.

### 3.5 `php_stat` API Change (PHP 8.1.1+)

Already handled in `application.c:181-188` with `#if PHP_VERSION_ID >= 80101`.

### 3.6 `zend_compile_string` Signature Change (PHP 8.0+)

Already handled in `execute.c:97-101` with `#if PHP_VERSION_ID < 80000`.

---

## 4. Coroutine/Long-Running Process Issues

### 4.1 `GENE_G()` Module Globals Are Not Coroutine-Safe

All per-request state is stored in module globals (`GENE_G(method)`, `GENE_G(path)`, `GENE_G(module)`, `GENE_G(controller)`, `GENE_G(action)`, `GENE_G(lang)`, `GENE_G(path_params)`, etc.). In Swoole coroutine mode, multiple coroutines share the same process and the same module globals.

**Impact:** If two coroutines handle requests concurrently, they will **overwrite each other's** method, path, module, controller, action, etc.

**Current mitigation:** None visible in the code. The `runtime_type` setting is used only for DI cache strategy and PDO persistent connections.

**Fix options:**
1. Use Swoole's coroutine context storage (not practical from C extension)
2. Store per-request state in a request-scoped object instead of module globals
3. Document that coroutine mode requires external isolation (e.g., each coroutine must complete before the next starts)

### 4.2 Static Properties in Coroutine Mode

Several classes use static properties for singleton instances or shared state:
- `Gene\Application::$instance`
- `Gene\Di::$instance`, `Gene\Di::$reg`
- `Gene\View::$vars`, `Gene\View::$versionNo`
- `Gene\Db\Mysql::$history` (and other DB classes)
- `Gene\Request::$_attr`

In coroutine mode, these are shared across all coroutines in the same worker, which can cause data races.

### 4.3 Persistent Cache (`GENE_G(cache)`) Thread Safety

The persistent `HashTable` in `GENE_G(cache)` is allocated once in `MINIT` and shared across all requests. The `gene_memory_set_by_router` function modifies this HashTable without any locking mechanism.

**Impact:** In ZTS builds or multi-worker Swoole setups with shared memory, concurrent writes can corrupt the HashTable.

### 4.4 `RSHUTDOWN` Frees Per-Request State

`php_gene_close_globals()` is called in `RSHUTDOWN` and frees all per-request globals. In Swoole worker mode (non-coroutine), `RSHUTDOWN` may not be called between requests depending on the Swoole SAPI integration.

---

## 5. Memory Leaks

### 5.1 `getRouterUri` Leak (application.c:375-387)
See Section 2.1 above.

### 5.2 `router.c` `get_path_router_init` Potential Leak (router.c:179)

```c
uri = str_init(path_tmp);
```

`uri` is allocated but only freed in one branch (`efree(uri)` at line ~195). If the `langs` check fails or the language is not found, `uri` may leak.

### 5.3 `router.c` `get_router_content_run` Multiple Allocations (router.c:586-640)

`method` and `path` are allocated via `str_init()` and `str_concat()` but the cleanup at the end of the function (`efree(method); efree(path);`) only happens in some paths. Error early-returns may skip cleanup.

### 5.4 `mysqlSaveHistory` Reference Counting (mysql.c:143-148)

```c
add_next_index_zval(history, &z_row);
Z_TRY_ADDREF_P(&z_row);  // extra addref AFTER insertion
```

`add_next_index_zval` takes ownership of the zval. The subsequent `Z_TRY_ADDREF_P` creates an extra reference, causing `z_row` to never be fully freed.

---

## 6. Security Issues

### 6.1 SQL Injection in DB Query Builder

The `select()`, `count()`, `insert()`, `update()`, `delete()` methods in `mysql.c` (and other DB modules) use `spprintf` to build SQL strings with user-provided table names and field names **without escaping**:

```c
spprintf(&sql, 0, "SELECT %s FROM %s", Z_STRVAL_P(fields), table);
```

While parameter values are properly bound via PDO prepared statements, **table names and column names are injected directly**.

**Impact:** SQL injection if user input reaches table/column name parameters.
**Recommendation:** Add identifier quoting (backtick-wrapping) and validation for table/column names.

### 6.2 XSS in `gene_response::alert` (response.c:189-193)

```c
php_printf("\n<script type=\"text/javascript\">\nalert(\"%s\");\n", ZSTR_VAL(text));
```

The `text` parameter is output directly into a `<script>` tag without HTML/JS escaping.

**Impact:** If user-controlled data reaches this method, it enables XSS attacks.

### 6.3 XSS in Exception Handler HTML Output (exception.c)

The `doException` method outputs file paths, class names, function names, and argument values directly into HTML without escaping:

```c
length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_STR, file != NULL ? Z_STRVAL_P(file) : "");
```

**Impact:** Stack trace data could contain attacker-controlled strings (e.g., from class names or error messages).
**Mitigation:** This should only be active in development mode, but there's no environment check.

---

## 7. Code Quality Issues

### 7.1 Duplicate Code

The `success()`, `error()`, `data()` methods are copy-pasted identically across 4 files:
- `http/response.c`
- `mvc/controller.c`
- `mvc/model.c`
- `service/service.c`

Similarly, `__get`/`__set` DI proxy methods are duplicated in `controller.c`, `model.c`, `service.c`, `view.c`, `application.c`.

**Recommendation:** Extract into shared C functions or macros.

### 7.2 `session.c:85` Integer Overflow

```c
int jg;
jg = Z_LVAL(curtime) + Z_LVAL_P(lifetime);  // zend_long -> int truncation
```

`curtime` is a Unix timestamp (large number). Storing the sum in `int` risks overflow on 32-bit systems.

### 7.3 `config.m4` Version Check

```
if test "$gene_php_version" -le "5002000"; then
    AC_MSG_ERROR([You need at least PHP 5.2.0 ...])
```

The minimum version check is PHP 5.2.0, but the code requires PHP 8.0+ (uses `bool` type in globals, `gene_strip_obj` macro, etc.).

---

## 8. Summary of Recommended Fixes (Priority Order)

| # | Severity | Issue | File(s) |
|---|----------|-------|---------|
| 1 | **Critical** | `add_assoc_str_ex` ownership bug | response.c, controller.c, model.c, service.c |
| 2 | **Critical** | `GetOpcodes` out-of-bounds read | execute.c |
| 3 | **Critical** | `parser_templates` NULL stream write | view.c |
| 4 | **High** | Memory leak in `getRouterUri` | application.c |
| 5 | **High** | Global timing variables (thread-unsafe) | mysql.c, mssql.c, pgsql.c, sqlite.c, benchmark.c |
| 6 | **High** | SQL identifier injection | mysql.c, mssql.c, pgsql.c, sqlite.c |
| 7 | **High** | `mysqlSaveHistory` double addref | mysql.c, mssql.c, pgsql.c, sqlite.c |
| 8 | **Medium** | Coroutine-unsafe module globals | gene.h, gene.c |
| 9 | **Medium** | XSS in alert/exception output | response.c, exception.c |
| 10 | **Medium** | `gene_application_instance` crash risk | application.c |
| 11 | **Low** | Redundant `INIT_CLASS_ENTRY` | language.c, session.c, validate.c |
| 12 | **Low** | Dead code in exception base | exception.c |
| 13 | **Low** | Integer overflow in cookie time | session.c |
| 14 | **Low** | Outdated minimum PHP version in config.m4 | config.m4 |
| 15 | **Medium** | XSS in `redirectJs` method | response.c |
| 16 | **Medium** | Uninitialized `url` variable in `alert` method | response.c |
| 17 | **Medium** | `uri` memory leak in `get_path_router_init` | router.c |
| 18 | **Medium** | `op_array` not properly destroyed in `GetOpcodes` | execute.c |
| 19 | **Medium** | Early return leaks `opcodes_array`/`zv` in `GetOpcodes` | execute.c |

---

## 9. Implemented Fixes

All fixes below have been applied directly to the source files.

| # | File(s) | Fix Description |
|---|---------|-----------------|
| 1 | `http/response.c`, `mvc/controller.c`, `mvc/model.c`, `service/service.c` | Added `zend_string_copy(text)` in `add_assoc_str_ex` calls to properly add a reference for the borrowed `zend_string` from `zend_parse_parameters("S")` |
| 2 | `tool/execute.c` | Changed `op_array->cache_size` → `op_array->last` to iterate the correct number of opcodes |
| 3 | `tool/execute.c` | Added `destroy_op_array(op_array)` before `efree(op_array)` for proper cleanup of compiled op_array |
| 4 | `tool/execute.c` | Added cleanup of `zv` and `opcodes_array` on early return when `op_array` is NULL |
| 5 | `mvc/view.c` | Fixed `parser_templates` NULL stream check: now checks `CacheStream` (output) instead of `stream` (input), and guards against NULL dereference on write |
| 6 | `app/application.c` | Added `efree(path)` after `RETVAL_STRING(path)` in `getRouterUri` to fix memory leak |
| 7 | `db/mysql.c`, `db/mssql.c`, `db/pgsql.c`, `db/sqlite.c` | Removed spurious `Z_TRY_ADDREF_P(&z_row)` after `add_next_index_zval` in `SaveHistory` functions (double addref caused memory leak) |
| 8 | `tool/language.c`, `session/session.c`, `http/validate.c` | Removed redundant `INIT_CLASS_ENTRY` calls that preceded `GENE_INIT_CLASS_ENTRY` |
| 9 | `session/session.c` | Changed `int jg` → `zend_long jg` in `gene_cookie` to prevent integer overflow on 32-bit systems |
| 10 | `exception/exception.c` | Removed dead code in `gene_get_exception_base` (undefined `can_handle_soft_dependency_on_SPL` macro, PHP 5 APIs) |
| 11 | `http/response.c` | Added `php_addslashes()` escaping in `alert()` and `redirectJs()` methods to prevent XSS |
| 12 | `http/response.c` | Added `#include "ext/standard/php_string.h"` for `php_addslashes` |
| 13 | `http/response.c` | Initialized `url` to `NULL` in `alert()` method to prevent use of uninitialized pointer |
| 14 | `router/router.c` | Added `efree(uri)` in `get_path_router_init` when language is found (was leaked) |
| 15 | `config.m4` | Updated minimum PHP version check from 5.2.0 to 8.0.0 |

### Swoole / Coroutine Optimization Fixes (v5.2.2)

| # | File(s) | Fix Description |
|---|---------|-----------------|
| 16 | `db/mysql.c`, `db/mssql.c`, `db/pgsql.c`, `db/sqlite.c`, `tool/benchmark.c` | Made timing globals (`struct timeval`, `zend_long`) `static` to prevent linker collision and isolate per-file |
| 17 | `app/application.c` | Fixed `gene_application_instance` crash: when `this_ptr` is NULL, now uses local zval for `object_init_ex` instead of writing directly into the static property zval |
| 18 | `app/application.c` | Added `Gene\Application::clearState()` method to reset per-request GENE_G fields (`method`, `path`, `module`, `controller`, `action`, `lang`, `path_params`, `child_views`, `router_path`), clear `Request::$_attr`, and call `gene_view_clear_vars()`. Preserves Application singleton and DI container (worker-level singletons like Redis/Memcached persist across requests) |
| 19 | `app/application.c` | Fixed `setRuntimeType()` and `setEnvironment()` to return `$this` for method chaining (were returning `TRUE`) and added `ZEND_ACC_STATIC` so they can be called both statically and as instance method |
| 20 | `http/request.c` | Added `Gene\Request::clear()` static method to reset the static `$_attr` property for Swoole per-request cleanup |
| 21 | `demo/public/swoole.php` | Optimized Swoole integration: separated one-time init (`workerStart`) from per-request handling, added `clearState()` calls, added try/catch error handling, used `\Swoole\Http\Server` class name, auto CPU worker count, `SWOOLE_HOOK_ALL` |

### Not Fixed (Requires Design Decision)

These issues require broader design decisions and are documented for future consideration:

- **SQL identifier injection** in DB query builder — table/column names not escaped
- **Persistent cache thread safety** — no locking on `GENE_G(cache)` HashTable modifications
- **True coroutine isolation (runtime_type=3)** — `GENE_G()` is per-process, not per-coroutine; for full coroutine safety, would need Swoole coroutine-local storage or per-coroutine context objects
