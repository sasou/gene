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
#include "zend_smart_str.h" /* for smart_str */

#include "../gene.h"
#include "../cache/cache.h"
#include "../common/common.h"
#include "../factory/factory.h"
#include "../di/di.h"

zend_class_entry * gene_cache_ce;


void gene_cache_get(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "get");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_cache_set(zval *object, zval *key, zval *value, zval *ttl, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "set");
	zval params[] = { *key,*value,*ttl };
    call_user_function(NULL, object, &function_name, retval, 3, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_cache_key(zval *sign, int type, zval *retval) /*{{{*/
{
    zval args,serialize,md5;
    int size = ZEND_CALL_NUM_ARGS(EG(current_execute_data));
    array_init_size(&args, size);
    zend_copy_parameters_array(3, &args);
    gene_serialize(&args, &serialize);
    zval_ptr_dtor(&args);
    gene_md5(&serialize, &md5);
    zval_ptr_dtor(&serialize);
    smart_str key = {0};
    smart_str_appendl(&key, Z_STRVAL_P(sign), Z_STRLEN_P(sign));
    if (type) {
    	smart_str_appendl(&key, GENE_CACHE_TMP, sizeof(GENE_CACHE_TMP) -1);
    }
    if (Z_TYPE(md5) == IS_STRING) {
    	smart_str_appendl(&key, Z_STRVAL(md5), Z_STRLEN(md5));
    }
    zval_ptr_dtor(&md5);
    smart_str_0(&key);
    ZVAL_STR(retval, key.s);
    smart_str_free(&key);
}/*}}}*/


void gene_cache_call(zval *object, zval *args, zval *retval) /*{{{*/
{
	zval *class = NULL, *method = NULL, *element = NULL;
	zval tmp_class;
	if (Z_TYPE_P(object) != IS_ARRAY || Z_TYPE_P(args) != IS_ARRAY) {
		return;
	}
	class = zend_hash_index_find(Z_ARRVAL_P(object), 0);
	method = zend_hash_index_find(Z_ARRVAL_P(object), 1);
	ZVAL_NULL(&tmp_class);
	if (Z_TYPE_P(class) == IS_STRING ) {
		gene_factory_load_class(Z_STRVAL_P(class), Z_STRLEN_P(class), &tmp_class);
		class = &tmp_class;
	}

	zval params[10];
	int num = 0;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), element)
	{
		params[num] = *element;
		num++;
	}ZEND_HASH_FOREACH_END();
    call_user_function(NULL, class, method, retval, num, params);
    zval_ptr_dtor(&tmp_class);
}/*}}}*/

/*
 * {{{ gene_cache
 */
PHP_METHOD(gene_cache, __construct)
{
	zval *config = NULL, *self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"z", &config) == FAILURE)
    {
        return;
    }

    if (Z_TYPE_P(config) == IS_ARRAY) {
    	zend_update_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), config);
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_cache::cached($key)
 */
PHP_METHOD(gene_cache, cached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));

	hook = gene_di_get_easy(Z_STR_P(hookName));

	zval key, ret, data;
	gene_cache_key(sign, 1, &key);
	gene_cache_get(hook, &key, &ret);
	if (Z_TYPE(ret) == IS_FALSE) {
		zval tmp_ttl, set_ret;
		gene_cache_call(obj, args, &data);
		ZVAL_LONG(&tmp_ttl, 0);
		if (ttl == NULL) {
			ttl = &tmp_ttl;
		}
		gene_cache_set(hook, &key, &data, ttl, &set_ret);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&set_ret);
		zval_ptr_dtor(&tmp_ttl);
		zval_ptr_dtor(&ret);
		RETURN_ZVAL(&data, 0, 0);
	}
	zval_ptr_dtor(&key);
	RETURN_ZVAL(&ret, 0, 0);
}
/* }}} */

/*
 * {{{ public gene_cache::unsetCached($key)
 */
PHP_METHOD(gene_cache, unsetCached)
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
 * {{{ public gene_cache::cachedVersion($key)
 */
PHP_METHOD(gene_cache, cachedVersion)
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
 * {{{ public gene_cache::getVersion($key)
 */
PHP_METHOD(gene_cache, getVersion)
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
 * {{{ public gene_cache::updateVersion($key)
 */
PHP_METHOD(gene_cache, updateVersion)
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
 * {{{ gene_cache_methods
 */
zend_function_entry gene_cache_methods[] = {
		PHP_ME(gene_cache, cached, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, unsetCached, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, cachedVersion, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, getVersion, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, updateVersion, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(cache)
{
    zend_class_entry gene_cache;
    GENE_INIT_CLASS_ENTRY(gene_cache, "Gene_Cache_Cache", "Gene\\Cache\\Cache", gene_cache_methods);
    gene_cache_ce = zend_register_internal_class(&gene_cache TSRMLS_CC);

    zend_declare_property_null(gene_cache_ce, ZEND_STRL(GENE_CACHE_CONFIG), ZEND_ACC_PUBLIC TSRMLS_CC);
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
