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
#include "Zend/zend_API.h"

#include "../gene.h"
#include "../tool/monitor.h"
#include "../cache/memory.h"
#include "../db/pool.h"
#include "../cache/redis_pool.h"
#include "Zend/zend_smart_str.h"

zend_class_entry *gene_monitor_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_monitor_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

/* {{{ gene_monitor_collect_pools
 * [GENE_FEATURE:2026-07-30 F2] Aggregate one pool class's named instances:
 * reads the static 'instances' registry and calls each pool's own stats()
 * method, keyed by pool name. Read-only; the per-pool stats() methods are
 * the single source of truth (no counter duplication here). */
static void gene_monitor_collect_pools(zend_class_entry *pool_ce, const char *prop, size_t prop_len, zval *out) {
	zval *instances;
	zend_string *name;
	zval *pool;
	zend_function *fn_stats;

	if (!pool_ce) {
		return;
	}
	instances = zend_read_static_property(pool_ce, prop, prop_len, 1);
	if (!instances || Z_TYPE_P(instances) != IS_ARRAY) {
		return;
	}
	fn_stats = zend_hash_str_find_ptr(&pool_ce->function_table, ZEND_STRL("stats"));
	if (!fn_stats) {
		return;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(instances), name, pool) {
		zval stats_ret;
		if (!name || Z_TYPE_P(pool) != IS_OBJECT) {
			continue;
		}
		ZVAL_UNDEF(&stats_ret);
		zend_call_known_function(fn_stats, Z_OBJ_P(pool), pool_ce, &stats_ret, 0, NULL, NULL);
		if (UNEXPECTED(EG(exception))) {
			if (!Z_ISUNDEF(stats_ret)) {
				zval_ptr_dtor(&stats_ret);
			}
			break;
		}
		if (Z_TYPE(stats_ret) == IS_ARRAY) {
			/* [GENE_AUDIT:2026-07-30 D1] zend_hash_update takes ownership of
			 * the zval (COPY_VALUE, no addref); destructing stats_ret after
			 * the update would drop the array's refcount to zero and leave a
			 * dangling pointer in the pools partition. */
			zend_hash_update(Z_ARRVAL_P(out), name, &stats_ret);
		} else if (!Z_ISUNDEF(stats_ret)) {
			zval_ptr_dtor(&stats_ret);
		}
	} ZEND_HASH_FOREACH_END();
}
/* }}} */

/*
 * {{{ public static Gene\Monitor::stats(): array
 * [GENE_FEATURE:2026-07-30 F2] Single aggregated observability export.
 * 07-12 WP-02 landed per-partition stats (Memory / Pool / RedisPool /
 * sweep / ctx pool) but left no single aggregation point, so capacity
 * tuning (cache_max_items / co_contexts_max / ctx_pool_max) had no data
 * handle. Pure read, zero side effects.
 *
 * Return shape:
 *   [
 *     'memory'        => [...],  // same keys as Gene\Memory::stats()
 *     'db_pools'      => ['name' => [total,idle,using,overflow,min,max,closed]],
 *     'redis_pools'   => ['name' => [total,idle,using,overflow,min,max,closed]],
 *     'requests'      => ['count' => n, 'errors' => n],
 *     'redis_pool_cas_abandoned'    => n,  // [GENE_AUDIT:2026-07-30 L1]
 *     'db_pool_cas_abandoned'       => n,  // [GENE_AUDIT:2026-08-06 C1]
 *     'db_pool_get_timeout'         => n,  // [GENE_FEATURE:2026-08-06 F1-7]
 *     'memory_cache_hit'            => n,  // userland Memory::get only
 *     'memory_cache_miss'           => n,
 *     'db_slow_query_count'         => n,  // [GENE_FEATURE:2026-08-07 F1-7b]
 *     'slow_query_ms'               => n,  // active threshold (0 = disabled)
 *     'swoole_auto_cleanup_defers'  => n,  // [GENE_FEATURE:2026-07-30 F1]
 *     'swoole_auto_cleanup_reclaimed' => n,
 *   ]
 */
