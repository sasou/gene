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
#include "gene_reg.h"
#include "gene_memory.h"
#include "gene_config.h"


zend_class_entry * gene_model_ce;


ZEND_BEGIN_ARG_INFO_EX(gene_model_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_model_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()


zend_bool gene_factory_load_class(char *className, int tmp_len, zval *classObject) {
	zend_string *c_key = NULL;
	zend_class_entry *pdo_ptr = NULL;

	c_key = zend_string_init(className, tmp_len, 0);
	pdo_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);
	if (pdo_ptr) {
		object_init_ex(classObject, pdo_ptr);
		return 1;
	}
	return 0;
}

void gene_factory_construct(zval *object, zval *param, zval *retval) /*{{{*/
{
	zval *one = NULL,*two = NULL, *three = NULL, *fro = NULL;
    zval function_name;
    ZVAL_STRING(&function_name, "__construct");
    uint32_t param_count = 0;
    zval params[] = {0};
    if (param && Z_TYPE_P(param) == IS_ARRAY) {
    	param_count = zend_hash_num_elements(Z_ARRVAL_P(param));
        switch(param_count) {
        case 1:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	params[0] = *one;
        	break;
        case 2:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	two = zend_hash_index_find(Z_ARRVAL_P(param), 1);
        	params[0] = *one;
        	params[1] = *two;
        	break;
        case 3:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	two = zend_hash_index_find(Z_ARRVAL_P(param), 1);
        	three = zend_hash_index_find(Z_ARRVAL_P(param), 2);
        	params[0] = *one;
        	params[1] = *two;
        	params[2] = *three;
        	break;
        case 4:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	two = zend_hash_index_find(Z_ARRVAL_P(param), 1);
        	three = zend_hash_index_find(Z_ARRVAL_P(param), 2);
        	fro = zend_hash_index_find(Z_ARRVAL_P(param), 3);
        	params[0] = *one;
        	params[1] = *two;
        	params[2] = *three;
        	params[3] = *fro;
        	break;
        }
        call_user_function(NULL, object, &function_name, retval, param_count, params);
    } else {
    	call_user_function(NULL, object, &function_name, retval, param_count, NULL);
    }
    zval_ptr_dtor(&function_name);
}/*}}}*/

zend_bool gene_factory(char *className, int tmp_len, zval *params, zval *classObject) {
	zend_string *c_key = NULL;
	zend_class_entry *pdo_ptr = NULL;
	zval ret;
	c_key = zend_string_init(className, tmp_len, 0);
	pdo_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);
	if (pdo_ptr) {
		object_init_ex(classObject, pdo_ptr);
		gene_factory_construct(classObject, params, &ret);
		zval_ptr_dtor(&ret);
		return 1;
	}
	return 0;
}

zval *gene_model_instance(zval *obj) {
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
				zend_update_property(gene_model_ce, obj, ZEND_STRL(GENE_MODEL_ATTR), &prop);
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
 * {{{ gene_model
 */
PHP_METHOD(gene_model, __construct)
{

}
/* }}} */

/*
 * {{{ gene_model
 */
PHP_METHOD(gene_model, __set)
{
	zend_string *name;
	zval *value, *props, obj;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	props = zend_read_property(gene_model_ce, gene_model_instance(&obj), ZEND_STRL(GENE_MODEL_ATTR), 1, NULL);
	if (zend_hash_update(Z_ARRVAL_P(props), name, value) != NULL) {
		Z_TRY_ADDREF_P(value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */


/*
 * {{{ gene_model
 */
PHP_METHOD(gene_model, __get)
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
		props = zend_read_property(gene_model_ce, gene_model_instance(&obj), ZEND_STRL(GENE_MODEL_ATTR), 1, NULL);
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
			    	reg = gene_reg_instance();
					entrys = zend_read_property(gene_reg_ce, reg, GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), 1, NULL);
					if (type) {
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


/*
 * {{{ public gene_model::getInstance()
 */
PHP_METHOD(gene_model, getInstance)
{
	zval obj;
	RETURN_ZVAL(gene_model_instance(&obj), 1, 0);
}
/* }}} */

/** {{{ proto public gene_model::__destruct
*/
PHP_METHOD(gene_model, __destruct) {

}
/* }}} */

/*
 * {{{ gene_model_methods
 */
zend_function_entry gene_model_methods[] = {
		PHP_ME(gene_model, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(gene_model, __destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
		PHP_ME(gene_model, __get, gene_model_get, ZEND_ACC_PUBLIC)
		PHP_ME(gene_model, __set, gene_model_set, ZEND_ACC_PUBLIC)
		PHP_ME(gene_model, getInstance, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(model)
{
    zend_class_entry gene_model;
	GENE_INIT_CLASS_ENTRY(gene_model, "Gene_Model", "Gene\\Model", gene_model_methods);
	gene_model_ce = zend_register_internal_class_ex(&gene_model, NULL);
	//gene_model_ce->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;
	zend_declare_property_null(gene_model_ce, ZEND_STRL(GENE_MODEL_ATTR), ZEND_ACC_PUBLIC);

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
