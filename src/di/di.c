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
#include "Zend/zend_interfaces.h"
#include "zend_smart_str.h" /* for smart_str */

#include "../gene.h"
#include "../di/di.h"
#include "../config/configs.h"
#include "../cache/memory.h"
#include "../factory/factory.h"

zend_class_entry *gene_di_ce;

/* {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_di_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_di_has_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_di_del_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_di_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */


zval *gene_di_get(zend_string *name) {
	zval  *pzval = NULL,*class = NULL,*params = NULL, *instance = NULL,*cache = NULL, *di = NULL, *entrys = NULL;

	di = gene_di_instance();
	entrys = zend_read_property(gene_di_ce, gene_strip_obj(di), GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), 1, NULL);

	if ((pzval = zend_hash_find(Z_ARRVAL_P(entrys), name)) != NULL) {
		return pzval;
	}

	char *router_e = NULL;
	size_t router_e_len = 0;
	zend_bool type = 0;

	if (GENE_G(app_key)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(app_key), GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(directory), GENE_CONFIG_CACHE);
	}

	cache = gene_memory_get_by_config(router_e, router_e_len, ZSTR_VAL(name));
	efree(router_e);
	router_e = NULL;

	if (cache && Z_TYPE_P(cache) == IS_ARRAY) {
    	if ((class = zend_hash_str_find(cache->value.arr, "class", 5)) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "Factory need a valid class.");
    		 return NULL;
    	}
    	if ((params = zend_hash_str_find(cache->value.arr, "params", 6)) != NULL) {
    		 if (Z_TYPE_P(params) != IS_ARRAY) {
	    		 php_error_docref(NULL, E_ERROR, "Factory need a array param.");
	    		 return NULL;
    		 }
    	}
    	instance = zend_hash_str_find(cache->value.arr, "instance", 8);

    	if (instance && Z_TYPE_P(instance) == IS_TRUE) {
    		type = 1;
    	}

		if (type) {
			if ((pzval = zend_hash_find(Z_ARRVAL_P(entrys), Z_STR_P(class))) != NULL) {
				return pzval;
			}
		}

		zval classObject;
		if (gene_factory_load_class(Z_STRVAL_P(class), Z_STRLEN_P(class), &classObject)) {
			if (zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), ZEND_STRL("__construct"))) {
				zval tmp;
				zval local_params;
				if (params) {
					gene_memory_zval_local(&local_params, params);
				} else {
					ZVAL_NULL(&local_params);
				}
				gene_factory_call(&classObject, "__construct", &local_params, &tmp);
				zval_ptr_dtor(&tmp);
				zval_ptr_dtor(&local_params);
			}

			if (type) {
				zend_hash_update(Z_ARRVAL_P(entrys), Z_STR_P(class), &classObject);
			}
		    if ((pzval = zend_hash_update(Z_ARRVAL_P(entrys), name, &classObject)) != NULL ) {
		    	return pzval;
		    }
		}
	}
	return NULL;
}

zval *gene_class_instance(zval *obj, zval *class_name, zval *params) {
	zval *ppzval = NULL, *di, *entrys;
	di = gene_di_instance();
	entrys = zend_read_property(gene_di_ce, gene_strip_obj(di), ZEND_STRL(GENE_DI_PROPERTY_REG), 1, NULL);
	if ((ppzval = zend_hash_str_find(Z_ARRVAL_P(entrys), Z_STRVAL_P(class_name), Z_STRLEN_P(class_name))) != NULL) {
		return ppzval;
	}

	if (gene_factory_load_class(Z_STRVAL_P(class_name), Z_STRLEN_P(class_name), obj)) {
		if (zend_hash_str_exists(&(Z_OBJCE_P(obj)->function_table), ZEND_STRL("__construct"))) {
			zval tmp;
			gene_factory_call(obj, "__construct", params, &tmp);
			zval_ptr_dtor(&tmp);
		}

		if ((ppzval = zend_hash_str_update(Z_ARRVAL_P(entrys), Z_STRVAL_P(class_name), Z_STRLEN_P(class_name), obj)) != NULL ) {
			return ppzval;
		}
		return obj;
	}
	return NULL;
}


/*
 *  {{{ int gene_di_get_class(zend_string class_name, zend_string *name)
 */
zval *gene_di_get_class(zend_string *class_name, zend_string *name) {
	zval *di, *entrys, *ppzval = NULL;

	di = gene_di_instance();
	entrys = zend_read_property(gene_di_ce, gene_strip_obj(di), GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), 1, NULL);

    smart_str class_val = {0};
    smart_str_appendl(&class_val, class_name->val, class_name->len);
    smart_str_appendc(&class_val, '_');
    smart_str_appendl(&class_val, name->val, name->len);
    smart_str_0(&class_val);
	if ((ppzval = zend_hash_find(Z_ARRVAL_P(entrys), class_val.s)) != NULL) {
		return ppzval;
	}
	ppzval = gene_di_get(name);
	if (ppzval != NULL) {
		return ppzval;
	}
	smart_str_free(&class_val);
	return NULL;
}
/* }}} */


/*
 *  {{{ int gene_di_set_class(zend_string class_name, zend_string *name)
 */
