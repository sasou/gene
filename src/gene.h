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
 
 #define PHP_GENE_VERSION "5.6.0"
 
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
 HashTable *cache;
 HashTable *cache_easy;
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
 