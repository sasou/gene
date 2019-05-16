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


#include "../gene.h"
#include "../cache/memcached.h"
#include "../factory/factory.h"

zend_class_entry * gene_memcached_ce;


ZEND_BEGIN_ARG_INFO_EX(gene_memcached_incr_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memcached_decr_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memcached_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memcached_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, flag)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memcached_call_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

void gene_memcached_construct(zval *object, zval *persistent_id) /*{{{*/
{
    zval retval;
    zval function_name;
    ZVAL_STRING(&function_name, "__construct");
    if (persistent_id) {
    	zval params[] = { *persistent_id };
    	call_user_function(NULL, object, &function_name, &retval, 1, params);
    } else {
        call_user_function(NULL, object, &function_name, &retval, 0, NULL);
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


int gene_memcached_resetServerList(zval *object) /*{{{*/
{
    zval function_name, retval;
    ZVAL_STRING(&function_name, "resetServerList");
    call_user_function(NULL, object, &function_name, &retval, 0, NULL);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&retval);
    return ret;
}/*}}}*/


int gene_memcached_addServers(zval *object, zval *servers) /*{{{*/
{
    zval function_name, retval;
    ZVAL_STRING(&function_name, "addServers");
	zval params[] = { *servers };
    call_user_function(NULL, object, &function_name, &retval, 1, params);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&retval);
    return ret;

}/*}}}*/

int gene_memcached_set(zval *object, zval *key, zval *value, zval *ttl) /*{{{*/
{
    zval function_name, retval;
    ZVAL_STRING(&function_name, "set");
	zval params[] = { *key, *value, *ttl };
    call_user_function(NULL, object, &function_name, &retval, 3, params);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&retval);
    return ret;
}/*}}}*/

int gene_memcached_increment(zval *object, zval *key, zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "increment");
	zval params[] = { *key, *value };
    call_user_function(NULL, object, &function_name, retval, 2, params);
    int ret =  (Z_TYPE_P(retval) == IS_FALSE ) ? 0 : 1;
    zval_ptr_dtor(&function_name);
    return ret;
}/*}}}*/

void gene_memcached_get(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "get");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_memcached_getMulti(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "getMulti");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

int gene_memcached_decrement(zval *object, zval *key, zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "decrement");
	zval params[] = { *key, *value };
    call_user_function(NULL, object, &function_name, retval, 2, params);
    int ret =  (Z_TYPE_P(retval) == IS_FALSE ) ? 0 : 1;
    zval_ptr_dtor(&function_name);
    return ret;
}/*}}}*/

void gene_memcache_get(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "get");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

int gene_memcache_set(zval *object, zval *key, zval *value, zval *flag, zval *ttl) /*{{{*/
{
    zval function_name, retval;
    ZVAL_STRING(&function_name, "set");
	zval params[] = { *key, *value, *flag, *ttl};
    call_user_function(NULL, object, &function_name, &retval, 2, params);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&retval);
    return ret;
}/*}}}*/

int gene_memcached_setOptions(zval *object, zval *options) /*{{{*/
{
    zval function_name, retval;
    ZVAL_STRING(&function_name, "setOptions");
	zval params[] = { *options };
    call_user_function(NULL, object, &function_name, &retval, 1, params);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&retval);
    return ret;

}/*}}}*/



