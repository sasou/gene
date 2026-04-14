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
#include "zend_smart_str.h" /* for smart_str */

#include "ext/standard/md5.h"

#include "../gene.h"
#include "../cache/cache.h"
#include "../common/common.h"
#include "../di/di.h"
#include "../factory/factory.h"

 static zend_string *gene_cache_method_get_name(void) /*{{{*/
 {
 	static zend_string *method = NULL;
 	if (!method) {
 		method = zend_string_init_interned("get", sizeof("get") - 1, 1);
 	}
 	return method;
 }/*}}}*/

 static zend_string *gene_cache_method_set_name(void) /*{{{*/
 {
 	static zend_string *method = NULL;
 	if (!method) {
 		method = zend_string_init_interned("set", sizeof("set") - 1, 1);
 	}
 	return method;
 }/*}}}*/

 static zend_string *gene_cache_method_incr_name(void) /*{{{*/
 {
 	static zend_string *method = NULL;
 	if (!method) {
 		method = zend_string_init_interned("incr", sizeof("incr") - 1, 1);
 	}
 	return method;
 }/*}}}*/

 static zend_string *gene_cache_method_delete_name(void) /*{{{*/
 {
 	static zend_string *method = NULL;
 	if (!method) {
 		method = zend_string_init_interned("delete", sizeof("delete") - 1, 1);
 	}
 	return method;
 }/*}}}*/

 static zend_string *gene_cache_function_apcu_store_name(void) /*{{{*/
 {
 	static zend_string *function_name = NULL;
 	if (!function_name) {
 		function_name = zend_string_init_interned("apcu_store", sizeof("apcu_store") - 1, 1);
 	}
 	return function_name;
 }/*}}}*/

 static zend_string *gene_cache_function_apcu_fetch_name(void) /*{{{*/
 {
 	static zend_string *function_name = NULL;
 	if (!function_name) {
 		function_name = zend_string_init_interned("apcu_fetch", sizeof("apcu_fetch") - 1, 1);
 	}
 	return function_name;
 }/*}}}*/

 static zend_string *gene_cache_function_apcu_delete_name(void) /*{{{*/
 {
 	static zend_string *function_name = NULL;
 	if (!function_name) {
 		function_name = zend_string_init_interned("apcu_delete", sizeof("apcu_delete") - 1, 1);
 	}
 	return function_name;
 }/*}}}*/

 static inline size_t gene_hash_len(int hash_mode, size_t input_len);
 static inline size_t gene_hash_write(int hash_mode, const char *data, size_t len, char *dst);
 static uint32_t gene_cache_version_field_count(zval *versionField, int include_top);
 static zend_always_inline void gene_cache_build_version_payload(zval *payload, zval *data, zval *version);

 static int gene_cache_can_fast_append_scalar(zval *element) /*{{{*/
 {
 	if (element == NULL) {
 		return 1;
 	}

 	switch (Z_TYPE_P(element)) {
 		case IS_NULL:
 		case IS_FALSE:
 		case IS_TRUE:
 		case IS_LONG:
 		case IS_DOUBLE:
 		case IS_STRING:
 		case IS_OBJECT:
 			return 1;
 		default:
 			return 0;
 	}
 }/*}}}*/

 static int gene_cache_can_fast_build_key(zval *object, zval *args) /*{{{*/
 {
 	zval *class = NULL, *method = NULL, *element = NULL;

 	if (Z_TYPE_P(object) != IS_ARRAY) {
 		return 0;
 	}

 	class = zend_hash_index_find(Z_ARRVAL_P(object), 0);
 	method = zend_hash_index_find(Z_ARRVAL_P(object), 1);
 	if (!class || !method || Z_TYPE_P(method) != IS_STRING) {
 		return 0;
 	}

 	if (Z_TYPE_P(class) != IS_STRING && Z_TYPE_P(class) != IS_OBJECT) {
 		return 0;
 	}

 	if (args != NULL && Z_TYPE_P(args) == IS_ARRAY) {
 		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), element)
 		{
 			if (!gene_cache_can_fast_append_scalar(element)) {
 				return 0;
 			}
 		}ZEND_HASH_FOREACH_END();
 	}

 	return 1;
 }/*}}}*/

 static void gene_cache_append_scalar_to_buf(char **p, zval *element) /*{{{*/
 {
 	if (element == NULL) {
 		return;
 	}

 	switch (Z_TYPE_P(element)) {
 		case IS_STRING:
 			memcpy(*p, Z_STRVAL_P(element), Z_STRLEN_P(element));
 			*p += Z_STRLEN_P(element);
 			return;
 		case IS_LONG:
 			*p += snprintf(*p, 32, ZEND_LONG_FMT, Z_LVAL_P(element));
 			return;
 		case IS_DOUBLE:
 			*p += snprintf(*p, 64, "%.*G", 14, Z_DVAL_P(element));
 			return;
 		case IS_TRUE:
 			*(*p)++ = '1';
 			return;
 		case IS_FALSE:
 		case IS_NULL:
 			return;
 		case IS_OBJECT: {
 			zend_string *name = Z_OBJCE_P(element)->name;
 			memcpy(*p, ZSTR_VAL(name), ZSTR_LEN(name));
 			*p += ZSTR_LEN(name);
 			return;
 		}
 		default:
 			return;
 	}
 }/*}}}*/

 static int gene_cache_try_build_simple_key(zval *sign, int type, zval *object, zval *args, zval *ttl, zval *retval, int hash_mode) /*{{{*/
 {
 	char stack_buf[512];
 	char *buf = stack_buf;
 	char *p = buf;
 	size_t content_len = 0;
 	size_t total_needed = 0;
 	int heap = 0;
 	zval *class = NULL, *method = NULL, *element = NULL;
 	size_t sign_len, prefix_len, hash_len;
 	zend_string *result;

 	if (!gene_cache_can_fast_build_key(object, args)) {
 		return 0;
 	}

 	class = zend_hash_index_find(Z_ARRVAL_P(object), 0);
 	method = zend_hash_index_find(Z_ARRVAL_P(object), 1);

 	if (Z_TYPE_P(class) == IS_OBJECT) {
 		total_needed += ZSTR_LEN(Z_OBJCE_P(class)->name);
 	} else {
 		total_needed += Z_STRLEN_P(class);
 	}
 	total_needed += 1 + Z_STRLEN_P(method);

 	if (args != NULL && Z_TYPE_P(args) == IS_ARRAY) {
 		total_needed += 1;
 		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), element)
 		{
 			switch (Z_TYPE_P(element)) {
 				case IS_STRING:
 					total_needed += Z_STRLEN_P(element);
 					break;
 				case IS_LONG:
 					total_needed += 32;
 					break;
 				case IS_DOUBLE:
 					total_needed += 64;
 					break;
 				case IS_OBJECT:
 					total_needed += ZSTR_LEN(Z_OBJCE_P(element)->name);
 					break;
 				default:
 					break;
 			}
 			total_needed += 1;
 		}ZEND_HASH_FOREACH_END();
 	}

 	if (ttl != NULL && Z_TYPE_P(ttl) == IS_LONG) {
 		total_needed += 1 + 32;
 	}

 	if (total_needed >= sizeof(stack_buf)) {
 		buf = emalloc(total_needed + 1);
 		p = buf;
 		heap = 1;
 	}

 	if (Z_TYPE_P(class) == IS_OBJECT) {
 		zend_string *class_name = Z_OBJCE_P(class)->name;
 		memcpy(p, ZSTR_VAL(class_name), ZSTR_LEN(class_name));
 		p += ZSTR_LEN(class_name);
 	} else {
 		memcpy(p, Z_STRVAL_P(class), Z_STRLEN_P(class));
 		p += Z_STRLEN_P(class);
 	}

 	*p++ = '.';
 	memcpy(p, Z_STRVAL_P(method), Z_STRLEN_P(method));
 	p += Z_STRLEN_P(method);

 	if (args != NULL && Z_TYPE_P(args) == IS_ARRAY) {
 		*p++ = ':';
 		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), element)
 		{
 			gene_cache_append_scalar_to_buf(&p, element);
 			*p++ = '-';
 		}ZEND_HASH_FOREACH_END();
 	}

 	if (ttl != NULL && Z_TYPE_P(ttl) == IS_LONG) {
 		*p++ = ':';
 		p += snprintf(p, 32, ZEND_LONG_FMT, Z_LVAL_P(ttl));
 	}

 	*p = '\0';
 	content_len = (size_t)(p - buf);

 	sign_len = Z_STRLEN_P(sign);
 	prefix_len = (type > 0) ? (sizeof(GENE_CACHE_TMP) - 1) : 0;
 	hash_len = gene_hash_len(hash_mode, content_len);
 	result = zend_string_alloc(sign_len + prefix_len + hash_len, 0);

 	p = ZSTR_VAL(result);
 	memcpy(p, Z_STRVAL_P(sign), sign_len);
 	p += sign_len;
 	if (type > 0) {
 		memcpy(p, GENE_CACHE_TMP, sizeof(GENE_CACHE_TMP) - 1);
 		p += sizeof(GENE_CACHE_TMP) - 1;
 	}
 	gene_hash_write(hash_mode, buf, content_len, p);
 	ZSTR_VAL(result)[sign_len + prefix_len + hash_len] = '\0';

 	if (heap) {
 		efree(buf);
 	}

 	ZVAL_STR(retval, result);
 	return 1;
 }/*}}}*/

