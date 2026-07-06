/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "ext/standard/info.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "gene.h"
#include "app/application.h"
#include "factory/load.h"
#include "config/configs.h"
#include "router/router.h"
#include "tool/execute.h"
#include "tool/language.h"
#include "cache/memory.h"
#include "di/di.h"
#include "mvc/controller.h"
#include "http/request.h"
#include "http/webscan.h"
#include "http/response.h"
#include "http/validate.h"
#include "session/session.h"
#include "mvc/view.h"
#include "exception/exception.h"
#include "db/pdo.h"
#include "db/pool.h"
#include "db/mysql.h"
#include "db/mssql.h"
#include "db/pgsql.h"
#include "db/sqlite.h"
#include "common/common.h"
#include "mvc/model.h"
#include "mvc/hook.h"
#include "service/service.h"
#include "factory/factory.h"
#include "tool/benchmark.h"
#include "tool/log.h"
#include "cache/memcached.h"
#include "cache/redis_pool.h"
#include "cache/redis.h"
#include "cache/cache.h"

#ifndef PHP_WIN32
#include <dlfcn.h>
#endif

ZEND_DECLARE_MODULE_GLOBALS(gene);

/* [GENE_PERF:2026-06-19 P1] Direct C-API resolution of the current Swoole
 * coroutine id. gene_get_coroutine_id() previously always crossed into PHP
 * via zend_call_known_function() (~hundreds of ns). The vm_stack identity
 * fast path in gene_request_ctx() covers non-yielding call chains, but the
 * first GENE_REQ() access after every coroutine switch (i.e. after every IO)
 * still pays that cost. Swoole exports the static C++ method
 *   swoole::Coroutine::get_current_cid()
 * which simply reads a thread-local pointer and returns its cid (or -1 outside
 * a coroutine) — a few ns. We resolve it once per worker via dlsym against the
 * already-loaded swoole.so using the Itanium-ABI mangled symbol name (stable
 * across compilers/versions). If the symbol is unavailable (Swoole not loaded,
 * Windows, custom build with hidden visibility, or an incompatible fork) we
 * transparently fall back to the cached zend_function PHP path — so the
 * optimization is strictly best-effort and never a correctness hazard.
 *
 * The resolver state is process-global (the dlsym result is identical for all
 * threads); the one-time init is idempotent so the benign ZTS race of two
 * threads resolving concurrently writes the same pointer. */
typedef long (*gene_getcid_capi_t)(void);
static gene_getcid_capi_t gene_swoole_getcid_capi = NULL;
static volatile int gene_swoole_getcid_capi_resolved = 0;

/* [GENE_PERF:2026-07-03 T1#2] Direct C-API resolution of Swoole's
 * Coroutine::get_by_cid(long cid) for the co_contexts sweep. The sweep
 * previously called Swoole\Coroutine::exists($cid) via
 * zend_call_known_function for every entry in the co_contexts table —
 * N PHP method calls per sweep. get_by_cid returns a Coroutine* (nullptr
 * if the cid is dead/unknown), letting the sweep check liveness in a few
 * ns per entry with zero PHP crossings. Reuses the same dlsym(RTLD_DEFAULT)
 * pattern as the P1 get_current_cid resolver. Best-effort: falls back to
 * the PHP exists() path if the symbol is unavailable. */
typedef void *(*gene_co_get_by_cid_capi_t)(long);
static gene_co_get_by_cid_capi_t gene_swoole_co_get_by_cid_capi = NULL;
static volatile int gene_swoole_co_get_by_cid_resolved = 0;

static void gene_resolve_getcid_capi(void) {
	if (gene_swoole_getcid_capi_resolved) {
		return;
	}
	/* [GENE_AUDIT:2026-07-05 P3-2] Honor the gene.swoole_getcid_capi kill-
	 * switch. When disabled (0), mark both symbols resolved but skip dlsym
	 * so the fast path in gene_get_coroutine_id() (line ~152) never
	 * activates and the sweep's gene_swoole_co_exists() falls back to the
	 * PHP exists() path. Previously the sweep path called this resolver
	 * without checking the switch, populating gene_swoole_getcid_capi and
	 * making the kill-switch unreliable — exactly in the Swoole-
	 * incompatible environments where the escape hatch is needed. */
	if (!GENE_G(swoole_getcid_capi)) {
		gene_swoole_getcid_capi_resolved = 1;
		gene_swoole_co_get_by_cid_resolved = 1;
		return;
	}
#ifndef PHP_WIN32
	/* Search every loaded object (RTLD_DEFAULT) — swoole.so is fully loaded
	 * by the time any request executes, so the symbol is visible here even
	 * though it may not have been at gene's MINIT. */
	gene_swoole_getcid_capi = (gene_getcid_capi_t)dlsym(
		RTLD_DEFAULT, "_ZN6swoole9Coroutine15get_current_cidEv");
	/* [GENE_PERF:2026-07-03 T1#2] Also resolve get_by_cid for sweep liveness
	 * checks. Mangled name: swoole::Coroutine::get_by_cid(long) →
	 * _ZN6swoole9Coroutine11get_by_cidEl */
	gene_swoole_co_get_by_cid_capi = (gene_co_get_by_cid_capi_t)dlsym(
		RTLD_DEFAULT, "_ZN6swoole9Coroutine11get_by_cidEl");
#endif
	gene_swoole_getcid_capi_resolved = 1;
	gene_swoole_co_get_by_cid_resolved = 1;
}


/** {{{ PHP_INI
 */
