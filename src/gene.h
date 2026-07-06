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

 #ifndef PHP_GENE_H
 #define PHP_GENE_H
 
 extern zend_module_entry gene_module_entry;
 #define phpext_gene_ptr &gene_module_entry
 
 #define PHP_GENE_VERSION "5.6.8"
 
 #ifdef PHP_WIN32
 #	define PHP_GENE_API __declspec(dllexport)
 #elif defined(__GNUC__) && __GNUC__ >= 4
 #	define PHP_GENE_API __attribute__ ((visibility("default")))
 #else
 #	define PHP_GENE_API
 #endif
 
 #ifdef ZTS
 #include "TSRM.h"
 #endif
 
 #ifdef PHP_WIN32
 #include <synchapi.h>
 typedef SRWLOCK gene_rwlock_t;
 #define gene_rwlock_init(lock)    InitializeSRWLock(lock)
 #define gene_rwlock_rdlock(lock)  AcquireSRWLockShared(lock)
 #define gene_rwlock_rdunlock(lock) ReleaseSRWLockShared(lock)
 #define gene_rwlock_wrlock(lock)  AcquireSRWLockExclusive(lock)
 #define gene_rwlock_wrunlock(lock) ReleaseSRWLockExclusive(lock)
 #define gene_rwlock_destroy(lock)
 #else
 #include <pthread.h>
 typedef pthread_rwlock_t gene_rwlock_t;
 #define gene_rwlock_init(lock)    pthread_rwlock_init(lock, NULL)
 #define gene_rwlock_rdlock(lock)  pthread_rwlock_rdlock(lock)
 #define gene_rwlock_rdunlock(lock) pthread_rwlock_unlock(lock)
 #define gene_rwlock_wrlock(lock)  pthread_rwlock_wrlock(lock)
 #define gene_rwlock_wrunlock(lock) pthread_rwlock_unlock(lock)
 #define gene_rwlock_destroy(lock) pthread_rwlock_destroy(lock)
 #endif
 
 #ifdef ZTS
 #define GENE_G(v) TSRMG(gene_globals_id, zend_gene_globals *, v)
 #else
 #define GENE_G(v) (gene_globals.v)
 #endif
 
 /* [GENE_AUDIT:2026-07-03 P3/T1#4] ZTS-safe internal function pointer cache.
  * Non-ZTS: internal function pointers are process-level constants, so a
  * function-local static cache is safe and saves a hash lookup per call.
  * ZTS: CG(function_table) is per-thread, so a process-wide static would
  * cross threads — fall back to a per-call lookup (the hash find on a
  * pre-known interned key is cheap). Usage:
  *   static zend_function *fn = NULL;   // non-ZTS only; under ZTS fn is local
  *   fn = GENE_CG_FN_LOOKUP(fn, "json_encode"); */
 #ifndef ZTS
 static inline zend_function *gene_cg_fn_cache(zend_function **cache, const char *key, size_t keylen) {
     if (EXPECTED(*cache)) return *cache;
     *cache = zend_hash_str_find_ptr(CG(function_table), key, keylen);
     return *cache;
 }
 #define GENE_CG_FN_LOOKUP(cache_var, literal) gene_cg_fn_cache(&(cache_var), ZEND_STRL(literal))
 #else
 #define GENE_CG_FN_LOOKUP(cache_var, literal) zend_hash_str_find_ptr(CG(function_table), ZEND_STRL(literal))
 #endif
 
 #define gene_object zend_object
 #define gene_strip_obj(o) Z_OBJ_P(o)
 
 
 #define GENE_INIT_CLASS_ENTRY(ce, common_name, namespace_name, methods) \
	 if(GENE_G(use_namespace)) { \
		 INIT_CLASS_ENTRY(ce, namespace_name, methods); \
	 } else { \
		 INIT_CLASS_ENTRY(ce, common_name, methods); \
	 }
 #define GENE_MINIT_FUNCTION(module)   	    ZEND_MINIT_FUNCTION(gene_##module)
 #define GENE_MSHUTDOWN_FUNCTION(module)   	ZEND_MSHUTDOWN_FUNCTION(gene_##module)
 #define GENE_RINIT_FUNCTION(module)   	    ZEND_RINIT_FUNCTION(gene_##module)
 #define GENE_RSHUTDOWN_FUNCTION(module)   	ZEND_RSHUTDOWN_FUNCTION(gene_##module)
 #define GENE_MINFO_FUNCTION(module)   	PHP_MINFO_FUNCTION(module)
 #define GENE_STARTUP(module)	 		ZEND_MODULE_STARTUP_N(gene_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define GENE_SHUTDOWN(module)	 		ZEND_MODULE_SHUTDOWN_N(gene_##module)(SHUTDOWN_FUNC_ARGS_PASSTHRU)
 
 PHP_MINIT_FUNCTION (gene);
 PHP_MSHUTDOWN_FUNCTION (gene);
 PHP_RINIT_FUNCTION (gene);
 PHP_RSHUTDOWN_FUNCTION (gene);
 PHP_MINFO_FUNCTION (gene);
 
 #define GENE_DB_HISTORY_MAX 200
 
 typedef struct _gene_request_context {
	 char *method;
	 char *path;
	 char *router_path;
	 char *module;
	 char *controller;
	 char *action;
	 char *child_views;
	 char *lang;
	 /* [GENE_PERF:2026-04-20] Cached string lengths populated at set-time so that
	  * hot-path code (router dispatch, getters, strreplace_fast) can skip the
	  * per-request strlen() calls on short strings like module/controller/action
	  * (typically ~10-20 chars each, ~15-25 cycles per strlen). Saves ~6-10
	  * strlen()s per request across dispatch + view + getter paths. */
	 size_t method_len;
	 size_t path_len;
	 size_t router_path_len;
	 size_t module_len;
	 size_t controller_len;
	 size_t action_len;
	 size_t lang_len;
	 size_t child_views_len;
	 /* [GENE_MEM:2026-04-24] path_params is now an inline zval (was a heap
	  * pointer). Previously every request burned 1x emalloc(sizeof(zval)) +
	  * 1x efree plus pointer chasing to reach the HashTable. The outer zval
	  * now lives with the context struct; only the backing HashTable is
	  * heap-allocated (via array_init). Consumers must use &ctx->path_params
	  * everywhere a zval* is needed. */
	 zval path_params;
	 zval request_attr;
	 zval di_regs;
	 zval response_obj;
	 zval view_vars;
	 zend_long view_scope_no;
	 zval db_mysql_history;
	 zval db_pgsql_history;
	 zval db_sqlite_history;
	 zval db_mssql_history;
	 struct timeval bench_start;
	 struct timeval bench_end;
	 zend_long bench_memory_start;
	 zend_long bench_memory_end;
	 char *log_file;
	 zend_long log_level;
	 zend_bool log_level_set;
 } gene_request_context;
 
 ZEND_BEGIN_MODULE_GLOBALS (gene)
 char *app_root;
 char *library_root;
 char *app_view;
 char *app_ext;
 char *app_key;
 size_t app_view_len;
 size_t app_ext_len;
 /* [GENE_PERF:2026-04-19 #2] Cache strlen of app_key/app_root populated at set-time
  * (autoload(), __construct safe arg) so per-request dispatch can look up the
  * persistent cache key prefix length directly instead of calling strlen() each
  * time. Populated by gene_application_instance() and gene_application::autoload(). */
 size_t app_key_len;
 size_t app_root_len;
 char *auto_load_fun;
 char *config_cache_key;
 size_t config_cache_key_len;
 bool use_library;
 zend_long run_environment;
 zend_long runtime_type;
 bool use_namespace;
 bool view_compile;
 bool view_compile_check_mtime;
 HashTable *cache;
 HashTable *cache_easy;
 /* [GENE_MEM:2026-06-19 M1] Approximate-LRU tracking set for the Gene\Cache
  * business partition (writes bracketed by cache_layer_memory_write_depth>0).
  * Insertion-ordered set of persistent key copies (least→most recently set);
  * only allocated when gene.cache_max_items > 0. Framework metadata
  * (routes/config/events) and plain Memory::set entries are never tracked. */
 HashTable *cache_lru;
 /* [GENE_MEM:2026-06-19 M1] Hard cap on the number of tracked Gene\Cache
  * business entries. 0 = unlimited (default, fully backward-compatible);
  * when > 0, business writes evict the oldest tracked entries past the cap. */
 zend_long cache_max_items;
 gene_rwlock_t cache_lock;
 gene_request_context default_ctx;
 gene_request_context *resident_ctx;
 HashTable *co_contexts;
 gene_request_context *current_ctx;
 zend_long current_cid;
 /* [GENE_PERF:2026-04-17] VM stack pointer snapshot at the time current_ctx/current_cid
  * was cached. Swoole swaps EG(vm_stack) on every coroutine switch, so if the current
  * EG(vm_stack) equals this snapshot we know we're still in the same coroutine and can
  * skip the Swoole getcid() PHP call entirely. Reset to NULL on request/context clear. */
 void *current_vm_stack;