/* Direct-write hash helpers: write hash output directly into dst buffer,
 * avoiding intermediate zend_string + zval allocation. Return bytes written. */

static size_t gene_md5_write(const char *data, size_t len, char *dst) /*{{{*/
{
	PHP_MD5_CTX ctx;
	unsigned char digest[16];
	PHP_MD5Init(&ctx);
	if (len != 0) {
		PHP_MD5Update(&ctx, data, len);
	}
	PHP_MD5Final(digest, &ctx);
	make_digest_ex(dst, digest, 16);
	return 32;
}/*}}}*/

static size_t gene_hash_fast_write(const char *data, size_t len, char *dst) /*{{{*/
{
	uint64_t hash = gene_fnv1a_64(data, len);
	gene_u64_to_hex(hash, dst);
	return 16;
}/*}}}*/

static size_t gene_xxhash64_write(const char *data, size_t len, char *dst) /*{{{*/
{
	uint64_t hash = gene_xxhash64(data, len);
	gene_u64_to_hex(hash, dst);
	return 16;
}/*}}}*/

static size_t gene_farmhash64_write(const char *data, size_t len, char *dst) /*{{{*/
{
	uint64_t hash = gene_farmhash64(data, len);
	gene_u64_to_hex(hash, dst);
	return 16;
}/*}}}*/

static size_t gene_murmur3_32_write(const char *data, size_t len, char *dst) /*{{{*/
{
	uint32_t hash = gene_murmur3_32(data, len);
	gene_u32_to_hex(hash, dst);
	return 8;
}/*}}}*/

