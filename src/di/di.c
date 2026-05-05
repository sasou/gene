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
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
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
	/* [GENE_PERF:2026-04-19] Hot path: di_regs is IS_ARRAY after the first
	 * DI access per request. Hint the compiler to keep the init branch cold. */
	if (UNEXPECTED(Z_TYPE(ctx->di_regs) == IS_UNDEF || Z_TYPE(ctx->di_regs) == IS_NULL)) {
		if (Z_TYPE(ctx->di_regs) == IS_NULL) {
			zval_ptr_dtor(&ctx->di_regs);
		}
		/* [GENE_PERF:2026-04-24 v5.5.8] Typical DI graphs register ~8-20
		 * services per request (db, redis, memcache, session, view, language,
		 * validate, response, memory, plus class-qualified per-object keys).
		 * Pre-size at 16 to skip the default-8 initial bucket grow that hits
		 * on the 9th insert — a minor cache-hot rehash we can trivially avoid. */
		array_init_size(&ctx->di_regs, 16);
	}
	return &ctx->di_regs;
}

#define GENE_DI_STACK_PARAM_CAP 8

static zend_always_inline zval *gene_di_pack_array_params(zval *param_arr, uint32_t *param_count, zval *stack_buf, uint32_t stack_cap, int *heap_allocated) {
	zval *params = NULL, *element;
	uint32_t count = 0, i = 0;

	*param_count = 0;
	*heap_allocated = 0;
	if (!param_arr || Z_TYPE_P(param_arr) != IS_ARRAY) {
		return NULL;
	}

	count = zend_hash_num_elements(Z_ARRVAL_P(param_arr));
	*param_count = count;
	if (count == 0) {
		return NULL;
	}

	params = (count <= stack_cap) ? stack_buf : (zval *)safe_emalloc(count, sizeof(zval), 0);
	*heap_allocated = (count > stack_cap);
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(param_arr), element) {
		if (i < count) {
			params[i++] = *element;
		}
	} ZEND_HASH_FOREACH_END();
	return params;
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

	const char *cache_key;
	size_t cache_key_len;
	bool type = 0;
	char stack_buf[256];
	int cache_key_heap = 0;

	if (GENE_G(config_cache_key)) {
		cache_key = GENE_G(config_cache_key);
		cache_key_len = GENE_G(config_cache_key_len);
	} else {
		const char *prefix = NULL;
		size_t prefix_len = 0;
		if (GENE_G(app_key) && GENE_G(app_key)[0] != '\0') {
			prefix = GENE_G(app_key);
			prefix_len = GENE_G(app_key_len);
		} else if (GENE_G(app_root) && GENE_G(app_root)[0] != '\0') {
			prefix = GENE_G(app_root);
			prefix_len = GENE_G(app_root_len);
		}
		if (prefix) {
			cache_key_len = prefix_len + sizeof(GENE_CONFIG_CACHE) - 1;
			if (cache_key_len >= sizeof(stack_buf)) {
				cache_key = emalloc(cache_key_len + 1);
				cache_key_heap = 1;
			} else {
				cache_key = stack_buf;
			}
			memcpy((char *)cache_key, prefix, prefix_len);
			memcpy((char *)cache_key + prefix_len, GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE));
		} else {
			cache_key = GENE_CONFIG_CACHE;
			cache_key_len = sizeof(GENE_CONFIG_CACHE) - 1;
		}
	}

	cache = gene_memory_get_by_config((char *)cache_key, cache_key_len, ZSTR_VAL(name));
	if (cache_key_heap) efree((char *)cache_key);

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

		/* [GENE_PERF:2026-04-19] Use the persistent interned zend_string directly from
		 * the cache instead of allocating a fresh zend_string via zend_string_init.
		 * The cache entry is created with IS_STR_INTERNED | IS_STR_PERMANENT flags at
		 * config load time and its hash is precomputed, so zend_hash_find/update can
		 * consume it without issue and without incurring an allocation per DI lookup. */
		zend_string *class_str = Z_STR_P(class);
		zval local_params;
		if (params) {
			gene_memory_zval_local(&local_params, params);
		} else {
			ZVAL_NULL(&local_params);
		}

		if (type) {
			if ((pzval = zend_hash_find(Z_ARRVAL_P(entrys), class_str)) != NULL) {
				zval_ptr_dtor(&local_params);
				return pzval;
			}
		}

		zval classObject;
		if (gene_factory_load_class(ZSTR_VAL(class_str), ZSTR_LEN(class_str), &classObject)) {
			if (Z_OBJCE(classObject)->constructor) {
				zval tmp;
				ZVAL_UNDEF(&tmp);
				uint32_t ctor_argc = 0;
				zval *ctor_params = NULL;
				zval ctor_stack[GENE_DI_STACK_PARAM_CAP];
				int ctor_params_heap = 0;
				ctor_params = gene_di_pack_array_params(&local_params, &ctor_argc, ctor_stack, GENE_DI_STACK_PARAM_CAP, &ctor_params_heap);
				zend_call_known_function(Z_OBJCE(classObject)->constructor, Z_OBJ(classObject), Z_OBJCE(classObject), &tmp, ctor_argc, ctor_params, NULL);
				if (ctor_params_heap) efree(ctor_params);
				if (!Z_ISUNDEF(tmp)) zval_ptr_dtor(&tmp);
			}

			if (type) {
				Z_TRY_ADDREF(classObject);
				zend_hash_update(Z_ARRVAL_P(entrys), class_str, &classObject);
			}
		    if ((pzval = zend_hash_update(Z_ARRVAL_P(entrys), name, &classObject)) != NULL ) {
		    	zval_ptr_dtor(&local_params);
		    	return pzval;
		    }
		}
		zval_ptr_dtor(&local_params);
	}
	return NULL;
}