zend_function *swoole_getcid_func;
zend_bool swoole_getcid_resolved;
/* [GENE_PERF:2026-06-19 P1] Kill-switch for the dlsym-based direct C-API
 * resolution of swoole::Coroutine::get_current_cid(). Default on; set
 * gene.swoole_getcid_capi=0 in php.ini to force the PHP-call fallback. */
zend_bool swoole_getcid_capi;
/* [GENE_MEM:2026-04-23] Cached Swoole\Coroutine::exists resolver used by
 * gene_co_contexts_sweep() to reclaim contexts of already-dead coroutines
 * in long-running workers where users may forget to call cleanup(). */
zend_function *swoole_co_exists_func;
zend_bool swoole_co_exists_resolved;
/* [GENE_MEM:2026-04-23] Soft cap on co_contexts size. When exceeded we run
 * a sweep (exists() check + insertion-order fallback) in the slow path of
 * gene_request_ctx(). Configurable via gene.co_contexts_max ini. */
zend_long co_contexts_max;
/* [GENE_PERF:2026-04-24] Free-list of recycled gene_request_context structs.
 * Populated by gene_co_context_dtor() and consumed by gene_request_ctx() slow
 * path when spawning a new coroutine context. Bounds request-cycle allocator
 * traffic to ~zero under steady-state Swoole load: one ecalloc amortized over
 * thousands of request scopes. Pool is capped via gene.ctx_pool_max (default
 * 256) and fully drained in MSHUTDOWN / worker exit. */