int gene_di_set_class(zend_string *class_name, zend_string *name, zval *value) {
	zval *di, *entrys, *ppzval = NULL;
	di = gene_di_instance();
	entrys = zend_read_property(gene_di_ce, gene_strip_obj(di), GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), 1, NULL);
    smart_str class_val = {0};
    smart_str_appendl(&class_val, class_name->val, class_name->len);
    smart_str_appendc(&class_val, '_');
    smart_str_appendl(&class_val, name->val, name->len);
    smart_str_0(&class_val);
    Z_TRY_ADDREF_P(value);
    zend_hash_update(Z_ARRVAL_P(entrys), class_val.s, value);
	smart_str_free(&class_val);
	return 1;
}
/* }}} */

/** {{{ proto private gene_di::__construct(void)
 */
PHP_METHOD(gene_di, __construct) {
}
/* }}} */

/** {{{ proto private gene_di::__sleep(void)
 */
PHP_METHOD(gene_di, __sleep) {
}
/* }}} */

/** {{{ proto private gene_di::__wakeup(void)
 */
PHP_METHOD(gene_di, __wakeup) {
}
/* }}} */

/** {{{ proto private gene_di::__clone(void)
 */
PHP_METHOD(gene_di, __clone) {
}
/* }}} */

/*
 *  {{{ zval *gene_di_instance(zval *this_ptr)
 */
zval* gene_di_instance() {
	zval *instance = zend_read_static_property(gene_di_ce, GENE_DI_PROPERTY_INSTANCE, strlen(GENE_DI_PROPERTY_INSTANCE), 1);

	if (Z_TYPE_P(instance) != IS_OBJECT) {
		zval regs, this_ptr;

		object_init_ex(&this_ptr, gene_di_ce);

		array_init(&regs);
		zend_update_property(gene_di_ce, gene_strip_obj(&this_ptr), GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), &regs);
		zend_update_static_property(gene_di_ce, GENE_DI_PROPERTY_INSTANCE, strlen(GENE_DI_PROPERTY_INSTANCE), &this_ptr);
		zval_ptr_dtor(&regs);
		zval_ptr_dtor(&this_ptr);
		instance = zend_read_static_property(gene_di_ce, GENE_DI_PROPERTY_INSTANCE, strlen(GENE_DI_PROPERTY_INSTANCE), 1);
	}
	return instance;
}
/* }}} */


/*
 *  {{{ public static gene_di::get($name)
 */
PHP_METHOD(gene_di, get) {
	zend_string *name;
	zval *ppzval = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}

	ppzval = gene_di_get(name);
	if (ppzval) {
		RETURN_ZVAL(ppzval, 1, 0);
	}
	RETURN_NULL();
}
/* }}} */

/*
 *  {{{ public static gene_di::set($name, $value)
 */
PHP_METHOD(gene_di, set) {
	zval *value, *di, *entrys;
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		RETURN_NULL();
	}
	di = gene_di_instance();
	entrys = zend_read_property(gene_di_ce, gene_strip_obj(di), GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), 1, NULL);
	if (zend_hash_update(Z_ARRVAL_P(entrys), name, value) != NULL) {
		Z_TRY_ADDREF_P(value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 *  {{{ public static gene_di::del($name)
 */
PHP_METHOD(gene_di, del) {
	zend_string *name;
	zval *di, *entrys;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}
	di = gene_di_instance();
	entrys = zend_read_property(gene_di_ce, gene_strip_obj(di), GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), 1, NULL);
	zend_hash_del(Z_ARRVAL_P(entrys), name);
	RETURN_TRUE;
}
/* }}} */

/*
 *  {{{ public gene_di::has($name)
 */
PHP_METHOD(gene_di, has) {
	zend_string *name;
	zval *di, *entrys;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}
	di = gene_di_instance();
	entrys = zend_read_property(gene_di_ce, gene_strip_obj(di), GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), 1, NULL);
	if (zend_hash_exists(Z_ARRVAL_P(entrys), name) == 1) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 *  {{{ public gene_di::getInstance(void)
 */
PHP_METHOD(gene_di, getInstance) {
	zval *di;
	di = gene_di_instance();
	RETURN_ZVAL(di, 1, 0);
}
/* }}} */

/*
 * {{{ gene_di_methods
 */
zend_function_entry gene_di_methods[] = {
	PHP_ME(gene_di, __construct, NULL, ZEND_ACC_CTOR|ZEND_ACC_PRIVATE)
	PHP_ME(gene_di, __clone, NULL, ZEND_ACC_PRIVATE)
	PHP_ME(gene_di, getInstance, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_di, get, gene_di_get_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_di, has, gene_di_has_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_di, set, gene_di_set_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_di, del, gene_di_del_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_MALIAS(gene_di, __set, set, gene_di_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_di, __get, get, gene_di_get_arginfo, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(di) {
	zend_class_entry gene_di;
	GENE_INIT_CLASS_ENTRY(gene_di, "gene_di", "Gene\\Di", gene_di_methods);
	gene_di_ce = zend_register_internal_class_ex(&gene_di, NULL);
	gene_di_ce->ce_flags |= ZEND_ACC_FINAL;

	//static
	zend_declare_property_null(gene_di_ce, GENE_DI_PROPERTY_INSTANCE, strlen(GENE_DI_PROPERTY_INSTANCE), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_null(gene_di_ce, GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), ZEND_ACC_PROTECTED);
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
