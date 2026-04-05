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

#include "../gene.h"
#include "../di/di.h"
#include "../config/configs.h"
#include "../cache/memory.h"
#include "../factory/factory.h"

zend_class_entry *gene_di_ce;

zval *gene_di_regs() {
	gene_request_context *ctx = gene_request_ctx();
	if (Z_TYPE(ctx->di_regs) == IS_UNDEF || Z_TYPE(ctx->di_regs) == IS_NULL) {
		if (Z_TYPE(ctx->di_regs) == IS_NULL) {
			zval_ptr_dtor(&ctx->di_regs);
		}
		array_init(&ctx->di_regs);
	}
	return &ctx->di_regs;
}

/* {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_di_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

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
	zval  *pzval = NULL,*class = NULL,*params = NULL, *instance = NULL,*cache = NULL, *entrys = NULL;

	entrys = gene_di_regs();
	if ((pzval = zend_hash_find(Z_ARRVAL_P(entrys), name)) != NULL) {
		return pzval;
	}

	char router_e_stack[256];
	char *router_e = router_e_stack;
	size_t router_e_len = 0;
	bool type = 0;
	int router_e_heap = 0;

	{
		const char *prefix = NULL;
		size_t prefix_len = 0;
		if (GENE_G(app_key) && GENE_G(app_key)[0] != '\0') {
			prefix = GENE_G(app_key);
			prefix_len = strlen(GENE_G(app_key));
		} else if (GENE_G(app_root) && GENE_G(app_root)[0] != '\0') {
			prefix = GENE_G(app_root);
			prefix_len = strlen(GENE_G(app_root));
		}
		if (prefix) {
			router_e_len = prefix_len + sizeof(GENE_CONFIG_CACHE) - 1;
			if (router_e_len >= sizeof(router_e_stack)) {
				router_e = emalloc(router_e_len + 1);
				router_e_heap = 1;
			}
			memcpy(router_e, prefix, prefix_len);
			memcpy(router_e + prefix_len, GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE));
		} else {
			router_e_len = sizeof(GENE_CONFIG_CACHE) - 1;
			memcpy(router_e, GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE));
		}
	}

	cache = gene_memory_get_by_config(router_e, router_e_len, ZSTR_VAL(name));
	if (router_e_heap) efree(router_e);

	if (cache && Z_TYPE_P(cache) == IS_ARRAY) {
    	if ((class = zend_hash_str_find(Z_ARRVAL_P(cache), "class", 5)) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "Factory need a valid class.");
    		 return NULL;
    	}
    	if ((params = zend_hash_str_find(Z_ARRVAL_P(cache), "params", 6)) != NULL) {
    		 if (Z_TYPE_P(params) != IS_ARRAY) {
	    		 php_error_docref(NULL, E_ERROR, "Factory need a array param.");
	    		 return NULL;
    		 }
    	}
    	instance = zend_hash_str_find(Z_ARRVAL_P(cache), "instance", 8);

    	if (instance && Z_TYPE_P(instance) == IS_TRUE) {
    		type = 1;
    	}

		zend_string *local_class_str = zend_string_init(Z_STRVAL_P(class), Z_STRLEN_P(class), 0);
		zval local_params;
		if (params) {
			gene_memory_zval_local(&local_params, params);
		} else {
			ZVAL_NULL(&local_params);
		}

		if (type) {
			if ((pzval = zend_hash_find(Z_ARRVAL_P(entrys), local_class_str)) != NULL) {
				zend_string_release(local_class_str);
				zval_ptr_dtor(&local_params);
				return pzval;
			}
		}

		zval classObject;
		if (gene_factory_load_class(ZSTR_VAL(local_class_str), ZSTR_LEN(local_class_str), &classObject)) {
			if (Z_OBJCE(classObject)->constructor) {
				zval tmp;
				ZVAL_UNDEF(&tmp);
				uint32_t ctor_argc = 0;
				zval *ctor_params = NULL;
				if (Z_TYPE(local_params) == IS_ARRAY) {
					ctor_argc = zend_hash_num_elements(Z_ARRVAL(local_params));
					if (ctor_argc > 0) {
						ctor_params = (zval *)safe_emalloc(ctor_argc, sizeof(zval), 0);
						uint32_t ci = 0;
						zval *el;
						ZEND_HASH_FOREACH_VAL(Z_ARRVAL(local_params), el) {
							if (ci < ctor_argc) ctor_params[ci++] = *el;
						} ZEND_HASH_FOREACH_END();
					}
				}
				zend_call_known_function(Z_OBJCE(classObject)->constructor, Z_OBJ(classObject), Z_OBJCE(classObject), &tmp, ctor_argc, ctor_params, NULL);
				if (ctor_params) efree(ctor_params);
				if (!Z_ISUNDEF(tmp)) zval_ptr_dtor(&tmp);
			}

			if (type) {
				Z_TRY_ADDREF(classObject);
				zend_hash_update(Z_ARRVAL_P(entrys), local_class_str, &classObject);
			}
		    if ((pzval = zend_hash_update(Z_ARRVAL_P(entrys), name, &classObject)) != NULL ) {
		    	zend_string_release(local_class_str);
		    	zval_ptr_dtor(&local_params);
		    	return pzval;
		    }
		}
		zend_string_release(local_class_str);
		zval_ptr_dtor(&local_params);
	}
	return NULL;
}

zval *gene_class_instance(zval *obj, zval *class_name, zval *params) {
	zval *ppzval = NULL, *entrys;
	entrys = gene_di_regs();
	if ((ppzval = zend_hash_str_find(Z_ARRVAL_P(entrys), Z_STRVAL_P(class_name), Z_STRLEN_P(class_name))) != NULL) {
		return ppzval;
	}

	if (gene_factory_load_class(Z_STRVAL_P(class_name), Z_STRLEN_P(class_name), obj)) {
		if (Z_OBJCE_P(obj)->constructor) {
			zval tmp;
			ZVAL_UNDEF(&tmp);
			uint32_t ctor_argc = 0;
			zval *ctor_params = NULL;
			if (params && Z_TYPE_P(params) == IS_ARRAY) {
				ctor_argc = zend_hash_num_elements(Z_ARRVAL_P(params));
				if (ctor_argc > 0) {
					ctor_params = (zval *)safe_emalloc(ctor_argc, sizeof(zval), 0);
					uint32_t ci = 0;
					zval *el;
					ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(params), el) {
						if (ci < ctor_argc) ctor_params[ci++] = *el;
					} ZEND_HASH_FOREACH_END();
				}
			}
			zend_call_known_function(Z_OBJCE_P(obj)->constructor, Z_OBJ_P(obj), Z_OBJCE_P(obj), &tmp, ctor_argc, ctor_params, NULL);
			if (ctor_params) efree(ctor_params);
			if (!Z_ISUNDEF(tmp)) zval_ptr_dtor(&tmp);
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
	zval *entrys, *ppzval = NULL;
	char stack_buf[256];
	char *key_buf;
	size_t key_len;

	entrys = gene_di_regs();

	key_len = ZSTR_LEN(class_name) + 1 + ZSTR_LEN(name);
	key_buf = (key_len < sizeof(stack_buf)) ? stack_buf : emalloc(key_len + 1);
	memcpy(key_buf, ZSTR_VAL(class_name), ZSTR_LEN(class_name));
	key_buf[ZSTR_LEN(class_name)] = '_';
	memcpy(key_buf + ZSTR_LEN(class_name) + 1, ZSTR_VAL(name), ZSTR_LEN(name));
	key_buf[key_len] = '\0';

	ppzval = zend_hash_str_find(Z_ARRVAL_P(entrys), key_buf, key_len);
	if (key_buf != stack_buf) efree(key_buf);

	if (ppzval != NULL) {
		return ppzval;
	}
	return gene_di_get(name);
}
/* }}} */