static size_t gene_turbo_hash32_write(const char *data, size_t len, char *dst) /*{{{*/
{
	uint32_t hash = gene_turbo_hash32(data, len);
	gene_u32_to_hex(hash, dst);
	return 8;
}/*}}}*/

/* Compute hash output length for given mode and input length */
static inline size_t gene_hash_len(int hash_mode, size_t input_len) {
	if (hash_mode == 5) return 8; /* TurboHash32 */
	if (hash_mode == 4) return 8; /* MurmurHash3 */
	if (hash_mode == 3) return 16; /* FarmHash64 */
	if (hash_mode == 2) return 16; /* xxHash64 */
	if (hash_mode == 1) return 16; /* FNV-1a */
	return 32; /* MD5 */
}

/* Write hash into dst using given mode. Returns bytes written. */
static inline size_t gene_hash_write(int hash_mode, const char *data, size_t len, char *dst) {
	if (hash_mode == 5) return gene_turbo_hash32_write(data, len, dst);
	if (hash_mode == 4) return gene_murmur3_32_write(data, len, dst);
	if (hash_mode == 3) return gene_farmhash64_write(data, len, dst);
	if (hash_mode == 2) return gene_xxhash64_write(data, len, dst);
	if (hash_mode == 1) return gene_hash_fast_write(data, len, dst);
	return gene_md5_write(data, len, dst);
}

zend_class_entry * gene_cache_ce;
uint32_t gene_cache_offset_config;

ZEND_BEGIN_ARG_INFO_EX(gene_cache_construct_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, configs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_cached_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, args)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_locaclCached_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, args)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_unsetcached_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, args)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_unsetlocalcached_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, args)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_cachedversion_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, args)
	ZEND_ARG_INFO(0, version)
	ZEND_ARG_INFO(0, ttl)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_localcachedversion_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, obj)
	ZEND_ARG_INFO(0, args)
	ZEND_ARG_INFO(0, version)
	ZEND_ARG_INFO(0, ttl)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_getversion_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, version)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_cache_updateversion_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, version)
ZEND_END_ARG_INFO()


void gene_cache_get(zval *object, zval *key, zval *retval) /*{{{*/
{
	zend_string *method = gene_cache_method_get_name();
	zend_function *fn = zend_hash_find_ptr(&Z_OBJCE_P(object)->function_table, method);
	zval params[] = { *key };
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 1, params, NULL);
	} else {
		zval function_name;
		ZVAL_STR(&function_name, zend_string_copy(method));
		call_user_function(NULL, object, &function_name, retval, 1, params);
		zval_ptr_dtor(&function_name);
	}
}/*}}}*/

void gene_cache_set(zval *object, zval *key, zval *value, zval *ttl, zval *retval) /*{{{*/
{
	zend_string *method = gene_cache_method_set_name();
	zend_function *fn = zend_hash_find_ptr(&Z_OBJCE_P(object)->function_table, method);
	zval params[] = { *key, *value, *ttl };
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 3, params, NULL);
	} else {
		zval function_name;
		ZVAL_STR(&function_name, zend_string_copy(method));
		call_user_function(NULL, object, &function_name, retval, 3, params);
		zval_ptr_dtor(&function_name);
	}
}/*}}}*/

void gene_cache_incr(zval *object, zval *key, zval *val, zval *retval) /*{{{*/
{
	zend_string *method = gene_cache_method_incr_name();
	zend_function *fn = zend_hash_find_ptr(&Z_OBJCE_P(object)->function_table, method);
	zval params[] = { *key, *val };
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 2, params, NULL);
	} else {
		zval function_name;
		ZVAL_STR(&function_name, zend_string_copy(method));
		call_user_function(NULL, object, &function_name, retval, 2, params);
		zval_ptr_dtor(&function_name);
	}
}/*}}}*/

void gene_cache_del(zval *object, zval *key, zval *retval) /*{{{*/
{
	zend_string *method = gene_cache_method_delete_name();
	zend_function *fn = zend_hash_find_ptr(&Z_OBJCE_P(object)->function_table, method);
	zval params[] = { *key };
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 1, params, NULL);
	} else {
		zval function_name;
		ZVAL_STR(&function_name, zend_string_copy(method));
		call_user_function(NULL, object, &function_name, retval, 1, params);
		zval_ptr_dtor(&function_name);
	}
}/*}}}*/

void gene_apcu_store(zval *key, zval *value, zval *ttl, zval *retval) /*{{{*/
{
	zend_string *name = gene_cache_function_apcu_store_name();
	zend_function *fn = zend_hash_find_ptr(CG(function_table), name);
	zval params[] = { *key, *value, *ttl };
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, NULL, NULL, retval, 3, params, NULL);
	} else {
		ZVAL_FALSE(retval);
	}
}/*}}}*/


void gene_apcu_fetch(zval *key, zval *retval) /*{{{*/
{
	zend_string *name = gene_cache_function_apcu_fetch_name();
	zend_function *fn = zend_hash_find_ptr(CG(function_table), name);
	zval params[] = { *key };
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
	} else {
		ZVAL_FALSE(retval);
	}
}/*}}}*/

void gene_apcu_del(zval *key, zval *retval) /*{{{*/
{
	zend_string *name = gene_cache_function_apcu_delete_name();
	zend_function *fn = zend_hash_find_ptr(CG(function_table), name);
	zval params[] = { *key };
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
	} else {
		ZVAL_FALSE(retval);
	}
}/*}}}*/