PHP_INI_BEGIN()
STD_PHP_INI_ENTRY("gene.run_environment", "1", PHP_INI_SYSTEM, OnUpdateLong, run_environment, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.runtime_type", "1", PHP_INI_SYSTEM, OnUpdateLong, runtime_type, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.use_namespace", "1", PHP_INI_SYSTEM, OnUpdateBool, use_namespace, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.view_compile", "0", PHP_INI_SYSTEM, OnUpdateBool, view_compile, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.view_compile_check_mtime", "0", PHP_INI_SYSTEM, OnUpdateBool, view_compile_check_mtime, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.use_library", "0", PHP_INI_SYSTEM, OnUpdateBool, use_library, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.library_root", "", PHP_INI_SYSTEM, OnUpdateString, library_root, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.co_contexts_max", "1024", PHP_INI_SYSTEM, OnUpdateLong, co_contexts_max, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.ctx_pool_max", "256", PHP_INI_SYSTEM, OnUpdateLong, ctx_pool_max, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.ctx_pool_prewarm", "0", PHP_INI_SYSTEM, OnUpdateLong, ctx_pool_prewarm, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.swoole_getcid_capi", "1", PHP_INI_SYSTEM, OnUpdateBool, swoole_getcid_capi, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.cache_max_items", "0", PHP_INI_SYSTEM, OnUpdateLong, cache_max_items, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.route_precompile", "0", PHP_INI_SYSTEM, OnUpdateBool, route_precompile, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
PHP_INI_END();
/* }}} */

/* {{{ gene_get_coroutine_id
 * Get coroutine ID using cached zend_function pointer to avoid
 * allocating a callable array on every call (hot path in Swoole mode).
 */
zend_long gene_get_coroutine_id(void) {
	zval retval;

	/* [GENE_PERF:2026-06-19 P1] Prefer the resolved C-API direct call. */
	if (EXPECTED(gene_swoole_getcid_capi != NULL)) {
		return (zend_long)gene_swoole_getcid_capi();
	}
	if (UNEXPECTED(!gene_swoole_getcid_capi_resolved) && GENE_G(swoole_getcid_capi)) {
		gene_resolve_getcid_capi();
		if (gene_swoole_getcid_capi != NULL) {
			return (zend_long)gene_swoole_getcid_capi();
		}
	}

	if (!GENE_G(swoole_getcid_resolved)) {
		zend_class_entry *co_ce = gene_lookup_class_str(ZEND_STRL("swoole\\coroutine"));
		if (co_ce) {
			GENE_G(swoole_getcid_func) = zend_hash_str_find_ptr(
				&co_ce->function_table, ZEND_STRL("getcid"));
		}
		GENE_G(swoole_getcid_resolved) = 1;
	}

	if (!GENE_G(swoole_getcid_func)) {
		return -1;
	}

	ZVAL_UNDEF(&retval);
	zend_call_known_function(GENE_G(swoole_getcid_func), NULL, NULL, &retval, 0, NULL, NULL);

	if (Z_TYPE(retval) == IS_LONG) {
		return Z_LVAL(retval);
	}
	if (Z_TYPE(retval) != IS_UNDEF) {
		zval_ptr_dtor(&retval);
	}
	return -1;
}
/* }}} */

/* {{{ gene_interned_str_persistent
 * [GENE_FIX:2026-05-24] See gene.h for full rationale. The slot is only
 * populated when zend_string_init_interned() returned a string carrying
 * IS_STR_PERMANENT — the canonical Zend marker for "lives at process scope".
 * Under opcache.file_cache_only=1 / no-opcache / CLI, the call returns a
 * request-scope string (no IS_STR_PERMANENT); we deliberately leave *slot
 * NULL so that the next request re-resolves through the interned strings
 * table instead of dereferencing a freed pointer.
 *
 * Within a single request, repeated calls hit the fast path on the second
 * invocation onward: the *slot check short-circuits before the
 * zend_string_init_interned hash probe. Across requests, the worst case is
 * one zend_string_init_interned call per site per request — a bucket lookup
 * in CG(interned_strings) plus a single store. Benchmarks: ~25 ns extra per
 * site versus the prior unsafe cache, dwarfed by even one disk syscall. */
zend_string *gene_interned_str_persistent(zend_string **slot, const char *s, size_t l) {
	zend_string *cached = *slot;
	if (EXPECTED(cached != NULL)) {
		return cached;
	}
	zend_string *resolved = zend_string_init_interned(s, l, 1);
	if (resolved && (GC_FLAGS(resolved) & IS_STR_PERMANENT)) {
		*slot = resolved;
	}
	/* When permanent caching is unavailable, *slot stays NULL — the
	 * returned string is still safe to use for this request only. */
	return resolved;
}
/* }}} */

/* {{{ gene_lookup_class_str
 * [GENE_FIX:2026-05-24] Drop-in replacement for the unsafe pattern:
 *   static zend_string *cls = NULL;
 *   if (!cls) cls = zend_string_init_interned("Foo", 3, 1);
 *   zend_lookup_class(cls);
 *
 * Fast path: probe EG(class_table) directly with a stack-allocated
 * lowercase copy. PHP stores class keys lowercased, so this hit covers all
 * already-loaded classes (>99% of calls in steady state).
 *
 * Slow path (autoload required): allocate a request-scope zend_string,
 * call zend_lookup_class, release. No persistent cache → no dangling
 * pointer hazard across requests. */
zend_class_entry *gene_lookup_class_str(const char *name, size_t len) {
	zend_class_entry *ce;
	char lc_buf[256];
	const char *src = name;
	size_t src_len = len;
	size_t i;
	zend_string *tmp;

	if (UNEXPECTED(src_len == 0)) {
		return NULL;
	}
	if (UNEXPECTED(src[0] == '\\')) {
		src++;
		src_len--;
		if (src_len == 0) return NULL;
	}
	if (EXPECTED(src_len < sizeof(lc_buf))) {
		for (i = 0; i < src_len; i++) {
			unsigned char c = (unsigned char)src[i];
			lc_buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c | 0x20) : (char)c;
		}
		ce = zend_hash_str_find_ptr(EG(class_table), lc_buf, src_len);
		if (EXPECTED(ce != NULL)) {
			return ce;
		}
	}
	tmp = zend_string_init(name, len, 0);
	ce = zend_lookup_class(tmp);
	zend_string_release(tmp);
	return ce;
}
/* }}} */

/* {{{ gene_request_context_init
 * [GENE_MEM:2026-04-24] path_params is now an inline zval — only the backing
 * HashTable is heap-allocated via array_init. This removes one emalloc +
 * one efree from every request's allocator traffic (FPM + Swoole).
 * [GENE_PERF:2026-04-24 v5.5.8] path_params pre-sized to 8 (typical routes
 * have 1-3 bound parameters, worst-case dozens for API-heavy apps). The
 * default-initialized HashTable starts at size 0 and grows 0→8→16→... on
 * each insert; pre-sizing to 8 skips the first grow during setMca(). */
void gene_request_context_init(gene_request_context *ctx) {
	if (!ctx) return;
	memset(ctx, 0, sizeof(gene_request_context));
	array_init_size(&ctx->path_params, 8);
	ZVAL_UNDEF(&ctx->request_attr);
	ZVAL_UNDEF(&ctx->di_regs);
	ZVAL_UNDEF(&ctx->response_obj);
	ZVAL_UNDEF(&ctx->view_vars);
	ZVAL_UNDEF(&ctx->db_mysql_history);
	ZVAL_UNDEF(&ctx->db_pgsql_history);
	ZVAL_UNDEF(&ctx->db_sqlite_history);
	ZVAL_UNDEF(&ctx->db_mssql_history);
	ctx->view_scope_no = 0;
	ctx->log_file = NULL;
	ctx->log_level = 0;
	ctx->log_level_set = 0;
}
/* }}} */

/* {{{ gene_request_context_reset_path_params
 * [GENE_MEM:2026-04-24] With path_params inlined, reset is always an in-place
 * HashTable clean — no emalloc branch, no pointer indirection. */