zval *gene_class_instance(zval *obj, zval *class_name, zval *params) {
	zval *ppzval = NULL, *entrys;
	entrys = gene_di_regs();
	/* [GENE_PERF:2026-05-04 #10] Prefer prehashed zend_hash_find when class_name
	 * is a regular zend_string (cached hash); fall back to zend_hash_str_find
	 * otherwise. Saves a per-call full hash recompute on the singleton lookup. */
	if (Z_TYPE_P(class_name) == IS_STRING) {
		ppzval = zend_hash_find(Z_ARRVAL_P(entrys), Z_STR_P(class_name));
	} else {
		ppzval = zend_hash_str_find(Z_ARRVAL_P(entrys), Z_STRVAL_P(class_name), Z_STRLEN_P(class_name));
	}
	if (ppzval != NULL) {
		return ppzval;
	}

	if (gene_factory_load_class(Z_STRVAL_P(class_name), Z_STRLEN_P(class_name), obj)) {
		if (Z_OBJCE_P(obj)->constructor) {
			zval tmp;
			ZVAL_UNDEF(&tmp);
			uint32_t ctor_argc = 0;
			zval *ctor_params = NULL;
			zval ctor_stack[GENE_DI_STACK_PARAM_CAP];
			int ctor_params_heap = 0;
			ctor_params = gene_di_pack_array_params(params, &ctor_argc, ctor_stack, GENE_DI_STACK_PARAM_CAP, &ctor_params_heap);
			zend_call_known_function(Z_OBJCE_P(obj)->constructor, Z_OBJ_P(obj), Z_OBJCE_P(obj), &tmp, ctor_argc, ctor_params, NULL);
			if (ctor_params_heap) efree(ctor_params);
			if (!Z_ISUNDEF(tmp)) zval_ptr_dtor(&tmp);
		}

		if ((ppzval = zend_hash_str_update(Z_ARRVAL_P(entrys), Z_STRVAL_P(class_name), Z_STRLEN_P(class_name), obj)) != NULL ) {
			ZVAL_UNDEF(obj);
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
