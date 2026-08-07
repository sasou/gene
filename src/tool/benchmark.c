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
#include "zend_exceptions.h"
#include "Zend/zend_alloc.h"
#include "Zend/zend_interfaces.h"

#include "../gene.h"
#include "../tool/benchmark.h"

zend_class_entry * gene_benchmark_ce;

#ifdef PHP_WIN32
PHPAPI int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info);
#endif

/* Benchmark state moved to gene_request_context (gene.h) for coroutine safety.
 * Accessed via GENE_REQ(bench_start), GENE_REQ(bench_end), etc. */

ZEND_BEGIN_ARG_INFO_EX(gene_benchmark_start, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_benchmark_end, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_benchmark_time, 0, 0, 0)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_benchmark_memory, 0, 0, 0)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

/* [GENE_FEATURE:2026-08-07] mark($name): record a named high-res timestamp.
 * lap($name): return ms elapsed since the last mark($name) and reset it. */
ZEND_BEGIN_ARG_INFO_EX(gene_benchmark_mark, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_benchmark_lap, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()


/*
 * {{{ void markStart(timeval *start, zend_long *memory_start)
 */
void markStart(struct timeval *start, zend_long *memory_start) {
	zval ret;
	gettimeofday( start, NULL );

	/* [GENE_AUDIT:2026-07-03 T1#4] Use GENE_CG_FN_LOOKUP for ZTS-safe
	 * function pointer caching (non-ZTS caches in static, ZTS per-call). */
#ifndef ZTS
	static zend_function *mem_fn = NULL;
#else
	zend_function *mem_fn = NULL;
#endif
	mem_fn = GENE_CG_FN_LOOKUP(mem_fn, "memory_get_peak_usage");
	ZVAL_UNDEF(&ret);
	if (EXPECTED(mem_fn)) {
		zend_call_known_function(mem_fn, NULL, NULL, &ret, 0, NULL, NULL);
	}
	if (Z_TYPE(ret) == IS_LONG) {
		*memory_start = Z_LVAL(ret);
	}
	zval_ptr_dtor(&ret);
}
/* }}} */

/*
 * {{{ void markEnd()
 */
void markEnd(struct timeval *end, zend_long *memory_end) {
	zval ret;
    gettimeofday( end, NULL );

	/* [GENE_AUDIT:2026-07-03 T1#4] See markStart. */
#ifndef ZTS
	static zend_function *mem_fn = NULL;
#else
	zend_function *mem_fn = NULL;
#endif
	mem_fn = GENE_CG_FN_LOOKUP(mem_fn, "memory_get_peak_usage");
	ZVAL_UNDEF(&ret);
	if (EXPECTED(mem_fn)) {
		zend_call_known_function(mem_fn, NULL, NULL, &ret, 0, NULL, NULL);
	}
	if (Z_TYPE(ret) == IS_LONG) {
		*memory_end = Z_LVAL(ret);
	}
	zval_ptr_dtor(&ret);
}
/* }}} */

/*
 * {{{ double difftimeval(const struct timeval *start, const struct timeval *end)
 */
double difftimeval(const struct timeval *start, const struct timeval *end)
{
	double timeuse;
	timeuse= 1000000 * (end->tv_sec-start->tv_sec) + end->tv_usec - start->tv_usec;
	timeuse /= 1000000;
	return(timeuse);
}

void getBenchTime(struct timeval *start, struct timeval *end, char **ret, bool type) {
	double time;
	char time_buf[32];
	time = difftimeval(start, end);

	if (type) {
		snprintf(time_buf, sizeof(time_buf), "%f", time);
	} else {
		snprintf(time_buf, sizeof(time_buf), "%.3f", time);
	}
	*ret = estrdup(time_buf);
}

void getBenchMemory(zend_long *memory_start, zend_long *memory_end, char **ret, bool type) {
	zend_long memory;
	char mem_buf[32];
	memory = *memory_end - *memory_start;

	if (type) {
		snprintf(mem_buf, sizeof(mem_buf), "%.3f", (double)memory / 1024.0);
	} else {
		snprintf(mem_buf, sizeof(mem_buf), "%.3f", (double)memory / 1048576.0);
	}
	*ret = estrdup(mem_buf);
}

/*
 * {{{ public gene_benchmark::start()
 */
PHP_METHOD(gene_benchmark, start)
{
	gene_request_context *ctx = gene_request_ctx();
	markStart(&ctx->bench_start, &ctx->bench_memory_start);
}
/* }}} */


/*
 * {{{ public gene_benchmark::end()
 */
PHP_METHOD(gene_benchmark, end)
{
	gene_request_context *ctx = gene_request_ctx();
	markEnd(&ctx->bench_end, &ctx->bench_memory_end);
}
/* }}} */

/*
 * {{{ public gene_benchmark::time($type)
 */
PHP_METHOD(gene_benchmark, time)
{
	char *ret = NULL;
	bool type = 0;
	gene_request_context *ctx;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &type) == FAILURE) {
		return;
	}

	ctx = gene_request_ctx();
	getBenchTime(&ctx->bench_start, &ctx->bench_end, &ret, type);

	ZVAL_STRING(return_value, ret);
	efree(ret);
	return;
}
/* }}} */