PHP_METHOD(gene_monitor, stats) {
	zval mem, dbp, rdp, req;

	array_init(return_value);

	/* memory partition — mirrors Gene\Memory::stats() (same GENE_G reads). */
	array_init(&mem);
	GENE_CACHE_RDLOCK();
	add_assoc_long(&mem, "cache_items",
		GENE_G(cache) ? (zend_long)zend_hash_num_elements(GENE_G(cache)) : 0);
	add_assoc_long(&mem, "cache_easy_items",
		GENE_G(cache_easy) ? (zend_long)zend_hash_num_elements(GENE_G(cache_easy)) : 0);
	GENE_CACHE_RDUNLOCK();
	add_assoc_long(&mem, "fn_cache_items",
		GENE_G(fn_cache) ? (zend_long)zend_hash_num_elements(GENE_G(fn_cache)) : 0);
	add_assoc_long(&mem, "co_contexts_items",
		GENE_G(co_contexts) ? (zend_long)zend_hash_num_elements(GENE_G(co_contexts)) : 0);
	add_assoc_long(&mem, "co_contexts_max", GENE_G(co_contexts_max));
	add_assoc_long(&mem, "co_contexts_watermark", (zend_long)GENE_G(co_contexts_watermark));
	add_assoc_long(&mem, "co_contexts_sweep_count", (zend_long)GENE_G(co_contexts_sweep_count));
	add_assoc_long(&mem, "co_contexts_sweep_scanned", (zend_long)GENE_G(co_contexts_sweep_scanned));
	add_assoc_long(&mem, "co_contexts_sweep_us", (zend_long)GENE_G(co_contexts_sweep_us));
	/* [GENE_PERF:2026-07-30 M1] cooldown telemetry: how many cap triggers
	 * were suppressed by the sweep cooldown. */
	add_assoc_long(&mem, "co_contexts_sweep_skipped", (zend_long)GENE_G(co_contexts_sweep_skipped));
	add_assoc_long(&mem, "ctx_pool_size", GENE_G(ctx_pool_size));
	add_assoc_long(&mem, "ctx_pool_max", GENE_G(ctx_pool_max));
	add_assoc_long(&mem, "ctx_pool_hit", (zend_long)GENE_G(ctx_pool_hit));
	add_assoc_long(&mem, "ctx_pool_miss", (zend_long)GENE_G(ctx_pool_miss));
	add_assoc_long(&mem, "cache_business_items",
		GENE_G(cache_lru) ? (zend_long)zend_hash_num_elements(GENE_G(cache_lru)) : 0);
	add_assoc_long(&mem, "route_pc_items",
		GENE_G(route_pc) ? (zend_long)zend_hash_num_elements(GENE_G(route_pc)) : 0);
	add_assoc_long(&mem, "closure_src_cache_items", gene_closure_src_cache_items());
	add_assoc_long(&mem, "closure_src_cache_flushes", (zend_long)GENE_G(closure_src_cache_flushes));
	/* [GENE_FEATURE:2026-07-30 F6] cache_easy TTL governance telemetry. */
	add_assoc_long(&mem, "cache_easy_ttl", GENE_G(cache_easy_ttl));
	add_assoc_long(&mem, "cache_easy_expired", (zend_long)GENE_G(cache_easy_expired));
	add_assoc_zval_ex(return_value, ZEND_STRL("memory"), &mem);

	/* named DB pools (Gene\Pool static instances registry). */
	array_init(&dbp);
	gene_monitor_collect_pools(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), &dbp);
	add_assoc_zval_ex(return_value, ZEND_STRL("db_pools"), &dbp);

	/* named Redis pools (Gene\Cache\RedisPool static instances registry). */
	array_init(&rdp);
	gene_monitor_collect_pools(gene_redis_pool_ce, ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), &rdp);
	add_assoc_zval_ex(return_value, ZEND_STRL("redis_pools"), &rdp);

	/* cumulative request telemetry (Application::run). */
	array_init(&req);
	add_assoc_long(&req, "count", (zend_long)GENE_G(request_count));
	add_assoc_long(&req, "errors", (zend_long)GENE_G(request_error_count));
	add_assoc_zval_ex(return_value, ZEND_STRL("requests"), &req);

	/* [GENE_AUDIT:2026-07-30 L1] RedisPool CAS abandonment counter. */
	add_assoc_long(return_value, "redis_pool_cas_abandoned", (zend_long)GENE_G(redis_pool_cas_abandoned));
	/* [GENE_AUDIT:2026-08-06 C1] DB Pool CAS abandonment counter. */
	add_assoc_long(return_value, "db_pool_cas_abandoned", (zend_long)GENE_G(db_pool_cas_abandoned));
	/* [GENE_FEATURE:2026-08-06 F1-7] Pool acquisition timeouts + userland
	 * Memory::get hit/miss. */
	add_assoc_long(return_value, "db_pool_get_timeout", (zend_long)GENE_G(db_pool_get_timeout));
	add_assoc_long(return_value, "memory_cache_hit", (zend_long)GENE_G(memory_cache_hit));
	add_assoc_long(return_value, "memory_cache_miss", (zend_long)GENE_G(memory_cache_miss));
	/* [GENE_FEATURE:2026-08-07 F1-7b] Slow-query counter + active threshold
	 * (gene.slow_query_ms, 0 = disabled). */
	add_assoc_long(return_value, "db_slow_query_count", (zend_long)GENE_G(db_slow_query_count));
	add_assoc_long(return_value, "slow_query_ms", GENE_G(slow_query_ms));
	/* [GENE_FEATURE:2026-07-30 F1] auto-cleanup activity. */
	add_assoc_long(return_value, "swoole_auto_cleanup_defers", (zend_long)GENE_G(swoole_auto_cleanup_defers));
	add_assoc_long(return_value, "swoole_auto_cleanup_reclaimed", (zend_long)GENE_G(swoole_auto_cleanup_reclaimed));
	/* [GENE_FIX:2026-08-23 UAF-1] Business inserts refused after the
	 * workerReady() freeze because GENE_G(cache) was full (raise
	 * gene.cache_reserve if this keeps growing). */
	add_assoc_long(return_value, "cache_insert_refused", (zend_long)GENE_G(cache_insert_refused));
}
/* }}} */

