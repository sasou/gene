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
#include "zend_smart_str.h"

#include "../gene.h"
#include "../app/application.h"
#include "../cache/memory.h"
#include "../common/common.h"

zend_class_entry * gene_memory_ce;


ZEND_BEGIN_ARG_INFO_EX(gene_memory_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

static void gene_memory_hash_copy(HashTable *target, HashTable *source);
static void gene_memory_zval_persistent(zval *dst, zval *source);


ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, safe)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_set, 0, 0, 3)
	ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_del, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

/* }}} */

static zend_string* gene_str_persistent(char *str, size_t len) /* {{{ */{
	zend_string *key = zend_string_init(str, len, 1);
	if (key == NULL) {
		zend_error(E_ERROR, "Cannot allocate string, not enough memory?");
	}
	key->h = zend_string_hash_val(key);
#if PHP_VERSION_ID < 70300
	GC_FLAGS(key) |= (IS_STR_INTERNED | IS_STR_PERMANENT);
#else
	GC_ADD_FLAGS(key, IS_STR_INTERNED | IS_STR_PERMANENT);
#endif
	return key;
}
/* }}} */

/** {{{ static void gene_memory_zval_dtor(zval *value)
 */
static void gene_memory_zval_dtor(zval *zvalue) {
	switch(Z_TYPE_P(zvalue)){
	case IS_PTR:
#if PHP_VERSION_ID < 70300
		case IS_CONSTANT:
#endif
	case IS_STRING:
		free(Z_PTR_P(zvalue));
		break;
	case IS_ARRAY:
		gene_hash_destroy(Z_ARRVAL_P(zvalue));
		break;
	default:
			break;
	}
    zval_ptr_dtor(zvalue);
}
/* }}} */

static zval* gene_symtable_update(HashTable *ht, zend_string *key, zval *zv) /* {{{ */{
	zend_ulong idx;
	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		free(key);
		return zend_hash_index_update(ht, idx, zv);
	} else {
		return zend_hash_update(ht, key, zv);
	}
}
/* }}} */

static void gene_hash_init(zval *zv, uint32_t size) /* {{{ */{
	HashTable *ht;
	PALLOC_HASHTABLE(ht);
	zend_hash_init(ht, size, NULL, gene_memory_zval_dtor, 1);
#if PHP_VERSION_ID < 70300
	GC_FLAGS(ht) |= (IS_ARRAY_IMMUTABLE | HASH_FLAG_STATIC_KEYS);
#else
	HT_FLAGS(ht) |= (IS_ARRAY_IMMUTABLE | HASH_FLAG_STATIC_KEYS);
#endif
#if PHP_VERSION_ID >= 70200
	HT_ALLOW_COW_VIOLATION(ht);
#endif
#if PHP_VERSION_ID < 70300
	GC_FLAGS(ht) &= ~HASH_FLAG_APPLY_PROTECTION;
#endif

#if PHP_VERSION_ID < 70300
	GC_REFCOUNT(ht) = 2;
#else
	GC_SET_REFCOUNT(ht, 2);
#endif

	ZVAL_ARR(zv, ht);
#if PHP_VERSION_ID < 70200
	Z_TYPE_FLAGS_P(zv) = IS_TYPE_IMMUTABLE;
#elif PHP_VERSION_ID < 70300
	Z_TYPE_FLAGS_P(zv) = IS_TYPE_COPYABLE;
#else
	Z_TYPE_FLAGS_P(zv) = 0;
#endif
}
/* }}} */