static zend_always_inline void gene_request_context_reset_path_params(gene_request_context *ctx) {
	zval *pp;
	if (UNEXPECTED(!ctx)) return;
	pp = &ctx->path_params;
	if (EXPECTED(Z_TYPE_P(pp) == IS_ARRAY)) {
		/* Fast path: live array → just clear entries. If the backing
		 * HashTable has grown huge (e.g. a prior request stuffed 10k+
		 * params into it), drop it entirely so the next array_init
		 * starts at the minimum bucket footprint. Keeps Swoole worker
		 * RSS bounded across pathological requests. */
		if (UNEXPECTED(Z_ARRVAL_P(pp)->nTableSize > 128)) {
			zval_ptr_dtor(pp);
			array_init_size(pp, 8);
			return;
		}
		zend_hash_clean(Z_ARRVAL_P(pp));
		return;
	}
	if (Z_TYPE_P(pp) != IS_UNDEF) {
		zval_ptr_dtor(pp);
	}
	array_init_size(pp, 8);
}
/* }}} */

/* {{{ gene_ctx_reuse_lazy_array
 * [GENE_MEM:2026-06-19 M5] Recycle a lazily-initialized request-scope array
 * across reset() instead of destroying + re-array_init'ing it next request.
 * Mirrors gene_request_context_reset_path_params()'s "reuse small, drop large"
 * policy: an empty cleaned HashTable is observationally identical to a fresh
 * array_init for consumers that key/iterate it, so the only effect is one
 * fewer alloc/free pair per request on the common path. If a pathological
 * request ballooned the table (>128 buckets) we drop it back to IS_UNDEF so
 * the next lazy init starts at the minimum footprint — keeping Swoole worker
 * RSS bounded exactly as the prior destroy-on-reset did. Unlike path_params,
 * these arrays are re-initialized on demand by their accessor (e.g.
 * gene_request_attr()), so we leave them UNDEF rather than eagerly re-init. */
static zend_always_inline void gene_ctx_reuse_lazy_array(zval *zv) {
	if (EXPECTED(Z_TYPE_P(zv) == IS_ARRAY)) {
		if (UNEXPECTED(Z_ARRVAL_P(zv)->nTableSize > 128)) {
			zval_ptr_dtor(zv);
			ZVAL_UNDEF(zv);
			return;
		}
		zend_hash_clean(Z_ARRVAL_P(zv));
		return;
	}
	if (Z_TYPE_P(zv) != IS_UNDEF) {
		zval_ptr_dtor(zv);
		ZVAL_UNDEF(zv);
	}
}
/* }}} */

/* {{{ gene_request_context_free_fields - shared cleanup for reset/destroy
 * preserve_for_reuse: 1 on reset() (request boundary for a recycled ctx) —
 * path_params and request_attr are recycled in place rather than freed; 0 on
 * destroy() (pool release / worker exit) — everything is fully freed.
 *
 * [GENE_MEM:2026-06-19 M5] Note on the char* fields (method/path/module/...):
 * buffer pooling for these was evaluated and deliberately NOT done. Readers
 * across the codebase test `if (ctx->module)` (pointer non-NULL) as the
 * "is-set" signal, so preserving a non-NULL buffer with stale content past
 * reset would silently leak the previous request's value into handlers that
 * don't re-set it. A stash-based scheme (move buffer aside on reset, reuse on
 * next set) preserves that contract but adds per-field stash+capacity state and
 * touches every assignment site for a ~100-200ns/req gain — not worth the
 * surface area until it can be validated under ASAN. The arrays below are safe
 * to recycle because their accessors gate on IS_ARRAY/IS_UNDEF, not a pointer. */
static void gene_request_context_free_fields(gene_request_context *ctx, int preserve_for_reuse) {
	if (ctx->method) { efree(ctx->method); ctx->method = NULL; }
	ctx->method_len = 0;
	if (ctx->path) { efree(ctx->path); ctx->path = NULL; }
	ctx->path_len = 0;
	if (ctx->router_path) { efree(ctx->router_path); ctx->router_path = NULL; }
	ctx->router_path_len = 0;
	if (ctx->module) { efree(ctx->module); ctx->module = NULL; }
	ctx->module_len = 0;
	if (ctx->controller) { efree(ctx->controller); ctx->controller = NULL; }
	ctx->controller_len = 0;
	if (ctx->action) { efree(ctx->action); ctx->action = NULL; }
	ctx->action_len = 0;
	if (ctx->child_views) { efree(ctx->child_views); ctx->child_views = NULL; }
	ctx->child_views_len = 0;
	if (ctx->lang) { efree(ctx->lang); ctx->lang = NULL; }
	ctx->lang_len = 0;
	if (ctx->log_file) { efree(ctx->log_file); ctx->log_file = NULL; }
	if (!preserve_for_reuse) {
		/* [GENE_MEM:2026-04-24] Inlined path_params: dtor the HashTable
		 * only; the zval container itself lives with the struct. */
		if (Z_TYPE(ctx->path_params) != IS_UNDEF) {
			zval_ptr_dtor(&ctx->path_params);
			ZVAL_UNDEF(&ctx->path_params);
		}
	}
	/* [GENE_MEM:2026-06-19 M5] request_attr is set on virtually every request
	 * (any getVal/setVal of GET/POST/COOKIE/... routes through it), so on reset
	 * we recycle it in place instead of free+re-init — saving one alloc/free
	 * pair per request on the hot path. The "drop if >128 buckets" guard inside
	 * gene_ctx_reuse_lazy_array() preserves the old RSS bound for pathological
	 * requests. On destroy (preserve_for_reuse==0) it is fully freed below.
	 * [GENE_MEM:2026-04-24 v5.5.8] The remaining request-scope arrays are fully
	 * destroyed (not just emptied) so backing storage that ballooned on a single
	 * pathological request (huge DI graph, view with thousands of vars) is
	 * returned to ZMM instead of lingering in the struct pool for the worker
	 * lifetime. They are re-array_init'd on first use next request. */
	if (preserve_for_reuse) {
		gene_ctx_reuse_lazy_array(&ctx->request_attr);
	} else if (Z_TYPE(ctx->request_attr) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->request_attr);
		ZVAL_UNDEF(&ctx->request_attr);
	}
	if (Z_TYPE(ctx->di_regs) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->di_regs);
		ZVAL_UNDEF(&ctx->di_regs);
	}
	if (Z_TYPE(ctx->response_obj) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->response_obj);
		ZVAL_UNDEF(&ctx->response_obj);
	}
	if (Z_TYPE(ctx->view_vars) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->view_vars);
		ZVAL_UNDEF(&ctx->view_vars);
	}
	if (Z_TYPE(ctx->db_mysql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_mysql_history);
		ZVAL_UNDEF(&ctx->db_mysql_history);
	}
	if (Z_TYPE(ctx->db_pgsql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_pgsql_history);
		ZVAL_UNDEF(&ctx->db_pgsql_history);
	}
	if (Z_TYPE(ctx->db_sqlite_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_sqlite_history);
		ZVAL_UNDEF(&ctx->db_sqlite_history);
	}
	if (Z_TYPE(ctx->db_mssql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_mssql_history);
		ZVAL_UNDEF(&ctx->db_mssql_history);
	}
	ctx->log_level = 0;
	ctx->log_level_set = 0;
	ctx->view_scope_no = 0;
	/* [GENE_MEM:2026-04-24 #2] Reset Benchmark fields too. Previously these
	 * 4 inline scalars were left with the prior request's values after
	 * reset, so in Swoole where the same ctx is reused across many
	 * requests / coroutines Gene\Benchmark::time()/memory() could report
	 * bleed from the previous request when benchmark::start() was not
	 * called by the current handler. Scalars → unconditional clears. */
	ctx->bench_start.tv_sec = 0;
	ctx->bench_start.tv_usec = 0;
	ctx->bench_end.tv_sec = 0;
	ctx->bench_end.tv_usec = 0;
	ctx->bench_memory_start = 0;
	ctx->bench_memory_end = 0;
}
/* }}} */