/*
 * {{{ public static Gene\Monitor::reset(): bool
 * [GENE_FEATURE:2026-08-07] Zero all cumulative telemetry counters. Config
 * and threshold values (co_contexts_max, ctx_pool_max, slow_query_ms,
 * cache_easy_ttl) are deliberately NOT touched. Watermarks are reset to 0 and
 * re-derive from live table sizes on the next update. Intended use: metric
 * scrapers that prefer delta-over-interval over monotonic counters.
 * Note: with multiple Swoole workers each worker resets only its own
 * process-local counters.
 */
PHP_METHOD(gene_monitor, reset) {
	GENE_G(request_count) = 0;
	GENE_G(request_error_count) = 0;
	GENE_G(redis_pool_cas_abandoned) = 0;
	GENE_G(db_pool_cas_abandoned) = 0;
	GENE_G(db_pool_get_timeout) = 0;
	GENE_G(memory_cache_hit) = 0;
	GENE_G(memory_cache_miss) = 0;
	GENE_G(db_slow_query_count) = 0;
	GENE_G(swoole_auto_cleanup_defers) = 0;
	GENE_G(swoole_auto_cleanup_reclaimed) = 0;
	GENE_G(co_contexts_watermark) = 0;
	GENE_G(co_contexts_sweep_count) = 0;
	GENE_G(co_contexts_sweep_scanned) = 0;
	GENE_G(co_contexts_sweep_us) = 0;
	GENE_G(co_contexts_sweep_skipped) = 0;
	GENE_G(ctx_pool_hit) = 0;
	GENE_G(ctx_pool_miss) = 0;
	GENE_G(closure_src_cache_flushes) = 0;
	GENE_G(cache_easy_expired) = 0;
	GENE_G(cache_insert_refused) = 0;
	RETURN_TRUE;
}
/* }}} */

/* {{{ gene_monitor_prom_counter / _labeled — append one Prometheus line. */
static void gene_monitor_prom_counter(smart_str *buf, const char *name, const char *help, zend_long value) {
	smart_str_appends(buf, "# HELP ");
	smart_str_appends(buf, name);
	smart_str_appendc(buf, ' ');
	smart_str_appends(buf, help);
	smart_str_appends(buf, "\n# TYPE ");
	smart_str_appends(buf, name);
	smart_str_appends(buf, " counter\n");
	smart_str_appends(buf, name);
	smart_str_appendc(buf, ' ');
	smart_str_append_long(buf, value);
	smart_str_appendc(buf, '\n');
}

/* Escape a Prometheus label value (\ " and newline). */
static void gene_monitor_prom_label_escape(smart_str *buf, const char *s, size_t len) {
	size_t i;
	for (i = 0; i < len; i++) {
		char c = s[i];
		if (c == '\\' || c == '"') {
			smart_str_appendc(buf, '\\');
			smart_str_appendc(buf, c);
		} else if (c == '\n') {
			smart_str_appends(buf, "\\n");
		} else {
			smart_str_appendc(buf, c);
		}
	}
}

static void gene_monitor_prom_gauge_labeled(smart_str *buf, const char *name, const char *label_val, size_t label_len, zend_long value) {
	smart_str_appends(buf, "# TYPE ");
	smart_str_appends(buf, name);
	smart_str_appends(buf, " gauge\n");
	smart_str_appends(buf, name);
	smart_str_appends(buf, "{pool=\"");
	gene_monitor_prom_label_escape(buf, label_val, label_len);
	smart_str_appends(buf, "\"} ");
	smart_str_append_long(buf, value);
	smart_str_appendc(buf, '\n');
}

/* Emit one pool partition (db_pools / redis_pools) as labeled gauges. */
static void gene_monitor_prom_pools(smart_str *buf, zval *pools, const char *metric_prefix) {
	static const char *fields[] = { "total", "idle", "using", "overflow", "min", "max", "closed" };
	zend_string *pname;
	zval *stats;
	if (!pools || Z_TYPE_P(pools) != IS_ARRAY) {
		return;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(pools), pname, stats) {
		size_t i;
		if (!pname || Z_TYPE_P(stats) != IS_ARRAY) {
			continue;
		}
		for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
			zval *v = zend_hash_str_find(Z_ARRVAL_P(stats), fields[i], strlen(fields[i]));
			char metric[96];
			if (!v) {
				continue;
			}
			snprintf(metric, sizeof(metric), "%s_%s", metric_prefix, fields[i]);
			gene_monitor_prom_gauge_labeled(buf, metric, ZSTR_VAL(pname), ZSTR_LEN(pname), zval_get_long(v));
		}
	} ZEND_HASH_FOREACH_END();
}
/* }}} */