/* hash_mode: 0=MD5 (default), 1=fast FNV-1a, 2=xxHash64, 3=FarmHash64, 4=MurmurHash3, 5=TurboHash32 */
void gene_cache_key(zval *sign, int type, zval *object, zval *args, zval *ttl, zval *retval, int hash_mode) /*{{{*/
{
	smart_str tmp_s = {0};
	zval *class = NULL, *method = NULL;

	if (gene_cache_try_build_simple_key(sign, type, object, args, ttl, retval, hash_mode)) {
		return;
	}
	
	smart_str_alloc(&tmp_s, 128, 0);
	
	if (Z_TYPE_P(object) == IS_ARRAY) {
		class = zend_hash_index_find(Z_ARRVAL_P(object), 0);
		method = zend_hash_index_find(Z_ARRVAL_P(object), 1);
		if (Z_TYPE_P(class) == IS_OBJECT) {
			zend_string *class_name = Z_OBJCE_P(class)->name;
			smart_str_appendl(&tmp_s, ZSTR_VAL(class_name), ZSTR_LEN(class_name));
		} else if (Z_TYPE_P(class) == IS_STRING) {
			smart_str_appendl(&tmp_s, Z_STRVAL_P(class), Z_STRLEN_P(class));
		}
		smart_str_appendc(&tmp_s, '.');
		if (Z_TYPE_P(method) == IS_STRING) {
			smart_str_appendl(&tmp_s, Z_STRVAL_P(method), Z_STRLEN_P(method));
		}
	}
	if (Z_TYPE_P(args) == IS_ARRAY) {
		smart_str_appendc(&tmp_s, ':');
		makeArgsArr(args, &tmp_s);
	}
	if (ttl != NULL && Z_TYPE_P(ttl) == IS_LONG) {
		smart_str_appendc(&tmp_s, ':');
		smart_str_append_long(&tmp_s, Z_LVAL_P(ttl));
	}
	smart_str_0(&tmp_s);
	
	/* Compute final key = sign + [prefix] + hash, writing hash directly into result */
	size_t sign_len = Z_STRLEN_P(sign);
	size_t prefix_len = (type > 0) ? (sizeof(GENE_CACHE_TMP) - 1) : 0;
	size_t input_len = tmp_s.s ? ZSTR_LEN(tmp_s.s) : 0;
	size_t hash_len = gene_hash_len(hash_mode, input_len);
	size_t total_len = sign_len + prefix_len + hash_len;
	
	zend_string *result = zend_string_alloc(total_len, 0);
	char *p = ZSTR_VAL(result);
	memcpy(p, Z_STRVAL_P(sign), sign_len);
	p += sign_len;
	if (type > 0) {
		memcpy(p, GENE_CACHE_TMP, sizeof(GENE_CACHE_TMP) - 1);
		p += sizeof(GENE_CACHE_TMP) - 1;
	}
	gene_hash_write(hash_mode, tmp_s.s ? ZSTR_VAL(tmp_s.s) : "", input_len, p);
	ZSTR_VAL(result)[total_len] = '\0';
	
	smart_str_free(&tmp_s);
	ZVAL_STR(retval, result);
}/*}}}*/

void makeArgsArr(zval *arr, smart_str *tmp_s) { /*{{{*/
	zend_ulong indexs;
	zend_string *id = NULL;
	zval *element = NULL;
	ZEND_HASH_FOREACH_KEY_VAL_IND(Z_ARRVAL_P(arr), indexs, id, element)
	{
		makeArgsKey(indexs, id, element, tmp_s);
		smart_str_appendc(tmp_s,  '-');
	}ZEND_HASH_FOREACH_END();
}
/*}}}*/
void convert_val_to_string(zval *element, smart_str *tmp_s) { /*{{{*/
	switch (Z_TYPE_P(element)) {
		case IS_LONG:
			smart_str_append_long(tmp_s, Z_LVAL_P(element));
			return;
		case IS_DOUBLE: {
			char buf[64];
			int len = snprintf(buf, sizeof(buf), "%.*G", 14, Z_DVAL_P(element));
			smart_str_appendl(tmp_s, buf, (size_t)len);
			return;
		}
		case IS_TRUE:
			smart_str_appendc(tmp_s, '1');
			return;
		case IS_FALSE:
		case IS_NULL:
			return;
		default: {
			zval tmp;
			ZVAL_COPY(&tmp, element);
			convert_to_string(&tmp);
			smart_str_appendl(tmp_s, Z_STRVAL(tmp), Z_STRLEN(tmp));
			zval_ptr_dtor(&tmp);
		}
	}
}
/*}}}*/
void makeArgsKey(zend_ulong indexs, zend_string *id, zval *element, smart_str *tmp_s) { /*{{{*/
	if (id != NULL) {
		smart_str_appendl(tmp_s,  ZSTR_VAL(id), ZSTR_LEN(id));
	} else {
		smart_str_append_long(tmp_s,  indexs);
	}
	if (element != NULL) {
		smart_str_appendc(tmp_s,  '.');
		switch (Z_TYPE_P(element)) {
			case IS_STRING:
				smart_str_appendl(tmp_s,  Z_STRVAL_P(element), Z_STRLEN_P(element));
				break;
			case IS_ARRAY:
				makeArgsArr(element, tmp_s);
				break;
			case IS_OBJECT: {
				smart_str_appendl(tmp_s,  ZSTR_VAL(Z_OBJCE_P(element)->name), ZSTR_LEN(Z_OBJCE_P(element)->name));
				break;
			}
			case IS_REFERENCE:
				break;
			default:
				convert_val_to_string(element, tmp_s);
				break;
		}
	}
}
/*}}}*/
void gene_cache_call(zval *object, zval *args, zval *retval) /*{{{*/
{
	zval *class = NULL, *method = NULL, *element = NULL;
	zval stack_params[8];
	zval *params = stack_params;
	zval tmp_class;
	uint32_t argc = 0, i = 0;
	int use_heap = 0;
	ZVAL_UNDEF(&tmp_class);
	ZVAL_NULL(retval);

	if (Z_TYPE_P(object) != IS_ARRAY || Z_TYPE_P(args) != IS_ARRAY) {
		return;
	}

	argc = zend_hash_num_elements(Z_ARRVAL_P(args));
	if (argc > 8) {
		params = safe_emalloc(argc, sizeof(zval), 0);
		use_heap = 1;
	}

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), element)
	{
		params[i++] = *element;
	}ZEND_HASH_FOREACH_END();

	class = zend_hash_index_find(Z_ARRVAL_P(object), 0);
	method = zend_hash_index_find(Z_ARRVAL_P(object), 1);
	if (Z_TYPE_P(class) == IS_STRING ) {
		class = gene_class_instance(&tmp_class, class, NULL);
		call_user_function(NULL, class, method, retval, argc, params);
	} else {
	    call_user_function(NULL, class, method, retval, argc, params);
	}

	if (use_heap) {
		efree(params);
	}
	if (Z_TYPE(tmp_class) != IS_UNDEF) {
		zval_ptr_dtor(&tmp_class);
	}
}/*}}}*/