void gene_hash_destroy(HashTable *ht) /* {{{ */{
	zend_string *key = NULL;
	zval *element = NULL;

#if PHP_VERSION_ID < 70400
	if (((ht)->u.flags & HASH_FLAG_INITIALIZED)) {
#else
	if (HT_IS_INITIALIZED(ht)) {
#endif
		ZEND_HASH_FOREACH_STR_KEY_VAL(ht, key, element)
		{
			if (key) {
				free(key);
			}
			switch (Z_TYPE_P(element)) {
			case IS_PTR:
			#if PHP_VERSION_ID < 70300
			case IS_CONSTANT:
			#endif
			case IS_STRING:
				free(Z_PTR_P(element));
				break;
			case IS_ARRAY:
				gene_hash_destroy(Z_ARRVAL_P(element));
				break;
			}
			zval_ptr_dtor(element);
		}ZEND_HASH_FOREACH_END();
		free(HT_GET_DATA_ADDR(ht));
	}
	free(ht);
} /* }}} */
/*
 * {{{ static void * gene_memory_init()
 */
void gene_memory_init(TSRMLS_DC) {
	if (!GENE_G(cache)) {
		PALLOC_HASHTABLE(GENE_G(cache));
		zend_hash_init(GENE_G(cache), 8, NULL, gene_memory_zval_dtor, 1);
	}
	if (!GENE_G(cache_easy)) {
		PALLOC_HASHTABLE(GENE_G(cache_easy));
		zend_hash_init(GENE_G(cache_easy), 8, NULL, NULL, 1);
	}
	return;
}
/* }}} */

static void gene_memory_hash_copy(HashTable *target, HashTable *source) /* {{{ */{
	zend_string *key;
	zend_long idx;
	zval *element, rv;

	ZEND_HASH_FOREACH_KEY_VAL(source, idx, key, element)
	{
		gene_memory_zval_persistent(&rv, element);
		if (key) {
			gene_symtable_update(target,
					gene_str_persistent(ZSTR_VAL(key),
							ZSTR_LEN(key)), &rv);
		} else {
			zend_hash_index_update(target, idx, &rv);
		}
	}ZEND_HASH_FOREACH_END();
} /* }}} */

static void gene_memory_zval_persistent(zval *dst, zval *source) /* {{{ */{
	switch (Z_TYPE_P(source)) {
#if PHP_VERSION_ID < 70300
		case IS_CONSTANT:
#endif
	case IS_STRING:
		ZVAL_INTERNED_STR(dst, gene_str_persistent(Z_STRVAL_P(source), Z_STRLEN_P(source)));
		break;
	case IS_ARRAY: {
		gene_hash_init(dst, zend_hash_num_elements(Z_ARRVAL_P(source)));
		gene_memory_hash_copy(Z_ARRVAL_P(dst), Z_ARRVAL_P(source));
	}
		break;
	case IS_TRUE:
	case IS_FALSE:
	case IS_DOUBLE:
	case IS_LONG:
	case IS_NULL:
		ZVAL_COPY_VALUE(dst, source);
		break;
	case IS_RESOURCE:
	case IS_OBJECT:
		zend_error(E_ERROR, "An unsupported data type");
		break;
	}
} /* }}} */

/** {{{ static void * gene_memory_zval_edit_persistent(zval *zvalue)
 */
static void * gene_memory_zval_edit_persistent(zval *dst, zval *source) {
	switch (Z_TYPE_P(dst)) {
	case IS_PTR:
#if PHP_VERSION_ID < 70300
		case IS_CONSTANT:
#endif
	case IS_STRING:
		free(Z_PTR_P(dst));
		break;
	case IS_ARRAY:
		gene_hash_destroy(Z_ARRVAL_P(dst));
		break;
	}
	switch (Z_TYPE_P(source)) {
#if PHP_VERSION_ID < 70300
		case IS_CONSTANT:
#endif
	case IS_STRING:
		ZVAL_INTERNED_STR(dst,
				gene_str_persistent(Z_STRVAL_P(source), Z_STRLEN_P(source)));
		break;
	case IS_ARRAY: {
		gene_hash_init(dst, zend_hash_num_elements(Z_ARRVAL_P(source)));
		gene_memory_hash_copy(Z_ARRVAL_P(dst), Z_ARRVAL_P(source));
	}
		break;
	case IS_TRUE:
	case IS_FALSE:
	case IS_DOUBLE:
	case IS_LONG:
	case IS_NULL:
		ZVAL_COPY_VALUE(dst, source);
		break;
	case IS_RESOURCE:
	case IS_OBJECT:
		zend_error(E_ERROR, "An unsupported data type");
		break;
	}
	return NULL;
}
/* }}} */

void gene_memory_hash_copy_local(HashTable *target, HashTable *source) /* {{{ */{
	zend_string *key;
	zend_long idx;
	zval *element;
	zend_string *str_key = NULL;
	ZEND_HASH_FOREACH_KEY_VAL(source, idx, key, element)
	{
		zval rv;
		gene_memory_zval_local(&rv, element);
		if (key) {
			str_key = zend_string_init(ZSTR_VAL(key), ZSTR_LEN(key), 0);
			gene_symtable_update(target, str_key, &rv);
		} else {
			zend_hash_index_update(target, idx, &rv);
		}
	}ZEND_HASH_FOREACH_END();
} /* }}} */

zval *gene_memory_zval_local(zval *dst, zval *source) /* {{{ */
{
	zend_string *str_key = NULL;
	switch (Z_TYPE_P(source)) {
#if PHP_VERSION_ID < 70300
		case IS_CONSTANT:
#endif
	case IS_STRING:
		str_key = zend_string_init(Z_STRVAL_P(source), Z_STRLEN_P(source), 0);
		ZVAL_INTERNED_STR(dst, str_key);
		break;
	case IS_ARRAY:
		array_init(dst);
		gene_memory_hash_copy_local(Z_ARRVAL_P(dst), Z_ARRVAL_P(source));
		break;
	case IS_TRUE:
	case IS_FALSE:
	case IS_DOUBLE:
	case IS_LONG:
	case IS_NULL:
		ZVAL_COPY_VALUE(dst, source);
		break;
	case IS_RESOURCE:
	case IS_OBJECT:
		zend_error(E_ERROR, "An unsupported data type");
		break;
	}
	return dst;
} /* }}} */

/* }}} */


/** {{{ void gene_memory_set(char *keyString,int keyString_len,zval *zvalue, int validity)
 */
void gene_memory_set(char *keyString, size_t keyString_len, zval *zvalue,
		int validity) {
	zval *copyval, ret;
	zend_string *key;
	if (zvalue) {
		copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
		if (copyval == NULL) {
			gene_memory_zval_persistent(&ret, zvalue);
			key = gene_str_persistent(keyString, keyString_len);
			gene_symtable_update(GENE_G(cache), key, &ret);
			zend_string_free(key);
			return;
		}
		gene_memory_zval_edit_persistent(copyval, zvalue);
	}
}
/* }}} */

/** {{{ void gene_memory_get(char *keyString, size_t keyString_len)
 */
zval * gene_memory_get(char *keyString, size_t keyString_len) {
	zval *zvalue;
	zvalue = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	return zvalue;
}
/* }}} */

/** {{{ void gene_memory_get_quick(char *keyString, int keyString_len)
 */
zval * gene_memory_get_quick(char *keyString, size_t keyString_len) {
	return zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
}
/* }}} */

/** {{{ zval * gene_memory_get_by_config(char *keyString, int keyString_len,char *path)
 */
zval * gene_memory_get_by_config(char *keyString, size_t keyString_len, char *path) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp = NULL;
	zval *ret = NULL;
	zval *copyval = NULL;

	copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (copyval) {
		tmp = copyval;
		if (path != NULL) {
			seg = php_strtok_r(path, "/", &ptr);
			while (seg) {
				if (Z_TYPE_P(tmp) != IS_ARRAY) {
					return NULL;
				}
				tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
				if (tmp == NULL) {
					return NULL;
				}
				seg = php_strtok_r(NULL, "/", &ptr);
			}
		}
		return tmp;
	}
	return NULL;
}
/* }}} */