/*
 * {{{ public static Gene\Monitor::prometheus(): string
 * [GENE_FEATURE:2026-08-07] Prometheus text exposition of the same data
 * stats() returns: flat counters for the process-level telemetry, labeled
 * gauges for named DB/Redis pools. Pure read, zero side effects; scrape via
 * a dedicated HTTP endpoint, e.g.:
 *     $response->end(Gene\Monitor::prometheus());
 */
PHP_METHOD(gene_monitor, prometheus) {
	smart_str buf = {0};
	zval dbp, rdp;

	gene_monitor_prom_counter(&buf, "gene_requests_total", "Total dispatched requests.", (zend_long)GENE_G(request_count));
	gene_monitor_prom_counter(&buf, "gene_request_errors_total", "Requests that finished with a pending exception.", (zend_long)GENE_G(request_error_count));
	gene_monitor_prom_counter(&buf, "gene_memory_cache_hits_total", "Userland Memory::get hits.", (zend_long)GENE_G(memory_cache_hit));
	gene_monitor_prom_counter(&buf, "gene_memory_cache_misses_total", "Userland Memory::get misses.", (zend_long)GENE_G(memory_cache_miss));
	gene_monitor_prom_counter(&buf, "gene_db_pool_get_timeouts_total", "Pool acquisitions that exhausted waitTimeout.", (zend_long)GENE_G(db_pool_get_timeout));
	gene_monitor_prom_counter(&buf, "gene_db_pool_cas_abandoned_total", "DB pool CAS decrement rounds abandoned.", (zend_long)GENE_G(db_pool_cas_abandoned));
	gene_monitor_prom_counter(&buf, "gene_redis_pool_cas_abandoned_total", "Redis pool CAS decrement rounds abandoned.", (zend_long)GENE_G(redis_pool_cas_abandoned));
	gene_monitor_prom_counter(&buf, "gene_db_slow_queries_total", "Queries slower than gene.slow_query_ms.", (zend_long)GENE_G(db_slow_query_count));
	gene_monitor_prom_counter(&buf, "gene_swoole_auto_cleanup_defers_total", "Registered coroutine auto-cleanup defers.", (zend_long)GENE_G(swoole_auto_cleanup_defers));
	gene_monitor_prom_counter(&buf, "gene_swoole_auto_cleanup_reclaimed_total", "Contexts reclaimed by auto-cleanup.", (zend_long)GENE_G(swoole_auto_cleanup_reclaimed));
	gene_monitor_prom_counter(&buf, "gene_co_contexts_sweeps_total", "Coroutine context sweep runs.", (zend_long)GENE_G(co_contexts_sweep_count));
	gene_monitor_prom_counter(&buf, "gene_co_contexts_sweep_skipped_total", "Sweep triggers suppressed by the cooldown.", (zend_long)GENE_G(co_contexts_sweep_skipped));

	array_init(&dbp);
	gene_monitor_collect_pools(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), &dbp);
	gene_monitor_prom_pools(&buf, &dbp, "gene_db_pool");
	zval_ptr_dtor(&dbp);

	array_init(&rdp);
	gene_monitor_collect_pools(gene_redis_pool_ce, ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), &rdp);
	gene_monitor_prom_pools(&buf, &rdp, "gene_redis_pool");
	zval_ptr_dtor(&rdp);

	smart_str_0(&buf);
	if (buf.s) {
		RETURN_STR(buf.s);
	}
	RETURN_EMPTY_STRING();
}
/* }}} */

/*
 * {{{ gene_monitor_methods
 */
const zend_function_entry gene_monitor_methods[] = {
	PHP_ME(gene_monitor, stats, gene_monitor_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	/* [GENE_FEATURE:2026-08-07] Counter reset + Prometheus text export. */
	PHP_ME(gene_monitor, reset, gene_monitor_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_monitor, prometheus, gene_monitor_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(monitor) {
	zend_class_entry gene_monitor;
	GENE_INIT_CLASS_ENTRY(gene_monitor, "Gene_Monitor", "Gene\\Monitor", gene_monitor_methods);
	gene_monitor_ce = zend_register_internal_class(&gene_monitor);
	gene_monitor_ce->ce_flags |= ZEND_ACC_FINAL;
#if PHP_VERSION_ID >= 80200
	gene_monitor_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
