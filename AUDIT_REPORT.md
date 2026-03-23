# Gene PHP Extension — Comprehensive Source Code Audit Report

**Version**: 5.3.6  
**Date**: 2025  
**Scope**: All `.c` and `.h` files under `src/`

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Previously Fixed Issues (Verified)](#2-previously-fixed-issues-verified)
3. [New Findings — Bugs & Potential Crashes](#3-new-findings--bugs--potential-crashes)
4. [New Findings — Memory Management](#4-new-findings--memory-management)
5. [New Findings — Security Concerns](#5-new-findings--security-concerns)
6. [New Findings — Code Quality & Maintainability](#6-new-findings--code-quality--maintainability)
7. [New Findings — Coroutine / Swoole Safety](#7-new-findings--coroutine--swoole-safety)
8. [Module-by-Module Summary](#8-module-by-module-summary)
9. [Recommendations](#9-recommendations)

---

## 1. Architecture Overview

The Gene extension is a full-featured PHP MVC framework implemented as a C extension. Key components:

- **Core** (`gene.h`, `gene.c`): Module globals, request context lifecycle, coroutine context management via `gene_request_context` struct and `GENE_REQ()` macro.
- **Application** (`app/application.c`): Bootstrap, routing dispatch, two-phase cleanup (`clearState` + `destroyContext`).
- **DI Container** (`di/di.c`): Service registration/retrieval with instance caching in per-request `di_regs` hash.
- **Router** (`router/router.c`): URL routing with hook system, reflection-based source caching, `zend_eval_stringl` execution.
- **Database** (`db/mysql.c`, `db/mssql.c`, `db/pgsql.c`, `db/sqlite.c`, `db/pdo.c`, `db/pool.c`): PDO-wrapper query builders for MySQL, MSSQL, PostgreSQL, SQLite with connection pooling.
- **Cache** (`cache/cache.c`, `cache/memory.c`, `cache/redis.c`, `cache/memcached.c`): In-memory (HashTable), Redis, Memcached cache drivers with RWLock protection.
- **HTTP** (`http/request.c`, `http/response.c`, `http/validate.c`, `http/webscan.c`): Request/response wrappers, validation, web security scanner.
- **MVC** (`mvc/controller.c`, `mvc/model.c`, `mvc/view.c`): Controller/Model base classes, view rendering with PHP include.
- **Session** (`session/session.c`): Custom session management with DI-driven storage drivers, cookie handling, path-based get/set.
- **Tools** (`tool/benchmark.c`, `tool/execute.c`, `tool/language.c`, `tool/log.c`): Benchmarking, dynamic code execution, i18n language files, structured logging.
- **Other** (`factory/factory.c`, `factory/load.c`, `config/configs.c`, `common/common.c`, `exception/exception.c`, `service/service.c`): Factory pattern, autoloading, INI config parsing, utility functions, exception handling, service layer.

### Request Context Model

```
gene_request_context {
    method, path, router_path, module, controller, action, child_views, lang  (char*)
    path_params       (zval* → array)
    request_attr      (zval → array)
    di_regs           (zval → array)
    view_vars         (zval → array)
    db_*_history      (zval → array)
    bench_start/end   (struct timeval)
    bench_memory_*    (zend_long)
}
```

Contexts are managed per-mode:
- **FPM/CLI**: `default_ctx` (module globals)
- **Swoole resident**: `resident_ctx` (heap-allocated, persistent across requests)
- **Swoole coroutine**: `co_contexts` HashTable keyed by coroutine ID

---

## 2. Previously Fixed Issues (Verified)

These fixes were already applied and verified in the codebase:

| # | File | Issue | Fix |
|---|------|-------|-----|
| 5 | `router/router.c` | `get_function_content` missing error checks → SIGSEGV | Added exception/type checks after Reflection calls |
| 6 | `router/router.c` | `get_router_content_F` NULL src → undefined behavior | Added NULL check |
| 7 | `router/router.c` | `get_router_info` NULL cacheHook dereference → SIGSEGV | Added NULL/type checks before `Z_ARRVAL_P` |
| 8 | `router/router.c` | `zend_eval_stringl` unprotected → worker crash | Wrapped in `zend_try`/`zend_catch` |
| 11 | `app/application.c` | `clearState` destroyed context too early (coroutine) | Changed to `gene_request_context_reset()` |
| 12 | `app/application.c` | `destroyContext` stale pointer window | Invalidate `current_ctx` BEFORE hash delete |
| 13 | `gene.c` | `RSHUTDOWN` stale pointer | Move NULL assignment before destroy |
| 14 | `db/mysql.c` | `ATTR_PERSISTENT` causes socket conflict in coroutines | Removed persistent flag for `runtime_type >= 2` |
| 15 | `di/di.c` | Refcount bug — double hash entry, single refcount → UAF | Added `Z_TRY_ADDREF` before first `zend_hash_update` |

---

## 3. New Findings — Bugs & Potential Crashes

### Finding 3.1: `spprintf` format mismatch in `in()` method (all DB drivers)

**Files**: `db/mssql.c:657`, `db/pgsql.c:657`, `db/sqlite.c:656`  
**Severity**: Medium (potential truncation / undefined behavior)

```c
spprintf(&in_tmp, 0, "%s", in, in_len);
```

`spprintf` with `%s` format ignores the extra `in_len` argument. This is harmless when `in` is NUL-terminated (which it is, from `zend_parse_parameters "s"`), but the code appears to intend length-bounded copy. The extra argument is dead code.

**Recommendation**: Remove the unused `in_len` argument from `spprintf`, or use `spprintf(&in_tmp, 0, "%.*s", (int)in_len, in)` if length-bounding is intended.

---

### Finding 3.2: `getBenchMemory` integer division truncation

**File**: `tool/benchmark.c:114-123`  
**Severity**: Low (incorrect results)

```c
void getBenchMemory(zend_long *memory_start, zend_long *memory_end, char **ret, bool type) {
    zend_long memory;
    memory = *memory_end - *memory_start;
    if (type) {
        spprintf(ret, 0, "%.3f", memory/1024);
    } else {
        spprintf(ret, 0, "%.3f", memory/1048576);
    }
}
```

`memory` is `zend_long` (integer). `memory/1024` performs **integer division**, then the integer result is implicitly converted to `double` for `%f`. This loses fractional precision. For example, 1500 bytes → `1500/1024 = 1` (integer) → prints `1.000` instead of `1.465`.

**Fix**: Cast to double before division: `(double)memory / 1024.0`.

---

### Finding 3.3: `sqliteInitPdo` requires username/password but SQLite doesn't use them

**File**: `db/sqlite.c:187-191`  
**Severity**: Low (usability issue)

```c
if ((user = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("username"))) == NULL) {
     php_error_docref(NULL, E_ERROR, "PDO need a valid username.");
}
if ((pass = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("password"))) == NULL) {
     php_error_docref(NULL, E_ERROR, "PDO need a valid password.");
}
```

SQLite databases don't require username/password. This forces users to provide empty/dummy credentials. Should be made optional for the SQLite driver.

---

### Finding 3.4: `pgsqlInitPdo` missing `ATTR_EMULATE_PREPARES` option

**File**: `db/pgsql.c:199-201`  
**Severity**: Low (inconsistency)

```c
add_index_long(&option, 3, 2);   // ATTR_ERRMODE = EXCEPTION
add_index_long(&option, 19, 2);  // ATTR_DEFAULT_FETCH_MODE = ASSOC
```

MySQL driver (`mysql.c`) sets 4 PDO attributes (ERRMODE, DEFAULT_FETCH_MODE, EMULATE_PREPARES, STRINGIFY_FETCHES). PostgreSQL and SQLite only set 2. MSSQL sets 4. This inconsistency could cause unexpected behavior differences across drivers.

---

### Finding 3.5: Typo in error messages — "dns" instead of "dsn"

**Files**: `db/mssql.c:173`, `db/pgsql.c:185`, `db/sqlite.c:185`  
**Severity**: Trivial

```c
php_error_docref(NULL, E_ERROR, "PDO need a valid dns.");
```

Should be "dsn" (Data Source Name), not "dns" (Domain Name System).

---

### Finding 3.6: Unused variables in multiple DB drivers

**Files**: All DB driver `select()`, `insert()`, `update()` methods  
**Severity**: Trivial (compiler warnings)

Variables like `*select` in `insert()` and `update()` methods are declared but never used. For example in `mssql.c:410`: `char *table = NULL, *select = NULL, *sql = NULL;` — `select` is unused in `insert`.

---

## 4. New Findings — Memory Management

### Finding 4.1: `gene_execite_opcodes_run` missing `zval_ptr_dtor` for `ret`

**File**: `tool/execute.c:47-58`  
**Severity**: Medium (memory leak)

```c
void *gene_execite_opcodes_run(zend_op_array *op_array) {
    zend_op_array *orig_op_array = CG(active_op_array);
    zval ret;
    CG(active_op_array) = op_array;
    if (CG(active_op_array)) {
        zend_execute(CG(active_op_array), &ret);
        destroy_op_array(CG(active_op_array));
        efree(CG(active_op_array));
    }
    CG(active_op_array) = orig_op_array;
    return NULL;
}
```

`zend_execute` writes a return value into `ret`, but `ret` is never cleaned up with `zval_ptr_dtor(&ret)`. If the executed code returns a complex value (string, array, object), this leaks.

**Fix**: Add `zval_ptr_dtor(&ret);` after `efree(CG(active_op_array));`.

---

### Finding 4.2: `GetOpcodes` leaks `opcodes_array` on success path

**File**: `tool/execute.c:110`  
**Severity**: Low (minor leak)

```c
RETURN_ZVAL(&opcodes_array, 1, 0);
```

`RETURN_ZVAL` with `dtor=0` means `opcodes_array` is not destroyed after copy. Since `RETURN_ZVAL` copies the value, the original `opcodes_array` zval's array should be released. Should be `RETURN_ZVAL(&opcodes_array, 1, 1)` to destroy after copy.

---

### Finding 4.3: `SaveHistory` functions potential NULL `param` dereference

**Files**: `db/mssql.c:127`, `db/pgsql.c:127`, `db/sqlite.c:127`, and equivalent in `mysql.c`  
**Severity**: Low

```c
jsonEncode(&z_data, param);
```

`param` comes from `zend_read_property(...GENE_DB_*_DATA...)`. If the DATA property is NULL (e.g., for a raw SQL query with no params), `jsonEncode` receives a NULL-typed zval. The behavior depends on `jsonEncode` implementation. Worth adding a NULL/type check.

---

### Finding 4.4: `smart_str_appends(&field_str, "")` is a no-op allocation

**Files**: All DB drivers' `insert()`, `batchInsert()` methods  
**Severity**: Trivial

```c
smart_str_appends(&field_str, "");
smart_str_appends(&value_str, "");
```

These allocate a `smart_str` buffer just to write an empty string. This is done to ensure `field_str.s` is non-NULL before `smart_str_0()`, but `smart_str_0()` already handles NULL. These lines can be removed.

---

## 5. New Findings — Security Concerns

### Finding 5.1: `Gene_Execute::StringRun` executes arbitrary PHP code

**File**: `tool/execute.c:117-130`  
**Severity**: High (security design concern)

```c
PHP_METHOD(gene_execute, StringRun) {
    zend_string *php_script;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &php_script) == FAILURE) {
        return;
    }
    zend_try {
        zend_eval_stringl(ZSTR_VAL(php_script), ZSTR_LEN(php_script), NULL, "");
    } zend_catch {
        zend_bailout();
    } zend_end_try();
    RETURN_TRUE;
}
```

This provides a direct PHP-level `eval()` equivalent. While `eval()` itself exists in PHP, exposing it as a class method could bypass `disable_functions` restrictions. **No access control or validation** is performed.

**Recommendation**: Consider removing this class or adding a runtime check against `GENE_G(run_environment)` to restrict to development mode only. At minimum, document the security implications prominently.

---

### Finding 5.2: `group()`, `having()`, `order()` methods accept raw SQL strings

**Files**: All DB drivers (`mssql.c`, `pgsql.c`, `sqlite.c`, `mysql.c`)  
**Severity**: Medium (SQL injection if user input reaches these methods)

```c
PHP_METHOD(gene_db_mssql, group) {
    // ...
    spprintf(&group_tmp, 0, " GROUP BY %s", group);
```

The `group()`, `having()` methods directly interpolate user-provided strings into SQL without parameterization. While `where()` and `in()` use parameterized queries (`?` placeholders), these clause builders do not. If user input flows into `->group($userInput)`, it's a SQL injection vector.

**Recommendation**: Document clearly that these methods must NOT receive unsanitized user input, or add identifier quoting.

---

### Finding 5.3: `webscan` patterns are compile-time static

**File**: `http/webscan.c`  
**Severity**: Low (limited flexibility)

The XSS/SQLi detection patterns are hardcoded in C. They cannot be updated without recompiling the extension. New attack vectors require a code change and recompilation.

**Recommendation**: Consider allowing pattern configuration via INI or a config file for production flexibility.

---

## 6. New Findings — Code Quality & Maintainability

### Finding 6.1: Massive code duplication across DB drivers

**Files**: `db/mssql.c` (1200 lines), `db/pgsql.c` (1201 lines), `db/sqlite.c` (1200 lines), `db/mysql.c`  
**Severity**: High (maintainability)

The four DB driver files are **95%+ identical**. The only differences are:
- Quote characters: MySQL uses `` ` ``, MSSQL uses `[`/`]`, PostgreSQL uses `"`, SQLite uses `` ` ``
- MSSQL `limit()` uses `OFFSET...FETCH NEXT` syntax vs `LIMIT...OFFSET`
- Class entry names and property macro names
- MSSQL has `add_index_long(&option, 11, 2)` and `add_index_long(&option, 8, 2)` extra PDO options

Each bug fix must be applied to all four files identically. This has already caused inconsistencies (Finding 3.4).

**Recommendation**: Extract a common base implementation with driver-specific callbacks/config for quote characters and dialect differences. A single `gene_db_base.c` with a driver struct would reduce ~4800 lines of near-duplicate code to ~1500.

---

### Finding 6.2: Function name typo: `gene_execite_opcodes_run`

**File**: `tool/execute.c:47`, `tool/execute.h:23`  
**Severity**: Trivial

"execite" should be "execute" → `gene_execute_opcodes_run`. This is a public API name.

---

### Finding 6.3: `Gene_Log` uses static properties — not coroutine-safe

**File**: `tool/log.c:107,121`  
**Severity**: Medium

```c
prop_level = zend_read_static_property(gene_log_ce, ZEND_STRL("level"), 1);
prop_file = zend_read_static_property(gene_log_ce, ZEND_STRL("file"), 1);
```

`Gene\Log::$file` and `Gene\Log::$level` are static class properties. In Swoole coroutine mode, if one coroutine calls `Log::setFile()`, it affects ALL coroutines. This is a shared mutable state issue.

**Recommendation**: Either document this as intentional global config (acceptable for log config), or move to request context if per-request log routing is needed.

---

### Finding 6.4: `Gene_Language` config cache is per-object, not per-request

**File**: `tool/language.c:170-179`  
**Severity**: Low

The language file cache (`config` property) is stored as an instance property. This means language files are re-loaded for each new `Gene\Language` object instance. If the framework creates a new Language object per request (common in DI), the caching is ineffective.

**Recommendation**: Consider storing the cache in request context or a static property for effective reuse within a request.

---

## 7. New Findings — Coroutine / Swoole Safety

### Finding 7.1: DB history stored in request context — grows unbounded

**File**: `gene.h:100-103`, all DB drivers  
**Severity**: Low (memory pressure in long-running processes)

```c
zval db_mysql_history;
zval db_pgsql_history;
zval db_sqlite_history;
zval db_mssql_history;
```

Each query execution appends to the history array (in non-production mode). In a Swoole server processing many requests before `clearState`, this array grows indefinitely.

**Recommendation**: Add a configurable history limit, or clear history at request boundaries even in non-production mode.

---

### Finding 7.2: History cleared only in constructor, not across requests

**Files**: All DB driver `__construct` methods (e.g., `mssql.c:300-303`)  
**Severity**: Low

```c
if (Z_TYPE(GENE_REQ(db_mssql_history)) != IS_UNDEF) {
    zval_ptr_dtor(&GENE_REQ(db_mssql_history));
}
ZVAL_UNDEF(&GENE_REQ(db_mssql_history));
```

History is only reset when a new DB instance is constructed. If a Swoole worker reuses a pooled connection (no new constructor call), history accumulates from the previous request.

---

### Finding 7.3: Benchmark state correctly moved to request context ✓

**File**: `tool/benchmark.c:38-39`

The comment confirms benchmark variables were moved from file scope to `gene_request_context`. This is correct for coroutine safety. The DB driver `SaveHistory` functions also correctly use local variables for timing.

---

## 8. Module-by-Module Summary

| Module | Files | Lines | Status | Key Issues |
|--------|-------|-------|--------|------------|
| **Core** | `gene.h`, `gene.c` | ~300 | ✅ Good | Coroutine context model is sound |
| **Application** | `app/application.c` | ~800 | ✅ Good | Two-phase cleanup properly implemented |
| **DI** | `di/di.c` | ~400 | ✅ Fixed | Refcount bug fixed (Fix 15) |
| **Router** | `router/router.c` | ~1200 | ✅ Fixed | Multiple crash fixes applied (Fix 5-8) |
| **DB MySQL** | `db/mysql.c` | ~1200 | ✅ Fixed | Persistent connection fix applied (Fix 14) |
| **DB MSSQL** | `db/mssql.c` | 1200 | ⚠️ | spprintf format mismatch (3.1), code duplication (6.1) |
| **DB PgSQL** | `db/pgsql.c` | 1201 | ⚠️ | Same issues as MSSQL, missing PDO options (3.4) |
| **DB SQLite** | `db/sqlite.c` | 1200 | ⚠️ | Unnecessary username/password requirement (3.3) |
| **DB PDO** | `db/pdo.c` | ~630 | ✅ Good | Clean PDO wrapper implementation |
| **DB Pool** | `db/pool.c`, `pool.h` | ~200 | ✅ Good | Pool get/return/notify pattern |
| **Cache** | `cache/*.c` | ~600 | ✅ Good | RWLock-protected cache operations |
| **HTTP** | `http/*.c` | ~1200 | ✅ Good | Request/Response/Validate/Webscan |
| **MVC** | `mvc/*.c` | ~800 | ✅ Good | Controller/Model/View |
| **Session** | `session/session.c` | ~750 | ✅ Good | DI-driven session with cookie handling |
| **Benchmark** | `tool/benchmark.c` | 226 | ⚠️ | Integer division bug (3.2) |
| **Execute** | `tool/execute.c` | 169 | ⚠️ | Memory leak (4.1), security concern (5.1) |
| **Language** | `tool/language.c` | 283 | ✅ Good | Clean i18n implementation |
| **Log** | `tool/log.c` | 407 | ⚠️ | Static properties not coroutine-isolated (6.3) |
| **Config** | `config/configs.c` | ~400 | ✅ Good | INI/PHP config parsing |
| **Common** | `common/common.c` | ~500 | ✅ Good | Utility functions |
| **Factory** | `factory/factory.c`, `load.c` | ~400 | ✅ Good | Autoloading, factory pattern |
| **Exception** | `exception/exception.c` | ~200 | ✅ Good | Exception class registration |
| **Service** | `service/service.c` | ~300 | ✅ Good | Service layer |
| **Webscan** | `http/webscan.c` | ~320 | ✅ Good | Security filter (static patterns) |

---

## 9. Recommendations

### Priority 1 — Bug Fixes (should fix)

1. **Fix `getBenchMemory` integer division** (`benchmark.c:118-121`): Cast to `(double)` before division.
2. **Fix `gene_execite_opcodes_run` memory leak** (`execute.c`): Add `zval_ptr_dtor(&ret)`.
3. **Fix `GetOpcodes` return leak** (`execute.c:110`): Change to `RETURN_ZVAL(&opcodes_array, 1, 1)`.
4. **Fix "dns" typo** in error messages across MSSQL/PgSQL/SQLite drivers.

### Priority 2 — Security (should review)

5. **Restrict `Gene_Execute::StringRun`** to development mode or remove entirely.
6. **Document SQL injection risk** in `group()`/`having()`/`order()` methods.

### Priority 3 — Usability

7. **Make SQLite username/password optional** in `sqliteInitPdo`.
8. **Align PDO options** across all four DB drivers for consistency.

### Priority 4 — Maintainability (long-term)

9. **Refactor DB drivers** to eliminate ~3600 lines of duplicated code.
10. **Rename `gene_execite_opcodes_run`** to `gene_execute_opcodes_run`.

### Priority 5 — Coroutine Safety (low risk, good to fix)

11. **Add history size limit** or per-request clearing for DB query history.
12. **Document `Gene\Log` static property behavior** in coroutine mode.

---

## Summary

The Gene extension is a well-structured PHP framework with a mature coroutine-aware architecture. The previously applied fixes (5-8, 11-15) addressed the most critical crash and memory corruption issues. The remaining findings are predominantly:

- **2 genuine bugs** (integer division in benchmark, memory leak in execute)
- **1 security design concern** (unrestricted eval)
- **1 major maintainability issue** (DB driver code duplication)
- **Several minor issues** (typos, unused variables, inconsistencies)

The codebase demonstrates careful attention to PHP internal APIs and coroutine safety, with proper use of `zval` lifecycle management, `zend_hash` operations, and request context isolation.