/*
 * {{{ public gene_benchmark::memory($type)
 */
PHP_METHOD(gene_benchmark, memory)
{
	char *ret = NULL;
	bool type = 0;
	gene_request_context *ctx;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &type) == FAILURE) {
		return;
	}

	ctx = gene_request_ctx();
    if (!ctx->bench_memory_start || !ctx->bench_memory_end) {
    	RETURN_NULL();
    }
    getBenchMemory(&ctx->bench_memory_start, &ctx->bench_memory_end, &ret, type);

	ZVAL_STRING(return_value, ret);
	efree(ret);
	return;
}
/* }}} */

/* [GENE_FEATURE:2026-08-07] mark($name): record a named high-resolution
 * timestamp (nanoseconds) on the request context. Subsequent lap($name)
 * calls measure elapsed time against this mark. Returns true. */
PHP_METHOD(gene_benchmark, mark)
{
	zend_string *name = NULL;
	gene_request_context *ctx;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(name) == 0) {
		php_error_docref(NULL, E_WARNING, "Benchmark::mark name must not be empty");
		RETURN_FALSE;
	}

	ctx = gene_request_ctx();
	if (!ctx) RETURN_FALSE;

	if (Z_TYPE(ctx->bench_marks) != IS_ARRAY) {
		array_init(&ctx->bench_marks);
	}

	/* [GENE_NOTE:2026-08-07-5 N10] gene_hrtime() is uint64 nanoseconds; on
	 * 32-bit platforms zend_long is 32-bit and wraps after ~4.3s, so lap()
	 * would report garbage there. Gene targets 64-bit builds (same as the
	 * Swoole ecosystem); do not rely on mark/lap on 32-bit PHP. */
	zval ts;
	ZVAL_LONG(&ts, (zend_long)gene_hrtime());
	add_assoc_zval_ex(&ctx->bench_marks, ZSTR_VAL(name), ZSTR_LEN(name), &ts);

	RETURN_TRUE;
}
/* }}} */

/* [GENE_FEATURE:2026-08-07] lap($name): return milliseconds (float) elapsed
 * since the last mark($name), then reset the mark to now. Returns false if
 * no prior mark exists for $name. */
PHP_METHOD(gene_benchmark, lap)
{
	zend_string *name = NULL;
	gene_request_context *ctx;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(name) == 0) {
		php_error_docref(NULL, E_WARNING, "Benchmark::lap name must not be empty");
		RETURN_FALSE;
	}

	ctx = gene_request_ctx();
	if (!ctx || Z_TYPE(ctx->bench_marks) != IS_ARRAY) {
		RETURN_FALSE;
	}

	zval *prev = zend_hash_find(Z_ARRVAL(ctx->bench_marks), name);
	if (!prev || Z_TYPE_P(prev) != IS_LONG) {
		RETURN_FALSE;
	}

	uint64_t now = gene_hrtime();
	uint64_t prev_ns = (uint64_t)Z_LVAL_P(prev);
	double elapsed_ms = (double)(now - prev_ns) / 1000000.0;

	/* Reset the mark to now for the next lap. */
	zval ts;
	ZVAL_LONG(&ts, (zend_long)now);
	add_assoc_zval_ex(&ctx->bench_marks, ZSTR_VAL(name), ZSTR_LEN(name), &ts);

	RETURN_DOUBLE(elapsed_ms);
}
/* }}} */

/*
 * {{{ gene_benchmark_methods
 */
const zend_function_entry gene_benchmark_methods[] = {
		PHP_ME(gene_benchmark, start, gene_benchmark_start, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, end, gene_benchmark_end, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, time, gene_benchmark_time, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, memory, gene_benchmark_memory, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, mark, gene_benchmark_mark, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, lap, gene_benchmark_lap, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(benchmark)
{
    zend_class_entry gene_benchmark;
    GENE_INIT_CLASS_ENTRY(gene_benchmark, "Gene_Benchmark", "Gene\\Benchmark", gene_benchmark_methods);
    gene_benchmark_ce = zend_register_internal_class_ex(&gene_benchmark, NULL);
    gene_benchmark_ce->ce_flags |= ZEND_ACC_FINAL;
#if PHP_VERSION_ID >= 80200
    gene_benchmark_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
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