/* hash_mode: 0=MD5 (default), 1=fast FNV-1a, 2=xxHash64, 3=FarmHash64, 4=MurmurHash3, 5=TurboHash32 */
void makeKey(zval *versionSign, zend_string *id, zval *element, zval *retval, int hash_mode) {
	char stack_buf[256];
	char *buf = stack_buf;
	char *p = buf;
	size_t buf_len = 0;
	size_t sign_len = Z_STRLEN_P(versionSign);
	size_t id_len = id ? ZSTR_LEN(id) : 0;
	size_t elem_len = 0;
	int use_heap = 0;
	zval tmp_conv;
	const char *elem_str = NULL;
	ZVAL_UNDEF(&tmp_conv);

	if (id) {
		buf_len += id_len;
	}

	if (element != NULL && Z_TYPE_P(element) != IS_NULL) {
		buf_len += 1;
		switch (Z_TYPE_P(element)) {
			case IS_STRING:
				elem_str = Z_STRVAL_P(element);
				elem_len = Z_STRLEN_P(element);
				break;
			case IS_LONG:
				elem_len = 32;
				break;
			case IS_DOUBLE:
				elem_len = 64;
				break;
			case IS_TRUE:
				elem_len = 1;
				break;
			case IS_FALSE:
				break;
			default:
				ZVAL_COPY(&tmp_conv, element);
				convert_to_string(&tmp_conv);
				elem_str = Z_STRVAL(tmp_conv);
				elem_len = Z_STRLEN(tmp_conv);
				break;
		}
		buf_len += elem_len;
	}

	if (buf_len >= sizeof(stack_buf)) {
		buf = emalloc(buf_len + 1);
		p = buf;
		use_heap = 1;
	}

	if (id) {
		memcpy(p, ZSTR_VAL(id), id_len);
		p += id_len;
	}

	if (element != NULL && Z_TYPE_P(element) != IS_NULL) {
		*p++ = '.';
		switch (Z_TYPE_P(element)) {
			case IS_STRING:
				memcpy(p, Z_STRVAL_P(element), Z_STRLEN_P(element));
				p += Z_STRLEN_P(element);
				break;
			case IS_LONG:
				p += snprintf(p, 32, ZEND_LONG_FMT, Z_LVAL_P(element));
				break;
			case IS_DOUBLE:
				p += snprintf(p, 64, "%.*G", 14, Z_DVAL_P(element));
				break;
			case IS_TRUE:
				*p++ = '1';
				break;
			case IS_FALSE:
				break;
			default:
				memcpy(p, elem_str, elem_len);
				p += elem_len;
				break;
		}
	}

	*p = '\0';
	buf_len = (size_t)(p - buf);

	if (Z_TYPE(tmp_conv) != IS_UNDEF) {
		zval_ptr_dtor(&tmp_conv);
	}

	{
		size_t hash_len = gene_hash_len(hash_mode, buf_len);
		zend_string *result = zend_string_alloc(sign_len + hash_len, 0);
		memcpy(ZSTR_VAL(result), Z_STRVAL_P(versionSign), sign_len);
		gene_hash_write(hash_mode, buf, buf_len, ZSTR_VAL(result) + sign_len);
		ZSTR_VAL(result)[sign_len + hash_len] = '\0';
		if (use_heap) {
			efree(buf);
		}
		ZVAL_STR(retval, result);
	}
}

void gene_cache_get_version_arr(zval *versionSign, zval *versionField, zval *retval, zval *top, int hash_mode) /*{{{*/
{
	zval *element = NULL;
	zend_string *id = NULL;
	array_init_size(retval, gene_cache_version_field_count(versionField, top != NULL));
	if (top) {
		Z_TRY_ADDREF_P(top);
		add_next_index_zval(retval, top);
	}
	if (versionField != NULL && Z_TYPE_P(versionField) == IS_ARRAY) {
		ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(versionField), id, element)
		{
			if (element != NULL && Z_TYPE_P(element) == IS_ARRAY) {
				zval *sub_element = NULL;
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(element), sub_element)
				{
					zval tmp_key;
					makeKey(versionSign, id, sub_element, &tmp_key, hash_mode);
					add_next_index_zval(retval, &tmp_key);
				}ZEND_HASH_FOREACH_END();
			} else {
				zval tmp_key;
				makeKey(versionSign, id, element, &tmp_key, hash_mode);
				add_next_index_zval(retval, &tmp_key);
			}
		}ZEND_HASH_FOREACH_END();
	}
}/*}}}*/

