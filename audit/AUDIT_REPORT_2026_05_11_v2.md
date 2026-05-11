# Gene Framework Audit Report - 2026-05-11 v2

## Scope

- FPM request hot path performance.
- Swoole coroutine/resident-mode performance.
- Swoole resident-mode memory safety and resource lifecycle.

## Audited Paths

- `Application::run -> gene_ini_router() -> get_router_content_run() -> get_path_router() -> get_router_info()`.
- Router direct dispatch, closure dispatch, and eval fallback error/hook paths.
- `Gene\Pool` and `Gene\Cache\RedisPool` create/close/closeAll/recycleIdle/get/put paths.
- Swoole request cleanup via `Gene\Application::cleanup(true)` in `demo/public/swoole.php`.
- Coroutine request context lifecycle in `src/gene.c`.

## Findings and Changes

### F1 - Router stack key truncation could cause out-of-bounds reads

**Severity:** High

**Files:**

- `src/router/router.c`

**Issue:**

Several router error/hook paths used `snprintf()` into fixed-size stack buffers and then passed the `snprintf()` return value as the key length to `zend_hash_str_find()`/`gene_memory_get_quick()`.

`snprintf()` returns the length that would have been written, not the bytes actually present in the stack buffer. If `errorName`, hook name, or app-root-derived safe prefix was longer than the stack buffer, subsequent hash lookup could read past the end of the stack buffer.

This affected both FPM and Swoole. In Swoole resident mode, malformed or unusually long route/error/hook names could repeatedly hit the same unsafe path in a long-lived worker.

**Fix:**

- Kept the short-key hot path on stack buffers.
- Added heap fallback for long keys in:
  - `get_router_info()` hook-name construction.
  - `get_router_error_run_by_router()` keys: `error:*`, `hsrc:*`, `fcl:*`.
  - `get_router_error_run()` router-event key, `error:*`, `hsrc:*`, `fcl:*`.
- Added explicit cleanup on all direct/closure/eval/early-return paths.
- Replaced closure-cache lookups using raw string pointer/length with `zend_hash_find(..., Z_STR_P(...))` where the lookup key is already a `zend_string`.

**Impact:**

- Normal short keys remain zero-heap and keep the previous fast path.
- Long keys are now safe and deterministic.
- Prevents out-of-bounds reads and potential worker instability.

### F2 - Pool replacement should close existing DB pool before registry overwrite

**Severity:** Medium

**Files:**

- `src/db/pool.c`

**Issue:**

`Gene\Pool::create()` registered a new pool by overwriting the static `instances[$name]` entry. If an existing pool with the same name was present, its destructor-driven cleanup was implicit and happened during/after hash update semantics.

In Swoole resident mode, pool closing may interact with timers and channels. Making the close explicit before replacement reduces lifecycle ambiguity and mirrors the safer Redis pool behavior.

**Fix:**

- Before constructing/registering the replacement pool, `Gene\Pool::create()` now checks the static instances registry for an existing object with the same name.
- If it exists and is not already closed, it calls `close()` explicitly.
- Final registration still re-reads/initializes the static instances property after construction before `zend_hash_update()`.

**Impact:**

- More deterministic resource release during same-name pool replacement.
- Lower risk of stale timers/channels surviving replacement.
- No public API change.

### F3 - RedisPool registry pointer refreshed after close/construct yield windows

**Severity:** Medium

**Files:**

- `src/cache/redis_pool.c`

**Issue:**

`Gene\Cache\RedisPool::create()` closes an existing same-name pool before registering a new one. `close()` may drain a Swoole channel and yield. The new pool constructor may also create/prefill Redis connections and start timers.

Using a previously read `instances` zval/HashTable pointer after such operations is fragile if another coroutine calls `closeAll()` or rebuilds the registry during a yield window.

**Fix:**

- Re-read and, if needed, recreate the static `instances` registry after closing an existing pool.
- Re-read and, if needed, recreate the registry again after constructing the new pool and before final `zend_hash_update()`.
- Guard final registration with an `instances` array check.

**Impact:**

- Reduces stale HashTable pointer risk in Swoole resident mode.
- Preserves the C-layer named-cache fast path after successful registration.
- No public API change.

## Existing Safe Areas Re-checked

- `Gene\Pool::closeAll()` snapshots static instances before phase-2 close/drain.
- `Gene\Cache\RedisPool::closeAll()` snapshots static instances before phase-2 close/drain.
- `pool_recycle_idle()` and `rpool_recycle_idle()` only increment count after successful push and decrement count if push-back fails.
- `Gene\Application::cleanup(true)` is used in the Swoole demo request `finally` block.
- Swoole worker stop closes DB and Redis pools and runs `gc_collect_cycles()`.
- Request context cleanup destroys request-scoped zvals and drains the context pool at shutdown.

## Validation

- `git diff --check` passed.
- Static grep verified the modified router error path no longer uses truncated `snprintf()` stack-buffer lengths for `err_buf`, `hsrc_buf`, `fcl_buf`, or `router_e_buf` hash lookup.
- Confirmed `GENE_ROUTER_ROUTER_EVENT`, `GENE_ROUTER_ROUTER_TREE`, and `GENE_ROUTER_ROUTER_CONF` are string literal macros, so `sizeof(...) - 1` length calculations are valid.

## Files Modified

- `src/router/router.c`
- `src/db/pool.c`
- `src/cache/redis_pool.c`
- `audit/AUDIT_REPORT_2026_05_11_v2.md`

## Recommended Follow-up

- Build and run the PHP extension test suite on a Linux/Swoole environment.
- Stress test Swoole with repeated same-name `Pool::create()` / `RedisPool::create()` while concurrent requests borrow and return connections.
- Add a regression test for long route error names and long app-root/safe prefixes to ensure router error dispatch remains safe.
