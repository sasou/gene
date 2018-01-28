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


#include "php_gene.h"
#include "gene_benchmark.h"

zend_class_entry * gene_benchmark_ce;

/*
 * {{{ gene_benchmark
 */
PHP_METHOD(gene_benchmark, __construct)
{
	long debug = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &debug) == FAILURE)
    {
        RETURN_NULL();
    }
    php_printf(" time:%d ", clock());
}
/* }}} */


/*
 * {{{ public gene_benchmark::test($key)
 */
PHP_METHOD(gene_benchmark, test)
{
	zval *self = getThis();
	zend_string *script = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|S", &script) == FAILURE) {
		return;
	}
	php_printf(" key:%s ",ZSTR_VAL(script));
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ gene_benchmark_methods
 */
zend_function_entry gene_benchmark_methods[] = {
		PHP_ME(gene_benchmark, test, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_benchmark, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
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

	//static
	zend_declare_property_null(gene_benchmark_ce, GENE_BENCHMARK_T, strlen(GENE_BENCHMARK_T), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_null(gene_benchmark_ce, GENE_BENCHMARK_M, strlen(GENE_BENCHMARK_M), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC TSRMLS_CC);

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
