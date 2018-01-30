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
  | Author: Sasou  <admin@php-gene.com> web:www.php-gene.com             |
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
#include "Zend/zend_exceptions.h"
#include "Zend/zend_alloc.h"
#include "Zend/zend_interfaces.h"

#include "php_gene.h"
#include "gene_benchmark.h"

zend_class_entry * gene_benchmark_ce;

PHPAPI int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info);

struct timeval start, end;
long memory_start = 0, memory_end = 0;

/*
 * {{{ void markStart()
 */
void markStart() {
	zval func, ret;
	gettimeofday( &start, NULL );

	ZVAL_STRING(&func, "memory_get_peak_usage");
	ZVAL_NULL(&ret);
	call_user_function(EG(function_table), NULL, &func, &ret, 0, NULL);
	if (Z_TYPE(ret) == IS_LONG) {
		memory_start = Z_LVAL(ret);
	}
	zval_ptr_dtor(&func);
	zval_ptr_dtor(&ret);
}
/* }}} */

/*
 * {{{ void markEnd()
 */
void markEnd() {
	zval func, ret;
    gettimeofday( &end, NULL );

	ZVAL_STRING(&func, "memory_get_peak_usage");
	ZVAL_NULL(&ret);
	call_user_function(EG(function_table), NULL, &func, &ret, 0, NULL);
	if (Z_TYPE(ret) == IS_LONG) {
		memory_end = Z_LVAL(ret);
	}
	zval_ptr_dtor(&func);
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

/*
 * {{{ public gene_benchmark::start()
 */
PHP_METHOD(gene_benchmark, start)
{
	markStart();
}
/* }}} */


/*
 * {{{ public gene_benchmark::end()
 */
PHP_METHOD(gene_benchmark, end)
{
	markEnd();
}
/* }}} */

/*
 * {{{ public gene_benchmark::time($type)
 */
PHP_METHOD(gene_benchmark, time)
{
	double time;
	char *ret = NULL;
	zend_bool type = FALSE;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &type) == FAILURE) {
		return;
	}

	time = difftimeval(&start, &end);

	if (type) {
		spprintf(&ret, 0, "%f", time);
	} else {
		spprintf(&ret, 0, "%.3f", time);
	}

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
	double memory;
	char *ret = NULL;
	zend_bool type = FALSE;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &type) == FAILURE) {
		return;
	}

    if (!memory_start || !memory_end) {
    	RETURN_NULL();
    }
	memory = memory_end - memory_start;

	if (type) {
		spprintf(&ret, 0, "%.3f", memory/1024);
	} else {
		spprintf(&ret, 0, "%.3f", memory/1048576);
	}

	ZVAL_STRING(return_value, ret);
	efree(ret);
	return;
}
/* }}} */

/*
 * {{{ gene_benchmark_methods
 */
zend_function_entry gene_benchmark_methods[] = {
		PHP_ME(gene_benchmark, start, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, end, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, time, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_benchmark, memory, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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

	return SUCCESS;
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
