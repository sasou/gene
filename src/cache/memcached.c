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

#include "php.h"#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"


#include "../gene.h"
#include "../cache/memcached.h"

zend_class_entry * gene_memcached_ce;


void gene_memcached_construct(zval *object, zval *persistent_id) /*{{{*/
{
    zval retval;
    zval function_name;
    ZVAL_STRING(&function_name, "__construct");
    if (persistent_id) {
    	uint32_t param_count = 0;
    	zval params[] = { persistent_id };
    	call_user_function(NULL, object, &function_name, retval, param_count, params);
    } else {
           uint32_t param_count = 0;
           zval *params = NULL;
           call_user_function(NULL, object, &function_name, retval, param_count, params);
    }
    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_memcached_getServerList(zval *object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "getServerList");
    call_user_function(NULL, object, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_memcached_resetServerList(zval *object) /*{{{*/
{
    zval function_name, retval;
    ZVAL_STRING(&function_name, "resetServerList");
    call_user_function(NULL, object, &function_name, &retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_memcached_addServers(zval *object, zval *servers) /*{{{*/
{
    zval function_name, retval;
    ZVAL_STRING(&function_name, "addServers");
	zval params[] = { servers };
    call_user_function(NULL, object, &function_name, &retval, 1, params);
    zval_ptr_dtor(&function_name);

}/*}}}*/

/*
 * {{{ gene_memcached
 */
PHP_METHOD(gene_memcached, __construct)
{
	long debug = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &debug) == FAILURE)
    {
        RETURN_NULL();
    }
}
/* }}} */


/*
 * {{{ public gene_memcached::memcached($key)
 */
PHP_METHOD(gene_memcached, memcached)
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
 * {{{ gene_memcached_methods
 */
zend_function_entry gene_memcached_methods[] = {
		PHP_ME(gene_memcached, memcached, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_memcached, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(memcached)
{
    zend_class_entry gene_memcached;
    GENE_INIT_CLASS_ENTRY(gene_memcached, "Gene_Memcached", "Gene\\Memcached", gene_memcached_methods);
    gene_memcached_ce = zend_register_internal_class(&gene_memcached TSRMLS_CC);

	//debug
    //zend_declare_property_null(gene_application_ce, GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), ZEND_ACC_PUBLIC TSRMLS_CC);
    //
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
