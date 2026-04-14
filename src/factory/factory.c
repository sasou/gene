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
#include "../factory/factory.h"
#include "../di/di.h"

zend_class_entry * gene_factory_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_factory_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_factory_create, 0, 0, 1)
	ZEND_ARG_INFO(0, class)
    ZEND_ARG_INFO(0, params)
    ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

bool gene_factory_load_class(char *className, size_t tmp_len, zval *classObject) {
	zend_string *c_key = NULL;
	zend_class_entry *pdo_ptr = NULL;

	c_key = zend_string_init(className, tmp_len, 0);
	pdo_ptr = zend_lookup_class(c_key);
	zend_string_release(c_key);

	if (pdo_ptr) {
		object_init_ex(classObject, pdo_ptr);
		return 1;
	}
	return 0;
}

void gene_factory_call(zval *object, char *action, size_t action_len, zval *param, zval *retval) /*{{{*/
{
    uint32_t param_count = 0;
	zval *element;
	zend_string *key = NULL;
	zend_long id;
	uint32_t num = 0;
	zend_function *fn = NULL;
	zend_class_entry *ce = Z_OBJCE_P(object);

    if (retval) {
        ZVAL_UNDEF(retval);
    }
    fn = (zend_function *)zend_hash_str_find_ptr(&ce->function_table, action, action_len);
    if (param && Z_TYPE_P(param) == IS_ARRAY) {
    	param_count = zend_hash_num_elements(Z_ARRVAL_P(param));
    	if (param_count > 0) {
        	zval *params = (zval *) safe_emalloc(param_count, sizeof(zval), 0);
        	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(param), id, key, element)
        	{
        		if (num < param_count) {
        			params[num] = *element;
        			num++;
        		}
        	}ZEND_HASH_FOREACH_END();
        	if (fn) {
        		zend_call_known_function(fn, Z_OBJ_P(object), ce, retval, param_count, params, NULL);
        	} else {
        		zval function_name;
        		ZVAL_STRINGL(&function_name, action, action_len);
        		call_user_function(NULL, object, &function_name, retval, param_count, params);
        		zval_ptr_dtor(&function_name);
        	}
            efree(params);
    	} else {
    		if (fn) {
    			zend_call_known_function(fn, Z_OBJ_P(object), ce, retval, 0, NULL, NULL);
    		} else {
    			zval function_name;
    			ZVAL_STRINGL(&function_name, action, action_len);
    			call_user_function(NULL, object, &function_name, retval, 0, NULL);
    			zval_ptr_dtor(&function_name);
    		}
    	}
    } else {
    	if (fn) {
    		zend_call_known_function(fn, Z_OBJ_P(object), ce, retval, 0, NULL, NULL);
    	} else {
    		zval function_name;
    		ZVAL_STRINGL(&function_name, action, action_len);
    		call_user_function(NULL, object, &function_name, retval, 0, NULL);
    		zval_ptr_dtor(&function_name);
    	}
    }
}/*}}}*/

void gene_factory_function_call(char *action, zval *param_key, zval *param_arr, zval *retval) /*{{{*/
{
    uint32_t param_count = 0;
	zval *element;
	zend_string *key = NULL;
	zend_long id;
	uint32_t num = 1;
	uint32_t total;
	zval *params;

    if (retval) {
        ZVAL_UNDEF(retval);
    }
    if (param_arr && Z_TYPE_P(param_arr) == IS_ARRAY) {
    	param_count = zend_hash_num_elements(Z_ARRVAL_P(param_arr));
    }
    total = 1 + param_count;
    params = (zval *) safe_emalloc(total, sizeof(zval), 0);
    params[0] = *param_key;
    if (param_count > 0) {
    	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(param_arr), id, key, element)
    	{
    		if (num < total) {
    			params[num] = *element;
    			num++;
    		}
    	}ZEND_HASH_FOREACH_END();
    }
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), action, strlen(action));
    if (fn) {
        zend_call_known_function(fn, NULL, NULL, retval, num, params, NULL);
    } else {
        zval function_name;
        ZVAL_STRING(&function_name, action);
        call_user_function(NULL, NULL, &function_name, retval, num, params);
        zval_ptr_dtor(&function_name);
    }
    efree(params);
}/*}}}*/

void gene_factory_function_call_1(zval *function_name, zval *param_key, zval *param_arr, zval *retval) /*{{{*/
{
	uint32_t param_count = 0;
	zval *element;
	zend_string *key = NULL;
	zend_long id;
	uint32_t num = 1;
	uint32_t total;
    if (param_key) {
        if (param_arr && Z_TYPE_P(param_arr) == IS_ARRAY) {
        	param_count = zend_hash_num_elements(Z_ARRVAL_P(param_arr));
        }
        total = 1 + param_count;
        zval *params = (zval *) safe_emalloc(total, sizeof(zval), 0);
        params[0] = *param_key;
        if (param_count > 0) {
        	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(param_arr), id, key, element)
        	{
        		if (num < total) {
        			params[num] = *element;
        			num++;
        		}
        	}ZEND_HASH_FOREACH_END();
        }
        call_user_function(NULL, NULL, function_name, retval, num, params);
        efree(params);
    } else {
    	call_user_function(NULL, NULL, function_name, retval, 0, NULL);
    }
}/*}}}*/


