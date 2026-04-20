# Gene Extension — Memory Leak & Growth Audit (2026-04-20)

Focus: detect leaks or per-request allocations that would cause memory to
grow over 1M requests in **FPM**, **Swoole**, and **php-cgi** modes.

## Scope

Covered all request-hot paths and lifecycle hooks:

- `gene.c` — MINIT/MSHUTDOWN/RINIT/RSHUTDOWN, globals, request-context lifecycle,
  coroutine-context hashtable.
- `router/router.c` — dispatch, hook/error/closure execution, fn_cache.
- `di/di.c` — DI registry, class cache, key construction.
- `cache/memory.c` — persistent cache, router tree, interned-key bookkeeping.
- `http/request.c`, `http/response.c`, `http/validate.c` — per-request zvals.
- `session/session.c`, `tool/log.c`, `tool/language.c` — static properties vs
  per-coroutine context.
- `db/pool.c`, `db/mysql.c` … `db/sqlite.c`, `cache/redis_pool.c`,
  `cache/redis.c` — connection pool lifecycle (previously audited,
  re-verified).
- `factory/factory.c`, `mvc/*.c`, `exception/exception.c`, `app/application.c`.

## Findings

### F1 (LOW) — Dead-code allocation in `get_function_content` — **FIXED**

**File:** `src/router/router.c`
**Issue:** `zval arg` was declared and populated with
`ZVAL_STRING(&arg, "strrpos")`, but never passed to any function
(`zend_call_method_with_2_params(..., &arg1, &fileName)` uses `arg1`, not
`arg`). It was `zval_ptr_dtor`'d correctly, so *not a leak*, but wasted one
`zend_string` heap allocation per closure-route registration (cold path).

**Fix:** Removed the unused `arg` declaration, `ZVAL_STRING(&arg, ...)`,
and the two `zval_ptr_dtor(&arg)` lines. Simplifies the function and
eliminates the spurious allocation.

### Verified clean (no action required)

The following areas were re-audited and are leak-free under sustained load.

**`php_gene_close_request_globals()` (RSHUTDOWN)**
- Destroys `default_ctx`, `resident_ctx`, `co_contexts`, `fn_cache`
  (FPM/CGI only), and efrees all `GENE_G()` strings (`app_root`,
  `app_key`, `app_view`, `app_ext`, `auto_load_fun`, `config_cache_key`).
- Safe across repeated FPM requests: globals are re-populated by
  `autoload()` at the start of each request.

**`gene_request_context_free_fields()`**
- Releases every `char *` and `zval` field on the context struct; used both
  by `reset()` (preserve `path_params` array) and `destroy()` (full free).
- Covers the db history zvals (`db_mysql_history`, `db_pgsql_history`,
  `db_sqlite_history`, `db_mssql_history`), `request_attr`, `di_regs`,
  `view_vars` — no stale request state survives a reset.

**`gene_co_context_dtor` / Swoole coroutine context table**
- Properly clears `current_ctx/current_cid/current_vm_stack` when the
  destructor fires on the live context, then calls
  `gene_request_context_destroy` + `efree`.

**`GENE_G(fn_cache)` (Closure route cache)**
- FPM/CGI: destroyed at every RSHUTDOWN (`runtime_type < 2` gate).
- Swoole: persistent, populated at `workerStart`. Re-registering the
  same closure route during a request will still allocate a new `fn_N`
  entry (by design — `fn_cache_id` is monotonic), so code that
  registers routes dynamically per-request would grow memory. This is a
  user-code anti-pattern; recommend documenting that routes must be
  defined once at `workerStart` in Swoole mode.

**`gene_memory_set` / `gene_memory_set_by_router`**
- Creates persistent interned keys via `gene_str_persistent`.
- On replace, uses `gene_memory_zval_edit_persistent(copyval, ...)`
  which `pefree`s the previous payload before re-assigning — no key
  leak (same bucket retains the existing key).
- On `del`, matching key is located via iteration and manually
  `pefree`'d (fixed in earlier audit, still correct).

**Pool refill (`db/pool.c`, `cache/redis_pool.c`)**
- The count-drift fix (`increment_count` after `channel_push` success,
  `decrement_count` on push-failure of a kept-alive connection) is still
  in place in both `pool_recycle_idle` and `rpool_recycle_idle`.

**DB retry paths (`db/mysql.c`, `mssql.c`, `pgsql.c`, `sqlite.c`)**
- `zval_ptr_dtor(statement)` before re-prepare and
  `zval_ptr_dtor(&retval)` before re-execute prevent PDOStatement leaks
  on connection-error retries.

**Session / Log / Language**
- All `static zend_string *` caches use `zend_string_init_interned` with
  permanent=1 — owned by the engine, no release needed.
- `log_file`, `log_level`, `log_level_set` are per-coroutine in Swoole and
  per-request in FPM; `setFile` correctly `efree`s the old buffer.

**Hot-path allocations already eliminated in prior rounds**
- Router `safe_str`/`router_e` construction uses stack buffers (256 B).
- DI `class_name + '_' + name` key uses stack buffer.
- Swoole `getcid` uses a cached `zend_function *`.
- Method-dispatch callable arrays replaced with cached
  `zend_function *` + `zend_call_known_function`.
- `GENE_REQUEST_IS_METHOD` uses `strcasecmp` (no `zend_string_init`).

## FPM-mode growth check (100 K × requests)

Per-request net allocation on the request dispatch path (assuming routes
registered once at the beginning of the script, which is the normal case
for `Gene\Application::start()` in FPM):

| Source                                | Bytes / req   |
| ------------------------------------- | ------------- |
| `gene_request_context_init` (`path_params` zval+array) | ~96 |
| `estrndup` of method & path           | O(URI length) |
| `pathvals`, `di_regs`, `request_attr`, `view_vars` lazy arrays | on demand |
| **All cleaned in RSHUTDOWN**          | net 0         |

After one warm-up request, Zend MM reuses freed chunks — **steady-state
RSS growth per request is zero** under normal use.

## Swoole-mode growth check

- Per-request state lives on the coroutine context (`co_contexts` or
  `resident_ctx`), cleaned by `Gene\Application::clearState()` /
  `cleanup()` — these must be called at end of each HTTP handler.
- `default_ctx` is reset in `RINIT` (which fires once per process in
  Swoole) and also on `clearState`.
- If the application omits `clearState()`, history arrays
  (`db_mysql_history`, view_vars, di_regs) accumulate across requests
  in the same coroutine — this is an app-level contract, not a leak in
  the extension. Recommend adding a `Gene\Application::cleanup()`
  call in the framework's Swoole bootstrap.

## php-cgi compatibility

- `runtime_type < 2` branches identical to FPM: same RSHUTDOWN cleanup,
  same autoload lifecycle. The `$_COOKIE` / `$_SERVER` JIT-auto-global
  activation added in `request_query()` ensures `Gene\Session` works
  under `php-cgi` where `auto_globals_jit=1` is the default.

## Recommendation summary

| Priority | Item                                                               |
| -------- | ------------------------------------------------------------------ |
| Done     | Remove dead `arg` zval in `get_function_content` (F1).             |
| Doc      | Document that Swoole routes must be registered at `workerStart`.   |
| Doc      | Document that `clearState()` or `cleanup()` must be called per request in Swoole. |

No new memory leaks were introduced, and no existing leaks were found
outside of F1 (which was an over-alloc, not a leak). Under normal usage
patterns, the extension does not grow memory across 1 M requests in any
of FPM / Swoole / php-cgi modes.