/** {{{ filenode * file_cache_get_easy(char *keyString, int keyString_len)
 */
filenode * file_cache_get_easy(char *keyString, size_t keyString_len) {
	return zend_hash_str_find_ptr(GENE_G(cache_easy), keyString, keyString_len);
}
/* }}} */

/** {{{ void file_cache_set_val(char *val, size_t keyString_len, int times, int validity)
 */
void file_cache_set_val(char *val, size_t keyString_len, zend_long times,
		int validity) {
	filenode n;
	n.stime = time(NULL);
	n.ftime = times;
	n.validity = validity;
	n.status = 0;
	zend_hash_update_mem(GENE_G(cache_easy), gene_str_persistent(val, keyString_len), &n, sizeof(filenode));
}
/* }}} */

/** {{{ static zval * gene_memory_set_val(char *keyString, int keyString_len)
 */
static zval * gene_memory_set_val(zval *val, char *keyString, size_t keyString_len, zval *zvalue) {
	zval tmp, *copyval;
	zend_string *keyS = NULL;
	if (val == NULL) {
		return NULL;
	}
	copyval = zend_symtable_str_find(Z_ARRVAL_P(val), keyString, keyString_len);
	if (copyval == NULL) {
		if (zvalue) {
			gene_memory_zval_persistent(&tmp, zvalue);
		} else {
			gene_hash_init(&tmp, 1);
		}
		keyS = zend_string_init(keyString, keyString_len, 1);
		copyval = gene_symtable_update(Z_ARRVAL_P(val), keyS, &tmp);
		zval_ptr_dtor(&tmp);
	} else {
		if (zvalue) {
			gene_memory_zval_edit_persistent(copyval, zvalue);
		}
		//gene_memory_zval_persistent(&tmp, zvalue);
		//keyS = zend_string_init(keyString, keyString_len, 1);
		//return gene_symtable_update(Z_ARRVAL_P(val), keyS, &tmp);
		/*
		 else {
		 if (Z_TYPE_P(copyval) != IS_ARRAY) {
		 gene_hash_init(copyval, 8);
		 }
		 }
		 */
	}
	return copyval;
}
/* }}} */

