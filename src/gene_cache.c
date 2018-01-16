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

#include "php_gene.h"
#include "gene_application.h"
#include "gene_cache.h"
#include "gene_common.h"

zend_class_entry * gene_cache_ce;

static void gene_cache_hash_copy(HashTable *target, HashTable *source);
static void gene_cache_zval_persistent(zval *dst, zval *source TSRMLS_DC);
static void gene_cache_copy_persistent(HashTable *pdst,
		HashTable *src TSRMLS_DC);
static void gene_cache_copy_losable(HashTable *pdst, HashTable *src TSRMLS_DC);

/* }}} */

static zend_string* gene_str_persistent(char *str, size_t len) /* {{{ */{
	zend_string *key = zend_string_init(str, len, 1);
	if (key == NULL) {
		zend_error(E_ERROR, "Cannot allocate string, not enough memory?");
	}
	key->h = zend_string_hash_val(key);
	GC_FLAGS(key) |= IS_STR_INTERNED | IS_STR_PERMANENT;
	return key;
}
/* }}} */

/** {{{ static void gene_cache_zval_dtor(zval **value)
 */
static void gene_cache_zval_dtor(zval *zvalue) {
	switch (Z_TYPE_P(zvalue)) {
	case IS_PTR:
	case IS_CONSTANT:
	case IS_STRING:
		free(Z_PTR_P(zvalue));
		break;
	case IS_ARRAY:
		gene_hash_destroy(Z_ARRVAL_P(zvalue));
		break;
	}
	zval_ptr_dtor(zvalue);
}
/* }}} */

static zval* gene_symtable_update(HashTable *ht, zend_string *key, zval *zv) /* {{{ */{
	zend_ulong idx;
	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		zend_string_release(key);
		return zend_hash_index_update(ht, idx, zv);
	} else {
		return zend_hash_update(ht, key, zv);
	}
}
/* }}} */

static void gene_hash_init(zval *zv, size_t size) /* {{{ */{
	HashTable *ht;
	PALLOC_HASHTABLE(ht);
	zend_hash_init(ht, size, NULL, gene_cache_zval_dtor, 1);
	GC_FLAGS(ht) |= IS_ARRAY_IMMUTABLE;
	ZVAL_ARR(zv, ht);
	Z_ADDREF_P(zv);
	Z_TYPE_FLAGS_P(zv) = IS_TYPE_IMMUTABLE;
}
/* }}} */

void gene_hash_destroy(HashTable *ht) /* {{{ */{
	zend_string *key;
	zval *element;

	if (((ht)->u.flags & HASH_FLAG_INITIALIZED)) {
		ZEND_HASH_FOREACH_STR_KEY_VAL(ht, key, element)
					{
						if (key) {
							zend_string_release(key);
						}
						switch (Z_TYPE_P(element)) {
						case IS_PTR:
						case IS_CONSTANT:
						case IS_STRING:
							free(Z_PTR_P(element));
							break;
						case IS_ARRAY:
							gene_hash_destroy(Z_ARRVAL_P(element));
							break;
						}
					}ZEND_HASH_FOREACH_END();
		free(HT_GET_DATA_ADDR(ht));
	}
	free(ht);
} /* }}} */

static void gene_hash_copy(HashTable *dst, HashTable *source) /* {{{ */{
	zend_string *key;
	zend_long idx;
	zval *element, rv;

	ZEND_HASH_FOREACH_KEY_VAL(source, idx, key, element)
				{
					gene_cache_zval_persistent(&rv, element);
					if (key) {
						gene_symtable_update(dst,
								gene_str_persistent(ZSTR_VAL(key),
										ZSTR_LEN(key)), &rv);
					} else {
						zend_hash_index_update(dst, idx, &rv);
					}
				}ZEND_HASH_FOREACH_END();
} /* }}} */

/*
 * {{{ static void * gene_cache_init()
 */