zend_bool initObj (zval * self, zval *config) {
	zval  *servers = NULL, *persistent = NULL, *options = NULL;
	zval serverList, obj_object;

	if (config == NULL) {
		config =  zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_CONFIG), 1, NULL);
	}
	zend_string *c_key = zend_string_init(ZEND_STRL("Memcached"), 0);
	zend_class_entry *obj_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);

	object_init_ex(&obj_object, obj_ptr);

	servers = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("servers"));

	if (servers == NULL || Z_TYPE_P(servers) != IS_ARRAY) {
		 php_error_docref(NULL, E_ERROR, "param servers must a array.");
	}

	persistent = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("persistent"));
	options = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("options"));
    gene_memcached_construct(&obj_object, persistent);
    if (options) {
    	gene_memcached_setOptions(&obj_object, options);
    }
    gene_memcached_getServerList(&obj_object, &serverList);
    if (Z_TYPE(serverList) != IS_ARRAY ||  zend_hash_num_elements(Z_ARRVAL(serverList)) != zend_hash_num_elements(Z_ARRVAL_P(servers))) {
        gene_memcached_resetServerList(&obj_object);
        gene_memcached_addServers(&obj_object, servers);
    }
    zval_ptr_dtor(&serverList);
    zend_update_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_OBJ), &obj_object);
    zval_ptr_dtor(&obj_object);
	return 1;
}

zend_bool initObjWin (zval * self, zval *config) {
	zval  *servers = NULL, *element = NULL;
	zval obj_object;

	if (config == NULL) {
		config =  zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_CONFIG), 1, NULL);
	}
	zend_string *c_key = zend_string_init(ZEND_STRL("Memcache"), 0);
	zend_class_entry *obj_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);

	object_init_ex(&obj_object, obj_ptr);

	servers = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("servers"));

	if (servers == NULL || Z_TYPE_P(servers) != IS_ARRAY) {
		 php_error_docref(NULL, E_ERROR, "param servers must a array.");
	}

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(servers), element)
	{
		zval ret;
		gene_factory_call(&obj_object, "addServer", element, &ret);
		zval_ptr_dtor(&ret);
	}ZEND_HASH_FOREACH_END();

    zend_update_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_OBJ), &obj_object);
    zval_ptr_dtor(&obj_object);
	return 1;
}

void memcached_get(zval *object, zval *key, zval *ret) {
	if (Z_TYPE_P(key) == IS_ARRAY) {
		gene_memcached_getMulti(object, key, ret);
	} else {
		gene_memcached_get(object, key, ret);
	}
}

/*
 * {{{ gene_memcached
 */
PHP_METHOD(gene_memcached, __construct)
{
	zval *config = NULL, *self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"z", &config) == FAILURE)
    {
        return;
    }

    if (Z_TYPE_P(config) == IS_ARRAY) {
    	zend_update_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_CONFIG), config);
		#ifdef PHP_WIN32
			 initObjWin (self, config);
		#else
			 initObj (self, config);
		#endif
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_memcached::get()
 */
PHP_METHOD(gene_memcached, get) {
	zval *self = getThis(), *object = NULL, *key = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &key) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_OBJ), 1, NULL);
	if (object) {
		zval ret;
		#ifdef PHP_WIN32
			gene_memcache_get(object, key, &ret);
		#else
			memcached_get(object, key, &ret);
		#endif
		RETURN_ZVAL(&ret, 0, 0);
	}
	RETURN_NULL();
}

/*
 * {{{ public gene_memcached::set($key)
 */
PHP_METHOD(gene_memcached, set)
{
	zval *self = getThis(),  *object = NULL, *key = NULL, *value = NULL, *ttl = NULL, *flag = NULL, *config = NULL;
	zval tmp_ttl, tmp_flag;
    int ret = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|zz", &key, &value, &ttl, &flag) == FAILURE) {
		return;
	}
	ZVAL_NULL(&tmp_ttl);
	if (ttl == NULL) {
		config =  zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_CONFIG), 1, NULL);
		if ((ttl = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("ttl"))) == NULL) {
			ZVAL_LONG(&tmp_ttl, 0);
			ttl = &tmp_ttl;
		}
	}
	object = zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_OBJ), 1, NULL);
	#ifdef PHP_WIN32
		if (flag == NULL) {
			ZVAL_LONG(&tmp_flag, 0);
			flag = &tmp_flag;
		}
		ret = gene_memcache_set (object, key, value, ttl, flag);
		zval_ptr_dtor(&tmp_flag);
	#else
		ret = gene_memcached_set (object, key, value, ttl);
	#endif
	if (Z_TYPE(tmp_ttl) != IS_NULL) {
		zval_ptr_dtor(&tmp_ttl);
	}

	if (ret) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */


/*
 * {{{ public gene_memcached::__call($codeString)
 */
PHP_METHOD(gene_memcached, __call) {
	zval *self = getThis(), *object = NULL, *params = NULL, ret;
	long methodlen;
	char *method;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "sz", &method, &methodlen, &params) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_OBJ), 1, NULL);
	if (object) {
		gene_factory_call(object, method, params, &ret);
		RETURN_ZVAL(&ret, 0, 0);
	}
	RETURN_NULL();
}

/*
 * {{{ public gene_memcached::incr()
 */
PHP_METHOD(gene_memcached, incr) {
	zval *self = getThis(), *object = NULL, *key = NULL, *value = NULL;
	zval tmp_value, ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|z", &key, &value) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_OBJ), 1, NULL);
	if (object) {
		if (value == NULL) {
			ZVAL_LONG(&tmp_value, 1);
			value = &tmp_value;
		}
		if (gene_memcached_increment(object, key, value, &ret)) {
			RETURN_ZVAL(&ret, 1, 0);
		}
	    zval function_name, retval;
	    ZVAL_STRING(&function_name, "set");
		zval params[] = { *key, *value };
	    call_user_function(NULL, object, &function_name, &retval, 2, params);
	    zval_ptr_dtor(&function_name);
	    if (Z_TYPE(retval) == IS_TRUE) {
	    	zval_ptr_dtor(&retval);
	    	RETURN_LONG(1);
	    }
	    zval_ptr_dtor(&retval);
	    RETURN_FALSE;
	}
	RETURN_NULL();
}

/*
 * {{{ public gene_memcached::decr()
 */
PHP_METHOD(gene_memcached, decr) {
	zval *self = getThis(), *object = NULL, *key = NULL, *value = NULL;
	zval tmp_value, ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|z", &key, &value) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_memcached_ce, self, ZEND_STRL(GENE_MEM_OBJ), 1, NULL);
	if (object) {
		if (value == NULL) {
			ZVAL_LONG(&tmp_value, 1);
			value = &tmp_value;
		}
		if (gene_memcached_decrement(object, key, value, &ret)) {
			RETURN_ZVAL(&ret, 1, 0);
		}
	    zval function_name, retval;
	    ZVAL_STRING(&function_name, "set");
		zval params[] = { *key, *value };
	    call_user_function(NULL, object, &function_name, &retval, 2, params);
	    zval_ptr_dtor(&function_name);
	    if (Z_TYPE(retval) == IS_TRUE) {
	    	zval_ptr_dtor(&retval);
	    	RETURN_LONG(0);
	    }
	    zval_ptr_dtor(&retval);
	    RETURN_FALSE;
	}
	RETURN_NULL();
}

/*
 * {{{ gene_memcached_methods
 */
zend_function_entry gene_memcached_methods[] = {
		PHP_ME(gene_memcached, decr, gene_memcached_decr_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_memcached, incr, gene_memcached_incr_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_memcached, get, gene_memcached_get_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_memcached, set, gene_memcached_set_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_memcached, __call, gene_memcached_call_arginfo, ZEND_ACC_PUBLIC)
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
    GENE_INIT_CLASS_ENTRY(gene_memcached, "Gene_Cache_Memcached", "Gene\\Cache\\Memcached", gene_memcached_methods);
    gene_memcached_ce = zend_register_internal_class(&gene_memcached TSRMLS_CC);

    zend_declare_property_null(gene_memcached_ce, ZEND_STRL(GENE_MEM_CONFIG), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(gene_memcached_ce, ZEND_STRL(GENE_MEM_OBJ), ZEND_ACC_PUBLIC TSRMLS_CC);
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