/** {{{ void gene_memory_set_by_router(char *keyString, int keyString_len, char *path, zval *zvalue, int validity)
 */
void gene_memory_set_by_router(char *keyString, size_t keyString_len, char *path, zval *zvalue, int validity) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp;
	zval *copyval = NULL, ret;
	zend_string *keyS = NULL;
	copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (copyval == NULL) {
		gene_hash_init(&ret, 0);
		keyS = zend_string_init(keyString, keyString_len, 1);
		gene_symtable_update(GENE_G(cache), keyS, &ret);
		tmp = &ret;
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
		zval_ptr_dtor(&ret);
	} else {
		tmp = copyval;
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
	}
	return;
}
/* }}} */

/** {{{ void gene_memory_exists(char *keyString, int keyString_len)
 */
int gene_memory_exists(char *keyString, size_t keyString_len) {
	if (zend_symtable_str_exists(GENE_G(cache), keyString, keyString_len) != 1) {
		return 0;
	}
	return 1;
}
/* }}} */

/** {{{ void gene_memory_getTime(char *keyString, size_t keyString_len,gene_memory_container **zvalue)
 */
zend_long gene_memory_getTime(char *keyString, size_t keyString_len) {
	zval *zvalue = NULL;
	zvalue = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (zvalue && Z_TYPE_P(zvalue) == IS_LONG) {
		return Z_LVAL_P(zvalue);
	}
	return 0;
}
/* }}} */

/** {{{ void gene_memory_del(char *keyString, size_t keyString_len)
 */