/* {{{ gene_request_context_reset */
void gene_request_context_reset(gene_request_context *ctx) {
	if (!ctx) return;
	gene_request_context_free_fields(ctx, 1);
	gene_request_context_reset_path_params(ctx);
}
/* }}} */

/* {{{ gene_request_context_destroy */
void gene_request_context_destroy(gene_request_context *ctx) {
	if (!ctx) return;
	gene_request_context_free_fields(ctx, 0);
}
/* }}} */

/* {{{ gene_request_context_pool_*
 * [GENE_PERF:2026-04-24] Bounded free-list of cleared gene_request_context
 * structs. Singly-linked via the struct's path_params.value.ptr slot, which
 * is guaranteed UNDEF at release time (free_fields clears it). Only used by
 * the Swoole coroutine hot path; FPM default_ctx is a singleton and never
 * enters the pool.
 *
 * Steady-state cost:
 *   - Acquire: 1 load + 1 branch + 1 store (no syscall, no emalloc).
 *   - Release: 1 load + 1 compare + 2 stores (no efree).
 *
 * Worst-case bound: ctx_pool_max (default 256) * sizeof(gene_request_context)
 * ≈ 256 * ~320B ≈ 80KB per worker — negligible vs. a typical Swoole worker's
 * RSS. Pool overflow falls back to efree so memory never leaks. */
static zend_always_inline void **gene_ctx_pool_next_slot(gene_request_context *ctx) {
	/* Reuse path_params.value.ptr as the free-list link. Safe because a
	 * released context has path_params.u1.v.type == IS_UNDEF (ptr slot is
	 * scratch storage for the dead zval). */
	return (void **)&Z_PTR(ctx->path_params);
}

gene_request_context *gene_request_context_pool_acquire(void) {
	gene_request_context *ctx = (gene_request_context *)GENE_G(ctx_pool_head);
	if (ctx) {
		void **slot = gene_ctx_pool_next_slot(ctx);
		GENE_G(ctx_pool_head) = *slot;
		if (GENE_G(ctx_pool_size) > 0) {
			GENE_G(ctx_pool_size)--;
		}
		/* [GENE_PERF:2026-04-24 v5.5.8] path_params' value.ptr slot was
		 * used as the free-list link while on the pool. array_init_size
		 * below completely overwrites the zval (sets type=IS_ARRAY and
		 * stores the HashTable pointer), so the prior "*slot = NULL"
		 * scrub is redundant — removed to shave one store per acquire.
		 * All remaining fields are guaranteed NULL/UNDEF/zero by the
		 * caller-invoked destroy() that precedes every pool release. */
		array_init_size(&ctx->path_params, 8);
		/* The ZVAL_UNDEF assignments below are identity ops for a
		 * freshly-destroyed ctx — type is already 0 (IS_UNDEF) thanks
		 * to gene_request_context_destroy() setting each to UNDEF.
		 * Under -O2 the compiler folds them into a single cache-line
		 * store pair; under -O0/-O1 they are redundant writes. Keep
		 * them only in debug builds as executable documentation of
		 * the pool invariant. [GENE_PERF:2026-05-04 #17] */
#ifndef NDEBUG
		ZVAL_UNDEF(&ctx->request_attr);
		ZVAL_UNDEF(&ctx->di_regs);
		ZVAL_UNDEF(&ctx->response_obj);
		ZVAL_UNDEF(&ctx->view_vars);
		ZVAL_UNDEF(&ctx->db_mysql_history);
		ZVAL_UNDEF(&ctx->db_pgsql_history);
		ZVAL_UNDEF(&ctx->db_sqlite_history);
		ZVAL_UNDEF(&ctx->db_mssql_history);
#endif
		ctx->view_scope_no = 0;
		ctx->log_level = 0;
		ctx->log_level_set = 0;
		return ctx;
	}
	ctx = ecalloc(1, sizeof(gene_request_context));
	gene_request_context_init(ctx);
	return ctx;
}

void gene_request_context_pool_release(gene_request_context *ctx) {
	zend_long cap;
	void **slot;
	if (!ctx) return;
	cap = GENE_G(ctx_pool_max) > 0 ? GENE_G(ctx_pool_max) : 256;
	if (GENE_G(ctx_pool_size) >= cap) {
		/* Pool full: free outright. All fields already cleared by the
		 * caller via gene_request_context_destroy(). */
		efree(ctx);
		return;
	}
	slot = gene_ctx_pool_next_slot(ctx);
	*slot = GENE_G(ctx_pool_head);
	GENE_G(ctx_pool_head) = ctx;
	GENE_G(ctx_pool_size)++;
}

void gene_request_context_pool_drain(void) {
	void *head = GENE_G(ctx_pool_head);
	while (head) {
		gene_request_context *ctx = (gene_request_context *)head;
		void **slot = gene_ctx_pool_next_slot(ctx);
		head = *slot;
		efree(ctx);
	}
	GENE_G(ctx_pool_head) = NULL;
	GENE_G(ctx_pool_size) = 0;
}

/* {{{ gene_request_context_pool_prewarm
 * [GENE_PERF:2026-04-24 #2] Populate the context pool up to count entries
 * (bounded by ctx_pool_max). Contexts are built in the "already-destroyed"
 * steady state — all zvals UNDEF, all string pointers NULL — so the next
 * acquire skips zero-init and only pays for path_params' array_init.
 *
 * Idempotent: safe to call repeatedly; only tops up the delta.
 * Swoole workflow:
 *   Server::onWorkerStart { \Gene\Application::getInstance()->prewarmCtxPool(); }
 * absorbs the cold-start allocator pressure BEFORE the first request hits.
 *
 * FPM mode is a no-op: default_ctx is a singleton that doesn't use the pool.
 */
