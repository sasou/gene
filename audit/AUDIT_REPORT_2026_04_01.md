# Gene PHP Extension — Performance & Memory Audit Report
**Date:** 2026-04-01  
**Scope:** FPM/Swoole hot paths, memory lifecycle (FPM + Swoole)  
**Files reviewed:** gene.c, gene.h, router/router.c, factory/factory.c, factory/load.c, cache/memory.c, cache/memory.h, app/application.c, mvc/controller.c, mvc/view.c, mvc/hook.c, di/di.c, service/service.c, common/common.c

---

## 1. Performance Findings

### P1 (HIGH) — `gene_class_name()` uses `call_user_function` for DI access
**Location:** `common/common.c:789`, called from `__get`/`__set` in controller.c, view.c, hook.c, service.c, model.c, application.c, session.c (18 call sites)  
**Impact:** Every `$this->dependency` access in any Controller/View/Hook/Service/Model triggers a full `call_user_function("get_called_class")`, which:
- Allocates a `zend_string` for `"get_called_class"`
- Goes through the full function dispatch machinery
- Returns a zval string that must be dtor'd

**Fix:** Replace with `zend_get_executed_scope()` which returns the class entry directly from the execution context in O(1) with zero allocations. The class name `ce->name` is an interned string owned by the class entry — no allocation, no free needed.

**Estimated savings:** ~1-3μs per DI access × N dependencies per request.

### P2 (MEDIUM) — `get_path_router()` recursive estrdup per URL segment
**Location:** `router/router.c:260`  
**Impact:** Each recursion level duplicates the remaining path string. For `/api/v1/users/123/posts` (5 segments), this creates 5 heap allocations of decreasing size.  
**Fix:** Use a single `estrdup` at the top level and pass offset/pointer into the copy, or use `php_strtok_r` on a single copy.

### P3 (LOW) — `get_function_content()` wasteful zval allocations
**Location:** `router/router.c:884`  
**Impact:** `ZVAL_STRING(&arg, "strrpos")` allocates a zend_string that's never used as a function name (the literal `"strrpos"` is passed to `zend_call_method_with_2_params` directly). The zval is just freed later.  
**Fix:** Remove the unused `arg` zval and its associated `zval_ptr_dtor`.

### P4 (INFO) — `gene_application_webscan_check()` reads 3 static properties every request
**Location:** `app/application.c`  
**Impact:** Even when webscan is disabled, 3 `zend_read_static_property` calls execute per request. These are fast (hash lookups), so impact is minimal (~0.1μs total).  
**Fix:** Optional: cache the "enabled" flag in a request global to short-circuit. Low priority.

---

## 2. Memory Leak Findings (FPM Mode)

### M1 (CLEAN) — RSHUTDOWN cleanup is thorough
`php_gene_close_request_globals()` properly:
- Destroys `default_ctx` via `gene_request_context_destroy` (frees all char* fields, dtor's all zvals)
- Frees `app_root`, `app_view`, `app_ext`, `auto_load_fun`, `app_key`
- Destroys `resident_ctx` and `co_contexts`

**Verdict:** No leaks detected in the FPM request lifecycle.

### M2 (CLEAN) — MSHUTDOWN cleanup for persistent cache
`gene_hash_destroy()` is called for both `cache` and `cache_easy` in `PHP_MSHUTDOWN_FUNCTION`. The rwlock is also destroyed. Correct.

### M3 (CLEAN) — Router `clear` and `prefix` methods
Previously fixed (see AUDIT_REPORT_2026_03_25). `efree(router_e)` now always called, `efree(val)` after trim in prefix.

---

## 3. Memory Leak Findings (Swoole Mode)

### S1 (MEDIUM) — Coroutine context accumulation without `destroyContext()`
**Location:** `gene.c:274-278`  
**Impact:** Each new coroutine that calls `gene_request_ctx()` gets a new `gene_request_context` allocated and stored in `co_contexts`. If the coroutine completes without an explicit `destroyContext()` call, the context persists in the hash until:
- Another request's RSHUTDOWN cleans up, OR
- The worker process exits

In long-running Swoole workers with many coroutines, this can accumulate significant memory. Each context includes:
- 8 `char*` string fields (method, path, router_path, module, controller, action, child_views, lang, log_file)
- 1 heap-allocated `zval*` (path_params with array)
- 7 zval fields (request_attr, di_regs, view_vars, 4x db_history)

**Mitigation:** The `clearState()` method resets but doesn't remove from hash. The `destroyContext()` method properly removes from hash. Documentation should emphasize calling `destroyContext()` in Swoole coroutine cleanup hooks.

### S2 (LOW) — `GENE_G(app_view)` and `GENE_G(app_ext)` are request-scoped but shared
**Location:** `mvc/view.c:131-134`, `mvc/view.c:208-213`  
**Impact:** These globals are set with `estrndup()` (request-scoped) on first use, then freed in RSHUTDOWN. In Swoole mode, they're set once and shared across coroutines within the same request cycle, which is correct since they're effectively constants. No leak, but worth noting the shared access pattern.

### S3 (CLEAN) — `gene_get_coroutine_id()` cached function pointer
The `swoole_getcid_func` pointer is resolved once and cached in module globals. No leak.

---

## 4. Recommended Fixes (Priority Order)

| # | Severity | Fix | Files | Est. Impact |
|---|----------|-----|-------|-------------|
| F1 | HIGH | Replace `gene_class_name()` with `zend_get_executed_scope()` | common.c/h, controller.c, view.c, hook.c, service.c, model.c, application.c | ~1-3μs × N DI accesses/req |
| F2 | MEDIUM | Document `destroyContext()` requirement for Swoole | README, demo | Prevents memory accumulation |
| F3 | LOW | Optimize `get_path_router()` to single-copy | router.c | ~0.5μs per request |
| F4 | LOW | Remove unused zval in `get_function_content()` | router.c | Negligible |

---

## 5. Architecture Notes

**Strengths:**
- Clean separation of persistent (MINIT/MSHUTDOWN) vs request-scoped (RINIT/RSHUTDOWN) memory
- Proper rwlock protection on persistent cache
- Effective coroutine context isolation in Swoole mode
- Direct C dispatch path avoids `zend_eval_stringl` for string routes (hooks + controllers)
- Stack-based `snprintf` for cache key construction in hot path (already optimized)

**Areas of excellence:**
- The `gene_request_context` lifecycle management is well-structured with init/reset/destroy separation
- The `gene_memory_zval_persistent` / `gene_memory_zval_local` pair correctly handles the persistent↔request memory boundary
- The Hook fast-path via `instanceof_function(ce, gene_hook_ce)` + `gene_factory_load_class` is a good optimization