void *ctx_pool_head;
zend_long ctx_pool_size;
zend_long ctx_pool_max;
/* [GENE_PERF:2026-04-24 #2] Optional pre-warm count. When > 0 and
 * runtime_type >= 2, RINIT fills the context struct pool up to this
 * number so the first cold-start wave of Swoole coroutines pays zero
 * allocator cost. Capped internally at ctx_pool_max. */
zend_long ctx_pool_prewarm;
zend_bool autoload_registered;
zend_bool worker_ready;
/* [GENE_CACHE:2026-04-25] Incremented only around Gene\\Cache (cache.c) internal
 * gene_memory_set/del calls. Lets process-level cache fill after workerReady()
 * without opening userland Memory::set — enter/exit must not wrap user callbacks. */
zend_ulong cache_layer_memory_write_depth;
HashTable *fn_cache;
/* [GENE_MEM:2026-04-23] fn_cache_id removed. Keys are now derived from the
 * closure's zend_object->handle so re-registering the same closure doesn't
 * grow the table. See gene_fn_cache_store() in router.c. */
/* [GENE_PERF:2026-06-19 P3] Per-thread lazy memoization cache of precompiled
 * route-dispatch descriptors (gene_route_pc), keyed by the leaf HashTable
 * pointer. Populated on first dispatch of each route, executed thereafter with
 * zero hash lookups. Swoole-only and only after workerReady() (route tree +
 * fn_cache frozen, leaf pointers stable). Per-thread because it borrows
 * pointers from the per-thread GENE_G(cache)/fn_cache. Freed in MSHUTDOWN. */