zend_long gene_request_context_pool_prewarm(zend_long count) {
	zend_long cap;
	zend_long added = 0;
	gene_request_context *ctx;
	void **slot;

	if (GENE_G(runtime_type) < 2) {
		return 0;
	}
	cap = GENE_G(ctx_pool_max) > 0 ? GENE_G(ctx_pool_max) : 256;
	if (count <= 0 || count > cap) {
		count = cap;
	}
	while (GENE_G(ctx_pool_size) < count) {
		ctx = (gene_request_context *)ecalloc(1, sizeof(gene_request_context));
		/* The struct is zero-memset by ecalloc; all zvals are IS_UNDEF
		 * (type=0), all string pointers NULL, all scalars 0 — exactly
		 * the post-destroy() invariant the acquire path expects. */
		slot = gene_ctx_pool_next_slot(ctx);
		*slot = GENE_G(ctx_pool_head);
		GENE_G(ctx_pool_head) = ctx;
		GENE_G(ctx_pool_size)++;
		added++;
	}
	return added;
}
/* }}} */

/* {{{ gene_co_context_dtor - destructor for co_contexts HashTable entries */
static void gene_co_context_dtor(zval *zv) {
	gene_request_context *ctx = (gene_request_context *)Z_PTR_P(zv);
	if (ctx) {
		if (GENE_G(current_ctx) == ctx) {
			GENE_G(current_ctx) = NULL;
			GENE_G(current_cid) = -1;
			GENE_G(current_vm_stack) = NULL;
		}
		gene_request_context_destroy(ctx);
		/* [GENE_PERF:2026-04-24] Recycle the struct through the pool to
		 * amortize allocator traffic across the next wave of coroutines.
		 * Overflow naturally falls back to efree inside _release(). */
		gene_request_context_pool_release(ctx);
	}
}
/* }}} */

/* {{{ gene_init_co_contexts */
void gene_init_co_contexts(void) {
	if (!GENE_G(co_contexts)) {
		ALLOC_HASHTABLE(GENE_G(co_contexts));
		/* [GENE_PERF:2026-04-24 #2] 8 → 32 to skip rehashes on the first
		 * wave of Swoole coroutines. See RINIT for detailed rationale. */
		zend_hash_init(GENE_G(co_contexts), 32, NULL, gene_co_context_dtor, 0);
	}
}
/* }}} */

/* {{{ gene_swoole_co_exists_resolve — one-shot lazy resolve of exists()
 * Populates GENE_G(swoole_co_exists_func) and/or the dlsym C-API for
 * get_by_cid. Returns non-zero if either liveness-check path is available.
 * [GENE_AUDIT:2026-07-03 T1#2] Also triggers dlsym resolution for the
 * C-API fast path so the sweep can use it even without the PHP exists(). */
static int gene_swoole_co_exists_resolve(void) {
	if (!GENE_G(swoole_co_exists_resolved)) {
		zend_class_entry *co_ce = gene_lookup_class_str(ZEND_STRL("swoole\\coroutine"));
		if (co_ce) {
			GENE_G(swoole_co_exists_func) = zend_hash_str_find_ptr(
				&co_ce->function_table, ZEND_STRL("exists"));
		}
		GENE_G(swoole_co_exists_resolved) = 1;
	}
	/* [GENE_PERF:2026-07-03 T1#2] Also ensure the dlsym C-API is resolved. */
	if (UNEXPECTED(!gene_swoole_co_get_by_cid_resolved)) {
		gene_resolve_getcid_capi();
	}
	return (GENE_G(swoole_co_exists_func) != NULL) || (gene_swoole_co_get_by_cid_capi != NULL);
}
/* }}} */

/* {{{ gene_swoole_co_exists
 * Returns 1 if coroutine cid is still alive, 0 if known-dead, -1 if unknown
 * (Swoole not loaded / no exists() API). Assumes the resolver has been
 * warmed up by gene_swoole_co_exists_resolve() — cheap inner loop.
 */
static int gene_swoole_co_exists(zend_long cid) {
	/* [GENE_PERF:2026-07-03 T1#2] Prefer the dlsym C-API: get_by_cid returns
	 * a Coroutine* (non-null = alive, null = dead/unknown) in a few ns,
	 * avoiding a zend_call_known_function per entry during sweep. */
	if (UNEXPECTED(!gene_swoole_co_get_by_cid_resolved)) {
		gene_resolve_getcid_capi();
	}
	if (gene_swoole_co_get_by_cid_capi) {
		void *co = gene_swoole_co_get_by_cid_capi((long)cid);
		return co ? 1 : 0;
	}

	/* Fallback: PHP method call via cached zend_function. */
	zval cid_zv, retval;
	int res = -1;

	if (!GENE_G(swoole_co_exists_func)) {
		return -1;
	}
	ZVAL_LONG(&cid_zv, cid);
	ZVAL_UNDEF(&retval);
	zend_call_known_function(GENE_G(swoole_co_exists_func), NULL, NULL,
		&retval, 1, &cid_zv, NULL);
	if (Z_TYPE(retval) == IS_TRUE) {
		res = 1;
	} else if (Z_TYPE(retval) == IS_FALSE) {
		res = 0;
	}
	if (Z_TYPE(retval) != IS_UNDEF) {
		zval_ptr_dtor(&retval);
	}
	return res;
}
/* }}} */

/* {{{ gene_co_contexts_sweep
 * Reclaim memory from dead-coroutine entries in GENE_G(co_contexts).
 *
 * Called from the slow path of gene_request_ctx() when the table size is
 * at or above the configured cap (gene.co_contexts_max, default 1024).
 *
 * Two-stage strategy (bounded work per invocation):
 *   1) If Swoole\Coroutine::exists() is available, probe each non-current
 *      cid — drop entries the runtime reports as dead. This is the precise
 *      path and does NOT evict long-running live coroutines.
 *   2) If still over the cap (or Swoole API unavailable), evict by HashTable
 *      insertion order (oldest first, skipping the current cid) until we
 *      drop ~25% below the cap. This is a last-resort backstop only reached
 *      when exists() can't tell us.
 *
 * Never touches GENE_G(current_ctx) / GENE_G(current_cid).
 */