void gene_cache_update_version(zval *versionSign, zval *versionField, zval *object, int hash_mode) /*{{{*/
{
	zval *value = NULL;
	zend_string *key = NULL;
	zval incr_val;
	ZVAL_LONG(&incr_val, 1);
	if (versionField != NULL && Z_TYPE_P(versionField) == IS_ARRAY) {
		ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(versionField), key, value)
		{
			if (value != NULL && Z_TYPE_P(value) == IS_ARRAY) {
				zval *sub_value = NULL;
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(value), sub_value)
				{
					zval tmp_key, ret;
					makeKey(versionSign, key, sub_value, &tmp_key, hash_mode);
					gene_cache_incr(object, &tmp_key, &incr_val, &ret);
					zval_ptr_dtor(&ret);
					zval_ptr_dtor(&tmp_key);
				}ZEND_HASH_FOREACH_END();
			} else {
				zval tmp_key, ret;
				makeKey(versionSign, key, value, &tmp_key, hash_mode);
				gene_cache_incr(object, &tmp_key, &incr_val, &ret);
				zval_ptr_dtor(&ret);
				zval_ptr_dtor(&tmp_key);
			}
		}ZEND_HASH_FOREACH_END();
	}
}/*}}}*/

void hook_cache_set(zval *object, zval *key, zval *value, zval *ttl) {
	zval tmp_ttl, set_ret;
	ZVAL_LONG(&tmp_ttl, 0);
	if (ttl == NULL) {
		ttl = &tmp_ttl;
	}
	gene_cache_set(object, key, value, ttl, &set_ret);
	zval_ptr_dtor(&set_ret);
	zval_ptr_dtor(&tmp_ttl);
}

static uint32_t gene_cache_version_field_count(zval *versionField, int include_top)
{
	uint32_t count = include_top ? 1 : 0;
	zval *element = NULL;

	if (versionField != NULL && Z_TYPE_P(versionField) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(versionField), element)
		{
			if (element != NULL && Z_TYPE_P(element) == IS_ARRAY) {
				count += zend_hash_num_elements(Z_ARRVAL_P(element));
			} else {
				count++;
			}
		}ZEND_HASH_FOREACH_END();
	}

	return count;
}

static zend_always_inline void gene_cache_build_version_payload(zval *payload, zval *data, zval *version)
{
	array_init_size(payload, 2);
	Z_TRY_ADDREF_P(data);
	add_assoc_zval_ex(payload, ZEND_STRL("data"), data);
	add_assoc_zval_ex(payload, ZEND_STRL("version"), version);
}

void curVersion(zval *versionField, zval *cache, zval *retval) {
	uint32_t version_count = 0;
	zval *element;
	zend_ulong i = 0;
	if (versionField != NULL && Z_TYPE_P(versionField) == IS_ARRAY) {
		version_count = zend_hash_num_elements(Z_ARRVAL_P(versionField));
	}
	array_init_size(retval, version_count > 0 ? version_count - 1 : 0);
	if (versionField != NULL && Z_TYPE_P(versionField) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(versionField), element)
		{
			if (i > 0) {
				zval *val = zend_hash_find(Z_ARRVAL_P(cache), Z_STR_P(element));
				if (val) {
					Z_TRY_ADDREF_P(val);
					add_assoc_zval_ex(retval, Z_STRVAL_P(element), Z_STRLEN_P(element), val);
				}
			}
			i++;
		}ZEND_HASH_FOREACH_END();
	}
}

static zend_always_inline zend_long gene_cache_zval_get_long_fast(zval *value)
{
	switch (Z_TYPE_P(value)) {
		case IS_LONG:
			return Z_LVAL_P(value);
		case IS_TRUE:
			return 1;
		case IS_FALSE:
		case IS_NULL:
			return 0;
		default:
			return zval_get_long(value);
	}
}

int checkVersion(zval *oldVersion, zval *newVersion, zval *mode) {
	zval *element;
	zend_string *id;

	if (oldVersion == NULL || newVersion == NULL || Z_TYPE_P(oldVersion) != IS_ARRAY || Z_TYPE_P(newVersion) != IS_ARRAY) {
		return 0;
	}
	if (mode && Z_TYPE_P(mode) == IS_TRUE) {
		if (zend_hash_num_elements(Z_ARRVAL_P(newVersion)) != zend_hash_num_elements(Z_ARRVAL_P(oldVersion))) {
			return 0;
		}
		if (zend_hash_num_elements(Z_ARRVAL_P(newVersion)) == 0) {
			return 0;
		}
	}

	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(newVersion), id, element)
	{
		zval *val = zend_hash_find(Z_ARRVAL_P(oldVersion), id);
		if (!val) {
			return 0;
		}
		if (gene_cache_zval_get_long_fast(val) != gene_cache_zval_get_long_fast(element)) {
			return 0;
		}
	}ZEND_HASH_FOREACH_END();

	return 1;
}

/*
 * {{{ gene_cache
 */
PHP_METHOD(gene_cache, __construct)
{
	zval *config = NULL, *self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS(),"z", &config) == FAILURE)
    {
        return;
    }

    if (Z_TYPE_P(config) == IS_ARRAY) {
        zval *slot = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
        zval_ptr_dtor(slot);
        ZVAL_COPY(slot, config);
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_cache::cached($key)
 */
PHP_METHOD(gene_cache, cached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!sign || !hookName) {
		RETURN_NULL();
	}
	hook = gene_di_get(Z_STR_P(hookName));
	if (!hook) {
		RETURN_NULL();
	}

	zval key, ret;
	gene_cache_key(sign, 1, obj, args, ttl, &key, (int)hash_mode);
	gene_cache_get(hook, &key, &ret);
	if (Z_TYPE(ret) == IS_FALSE) {
		zval data;
		gene_cache_call(obj, args, &data);
		hook = gene_di_get(Z_STR_P(hookName));
		if (hook) {
			hook_cache_set(hook, &key, &data, ttl);
		}
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&ret);
		RETURN_ZVAL(&data, 1, 1);
	}
	zval_ptr_dtor(&key);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_cache::localCached($key)
 */
PHP_METHOD(gene_cache, localCached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!sign) {
		RETURN_NULL();
	}

	zval key, ret;
	gene_cache_key(sign, 1, obj, args, ttl, &key, (int)hash_mode);
	gene_apcu_fetch(&key, &ret);
	if (Z_TYPE(ret) == IS_FALSE) {
		zval data;
		gene_cache_call(obj, args, &data);
		gene_apcu_store(&key, &data, ttl, &ret);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&ret);
		RETURN_ZVAL(&data, 1, 1);
	}
	zval_ptr_dtor(&key);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_cache::unsetCached($key)
 */
PHP_METHOD(gene_cache, unsetCached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!hookName || !sign) {
		RETURN_NULL();
	}
	hook = gene_di_get(Z_STR_P(hookName));
	if (!hook) {
		RETURN_NULL();
	}

	zval key;
	gene_cache_key(sign, 1, obj, args, ttl, &key, (int)hash_mode);
	gene_cache_del(hook, &key, return_value);
	zval_ptr_dtor(&key);
}
/* }}} */

/*
 * {{{ public gene_cache::unsetLocalCached($key)
 */
PHP_METHOD(gene_cache, unsetLocalCached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!sign) {
		RETURN_NULL();
	}

	zval key;
	gene_cache_key(sign, 1, obj, args, ttl, &key, (int)hash_mode);
	gene_apcu_del(&key, return_value);
	zval_ptr_dtor(&key);
}
/* }}} */

/*
 * {{{ public gene_cache::cachedVersion($key)
 */
PHP_METHOD(gene_cache, cachedVersion)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*versionSign = NULL, *obj = NULL, *args = NULL,*versionField = NULL, *ttl = NULL, *mode = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zzz|zz", &obj, &args, &versionField, &ttl, &mode) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	versionSign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!hookName || !sign || !versionSign) {
		RETURN_NULL();
	}
	hook = gene_di_get(Z_STR_P(hookName));
	if (!hook) {
		RETURN_NULL();
	}

	zval key, cache, cache_key;
	zval *data = NULL,*cacheData = NULL,*cacheVersion = NULL;
	gene_cache_key(sign, 0, obj, args, ttl, &key, (int)hash_mode);
	gene_cache_get_version_arr(versionSign, versionField, &cache_key, &key, (int)hash_mode);
	gene_cache_get(hook, &cache_key, &cache);

	if (Z_TYPE(cache) == IS_ARRAY) {
		data = zend_hash_find(Z_ARRVAL(cache), Z_STR(key));
		if (data != NULL && Z_TYPE_P(data) == IS_ARRAY) {
			cacheData = zend_hash_str_find(Z_ARRVAL_P(data), ZEND_STRL("data"));
			cacheVersion = zend_hash_str_find(Z_ARRVAL_P(data), ZEND_STRL("version"));
			zval cur_version;
			curVersion(&cache_key, &cache, &cur_version);
			if (cacheVersion == NULL || checkVersion(cacheVersion, &cur_version, mode) == 0) {
				zval data_new,cur_data;
				gene_cache_call(obj, args, &cur_data);
				gene_cache_build_version_payload(&data_new, &cur_data, &cur_version);
				hook = gene_di_get(Z_STR_P(hookName));
				if (hook) {
					hook_cache_set(hook, &key, &data_new, ttl);
				}
				zval_ptr_dtor(&data_new);
				zval_ptr_dtor(&cache_key);
				zval_ptr_dtor(&cache);
				zval_ptr_dtor(&key);
				RETURN_ZVAL(&cur_data, 1, 1);
			}
			zval saved_data;
			ZVAL_COPY(&saved_data, cacheData);
			zval_ptr_dtor(&cache);
			zval_ptr_dtor(&cache_key);
			zval_ptr_dtor(&cur_version);
			zval_ptr_dtor(&key);
			RETURN_ZVAL(&saved_data, 0, 1);
		} else {
			zval data_new,cur_data,cur_version;
			gene_cache_call(obj, args, &cur_data);
			curVersion(&cache_key, &cache, &cur_version);
			gene_cache_build_version_payload(&data_new, &cur_data, &cur_version);
			hook = gene_di_get(Z_STR_P(hookName));
			if (hook) {
				hook_cache_set(hook, &key, &data_new, ttl);
			}
			zval_ptr_dtor(&data_new);
			zval_ptr_dtor(&cache_key);
			zval_ptr_dtor(&cache);
			zval_ptr_dtor(&key);
			RETURN_ZVAL(&cur_data, 1, 1);
		}
	}
	zval_ptr_dtor(&cache_key);
	zval_ptr_dtor(&cache);
	zval_ptr_dtor(&key);
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_cache::localCachedVersion($key)
 */
PHP_METHOD(gene_cache, localCachedVersion)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*versionSign = NULL, *obj = NULL, *args = NULL,*versionField = NULL, *ttl = NULL, *mode = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zzz|zz", &obj, &args, &versionField, &ttl, &mode) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	versionSign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!hookName || !sign || !versionSign) {
		RETURN_NULL();
	}

	zval key, cache, cache_key, cur_version;
	gene_cache_key(sign, 0, obj, args, ttl, &key, (int)hash_mode);
	gene_apcu_fetch(&key, &cache);
	gene_cache_get_version_arr(versionSign, versionField, &cache_key, NULL, (int)hash_mode);
	hook = gene_di_get(Z_STR_P(hookName));
	if (!hook) {
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&cache);
		zval_ptr_dtor(&cache_key);
		RETURN_NULL();
	}
	gene_cache_get(hook, &cache_key, &cur_version);

	if (Z_TYPE(cache) == IS_ARRAY) {
		zval *cacheData = NULL,*cacheVersion = NULL;
		cacheData = zend_hash_str_find(Z_ARRVAL(cache), ZEND_STRL("data"));
		cacheVersion = zend_hash_str_find(Z_ARRVAL(cache), ZEND_STRL("version"));
		if (cacheVersion == NULL || checkVersion(cacheVersion, &cur_version, mode) == 0) {
			zval data_new,cur_data;
			gene_cache_call(obj, args, &cur_data);
			gene_cache_build_version_payload(&data_new, &cur_data, &cur_version);
			zval ret;
			gene_apcu_store(&key, &data_new, ttl, &ret);
			zval_ptr_dtor(&ret);
			zval_ptr_dtor(&key);
			zval_ptr_dtor(&data_new);
			zval_ptr_dtor(&cache_key);
			zval_ptr_dtor(&cache);
			RETURN_ZVAL(&cur_data, 1, 1);
		}
		zval saved_data;
		ZVAL_COPY(&saved_data, cacheData);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&cache);
		zval_ptr_dtor(&cache_key);
		zval_ptr_dtor(&cur_version);
		RETURN_ZVAL(&saved_data, 0, 1);
	} else {
		zval data_new,cur_data;
		gene_cache_call(obj, args, &cur_data);
		gene_cache_build_version_payload(&data_new, &cur_data, &cur_version);
		zval ret;
		gene_apcu_store(&key, &data_new, ttl, &ret);
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&data_new);
		zval_ptr_dtor(&cache_key);
		zval_ptr_dtor(&cache);
		RETURN_ZVAL(&cur_data, 1, 1);
	}
}
/* }}} */

/*
 * {{{ public gene_cache::getVersion($key)
 */
PHP_METHOD(gene_cache, getVersion)
{
	zval *self = getThis(), *versionField = NULL, *config = NULL, *hook = NULL, *hookName = NULL,*versionSign = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &versionField) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	versionSign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!hookName || !versionSign) {
		RETURN_NULL();
	}

	zval ret, new_arr;
	hook = gene_di_get(Z_STR_P(hookName));
	if (!hook) {
		RETURN_NULL();
	}
	gene_cache_get_version_arr(versionSign, versionField, &new_arr, NULL, (int)hash_mode);
	gene_cache_get(hook, &new_arr, &ret);
	zval_ptr_dtor(&new_arr);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */


/*
 * {{{ public gene_cache::updateVersion($key)
 */
PHP_METHOD(gene_cache, updateVersion)
{
	zval *self = getThis(), *versionField = NULL, *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL, *hash_mode_zv = NULL;
	zend_long hash_mode = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &versionField) == FAILURE) {
		return;
	}
	config = OBJ_PROP(Z_OBJ_P(self), gene_cache_offset_config);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		RETURN_NULL();
	}
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));
	hash_mode_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	if (!hookName || !sign) {
		RETURN_NULL();
	}

	hook = gene_di_get(Z_STR_P(hookName));
	if (!hook) {
		RETURN_NULL();
	}
	gene_cache_update_version(sign, versionField, hook, (int)hash_mode);
	RETURN_TRUE;
}
/* }}} */


/*
 * {{{ gene_cache_methods
 */
const zend_function_entry gene_cache_methods[] = {
		PHP_ME(gene_cache, cached, gene_cache_cached_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, localCached, gene_cache_locaclCached_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, unsetCached, gene_cache_unsetcached_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, unsetLocalCached, gene_cache_unsetlocalcached_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, cachedVersion, gene_cache_cachedversion_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, localCachedVersion, gene_cache_localcachedversion_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, getVersion, gene_cache_getversion_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, updateVersion, gene_cache_updateversion_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, __construct, gene_cache_construct_arginfo, ZEND_ACC_PUBLIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(cache)
{
    zend_class_entry gene_cache;
    GENE_INIT_CLASS_ENTRY(gene_cache, "Gene_Cache_Cache", "Gene\\Cache\\Cache", gene_cache_methods);
    gene_cache_ce = zend_register_internal_class(&gene_cache);
#if PHP_VERSION_ID >= 80200
    gene_cache_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

    zend_declare_property_null(gene_cache_ce, ZEND_STRL(GENE_CACHE_CONFIG), ZEND_ACC_PUBLIC);

    {
        zend_property_info *pi = zend_hash_str_find_ptr(
            &gene_cache_ce->properties_info,
            GENE_CACHE_CONFIG, sizeof(GENE_CACHE_CONFIG) - 1);
        gene_cache_offset_config = pi->offset;
    }
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
