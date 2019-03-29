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
  | Author: Sasou  <admin@caophp.com>                                    |
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


#include "../php_gene.h"
#include "../cache/redis.h"

zend_class_entry * gene_redis_ce;

/*
 * {{{ gene_redis
 */
PHP_METHOD(gene_redis, __construct)
{
	long debug = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &debug) == FAILURE)
    {
        RETURN_NULL();
    }
}
/* }}} */


/*
 * {{{ public gene_redis::redis($key)
 */
PHP_METHOD(gene_redis, redis)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ gene_redis_methods
 */
zend_function_entry gene_redis_methods[] = {
		PHP_ME(gene_redis, redis, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(redis)
{
    zend_class_entry gene_redis;
    GENE_INIT_CLASS_ENTRY(gene_redis, "Gene_Redis", "Gene\\Redis", gene_redis_methods);
    gene_redis_ce = zend_register_internal_class(&gene_redis TSRMLS_CC);

	//debug
    //zend_declare_property_null(gene_application_ce, GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), ZEND_ACC_PUBLIC TSRMLS_CC);
    //
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
