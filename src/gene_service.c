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
#include "Zend/zend_interfaces.h"
#include "zend_exceptions.h"


#include "php_gene.h"
#include "gene_model.h"
#include "gene_service.h"
#include "gene_reg.h"
#include "gene_memory.h"
#include "gene_config.h"
#include "gene_response.h"


zend_class_entry * gene_service_ce;


ZEND_BEGIN_ARG_INFO_EX(gene_service_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_service_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()


zval *gene_service_instance(zval *obj) {
	zval *ppzval = NULL, *reg, *entrys;
	zval ret;
	zend_call_method_with_0_params(NULL, NULL, NULL, "get_called_class", &ret);
	reg = gene_reg_instance();
	entrys = zend_read_property(gene_reg_ce, reg, ZEND_STRL(GENE_REG_PROPERTY_REG), 1, NULL);
	if ((ppzval = zend_hash_str_find(Z_ARRVAL_P(entrys), Z_STRVAL(ret), Z_STRLEN(ret))) != NULL) {
		zval_ptr_dtor(&ret);
		return ppzval;
	} else {
		if (gene_factory_load_class(Z_STRVAL(ret), Z_STRLEN(ret), obj)) {
			if (zend_hash_str_update(Z_ARRVAL_P(entrys), Z_STRVAL(ret), Z_STRLEN(ret), obj) != NULL) {
				Z_TRY_ADDREF_P(obj);
				zval prop;
				array_init(&prop);
				zend_update_property(gene_service_ce, obj, ZEND_STRL(GENE_SERVICE_ATTR), &prop);
				zval_ptr_dtor(&prop);
			}
			zval_ptr_dtor(&ret);
			return obj;
		}
	}
	zval_ptr_dtor(&ret);
	return NULL;
}

/*
 * {{{ gene_service
 */
PHP_METHOD(gene_service, __construct)
{

}
/* }}} */

/*
 * {{{ gene_service
 */
PHP_METHOD(gene_service, __set)
{
	zend_string *name;
	zval *value, *props, obj;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	props = zend_read_property(gene_service_ce, gene_service_instance(&obj), ZEND_STRL(GENE_SERVICE_ATTR), 1, NULL);
	if (zend_hash_update(Z_ARRVAL_P(props), name, value) != NULL) {
		Z_TRY_ADDREF_P(value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */


/*
 * {{{ gene_service
 */
PHP_METHOD(gene_service, __get)
{
	zval *pzval = NULL, *props = NULL, obj, classObject, *class = NULL, *params = NULL, *cache = NULL, *instance = NULL, *reg = NULL, *entrys = NULL;
	zend_string *name = NULL;
	zend_bool type = 0;
	zval local_params;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		props = zend_read_property(gene_service_ce, gene_service_instance(&obj), ZEND_STRL(GENE_SERVICE_ATTR), 1, NULL);
		if (props) {
			pzval = zend_hash_find(Z_ARRVAL_P(props), name);
			if (pzval == NULL) {
				char *router_e = NULL;
				int router_e_len = 0;
				if (GENE_G(app_key)) {
					router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(app_key), GENE_CONFIG_CACHE);
				} else {
					router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(directory), GENE_CONFIG_CACHE);
				}
				cache = gene_memory_get_by_config(router_e, router_e_len, ZSTR_VAL(name) TSRMLS_CC);
				efree(router_e);
				if (cache && Z_TYPE_P(cache) == IS_ARRAY) {
			    	if ((class = zend_hash_str_find(cache->value.arr, "class", 5)) == NULL) {
			    		 php_error_docref(NULL, E_ERROR, "Factory need a valid class.");
			    		 RETURN_FALSE;
			    	}
			    	if ((params = zend_hash_str_find(cache->value.arr, "params", 6)) == NULL) {
				    	php_error_docref(NULL, E_ERROR, "Factory need a valid param.");
				    	RETURN_FALSE;
			    	} else {
			    		 if (Z_TYPE_P(params) != IS_ARRAY) {
				    		 php_error_docref(NULL, E_ERROR, "Factory need a array param.");
				    		 RETURN_FALSE;
			    		 }
			    	}
			    	instance = zend_hash_str_find(cache->value.arr, "instance", 8);
			    	if (Z_TYPE_P(instance) == IS_TRUE) {
			    		type = 1;
			    	}

					if (type) {
				    	reg = gene_reg_instance();
						entrys = zend_read_property(gene_reg_ce, reg, GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), 1, NULL);
						if ((pzval = zend_hash_str_find(Z_ARRVAL_P(entrys), ZSTR_VAL(class->value.str), ZSTR_LEN(class->value.str))) != NULL) {
							if (zend_hash_update(Z_ARRVAL_P(props), name, pzval) != NULL) {
								Z_TRY_ADDREF_P(pzval);
								RETURN_ZVAL(pzval, 1, 0);
							}
						}
					}
					gene_memory_zval_local(&local_params, params);
					if (gene_factory(ZSTR_VAL(class->value.str), ZSTR_LEN(class->value.str), &local_params, &classObject)) {
						if (type) {
							if (zend_hash_str_update(Z_ARRVAL_P(entrys), ZSTR_VAL(class->value.str), ZSTR_LEN(class->value.str), &classObject) != NULL) {
								Z_TRY_ADDREF_P(&classObject);
							}
						}
						if (zend_hash_update(Z_ARRVAL_P(props), name, &classObject) != NULL) {
							Z_TRY_ADDREF_P(&classObject);
							zval_ptr_dtor(&local_params);
							RETURN_ZVAL(&classObject, 1, 0);
						}
					}
					zval_ptr_dtor(&local_params);
				}
				RETURN_NULL();
			}
			RETURN_ZVAL(pzval, 1, 0);
		}
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */


/** {{{ proto public gene_service::success(string $text, int $code)
 */
PHP_METHOD(gene_service, success) {
	zend_string *text;
	zend_long code = 2000;
	zval ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(&ret);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(&ret, ZEND_STRL(GENE_RESPONSE_MSG), text);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */


/** {{{ proto public gene_service::error(string $text, int $code)
 */
PHP_METHOD(gene_service, error) {
	zend_string *text;
	zend_long code = 4000;
	zval ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(&ret);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(&ret, ZEND_STRL(GENE_RESPONSE_MSG), text);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */

/** {{{ proto public gene_service::data(mixed $data, int $count,string $text, int $code)
 */
PHP_METHOD(gene_service, data) {
	zval *data = NULL;
	zend_long count = 0;
	zend_string *text = NULL;
	zend_long code = 2000;
	zval ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|lSl", &data, &count, &text, &code) == FAILURE) {
		return;
	}
	array_init(&ret);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(&ret, ZEND_STRL(GENE_RESPONSE_MSG), text);
	add_assoc_zval_ex(&ret, ZEND_STRL(GENE_RESPONSE_DATA), data);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_COUNT), count);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_service::getInstance()
 */
PHP_METHOD(gene_service, getInstance)
{
	zval obj;
	RETURN_ZVAL(gene_service_instance(&obj), 1, 0);
}
/* }}} */

/** {{{ proto public gene_service::__destruct
*/
PHP_METHOD(gene_service, __destruct) {

}
/* }}} */

/*
 * {{{ gene_service_methods
 */
zend_function_entry gene_service_methods[] = {
		PHP_ME(gene_service, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(gene_service, __destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
		PHP_ME(gene_service, __get, gene_service_get, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, __set, gene_service_set, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, success, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, error, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, data, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, getInstance, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(service)
{
    zend_class_entry gene_service;
	GENE_INIT_CLASS_ENTRY(gene_service, "Gene_Service", "Gene\\Service", gene_service_methods);
	gene_service_ce = zend_register_internal_class_ex(&gene_service, NULL);
	zend_declare_property_null(gene_service_ce, ZEND_STRL(GENE_SERVICE_ATTR), ZEND_ACC_PUBLIC);

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