bool gene_factory(char *className, size_t tmp_len, zval *params, zval *classObject) {
	zend_string *c_key = NULL;
	zend_class_entry *pdo_ptr = NULL;

	c_key = zend_string_init(className, tmp_len, 0);
	pdo_ptr = zend_lookup_class(c_key);
	zend_string_release(c_key);

	if (pdo_ptr) {
		object_init_ex(classObject, pdo_ptr);
		if (pdo_ptr->constructor) {
			zval ret;
			ZVAL_UNDEF(&ret);
			uint32_t param_count = 0;
			zval *call_params = NULL;
			if (params && Z_TYPE_P(params) == IS_ARRAY) {
				param_count = zend_hash_num_elements(Z_ARRVAL_P(params));
				if (param_count > 0) {
					call_params = (zval *)safe_emalloc(param_count, sizeof(zval), 0);
					uint32_t i = 0;
					zval *el;
					ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(params), el) {
						if (i < param_count) call_params[i++] = *el;
					} ZEND_HASH_FOREACH_END();
				}
			}
			zend_call_known_function(pdo_ptr->constructor, Z_OBJ_P(classObject), pdo_ptr, &ret, param_count, call_params, NULL);
			if (call_params) efree(call_params);
			if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
		}
		return 1;
	}
	return 0;
}

void gene_factory_call_1(zval *object, char *action, size_t action_len, zval *param, zval *retval) /*{{{*/
{
	zend_function *fn = NULL;
	zend_class_entry *ce = Z_OBJCE_P(object);
    if (retval) {
        ZVAL_UNDEF(retval);
    }
    fn = (zend_function *)zend_hash_str_find_ptr(&ce->function_table, action, action_len);
    if (param && Z_TYPE_P(param) == IS_ARRAY) {
    	zval params[1];
    	params[0] = *param;
    	if (fn) {
    		zend_call_known_function(fn, Z_OBJ_P(object), ce, retval, 1, params, NULL);
    	} else {
    		zval function_name;
    		ZVAL_STRINGL(&function_name, action, action_len);
    		call_user_function(NULL, object, &function_name, retval, 1, params);
    		zval_ptr_dtor(&function_name);
    	}
    } else {
    	if (fn) {
    		zend_call_known_function(fn, Z_OBJ_P(object), ce, retval, 0, NULL, NULL);
    	} else {
    		zval function_name;
    		ZVAL_STRINGL(&function_name, action, action_len);
    		call_user_function(NULL, object, &function_name, retval, 0, NULL);
    		zval_ptr_dtor(&function_name);
    	}
    }
}/*}}}*/

void gene_factory_call_2(char *method, zval *key, zval *retval) /*{{{*/
{
    if (retval) {
        ZVAL_UNDEF(retval);
    }
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), method, strlen(method));
    if (fn) {
        if (key) {
            zval params[1];
            params[0] = *key;
            zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
        } else {
            zend_call_known_function(fn, NULL, NULL, retval, 0, NULL, NULL);
        }
    } else {
        zval function_name;
        ZVAL_STRING(&function_name, method);
        if (key) {
            zval params[1];
            params[0] = *key;
            call_user_function(NULL, NULL, &function_name, retval, 1, params);
        } else {
            call_user_function(NULL, NULL, &function_name, retval, 0, NULL);
        }
        zval_ptr_dtor(&function_name);
    }
}/*}}}*/

/*
 * {{{ gene_factory
 */
PHP_METHOD(gene_factory, __construct)
{

}
/* }}} */


/*
 * {{{ public gene_factory::test($key)
 */
PHP_METHOD(gene_factory, create)
{
	zval *params = NULL, *entrys = NULL, *pzval = NULL, classObject;
	zend_string *name;
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|zl", &name, &params, &type) == FAILURE) {
		return;
	}

	if (type) {
		entrys = gene_di_regs();
		if ((pzval = zend_hash_find(Z_ARRVAL_P(entrys), name)) != NULL) {
			Z_TRY_ADDREF_P(pzval);
			RETURN_ZVAL(pzval, 0, 0);
		}
	}


	if (gene_factory(ZSTR_VAL(name), ZSTR_LEN(name), params, &classObject)) {
		if (type) {
			Z_TRY_ADDREF_P(&classObject);
			zend_hash_update(Z_ARRVAL_P(entrys), name, &classObject);
		}
		RETURN_ZVAL(&classObject, 0, 0);
	}
	RETURN_NULL();
}
/* }}} */


/*
 * {{{ gene_factory_methods
 */
const zend_function_entry gene_factory_methods[] = {
		PHP_ME(gene_factory, __construct, gene_factory_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_factory, create, gene_factory_create, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(factory)
{
    zend_class_entry gene_factory;
	GENE_INIT_CLASS_ENTRY(gene_factory, "Gene_Factory", "Gene\\Factory", gene_factory_methods);
	gene_factory_ce = zend_register_internal_class_ex(&gene_factory, NULL);
#if PHP_VERSION_ID >= 80200
	gene_factory_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
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