int gene_memory_del(char *keyString, size_t keyString_len) {
	if (zend_symtable_str_del(GENE_G(cache), keyString, keyString_len) == 0) {
		return 1;
	}
	return 0;
}
/* }}} */

/*
 * {{{ public gene_memory::__construct()
 */
PHP_METHOD(gene_memory, __construct) {

	zval *safe = NULL;
	int len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		return;
	}

	if (safe) {
		zend_update_property_string(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), Z_STRVAL_P(safe));
	} else {
		if (GENE_G(app_key)) {
			zend_update_property_string(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), GENE_G(app_key));
		} else {
			gene_ini_router();
			zend_update_property_string(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), GENE_G(directory));
		}
	}
}
/* }}} */

/*
 * {{{ public gene_memory::set($key, $data)
 */
PHP_METHOD(gene_memory, set) {
	zend_string *keyString = NULL;
	long validity = 0;
	zval *zvalue, *safe;
	smart_str router_e = {0};
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz|l", &keyString, &zvalue, &validity) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		smart_str_appends(&router_e, Z_STRVAL_P(safe));
	}
	smart_str_appends(&router_e, ":");
	smart_str_appendl(&router_e, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
	smart_str_0(&router_e);
	if (zvalue) {
		gene_memory_set(router_e.s->val, router_e.s->len, zvalue, validity);
	}
	smart_str_free(&router_e);
	RETURN_BOOL(1);
}
/* }}} */

/*
 * {{{ public gene_memory::get($key)
 */
PHP_METHOD(gene_memory, get) {
	zend_string *keyString;
	char *router_e;
	size_t router_e_len;
	zval *zvalue, *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe),
				ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	zvalue = gene_memory_get(router_e, router_e_len);
	efree(router_e);
	if (zvalue) {
		gene_memory_zval_local(return_value, zvalue);
		return;
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_memory::getTime($key)
 */
PHP_METHOD(gene_memory, getTime) {
	zend_string *keyString;
	char *router_e;
	size_t router_e_len;
	zend_long ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe),
				ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	ret = gene_memory_getTime(router_e, router_e_len);
	efree(router_e);
	RETURN_LONG(ret);
}
/* }}} */

/*
 * {{{ public gene_memory::exists($key)
 */
PHP_METHOD(gene_memory, exists) {
	zend_string *keyString;
	char *router_e;
	size_t router_e_len;
	long ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe), ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	ret = gene_memory_exists(router_e, router_e_len);
	efree(router_e);
	RETURN_BOOL(ret);
}
/* }}} */

/*
 * {{{ public gene_memory::del($key)
 */
PHP_METHOD(gene_memory, del) {
	zend_string *keyString;
	char *router_e;
	size_t router_e_len;
	long ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe), ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	ret = gene_memory_del(router_e, router_e_len);
	efree(router_e);
	RETURN_BOOL(ret);
}
/* }}} */

/*
 * {{{ public gene_memory::clean()
 */
PHP_METHOD(gene_memory, clean) {
	if (GENE_G(cache)) {
		gene_hash_destroy(GENE_G(cache));
		GENE_G(cache) = NULL;
	}
	gene_memory_init();
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_memory_methods
 */
zend_function_entry gene_memory_methods[] = {
	PHP_ME(gene_memory, __construct, gene_memory_arg_construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_memory, set, gene_memory_arg_set, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, get, gene_memory_arg_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, getTime, gene_memory_arg_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, exists, gene_memory_arg_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, del, gene_memory_arg_del, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, clean, gene_memory_void_arginfo, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(memory) {
	zend_class_entry gene_memory;
	GENE_INIT_CLASS_ENTRY(gene_memory, "Gene_Memory", "Gene\\Memory", gene_memory_methods);
	gene_memory_ce = zend_register_internal_class(&gene_memory);

	//debug
	zend_declare_property_string(gene_memory_ce, GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), "", ZEND_ACC_PUBLIC);
	//
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