void gene_co_contexts_sweep(void) {
	HashTable *ht = GENE_G(co_contexts);
	zend_ulong idx;
	zend_ulong cur_cid;
	zend_ulong *victims = NULL;
	uint32_t victim_count = 0;
	uint32_t total;
	uint32_t cap;
	uint32_t target_evict;
	uint32_t i;
	int have_exists;

	if (!ht) return;
	total = zend_hash_num_elements(ht);
	if (total < 16) return; /* not worth the walk */

	cap = (uint32_t)(GENE_G(co_contexts_max) > 0 ? GENE_G(co_contexts_max) : 1024);
	cur_cid = (GENE_G(current_cid) >= 0) ? (zend_ulong)GENE_G(current_cid) : ~(zend_ulong)0;

	/* Stage 1: precise eviction via Swoole\Coroutine::exists (if present).
	 * Collect dead cids first, then delete — iterating with concurrent delete
	 * is fragile across PHP versions. */
	have_exists = gene_swoole_co_exists_resolve();
	if (have_exists) {
		victims = (zend_ulong *)emalloc(sizeof(zend_ulong) * total);
		ZEND_HASH_FOREACH_NUM_KEY(ht, idx) {
			if (idx == cur_cid) continue;
			if (gene_swoole_co_exists((zend_long)idx) == 0) {
				victims[victim_count++] = idx;
			}
		} ZEND_HASH_FOREACH_END();
		for (i = 0; i < victim_count; i++) {
			zend_hash_index_del(ht, victims[i]);
		}
		efree(victims);
		victims = NULL;
		victim_count = 0;
	}

	/* Stage 2: if still above cap, evict oldest insertion-order entries.
	 * This only hits when exists() can't decide (unknown / API absent) or
	 * when every cid is reported alive but still blowing the cap — in which
	 * case the framework's bound takes precedence over strict correctness. */
	total = zend_hash_num_elements(ht);
	if (total <= cap) return;
	target_evict = total - (cap * 3 / 4); /* trim back to 75% of cap */
	if (target_evict == 0) return;

	victims = (zend_ulong *)emalloc(sizeof(zend_ulong) * target_evict);
	ZEND_HASH_FOREACH_NUM_KEY(ht, idx) {
		if (idx == cur_cid) continue;
		victims[victim_count++] = idx;
		if (victim_count >= target_evict) break;
	} ZEND_HASH_FOREACH_END();
	for (i = 0; i < victim_count; i++) {
		zend_hash_index_del(ht, victims[i]);
	}
	efree(victims);
}
/* }}} */

/* {{{ gene_request_ctx */
gene_request_context *gene_request_ctx(void) {
	gene_request_context *ctx;
	zend_long cid;
	int have_ctx_lookup = 0; /* v5.5.8: set when second-chance already did the hash probe */

	/* Fast path: FPM mode — no coroutine overhead */
	if (EXPECTED(GENE_G(runtime_type) < 2)) {
		return &GENE_G(default_ctx);
	}
	/* [GENE_PERF:2026-04-17] Ultra-fast path: same vm_stack pointer ⇒ same coroutine.
	 * Swoole saves/restores EG(vm_stack) on every coroutine switch, so identity holds
	 * as long as we haven't yielded. This skips the Swoole getcid() PHP call entirely
	 * for every GENE_REQ() access inside a non-yielding C call chain (the common case). */
	if (EXPECTED(GENE_G(current_ctx) != NULL && GENE_G(current_vm_stack) == (void *)EG(vm_stack))) {
		return GENE_G(current_ctx);
	}
	/* Slow path: resolve coroutine id via Swoole */
	cid = gene_get_coroutine_id();
	ctx = NULL;
	/* [GENE_FIX:2026-04-24 v5.5.8] Second-chance fast path — cid matches cached one.
	 * CRITICAL: When a coroutine dies without cleanup() and Swoole reuses its cid,
	 * blindly trusting current_cid would return the previous coroutine's ctx. We
	 * must verify the cached ctx is still the authoritative binding for this cid
	 * by consulting co_contexts. This turns a sub-nanosecond compare-only check
	 * into compare + single O(1) hash probe — still far cheaper than getcid()
	 * (which already ran above) and absolutely required for correctness.
	 *
	 * When second-chance hits, we reuse the freshly-fetched ctx pointer below
	 * (have_ctx_lookup=1) so the full-resolve path skips one redundant
	 * hash probe even on cid-reuse / evicted-binding paths. */
	if (EXPECTED(GENE_G(current_ctx) != NULL && cid >= 0
			&& cid == GENE_G(current_cid)
			&& GENE_G(co_contexts) != NULL)) {
		ctx = zend_hash_index_find_ptr(GENE_G(co_contexts), (zend_ulong)cid);
		have_ctx_lookup = 1;
		if (EXPECTED(ctx == GENE_G(current_ctx))) {
			GENE_G(current_vm_stack) = (void *)EG(vm_stack);
			return ctx;
		}
		/* Identity mismatch: cid reused, or ctx silently evicted by sweep.
		 * Invalidate the stale cache and fall through — ctx now holds the
		 * new live binding (or NULL if none exists, in which case we
		 * allocate below). */
		GENE_G(current_ctx) = NULL;
		GENE_G(current_cid) = -1;
		GENE_G(current_vm_stack) = NULL;
	}
	gene_init_co_contexts();
	if (UNEXPECTED(cid < 0)) {
		if (!GENE_G(resident_ctx)) {
			GENE_G(resident_ctx) = gene_request_context_pool_acquire();
		}
		GENE_G(current_cid) = -2;
		GENE_G(current_ctx) = GENE_G(resident_ctx);
		GENE_G(current_vm_stack) = (void *)EG(vm_stack);
		return GENE_G(resident_ctx);
	}
	if (!have_ctx_lookup) {
		ctx = zend_hash_index_find_ptr(GENE_G(co_contexts), (zend_ulong)cid);
	}
	if (!ctx) {
		/* [GENE_MEM:2026-04-23] Soft cap + probabilistic sweep. In Swoole
		 * long-running workers users sometimes forget to defer cleanup();
		 * without a backstop co_contexts would grow unboundedly. We run
		 * the sweep only when the cap is hit, so the steady-state cost
		 * is zero for well-behaved apps.
		 * [GENE_AUDIT:2026-07-03 P3] co_contexts_max=0 previously meant
		 * "unlimited" — sweep never fired, leading to unbounded growth on
		 * misconfiguration. Now 0 is treated as "use default 1024", matching
		 * gene_co_contexts_sweep()'s own fallback at line 687. */
		zend_long eff_cap = (GENE_G(co_contexts_max) > 0) ? GENE_G(co_contexts_max) : 1024;
		if (UNEXPECTED((zend_long)zend_hash_num_elements(GENE_G(co_contexts)) >= eff_cap)) {
			gene_co_contexts_sweep();
		}
		/* [GENE_PERF:2026-04-24] Acquire from the struct pool when available;
		 * ecalloc only when the pool is empty. In Swoole steady state this
		 * eliminates the per-coroutine-spawn allocator round trip. */
		ctx = gene_request_context_pool_acquire();
		zend_hash_index_update_ptr(GENE_G(co_contexts), (zend_ulong)cid, ctx);
	}
	GENE_G(current_cid) = cid;
	GENE_G(current_ctx) = ctx;
	GENE_G(current_vm_stack) = (void *)EG(vm_stack);
	return ctx;
}
/* }}} */