void gene_cache_init() {
	if (!GENE_G(cache)) {
		PALLOC_HASHTABLE(GENE_G(cache));
		zend_hash_init(GENE_G(cache), 8, NULL, gene_cache_zval_dtor, 1);
	}
	if (!GENE_G(cache_easy)) {
		PALLOC_HASHTABLE(GENE_G(cache_easy));
		zend_hash_init(GENE_G(cache_easy), 8, NULL, NULL, 1);
	}
	return;
}
/* }}} */

static void gene_cache_hash_copy(HashTable *target, HashTable *source) /* {{{ */{
	zend_string *key;
	zend_long idx;
	zval *element, rv;

	ZEND_HASH_FOREACH_KEY_VAL(source, idx, key, element)
				{
					gene_cache_zval_persistent(&rv, element);
					if (key) {
						gene_symtable_update(target,
								gene_str_persistent(ZSTR_VAL(key),
										ZSTR_LEN(key)), &rv);
					} else {
						zend_hash_index_update(target, idx, &rv);
					}
				}ZEND_HASH_FOREACH_END();
} /* }}} */

static void gene_cache_zval_persistent(zval *dst, zval *source) /* {{{ */{
	switch (Z_TYPE_P(source)) {
	case IS_CONSTANT:
	case IS_STRING:
		ZVAL_INTERNED_STR(dst,
				gene_str_persistent(Z_STRVAL_P(source), Z_STRLEN_P(source)));
		break;
	case IS_ARRAY: {
		gene_hash_init(dst, zend_hash_num_elements(Z_ARRVAL_P(source)));
		gene_cache_hash_copy(Z_ARRVAL_P(dst), Z_ARRVAL_P(source));
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

/** {{{ static void * gene_cache_zval_edit_persistent(zval *zvalue TSRMLS_DC)
 */
static void * gene_cache_zval_edit_persistent(zval *dst, zval *source) {
	switch (Z_TYPE_P(dst)) {
	case IS_PTR:
	case IS_CONSTANT:
	case IS_STRING:
		free(Z_PTR_P(dst));
		break;
	case IS_ARRAY:
		gene_hash_destroy(Z_ARRVAL_P(dst));
		break;
	}
	switch (Z_TYPE_P(source)) {
	case IS_CONSTANT:
	case IS_STRING:
		ZVAL_INTERNED_STR(dst,
				gene_str_persistent(Z_STRVAL_P(source), Z_STRLEN_P(source)));
		break;
	case IS_ARRAY: {
		gene_hash_init(dst, zend_hash_num_elements(Z_ARRVAL_P(source)));
		gene_cache_hash_copy(Z_ARRVAL_P(dst), Z_ARRVAL_P(source));
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

void gene_cache_hash_copy_local(HashTable *target, HashTable *source) /* {{{ */{
	zend_string *key;
	zend_long idx;
	zval *element, rv;

	ZEND_HASH_FOREACH_KEY_VAL(source, idx, key, element)
				{
					gene_cache_zval_local(&rv, element);
					if (key) {
						gene_symtable_update(target,
								zend_string_init(ZSTR_VAL(key), ZSTR_LEN(key),
										0), &rv);
					} else {
						zend_hash_index_update(target, idx, &rv);
					}
				}ZEND_HASH_FOREACH_END();
} /* }}} */

void gene_cache_zval_local(zval *dst, zval *source) /* {{{ */{
	switch (Z_TYPE_P(source)) {
	case IS_CONSTANT:
	case IS_STRING:
		ZVAL_INTERNED_STR(dst,
				zend_string_init(Z_STRVAL_P(source), Z_STRLEN_P(source), 0));
		break;
	case IS_ARRAY: {
		array_init(dst);
		gene_cache_hash_copy_local(Z_ARRVAL_P(dst), Z_ARRVAL_P(source));
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

/* }}} */

/** {{{ static void gene_cache_copy_persistent(HashTable *pdst, HashTable *src TSRMLS_DC)
 */
static void gene_cache_copy_persistent(HashTable *pdst,
		HashTable *src TSRMLS_DC) {
	zval *ppzval;
	zend_string *key;
	char *keyval;
	int keyvallen;
	zend_long idx;

	ZEND_HASH_FOREACH_KEY_VAL(pdst, idx, key, ppzval)
				{
					if (key) {
						if (zend_hash_exists(src, key) != 1) {
							zend_hash_del(pdst, key);
						}
					} else {
						if (zend_hash_index_exists(src, idx) != 1) {
							zend_hash_index_del(pdst, idx);
						}
					}
				}ZEND_HASH_FOREACH_END();

	ZEND_HASH_FOREACH_KEY_VAL(src, idx, key, ppzval)
				{
					if (ppzval == NULL) {
						continue;
					}
					if (key) {
						zval *copyval, *tmp;
						copyval = zend_hash_find(pdst, key);
						if (copyval == NULL) {
							gene_cache_zval_persistent(tmp, ppzval);
							if (tmp) {
								keyvallen = spprintf(&keyval, 0, "%s", key);
								zend_hash_str_update(pdst, keyval, keyvallen,
										tmp);
								efree(keyval);
								keyval = NULL;
							}
						} else {
							gene_cache_zval_edit_persistent(copyval, ppzval);
						}
					} else {
						zval *copyval, *tmp;
						copyval = zend_hash_index_find(pdst, idx);
						if (copyval == NULL) {
							gene_cache_zval_persistent(tmp, ppzval);
							if (tmp) {
								zend_hash_index_update(pdst, idx, tmp);
							}
						} else {
							gene_cache_zval_edit_persistent(copyval, ppzval);
						}
					}
				}ZEND_HASH_FOREACH_END();
}
/* }}} */

/** {{{ void gene_cache_set(char *keyString,int keyString_len,zval *zvalue, int validity TSRMLS_DC)
 */
void gene_cache_set(char *keyString, int keyString_len, zval *zvalue,
		int validity TSRMLS_DC) {
	zval *copyval, ret;
	zend_string *key;
	if (zvalue) {
		copyval = zend_symtable_str_find(GENE_G(cache), keyString,
				keyString_len);
		if (copyval == NULL) {
			gene_cache_zval_persistent(&ret, zvalue);
			key = gene_str_persistent(keyString, keyString_len);
			gene_symtable_update(GENE_G(cache), key, &ret);
			zend_string_free(key);
			return;
		}
		gene_cache_zval_edit_persistent(copyval, zvalue TSRMLS_CC);
	}
}
/* }}} */

/** {{{ void gene_cache_get(char *keyString, int keyString_len TSRMLS_DC)
 */
zval * gene_cache_get(char *keyString, int keyString_len TSRMLS_DC) {
	zval *zvalue;
	zvalue = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	return zvalue;
}
/* }}} */

/** {{{ void gene_cache_get_quick(char *keyString, int keyString_len TSRMLS_DC)
 */
zval * gene_cache_get_quick(char *keyString, int keyString_len TSRMLS_DC) {
	return zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
}
/* }}} */

/** {{{ zval * gene_cache_get_by_config(char *keyString, int keyString_len,char *path TSRMLS_DC)
 */
zval * gene_cache_get_by_config(char *keyString, int keyString_len,
		char *path TSRMLS_DC) {
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

/** {{{ filenode * file_cache_get_easy(char *keyString, int keyString_len TSRMLS_DC)
 */
filenode * file_cache_get_easy(char *keyString, size_t keyString_len TSRMLS_DC) {
	return zend_hash_str_find_ptr(GENE_G(cache_easy), keyString, keyString_len);
}
/* }}} */

/** {{{ void file_cache_set_val(char *val, int keyString_len, int times, int validity TSRMLS_DC)
 */
void file_cache_set_val(char *val, int keyString_len, long times,
		int validity TSRMLS_DC) {
	filenode n;
	n.stime = time(NULL);
	n.ftime = times;
	n.validity = validity;
	n.status = 0;
	zend_hash_update_mem(GENE_G(cache_easy),
			gene_str_persistent(val, keyString_len), &n, sizeof(filenode));
}
/* }}} */

/** {{{ static zval * gene_cache_set_val(char *keyString, int keyString_len TSRMLS_DC)
 */
static zval * gene_cache_set_val(zval *val, char *keyString, int keyString_len,
		zval *zvalue TSRMLS_DC) {
	zval tmp, *copyval;
	if (val == NULL) {
		return NULL;
	}
	copyval = zend_symtable_str_find(Z_ARRVAL_P(val), keyString, keyString_len);
	if (copyval == NULL) {
		if (zvalue) {
			gene_cache_zval_persistent(&tmp, zvalue);
		} else {
			gene_hash_init(&tmp, 8);
		}
		return gene_symtable_update(Z_ARRVAL_P(val),
				gene_str_persistent(keyString, keyString_len), &tmp);
	} else {
		if (zvalue) {
			gene_cache_zval_edit_persistent(copyval, zvalue);
		}
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

/** {{{ void gene_cache_set_by_router(char *keyString, int keyString_len, char *path, zval *zvalue, int validity TSRMLS_DC)
 */
void gene_cache_set_by_router(char *keyString, int keyString_len, char *path,
		zval *zvalue, int validity TSRMLS_DC) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp;
	zval *copyval = NULL, ret;
	copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (copyval == NULL) {
		gene_hash_init(&ret, 1);
		gene_symtable_update(GENE_G(cache),
				gene_str_persistent(keyString, keyString_len), &ret);
		tmp = &ret;
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_cache_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_cache_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
	} else {
		tmp = copyval;
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_cache_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_cache_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
	}
	return;
}
/* }}} */

/** {{{ void gene_cache_exists(char *keyString, int keyString_len TSRMLS_DC)
 */
int gene_cache_exists(char *keyString, int keyString_len TSRMLS_DC) {
	if (zend_symtable_str_exists(GENE_G(cache), keyString, keyString_len)
			!= 1) {
		return 0;
	}
	return 1;
}
/* }}} */

/** {{{ void gene_cache_getTime(char *keyString, int keyString_len,gene_cache_container **zvalue TSRMLS_DC)
 */
long gene_cache_getTime(char *keyString, int keyString_len TSRMLS_DC) {
	zval *zvalue;
	zvalue = zend_symtable_str_find(GENE_G(cache), keyString,
			keyString_len + 1);
	if (zvalue == NULL) {
		return 0;
	}
	return 0;
}
/* }}} */

/** {{{ void gene_cache_del(char *keyString, int keyString_len TSRMLS_DC)
 */
int gene_cache_del(char *keyString, int keyString_len TSRMLS_DC) {
	if (zend_symtable_str_del(GENE_G(cache), keyString, keyString_len) == 0) {
		return 1;
	}
	return 0;
}
/* }}} */

/*
 * {{{ public gene_cache::__construct()
 */
PHP_METHOD(gene_cache, __construct) {

	zval *safe = NULL;
	int len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		return;
	}

	if (safe) {
		zend_update_property_string(gene_cache_ce, getThis(), GENE_CACHE_SAFE,
				strlen(GENE_CACHE_SAFE), Z_STRVAL_P(safe));
	} else {
		if (GENE_G(app_key)) {
			zend_update_property_string(gene_cache_ce, getThis(),
			GENE_CACHE_SAFE, strlen(GENE_CACHE_SAFE), GENE_G(app_key));
		} else {
			gene_ini_router();
			zend_update_property_string(gene_cache_ce, getThis(),
			GENE_CACHE_SAFE, strlen(GENE_CACHE_SAFE), GENE_G(directory));
		}
	}
}
/* }}} */

/*
 * {{{ public gene_cache::set($key, $data)
 */
PHP_METHOD(gene_cache, set) {
	zend_string *keyString;
	char *router_e;
	int validity = 0, router_e_len;
	zval *zvalue, *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "Sz|l", &keyString,
			&zvalue, &validity) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_cache_ce, getThis(), GENE_CACHE_SAFE,
			strlen(GENE_CACHE_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe),
				ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	if (zvalue) {
		gene_cache_set(router_e, router_e_len, zvalue, validity TSRMLS_CC);
	}
	efree(router_e);
	RETURN_BOOL(1);
}
/* }}} */

/*
 * {{{ public gene_cache::get($key)
 */
PHP_METHOD(gene_cache, get) {
	zend_string *keyString;
	char *router_e;
	int router_e_len;
	zval *zvalue, *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &keyString)
			== FAILURE) {
		return;
	}
	safe = zend_read_property(gene_cache_ce, getThis(), GENE_CACHE_SAFE,
			strlen(GENE_CACHE_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe),
				ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	zvalue = gene_cache_get(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	if (zvalue) {
		gene_cache_zval_local(return_value, zvalue);
		return;
	}
	RETURN_NULL()
	;
}
/* }}} */

/*
 * {{{ public gene_cache::getTime($key)
 */
PHP_METHOD(gene_cache, getTime) {
	zend_string *keyString;
	char *router_e;
	int router_e_len, ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &keyString)
			== FAILURE) {
		return;
	}
	safe = zend_read_property(gene_cache_ce, getThis(), GENE_CACHE_SAFE,
			strlen(GENE_CACHE_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe),
				ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	ret = gene_cache_getTime(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	RETURN_LONG(ret);
}
/* }}} */

/*
 * {{{ public gene_cache::exists($key)
 */
PHP_METHOD(gene_cache, exists) {
	zend_string *keyString;
	char *router_e;
	int router_e_len, ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &keyString)
			== FAILURE) {
		return;
	}
	safe = zend_read_property(gene_cache_ce, getThis(), GENE_CACHE_SAFE,
			strlen(GENE_CACHE_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe),
				ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	ret = gene_cache_exists(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	RETURN_BOOL(ret);
}
/* }}} */

/*
 * {{{ public gene_cache::del($key)
 */
PHP_METHOD(gene_cache, del) {
	zend_string *keyString;
	char *router_e;
	int router_e_len, ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &keyString)
			== FAILURE) {
		return;
	}
	safe = zend_read_property(gene_cache_ce, getThis(), GENE_CACHE_SAFE,
			strlen(GENE_CACHE_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s:%s", Z_STRVAL_P(safe),
				ZSTR_VAL(keyString));
	} else {
		router_e_len = spprintf(&router_e, 0, ":%s", ZSTR_VAL(keyString));
	}
	ret = gene_cache_del(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	RETURN_BOOL(ret);
}
/* }}} */

/*
 * {{{ public gene_cache::clean()
 */
PHP_METHOD(gene_cache, clean) {
	if (GENE_G(cache)) {
		gene_hash_destroy(GENE_G(cache));
		GENE_G(cache) = NULL;
	}
	gene_cache_init();
	RETURN_TRUE
	;
}
/* }}} */

/*
 * {{{ gene_cache_methods
 */
zend_function_entry gene_cache_methods[] = {
	PHP_ME(gene_cache, set, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_cache, get, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_cache, getTime, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_cache, exists, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_cache, del, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_cache, clean, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_cache, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(cache) {
	zend_class_entry gene_cache;
	GENE_INIT_CLASS_ENTRY(gene_cache, "Gene_Cache", "Gene\\Cache",
			gene_cache_methods);
	gene_cache_ce = zend_register_internal_class(&gene_cache TSRMLS_CC);

	//debug
	zend_declare_property_string(gene_cache_ce, GENE_CACHE_SAFE,
			strlen(GENE_CACHE_SAFE), "", ZEND_ACC_PUBLIC);
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