/*
 *  {{{ int gene_di_set_class(zend_string class_name, zend_string *name)
 */
int gene_di_set_class(zend_string *class_name, zend_string *name, zval *value) {
	zval *entrys;
	char stack_buf[256];
	char *key_buf;
	size_t key_len;

	entrys = gene_di_regs();

	key_len = ZSTR_LEN(class_name) + 1 + ZSTR_LEN(name);
	key_buf = (key_len < sizeof(stack_buf)) ? stack_buf : emalloc(key_len + 1);
	memcpy(key_buf, ZSTR_VAL(class_name), ZSTR_LEN(class_name));
	key_buf[ZSTR_LEN(class_name)] = '_';
	memcpy(key_buf + ZSTR_LEN(class_name) + 1, ZSTR_VAL(name), ZSTR_LEN(name));
	key_buf[key_len] = '\0';

	Z_TRY_ADDREF_P(value);
	zend_hash_str_update(Z_ARRVAL_P(entrys), key_buf, key_len, value);
	if (key_buf != stack_buf) efree(key_buf);
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
		zval this_ptr;

		object_init_ex(&this_ptr, gene_di_ce);
		zend_update_static_property(gene_di_ce, GENE_DI_PROPERTY_INSTANCE, strlen(GENE_DI_PROPERTY_INSTANCE), &this_ptr);
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
	zval *value, *entrys;
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		RETURN_NULL();
	}
	entrys = gene_di_regs();
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
	zval *entrys;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}
	entrys = gene_di_regs();
	zend_hash_del(Z_ARRVAL_P(entrys), name);
	RETURN_TRUE;
}
/* }}} */

/*
 *  {{{ public gene_di::has($name)
 */
PHP_METHOD(gene_di, has) {
	zend_string *name;
	zval *entrys;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}
	entrys = gene_di_regs();
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
const zend_function_entry gene_di_methods[] = {
	PHP_ME(gene_di, __construct, gene_di_void_arginfo, ZEND_ACC_PRIVATE)
	PHP_ME(gene_di, __clone, gene_di_void_arginfo, ZEND_ACC_PRIVATE)
	PHP_ME(gene_di, getInstance, gene_di_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
#if PHP_VERSION_ID >= 80200
	gene_di_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

	//static
	zend_declare_property_null(gene_di_ce, GENE_DI_PROPERTY_INSTANCE, strlen(GENE_DI_PROPERTY_INSTANCE), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
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