/* {{{ php_gene_init_globals
 */
static void php_gene_init_globals() {
	GENE_G(app_root) = NULL;
	GENE_G(app_root_len) = 0;
	GENE_G(app_view) = NULL;
	GENE_G(app_ext) = NULL;
	GENE_G(app_view_len) = 0;
	GENE_G(app_ext_len) = 0;
	GENE_G(app_key) = NULL;
	GENE_G(app_key_len) = 0;
	GENE_G(auto_load_fun) = NULL;
	GENE_G(config_cache_key) = NULL;
	GENE_G(config_cache_key_len) = 0;
	memset(&GENE_G(default_ctx), 0, sizeof(gene_request_context));
	GENE_G(resident_ctx) = NULL;
	GENE_G(co_contexts) = NULL;
	GENE_G(current_ctx) = NULL;
	GENE_G(current_cid) = -1;
	GENE_G(current_vm_stack) = NULL;
	GENE_G(swoole_getcid_func) = NULL;
	GENE_G(swoole_getcid_resolved) = 0;
	GENE_G(swoole_co_exists_func) = NULL;
	GENE_G(swoole_co_exists_resolved) = 0;
	/* [GENE_PERF:2026-04-24] Context struct pool init. */
	GENE_G(ctx_pool_head) = NULL;
	GENE_G(ctx_pool_size) = 0;
	/* ctx_pool_prewarm is populated by PHP_INI loader before MINIT, so do
	 * NOT zero it here — doing so would clobber the user's php.ini value.
	 * (Leaving the field alone is safe: globals are zeroed by GINIT.) */
	GENE_G(autoload_registered) = 0;
	GENE_G(worker_ready) = 0;
	GENE_G(fn_cache) = NULL;
	/* [GENE_PERF:2026-06-19 P3] Precompiled-dispatch cache is lazily allocated
	 * on first dispatch (Swoole, post-workerReady). route_precompile comes from
	 * php.ini, so — like ctx_pool_prewarm — it must NOT be zeroed here. */
	GENE_G(route_pc) = NULL;
	GENE_G(cache) = NULL;
	GENE_G(cache_easy) = NULL;
	/* [GENE_MEM:2026-06-19 M1] LRU tracking set is lazily allocated on the
	 * first business write; cache_max_items is loaded from php.ini before
	 * MINIT so we must NOT zero it here (same rule as ctx_pool_prewarm). */
	GENE_G(cache_lru) = NULL;
	gene_rwlock_init(&GENE_G(cache_lock));
	gene_memory_init();
}
/* }}} */

/* {{{ php_gene_close_request_globals
 */
static void php_gene_close_request_globals() {
	if (GENE_G(fn_cache) && GENE_G(runtime_type) < 2) {
		zend_hash_destroy(GENE_G(fn_cache));
		FREE_HASHTABLE(GENE_G(fn_cache));
		GENE_G(fn_cache) = NULL;
	}
	gene_request_context_destroy(&GENE_G(default_ctx));
	if (GENE_G(app_root)) {
		efree(GENE_G(app_root));
		GENE_G(app_root) = NULL;
		GENE_G(app_root_len) = 0;
	}
	if (GENE_G(app_view)) {
		efree(GENE_G(app_view));
		GENE_G(app_view) = NULL;
		GENE_G(app_view_len) = 0;
	}
	if (GENE_G(app_ext)) {
		efree(GENE_G(app_ext));
		GENE_G(app_ext) = NULL;
		GENE_G(app_ext_len) = 0;
	}
	if (GENE_G(auto_load_fun)) {
		efree(GENE_G(auto_load_fun));
		GENE_G(auto_load_fun) = NULL;
	}
	if (GENE_G(app_key)) {
		efree(GENE_G(app_key));
		GENE_G(app_key) = NULL;
		GENE_G(app_key_len) = 0;
	}
	if (GENE_G(config_cache_key)) {
		efree(GENE_G(config_cache_key));
		GENE_G(config_cache_key) = NULL;
		GENE_G(config_cache_key_len) = 0;
	}
	GENE_G(current_ctx) = NULL;
	GENE_G(current_cid) = -1;
	GENE_G(current_vm_stack) = NULL;
	if (GENE_G(resident_ctx)) {
		gene_request_context *tmp = GENE_G(resident_ctx);
		GENE_G(resident_ctx) = NULL;
		gene_request_context_destroy(tmp);
		/* [GENE_PERF:2026-04-24] Route through pool too — resident_ctx may
		 * live across many lightweight calls in Swoole/Coroutine mode. */
		gene_request_context_pool_release(tmp);
	}
	if (GENE_G(co_contexts)) {
		zend_hash_destroy(GENE_G(co_contexts));
		FREE_HASHTABLE(GENE_G(co_contexts));
		GENE_G(co_contexts) = NULL;
	}
	/* [GENE_PERF:2026-04-24] Drain the struct pool on RSHUTDOWN for FPM so
	 * request-scoped memory is fully reclaimed. In Swoole mode RSHUTDOWN
	 * fires once at worker exit — same semantic. */
	gene_request_context_pool_drain();
}
/* }}} */

/* {{{ php_gene_close_globals
 * Only cleans up persistent (process-level) resources.
 * Request-scoped resources are cleaned in RSHUTDOWN via php_gene_close_request_globals().
 */
static void php_gene_close_globals() {
	/* Persistent resources (cache, cache_easy, cache_lock) are cleaned in MSHUTDOWN directly.
	 * Do NOT call php_gene_close_request_globals() here to avoid double-free
	 * since RSHUTDOWN already handles request-scoped cleanup. */
}
/* }}} */

/** {{{ PHP_GINIT_FUNCTION
 */
PHP_GINIT_FUNCTION(gene) {
	gene_globals->run_environment = 1;
	gene_globals->runtime_type = 1;
	gene_globals->use_namespace = 1;
	gene_globals->view_compile = 0;
	gene_globals->view_compile_check_mtime = 0;
	gene_globals->use_library = 0;
}
/* }}} */