HashTable *route_pc;
/* [GENE_PERF:2026-06-19 P3] Opt-in kill-switch for the precompiled dispatch
 * cache above. Default 0 (off) — the proven get_router_info_slow() path runs
 * unchanged until an operator enables gene.route_precompile=1 after validating
 * on Linux (phpize+make+ASAN + full route regression). */
zend_bool route_precompile;
ZEND_END_MODULE_GLOBALS (gene)
 
 extern ZEND_DECLARE_MODULE_GLOBALS (gene);
 
gene_request_context *gene_request_ctx(void);
void gene_request_context_init(gene_request_context *ctx);
void gene_request_context_reset(gene_request_context *ctx);
void gene_request_context_destroy(gene_request_context *ctx);
zend_long gene_get_coroutine_id(void);
void gene_init_co_contexts(void);
void gene_co_contexts_sweep(void);
/* [GENE_PERF:2026-04-24] Struct pool helpers (Swoole-only hot path). */
gene_request_context *gene_request_context_pool_acquire(void);
void gene_request_context_pool_release(gene_request_context *ctx);
void gene_request_context_pool_drain(void);
/* [GENE_PERF:2026-04-24 #2] Pre-warm N contexts into the pool. Called by
 * RINIT when gene.ctx_pool_prewarm > 0 (Swoole only), and optionally from
 * userland via Application::prewarmCtxPool(). Returns the number of
 * contexts actually added (bounded by ctx_pool_max). */
zend_long gene_request_context_pool_prewarm(zend_long count);

/* [GENE_FIX:2026-05-24] Cross-request-safe interned string helper.
 *
 * Background: zend_string_init_interned(s, l, (permanent)1) returns a
 * truly process-permanent string ONLY when opcache provides a SHM-backed
 * permanent table. Under opcache.file_cache_only=1, with opcache disabled,
 * or in CLI without SHM, the call degrades to a request-scope interned
 * string (allocated in CG(interned_strings), freed at RSHUTDOWN). Caching
 * such a pointer in a function-local static zend_string * leaves a
 * dangling pointer on the very next request, causing garbage ZSTR_LEN reads of
 * 160 GB+ (e.g. 0x2800000020), leading to fatal allocator errors / SIGSEGV.
 *
 * gene_interned_str_persistent() validates IS_STR_PERMANENT before caching
 * and returns NULL via *slot if the runtime cannot grant a permanent
 * string, forcing the next call to re-resolve via zend_string_init_interned
 * (which is a cheap CG(interned_strings) hash hit after the first request).
 * The returned string is always usable for the duration of the current
 * request and must NOT be released by the caller. */
zend_string *gene_interned_str_persistent(zend_string **slot, const char *s, size_t l);

/* Class lookup that never caches a zend_string across requests. Probes
 * EG(class_table) directly with the lowercased name (PHP stores keys
 * lowercased), and falls back to zend_lookup_class with a request-scope
 * zend_string for autoload. Drop-in replacement for the unsafe
 * static zend_string * + zend_lookup_class pattern. */
zend_class_entry *gene_lookup_class_str(const char *name, size_t len);

/* Convenience macro mirroring the prior static zend_string *X pattern.
 * Declares a request-scope variable name that is safe to use across
 * requests because the underlying cache slot is invalidated whenever the
 * runtime cannot provide IS_STR_PERMANENT. Usage:
 *
 *   GENE_INTERNED_STR(my_key, "Foo");
 *   zval *v = zend_hash_find(ht, my_key);
 */
#define GENE_INTERNED_STR(name, str_lit)                                                            \
    static zend_string *name##__slot = NULL;                                                        \
    zend_string *name = gene_interned_str_persistent(&name##__slot, "" str_lit "", sizeof(str_lit) - 1)

 #define GENE_REQ(v) (gene_request_ctx()->v)

 #endif	/* PHP_GENE_H */
 
 /*
  * Local variables:
  * tab-width: 4
  * c-basic-offset: 4
  * End:
  * vim600: noet sw=4 ts=4 fdm=marker
  * vim<600: noet sw=4 ts=4
  */
 