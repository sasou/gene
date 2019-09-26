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
#include "../common/common.h"
#include "../cache/redis.h"
#include "../factory/factory.h"

zend_class_entry * gene_redis_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_redis_call_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

int gene_redis_connect(zval *object, zval *host, zval *port, zval *timeout) /*{{{*/
{
    zval function_name,retval;
    ZVAL_STRING(&function_name, "connect");
	zval params[] = { *host, *port, *timeout };
    call_user_function(NULL, object, &function_name, &retval, 3, params);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&function_name);
    return ret;
}/*}}}*/


int gene_redis_pconnect(zval *object, zval *host, zval *port, zval *timeout) /*{{{*/
{
    zval function_name,retval;
    ZVAL_STRING(&function_name, "pconnect");
	zval params[] = { *host, *port, *timeout };
    call_user_function(NULL, object, &function_name, &retval, 3, params);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&function_name);
    return ret;
}/*}}}*/

int gene_redis_setOption(zval *object, zval *key, zval *value) /*{{{*/
{
    zval function_name,retval;
    ZVAL_STRING(&function_name, "setOption");
	zval params[] = { *key, *value };
    call_user_function(NULL, object, &function_name, &retval, 2, params);
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&function_name);
    return ret;
}/*}}}*/

void gene_redis_set(zval *object, zval *key, zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "set");
	zval params[] = { *key, *value };
    call_user_function(NULL, object, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_redis_setEx(zval *object, zval *key, zval *ttl, zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "setEx");
	zval params[] = { *key, *ttl, *value };
    call_user_function(NULL, object, &function_name, retval, 3, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_redis_get(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "get");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_redis_mGet(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "mGet");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

zend_bool checkError (zend_object *ex) {
	zval *msg;
	zend_class_entry *ce;
	zval zv, rv;
	int i;
	const char *pdoErrorStr[1] = { "read error on connection" };

	ZVAL_OBJ(&zv, ex);
	ce = Z_OBJCE(zv);

	msg = zend_read_property(ce, &zv, ZEND_STRL("message"), 0, &rv);
	for (i = 0; i < 1; i++) {
		if (strstr(Z_STRVAL_P(msg), pdoErrorStr[i]) != NULL) {
			return 1;
		}
	}
	return 0;
}

zend_bool initRObj (zval * self, zval *config) {
	zval  *host = NULL, *port = NULL, *timeout = NULL, *persistent = NULL, *options = NULL, *serializer = NULL;
	zval   obj_object;

	if (config == NULL) {
		config =  zend_read_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_CONFIG), 1, NULL);
	}
	zend_string *c_key = zend_string_init(ZEND_STRL("Redis"), 0);
	zend_class_entry *obj_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);

	object_init_ex(&obj_object, obj_ptr);

	host = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("host"));
	port = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("port"));
	timeout = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("timeout"));

	if (host == NULL || port == NULL || timeout == NULL) {
		 php_error_docref(NULL, E_ERROR, "param error.");
	}

	serializer = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("serializer"));
	if (serializer && Z_TYPE_P(serializer) == IS_LONG) {
		zend_update_property_long(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_SERIALIZE), Z_LVAL_P(serializer));
	}
	persistent = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("persistent"));
	options = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("options"));

    if (persistent) {
    	gene_redis_pconnect(&obj_object, host, port, timeout);
    } else {
    	gene_redis_connect(&obj_object, host, port, timeout);
    }

    if (options && Z_TYPE_P(options) == IS_ARRAY) {
    	zend_string *key;
    	zval *element;
    	zend_long id;
    	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(options), id, key, element)
    	{
    		zval key_v;
    		ZVAL_LONG(&key_v, id);
    		gene_redis_setOption(&obj_object, &key_v, element);
    		zval_ptr_dtor(&key_v);
    	}ZEND_HASH_FOREACH_END();
    }
    zend_update_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_OBJ), &obj_object);
    zval_ptr_dtor(&obj_object);
	return 1;
}

void redis_get(zval *object, zval *key, zval *ret) {
	if (Z_TYPE_P(key) == IS_ARRAY) {
		gene_redis_mGet(object, key, ret);
	} else {
		gene_redis_get(object, key, ret);
	}
}

void redis_set(zval *object, zval *key, zval *ttl, zval *value, zval *ret) {
	if (ttl && Z_LVAL_P(ttl) > 0) {
		gene_redis_setEx(object, key, ttl, value, ret);
	} else {
		gene_redis_set(object, key, value, ret);
	}
}

/*
 * {{{ gene_redis
 */
PHP_METHOD(gene_redis, __construct)
{
	zval *config = NULL, *self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"z", &config) == FAILURE)
    {
        return;
    }

    if (Z_TYPE_P(config) == IS_ARRAY) {
    	zend_update_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_CONFIG), config);
    	initRObj (self, config);
    	if (EG(exception)) {
    		if (checkError(EG(exception))) {
    			EG(exception) = NULL;
    			initRObj(self, config);
    		}
    	}
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_redis::get()
 */
PHP_METHOD(gene_redis, get) {
	zval *self = getThis(), *object = NULL, *serializer_handler = NULL, *key = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &key) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	if (object) {
		serializer_handler = zend_read_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_SERIALIZE), 1, NULL);
		zval ret;
		redis_get(object, key, &ret);
    	if (EG(exception)) {
    		if (checkError(EG(exception))) {
    			EG(exception) = NULL;
    			initRObj (self, NULL);
    			redis_get(object, key, &ret);
    		}
    	}
		if(serializer_handler) {
			if (Z_TYPE_P(key) == IS_ARRAY) {
				zval *element,*value;
				zval arr;
				array_init(&arr);
				zend_long i = 0;
				zval tmp_arr[10];
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(key), element)
		    	{
		    		value = zend_hash_index_find(Z_ARRVAL(ret), i);
		    		if (Z_TYPE_P(value) != IS_FALSE) {
			    		if (Z_TYPE_P(element) == IS_STRING) {
							if (unserialize(value, &tmp_arr[i], serializer_handler)) {
								add_assoc_zval_ex(&arr, Z_STRVAL_P(element), Z_STRLEN_P(element), &tmp_arr[i]);
							} else {
								Z_TRY_ADDREF_P(value);
								add_assoc_zval_ex(&arr, Z_STRVAL_P(element), Z_STRLEN_P(element), value);
							}
			    		}
		    		}
		    		i=i+1;
		    	}ZEND_HASH_FOREACH_END();
		    	zval_ptr_dtor(&ret);
		    	RETURN_ZVAL(&arr, 1, 1);
			} else {
				zval arr;
				if (unserialize(&ret, &arr, serializer_handler)) {
					zval_ptr_dtor(&ret);
					RETURN_ZVAL(&arr, 1, 1);
				}
				RETURN_ZVAL(&ret, 1, 1);
			}
		}
		RETURN_ZVAL(&ret, 1, 1);
	}
	RETURN_NULL();
}

/*
 * {{{ public gene_redis::set()
 */
PHP_METHOD(gene_redis, set) {
	zval *self = getThis(),  *object = NULL, *serializer_handler = NULL, *key = NULL, *value = NULL, *ttl = NULL, *config = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &key, &value, &ttl) == FAILURE) {
		return;
	}

	if (ttl == NULL) {
		config =  zend_read_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_CONFIG), 1, NULL);
		ttl = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("ttl"));
	}
	object = zend_read_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	if (object) {
		serializer_handler = zend_read_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_SERIALIZE), 1, NULL);
		if (serializer_handler) {
			zval ret_string;
			if (serialize(value, &ret_string, serializer_handler) > 0) {
				redis_set(object, key, ttl, &ret_string, return_value);
				if (EG(exception)) {
					if (checkError(EG(exception))) {
						EG(exception) = NULL;
						initRObj (self, NULL);
						redis_set(object, key, ttl, &ret_string, return_value);
					}
				}
				zval_ptr_dtor(&ret_string);
			} else {
				redis_set(object, key, ttl, value, return_value);
			}
			return;
		}
	}
	RETURN_NULL();
}

/*
 * {{{ public gene_redis::__call($codeString)
 */
PHP_METHOD(gene_redis, __call) {
	zval *self = getThis(), *object = NULL, *params = NULL, ret;
	long methodlen;
	char *method;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &method, &methodlen, &params) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	if (object) {
		gene_factory_call(object, method, params, &ret);
    	if (EG(exception)) {
    		if (checkError(EG(exception))) {
    			EG(exception) = NULL;
    			initRObj (self, NULL);
    			gene_factory_call(object, method, params, &ret);
    		}
    	}
		RETURN_ZVAL(&ret, 1, 1);
	}
	RETURN_NULL();
}

/*
 * {{{ gene_redis_methods
 */
zend_function_entry gene_redis_methods[] = {
		PHP_ME(gene_redis, set, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, get, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, __call, gene_redis_call_arginfo, ZEND_ACC_PUBLIC)
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
    GENE_INIT_CLASS_ENTRY(gene_redis, "Gene_Cache_Redis", "Gene\\Cache\\Redis", gene_redis_methods);
    gene_redis_ce = zend_register_internal_class(&gene_redis TSRMLS_CC);

    zend_declare_property_null(gene_redis_ce, ZEND_STRL(GENE_REDIS_CONFIG), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(gene_redis_ce, ZEND_STRL(GENE_REDIS_OBJ), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_long(gene_redis_ce, ZEND_STRL(GENE_REDIS_SERIALIZE), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
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