/*
 * {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(gene) {
	php_gene_init_globals();
	REGISTER_INI_ENTRIES();

	/* startup components */
	GENE_STARTUP(application);
	GENE_STARTUP(load);
	GENE_STARTUP(di);
	GENE_STARTUP(config);
	GENE_STARTUP(router);
	GENE_STARTUP(execute);
	GENE_STARTUP(language);
	GENE_STARTUP(memory);
	GENE_STARTUP(controller);
	GENE_STARTUP(request);
	GENE_STARTUP(webscan);
	GENE_STARTUP(response);
	GENE_STARTUP(validate);
	GENE_STARTUP(session);
	GENE_STARTUP(view);
	GENE_STARTUP(benchmark);
	GENE_STARTUP(pool);
	GENE_STARTUP(db_mysql);
	GENE_STARTUP(db_mssql);
	GENE_STARTUP(db_pgsql);
	GENE_STARTUP(db_sqlite);
	GENE_STARTUP(model);
	GENE_STARTUP(hook);
	GENE_STARTUP(service);
	GENE_STARTUP(factory);
	GENE_STARTUP(redis_pool);
	GENE_STARTUP(redis);
	GENE_STARTUP(memcached);
	GENE_STARTUP(cache);
	GENE_STARTUP(log);
	GENE_STARTUP(exception);

	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */


/** {{{ ARG_INFO
 *  */
ZEND_BEGIN_ARG_INFO_EX(gene_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()
/* }}} */

/*
 * {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(gene) {
	UNREGISTER_INI_ENTRIES();
	php_gene_close_globals();

	/* [GENE_FIX:2026-04-27] Clear sub-module process-level resources.
	 * Currently only pool carries a static HashTable that survives the
	 * Pool::closeAll() user contract; this is a safety net for valgrind
	 * cleanliness on abnormal shutdown. */
	GENE_SHUTDOWN(pool);
	GENE_SHUTDOWN(redis_pool);

	if (GENE_G(cache)) {
		gene_hash_destroy(GENE_G(cache));
		GENE_G(cache) = NULL;
	}
	/* [GENE_MEM:2026-06-19 M1] Tear down the business-cache LRU tracking set
	 * (frees its persistent key copies) before the lock is destroyed. */
	gene_cache_lru_destroy();
	/* [GENE_PERF:2026-06-19 P3] Free precompiled-dispatch descriptors. They only
	 * borrow pointers into the route tree (freed above) so order is irrelevant;
	 * the table dtor frees each descriptor's owned eval_str copy. */
	gene_router_pc_destroy();
	/* [GENE_PERF:2026-06-19 P6] Free the persistent FPM closure-source cache
	 * (NTS-only; never allocated under ZTS). Keeps valgrind/ASAN clean. */
	gene_closure_src_cache_destroy();
	if (GENE_G(cache_easy)) {
		gene_hash_destroy(GENE_G(cache_easy));
		GENE_G(cache_easy) = NULL;
	}
	if (GENE_G(fn_cache)) {
		zend_hash_destroy(GENE_G(fn_cache));
		FREE_HASHTABLE(GENE_G(fn_cache));
		GENE_G(fn_cache) = NULL;
	}
	gene_rwlock_destroy(&GENE_G(cache_lock));
	return SUCCESS; // @suppress("Symbol is not resolved")
}

/* }}} */

/*
 * {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(gene) {
	/* [GENE_MEM:2026-04-24] default_ctx is zero-initialized by
	 * php_gene_init_globals(); its inline path_params is IS_UNDEF until
	 * init() populates it. */
	if (Z_TYPE(GENE_G(default_ctx).path_params) == IS_UNDEF) {
		gene_request_context_init(&GENE_G(default_ctx));
	} else {
		gene_request_context_reset(&GENE_G(default_ctx));
	}
	if (GENE_G(runtime_type) >= 2) {
		if (!GENE_G(co_contexts)) {
			ALLOC_HASHTABLE(GENE_G(co_contexts));
			/* [GENE_PERF:2026-04-24 #2] Increased from 8 → 32. Early-phase
			 * Swoole workers immediately hit multiple coroutines; pre-sizing
			 * at 32 skips 2 rehashes on the first wave (8→16→32) for typical
			 * HTTP services, at a one-time cost of a few hundred extra bytes
			 * of HT bucket storage per worker. */
			zend_hash_init(GENE_G(co_contexts), 32, NULL, gene_co_context_dtor, 0);
		}
		if (GENE_G(resident_ctx)) {
			gene_request_context_reset(GENE_G(resident_ctx));
		}
		/* [GENE_PERF:2026-04-24 #2] Auto pre-warm the ctx struct pool on the
		 * first RINIT after MINIT. This is a one-shot: subsequent requests
		 * find ctx_pool_size > 0 and skip immediately. The guard is cheap
		 * (one load) so the steady-state cost is zero. */
		if (UNEXPECTED(GENE_G(ctx_pool_prewarm) > 0
				&& GENE_G(ctx_pool_size) == 0)) {
			gene_request_context_pool_prewarm(GENE_G(ctx_pool_prewarm));
		}
	}
	GENE_G(current_ctx) = NULL;
	GENE_G(current_cid) = -1;
	GENE_G(current_vm_stack) = NULL;
	GENE_G(autoload_registered) = 0;
	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(gene) {
	php_gene_close_request_globals();
	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(gene) {
	php_info_print_table_start();
	php_info_print_table_header(2, "gene support", "enabled");
	php_info_print_table_row(2, "gene author:", " sasou <zaipd@qq.com>");
	php_info_print_table_row(2, "gene site:", " https://www.1xm.net");
	php_info_print_table_row(2, "gene version:", PHP_GENE_VERSION);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/*
 * {{{ gene_version()
 */
PHP_FUNCTION(gene_version) {
	RETURN_STRING(PHP_GENE_VERSION);
}
/* }}} */

/* {{{ gene_functions[]
 *
 * Every user visible function must have an entry in gene_functions[].
 */
const zend_function_entry gene_functions[] = {
	PHP_FE(gene_version, gene_void_arginfo)
	PHP_FE_END
};
/* }}} */

/*
 * {{{ gene_module_entry
 */
#ifdef COMPILE_DL_GENE
ZEND_GET_MODULE(gene)
#endif
/* }}} */

/** {{{ module depends
 */
#if ZEND_MODULE_API_NO >= 20050922
const zend_module_dep gene_deps[] = {
	// Audit [2026-03-25] cannot add comma separation, window compilation fails
	ZEND_MOD_REQUIRED("spl")
	{ NULL, NULL, NULL }
};
#endif
/* }}} */

/** {{{ gene_module_entry
 */
zend_module_entry gene_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
	STANDARD_MODULE_HEADER_EX, NULL, gene_deps, // @suppress("Symbol is not resolved")
#else
	STANDARD_MODULE_HEADER,
#endif
	"gene", gene_functions,
	PHP_MINIT(gene),
	PHP_MSHUTDOWN(gene),
	PHP_RINIT(gene),
	PHP_RSHUTDOWN(gene),
	PHP_MINFO(gene),
	PHP_GENE_VERSION,
	PHP_MODULE_GLOBALS(gene),
	PHP_GINIT(gene),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */



