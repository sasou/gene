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
	GC_ADD_FLAGS(key, IS_STR_INTERNED | IS_STR_PERMANENT);
	return key;
}
/* }}} */

/** {{{ static void gene_memory_zval_dtor(zval *value)
 */
static void gene_memory_zval_dtor(zval *zvalue) {
	switch(Z_TYPE_P(zvalue)){
	case IS_PTR:
	case IS_STRING:
		pefree(Z_PTR_P(zvalue), 1);
		break;
	case IS_ARRAY:
		gene_hash_destroy(Z_ARRVAL_P(zvalue));
		break;
	default:
		break;
	}
}
/* }}} */

static zval* gene_symtable_update(HashTable *ht, zend_string *key, zval *zv) /* {{{ */{
	zend_ulong idx;
	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		pefree(key, 1);
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

	zend_hash_real_init(ht, 0);
	HT_ALLOW_COW_VIOLATION(ht);

	ZVAL_ARR(zv, ht);
	/* make immutable array */
	Z_TYPE_FLAGS_P(zv) = 0;
	GC_SET_REFCOUNT(ht, 2);
	GC_ADD_FLAGS(ht, IS_ARRAY_IMMUTABLE);
}
/* }}} */

void gene_hash_destroy(HashTable *ht) /* {{{ */{
	zend_string **keys = NULL;
	zend_string *key;
	uint32_t key_count = 0;
	uint32_t i;

	if (!ht) {
		return;
	}

	if (ht->nNumUsed > 0) {
		keys = (zend_string **) pemalloc(sizeof(zend_string *) * ht->nNumUsed, 1);
		ZEND_HASH_FOREACH_STR_KEY(ht, key) {
			if (key && (GC_FLAGS(key) & (IS_STR_INTERNED | IS_STR_PERMANENT))) {
				keys[key_count++] = key;
			}
		} ZEND_HASH_FOREACH_END();
	}

	zend_hash_destroy(ht);

	for (i = 0; i < key_count; i++) {
		pefree(keys[i], 1);
	}
	if (keys) {
		pefree(keys, 1);
	}
	pefree(ht, 1);
} /* }}} */
/*
 * {{{ static void * gene_memory_init()
 */
void gene_memory_init() {
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

/* {{{ gene_memory_write_allowed
 * In Swoole mode workerReady() is the freeze boundary for the process-level
 * cache: read paths may skip RDLOCK after that point, so writes must stop.
 * Configuration/router loading still happens before workerReady(), while FPM
 * keeps the existing per-request mutable behavior. */
int gene_memory_write_allowed(const char *op) {
	if (UNEXPECTED(GENE_G(cache_layer_memory_write_depth) > 0)) {
		return 1;
	}
	if (UNEXPECTED(GENE_G(runtime_type) >= 2 && GENE_G(worker_ready))) {
		php_error_docref(NULL, E_WARNING,
			"Gene memory cache is frozen after workerReady(); %s is not allowed in Swoole request runtime",
			op ? op : "write");
		return 0;
	}
	return 1;
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
	case IS_STRING:
		pefree(Z_PTR_P(dst), 1);
		break;
	case IS_ARRAY:
		gene_hash_destroy(Z_ARRVAL_P(dst));
		break;
	}
	switch (Z_TYPE_P(source)) {
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
	ZEND_HASH_FOREACH_KEY_VAL(source, idx, key, element)
	{
		zval rv;
		gene_memory_zval_local(&rv, element);
		if (key) {
			/* [GENE_PERF:2026-04-24 v5.5.8] Persistent cache keys are created
			 * via gene_str_persistent() which sets IS_STR_INTERNED|IS_STR_PERMANENT.
			 * PHP's hash_update/zend_string_release on such strings is a safe
			 * no-op for refcount management, so we can hand the persistent
			 * zend_string directly to the request-scope HashTable instead of
			 * rebuilding a fresh request-scope copy via zend_string_init.
			 * Saves one emalloc+memcpy per hash entry on every config read,
			 * DI graph traversal, and Memory::get() on array values. */
			if (GC_FLAGS(key) & IS_STR_INTERNED) {
				zend_symtable_update(target, key, &rv);
			} else {
				zend_string *str_key = zend_string_init(ZSTR_VAL(key), ZSTR_LEN(key), 0);
				zend_symtable_update(target, str_key, &rv);
				zend_string_release(str_key);
			}
		} else {
			zend_hash_index_update(target, idx, &rv);
		}
	}ZEND_HASH_FOREACH_END();
} /* }}} */

zval *gene_memory_zval_local(zval *dst, zval *source) /* {{{ */
{
	switch (Z_TYPE_P(source)) {
	case IS_STRING:
		/* [GENE_PERF:2026-04-24 v5.5.8] Persistent cache entries are stored
		 * as interned (IS_STR_INTERNED|IS_STR_PERMANENT) zend_strings. Share
		 * the interned pointer into the request scope via ZVAL_STR — no ref
		 * bump, no emalloc. The request-scope zval_ptr_dtor that eventually
		 * frees this container is a no-op for interned strings (the string
		 * header bypasses refcount release). This removes one emalloc +
		 * memcpy per string value returned from Gene\Memory::get,
		 * Gene\Application::config, and every DI/service config read, which
		 * in the DI-heavy path compounds across dozens of entries per req.
		 * Non-interned strings (shouldn't happen for values built via
		 * gene_memory_zval_persistent, but defensive here) fall through to
		 * the old-style fresh-copy path. */
		if (EXPECTED(GC_FLAGS(Z_STR_P(source)) & IS_STR_INTERNED)) {
			ZVAL_STR(dst, Z_STR_P(source));
		} else {
			zend_string *str_key = zend_string_init(Z_STRVAL_P(source), Z_STRLEN_P(source), 0);
			ZVAL_NEW_STR(dst, str_key);
		}
		break;
	case IS_ARRAY:
		array_init_size(dst, zend_hash_num_elements(Z_ARRVAL_P(source)));
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
		if (UNEXPECTED(!gene_memory_write_allowed("Memory::set"))) {
			return;
		}
		GENE_CACHE_WRLOCK();
		copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
		if (copyval == NULL) {
			gene_memory_zval_persistent(&ret, zvalue);
			key = gene_str_persistent(keyString, keyString_len);
			gene_symtable_update(GENE_G(cache), key, &ret);
			/* key is now owned by the hash table; do not free here.
			 * zend_string_release is a no-op for interned strings. */
			GENE_CACHE_WRUNLOCK();
			return;
		}
		gene_memory_zval_edit_persistent(copyval, zvalue);
		GENE_CACHE_WRUNLOCK();
	}
}
/* }}} */

/** {{{ void gene_memory_get(char *keyString, size_t keyString_len)
 * [GENE_AUDIT:2026-03-25] Returns pointer into persistent cache. The read lock
 * is released before return, so the pointer is valid only as long as no concurrent
 * write (set/del/clean) modifies this key. This is safe by design invariant:
 * persistent cache (routes, configs) is written at startup (MINIT or workerStart)
 * and only read during request handling. Gene\Memory::set/del MUST NOT be called
 * during Swoole request handling on keys also read by other coroutines.
 * Full-copy alternative was evaluated but rejected: deep-copying on every DI/route
 * lookup would add ~2-5us per call, unacceptable in the hot path.
 */
zval * gene_memory_get(char *keyString, size_t keyString_len) {
	zval *zvalue;
	GENE_CACHE_RDLOCK();
	zvalue = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	GENE_CACHE_RDUNLOCK();
	return zvalue;
}
/* }}} */

/* [GENE_PERF:2026-04-19] gene_memory_get_quick is now a macro in memory.h
 * (collapsed to gene_memory_get) — no function definition needed here. */

/** {{{ void gene_memory_get_triple(...)
 * [GENE_PERF:2026-04-24 v5.5.8] Single-lock batched lookup of up to three
 * persistent-cache keys. Used by the router dispatch hot path which previously
 * issued three back-to-back gene_memory_get_quick() calls — each taking and
 * releasing the cache rwlock independently. On ZTS / worker_ready==0 Swoole
 * builds that's 3× the atomic-contention footprint for a purely read-only
 * operation. Merging them into a single RDLOCK span reduces contended atomic
 * ops by 3× without changing correctness (all three reads observe the same
 * consistent snapshot of GENE_G(cache)).
 *
 * Any of out1/out2/out3 may be NULL for unused slots. Keys of length 0 are
 * likewise skipped. */
void gene_memory_get_triple(
	const char *k1, size_t k1_len, zval **out1,
	const char *k2, size_t k2_len, zval **out2,
	const char *k3, size_t k3_len, zval **out3)
{
	HashTable *ht;
	GENE_CACHE_RDLOCK();
	ht = GENE_G(cache);
	if (out1) {
		*out1 = (k1 && k1_len) ? zend_symtable_str_find(ht, (char *)k1, k1_len) : NULL;
	}
	if (out2) {
		*out2 = (k2 && k2_len) ? zend_symtable_str_find(ht, (char *)k2, k2_len) : NULL;
	}
	if (out3) {
		*out3 = (k3 && k3_len) ? zend_symtable_str_find(ht, (char *)k3, k3_len) : NULL;
	}
	GENE_CACHE_RDUNLOCK();
}
/* }}} */

/** {{{ zval * gene_memory_get_by_config(char *keyString, int keyString_len,char *path)
 * [GENE_AUDIT:2026-03-25] Returns pointer to nested value inside persistent cache.
 * Same safety invariant as gene_memory_get — persistent cache is write-once at startup.
 * [GENE_PERF:2026-04-24 v5.5.8] Walk the nested HashTable chain *outside* the
 * rwlock. We only need the lock to fetch the top-level entry pointer; once
 * obtained it remains valid under the documented write-once-at-startup
 * invariant, so there's no reason to hold the rwlock while doing strtok +
 * O(depth) hash lookups. For a 3-4 segment config path (e.g. `db/mysql/host`)
 * this halves the average lock hold time and moves strtok / string copy
 * entirely off the critical section — a sizable win under high concurrency.
 */
zval * gene_memory_get_by_config(char *keyString, size_t keyString_len, char *path) {
	char *ptr = NULL, *seg = NULL;
	char path_stack[256];
	char *path_copy = NULL;
	int path_heap = 0;
	zval *tmp = NULL;
	zval *copyval = NULL;

	GENE_CACHE_RDLOCK();
	copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	GENE_CACHE_RDUNLOCK();

	if (!copyval) {
		return NULL;
	}
	if (path == NULL) {
		return copyval;
	}

	tmp = copyval;
	{
		size_t path_len = strlen(path);
		if (path_len < sizeof(path_stack)) {
			memcpy(path_stack, path, path_len + 1);
			path_copy = path_stack;
		} else {
			path_copy = estrndup(path, path_len);
			path_heap = 1;
		}
	}
	seg = php_strtok_r(path_copy, "/", &ptr);
	while (seg) {
		if (Z_TYPE_P(tmp) != IS_ARRAY) {
			if (path_heap) efree(path_copy);
			return NULL;
		}
		tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
		if (tmp == NULL) {
			if (path_heap) efree(path_copy);
			return NULL;
		}
		seg = php_strtok_r(NULL, "/", &ptr);
	}
	if (path_heap) efree(path_copy);
	return tmp;
}
/* }}} */

/** {{{ filenode * file_cache_get_easy(char *keyString, int keyString_len)
 */
filenode * file_cache_get_easy(char *keyString, size_t keyString_len) {
	filenode *result;
	GENE_CACHE_RDLOCK();
	result = zend_hash_str_find_ptr(GENE_G(cache_easy), keyString, keyString_len);
	GENE_CACHE_RDUNLOCK();
	return result;
}
/* }}} */

/** {{{ void file_cache_set_val(char *val, size_t keyString_len, int times, int validity)
 */
void file_cache_set_val(char *val, size_t keyString_len, zend_long times,
		int validity) {
	filenode n;
	zend_string *key;
	if (UNEXPECTED(!gene_memory_write_allowed("file cache update"))) {
		return;
	}
	n.stime = time(NULL);
	n.ftime = times;
	n.validity = validity;
	n.status = 0;
	key = gene_str_persistent(val, keyString_len);
	GENE_CACHE_WRLOCK();
	zend_hash_update_mem(GENE_G(cache_easy), key, &n, sizeof(filenode));
	GENE_CACHE_WRUNLOCK();
	/* zend_hash_update_mem calls zend_hash_update internally, which
	 * increments the key's refcount. For persistent interned strings,
	 * zend_string_release is a no-op, so we must NOT pefree the key here.
	 * The key will be freed when the HashTable is destroyed. */
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
		keyS = gene_str_persistent(keyString, keyString_len);
		copyval = gene_symtable_update(Z_ARRVAL_P(val), keyS, &tmp);
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
	char path_stack[256];
	char *path_copy = NULL;
	int path_heap = 0;
	size_t path_len;
	zval *tmp;
	zval *copyval = NULL, ret;
	zend_string *keyS = NULL;
	if (UNEXPECTED(!gene_memory_write_allowed("router/config cache update"))) {
		return;
	}
	if (UNEXPECTED(!path)) {
		return;
	}
	path_len = strlen(path);
	if (path_len < sizeof(path_stack)) {
		memcpy(path_stack, path, path_len + 1);
		path_copy = path_stack;
	} else {
		path_copy = estrndup(path, path_len);
		path_heap = 1;
	}
	GENE_CACHE_WRLOCK();
	copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (copyval == NULL) {
		gene_hash_init(&ret, 0);
		keyS = gene_str_persistent(keyString, keyString_len);
		gene_symtable_update(GENE_G(cache), keyS, &ret);
		tmp = &ret;
		seg = php_strtok_r(path_copy, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
	} else {
		tmp = copyval;
		seg = php_strtok_r(path_copy, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_memory_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
	}
	GENE_CACHE_WRUNLOCK();
	if (path_heap) efree(path_copy);
	return;
}
/* }}} */

/** {{{ void gene_memory_exists(char *keyString, int keyString_len)
 */
int gene_memory_exists(char *keyString, size_t keyString_len) {
	int result;
	GENE_CACHE_RDLOCK();
	result = zend_symtable_str_exists(GENE_G(cache), keyString, keyString_len) == 1 ? 1 : 0;
	GENE_CACHE_RDUNLOCK();
	return result;
}
/* }}} */

/** {{{ void gene_memory_getTime(char *keyString, size_t keyString_len,gene_memory_container **zvalue)
 */
zend_long gene_memory_getTime(char *keyString, size_t keyString_len) {
	zval *zvalue = NULL;
	zend_long result = 0;
	GENE_CACHE_RDLOCK();
	zvalue = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (zvalue && Z_TYPE_P(zvalue) == IS_LONG) {
		result = Z_LVAL_P(zvalue);
	}
	GENE_CACHE_RDUNLOCK();
	return result;
}
/* }}} */

/** {{{ void gene_memory_del(char *keyString, size_t keyString_len)
 */
int gene_memory_del(char *keyString, size_t keyString_len) {
	zend_string *stored_key = NULL;
	zval *stored_val = NULL;
	zend_string *iter_key;
	zval *iter_val;
	dtor_func_t orig_dtor;

	if (UNEXPECTED(!gene_memory_write_allowed("Memory::del"))) {
		return 0;
	}
	GENE_CACHE_WRLOCK();
	/* Keys are allocated as persistent interned strings (IS_STR_INTERNED),
	 * so zend_string_release() is a no-op for them. We must find the stored
	 * key pointer and free() it manually to avoid a memory leak on each del. */
	ZEND_HASH_FOREACH_STR_KEY_VAL(GENE_G(cache), iter_key, iter_val) {
		if (iter_key
				&& ZSTR_LEN(iter_key) == keyString_len
				&& memcmp(ZSTR_VAL(iter_key), keyString, keyString_len) == 0) {
			stored_key = iter_key;
			stored_val = iter_val;
			break;
		}
	} ZEND_HASH_FOREACH_END();

	if (stored_key) {
		/* Free the value content first */
		gene_memory_zval_dtor(stored_val);

		/* Remove the bucket without triggering the destructor again */
		orig_dtor = GENE_G(cache)->pDestructor;
		GENE_G(cache)->pDestructor = NULL;
		zend_symtable_str_del(GENE_G(cache), keyString, keyString_len);
		GENE_G(cache)->pDestructor = orig_dtor;

		/* Only keys forced to permanent/interned need manual freeing */
		if (GC_FLAGS(stored_key) & (IS_STR_INTERNED | IS_STR_PERMANENT)) {
			pefree(stored_key, 1);
		}
		GENE_CACHE_WRUNLOCK();
		return 1;
	}

	/* Fallback for numeric keys (stored by index, no persistent key to free) */
	if (zend_symtable_str_del(GENE_G(cache), keyString, keyString_len) == SUCCESS) {
		GENE_CACHE_WRUNLOCK();
		return 1;
	}
	GENE_CACHE_WRUNLOCK();
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

	/* [GENE_FIX:2026-05-21 F5] Gate Z_STRVAL_P(safe) on IS_STRING; same UB
	 * concern as Application/Router/Config __construct. */
	if (safe && Z_TYPE_P(safe) == IS_STRING) {
		zend_update_property_string(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), Z_STRVAL_P(safe));
	} else {
		if (GENE_G(app_key)) {
			zend_update_property_string(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), GENE_G(app_key));
		} else if (GENE_G(app_root)) {
			zend_update_property_string(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), GENE_G(app_root));
		}
	}
}
/* }}} */

/*
 * {{{ public gene_memory::set($key, $data)
 */
PHP_METHOD(gene_memory, set) {
	zend_string *keyString = NULL;
	zend_long validity = 0;
	char stack_buf[256];
	char *router_e = stack_buf;
	size_t router_e_len;
	int router_e_heap = 0;
	zval *zvalue, *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz|l", &keyString, &zvalue, &validity) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = Z_STRLEN_P(safe) + 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		memcpy(router_e, Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		router_e[Z_STRLEN_P(safe)] = ':';
		memcpy(router_e + Z_STRLEN_P(safe) + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	} else {
		router_e_len = 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		router_e[0] = ':';
		memcpy(router_e + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	}
	if (zvalue) {
		gene_memory_set(router_e, router_e_len, zvalue, validity);
	}
	if (router_e_heap) efree(router_e);
	RETURN_BOOL(1);
}
/* }}} */

/*
 * {{{ public gene_memory::get($key)
 */
PHP_METHOD(gene_memory, get) {
	zend_string *keyString;
	char stack_buf[256];
	char *router_e = stack_buf;
	size_t router_e_len;
	int router_e_heap = 0;
	zval *zvalue, *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = Z_STRLEN_P(safe) + 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		memcpy(router_e, Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		router_e[Z_STRLEN_P(safe)] = ':';
		memcpy(router_e + Z_STRLEN_P(safe) + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	} else {
		router_e_len = 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		router_e[0] = ':';
		memcpy(router_e + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	}
	zvalue = gene_memory_get(router_e, router_e_len);
	if (router_e_heap) efree(router_e);
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
	char stack_buf[256];
	char *router_e = stack_buf;
	size_t router_e_len;
	int router_e_heap = 0;
	zend_long ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = Z_STRLEN_P(safe) + 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		memcpy(router_e, Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		router_e[Z_STRLEN_P(safe)] = ':';
		memcpy(router_e + Z_STRLEN_P(safe) + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	} else {
		router_e_len = 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		router_e[0] = ':';
		memcpy(router_e + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	}
	ret = gene_memory_getTime(router_e, router_e_len);
	if (router_e_heap) efree(router_e);
	RETURN_LONG(ret);
}
/* }}} */

/*
 * {{{ public gene_memory::exists($key)
 */
PHP_METHOD(gene_memory, exists) {
	zend_string *keyString;
	char stack_buf[256];
	char *router_e = stack_buf;
	size_t router_e_len;
	int router_e_heap = 0;
	zend_long ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = Z_STRLEN_P(safe) + 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		memcpy(router_e, Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		router_e[Z_STRLEN_P(safe)] = ':';
		memcpy(router_e + Z_STRLEN_P(safe) + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	} else {
		router_e_len = 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		router_e[0] = ':';
		memcpy(router_e + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	}
	ret = gene_memory_exists(router_e, router_e_len);
	if (router_e_heap) efree(router_e);
	RETURN_BOOL(ret);
}
/* }}} */

/*
 * {{{ public gene_memory::del($key)
 */
PHP_METHOD(gene_memory, del) {
	zend_string *keyString;
	char stack_buf[256];
	char *router_e = stack_buf;
	size_t router_e_len;
	int router_e_heap = 0;
	zend_long ret;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = Z_STRLEN_P(safe) + 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		memcpy(router_e, Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		router_e[Z_STRLEN_P(safe)] = ':';
		memcpy(router_e + Z_STRLEN_P(safe) + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	} else {
		router_e_len = 1 + ZSTR_LEN(keyString);
		if (router_e_len >= sizeof(stack_buf)) {
			router_e = emalloc(router_e_len + 1);
			router_e_heap = 1;
		}
		router_e[0] = ':';
		memcpy(router_e + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
		router_e[router_e_len] = '\0';
	}
	ret = gene_memory_del(router_e, router_e_len);
	if (router_e_heap) efree(router_e);
	RETURN_BOOL(ret);
}
/* }}} */

/*
 * {{{ public gene_memory::clean()
 */
PHP_METHOD(gene_memory, clean) {
	if (UNEXPECTED(!gene_memory_write_allowed("Memory::clean"))) {
		RETURN_FALSE;
	}
	GENE_CACHE_WRLOCK();
	if (GENE_G(cache)) {
		gene_hash_destroy(GENE_G(cache));
		GENE_G(cache) = NULL;
	}
	gene_memory_init();
	GENE_CACHE_WRUNLOCK();
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_memory::stats()
 * [GENE_MEM:2026-04-23] Observability backstop for long-running workers.
 * Exposes counters for the persistent process-level caches and Swoole
 * coroutine context table so operators can detect unexpected growth
 * before it turns into OOM. Read-only; zero side effects.
 *
 * Return shape:
 *   [
 *     'cache_items'       => int,   // main process cache entry count
 *     'cache_easy_items'  => int,   // file cache entry count
 *     'fn_cache_items'    => int,   // closure router dispatch cache
 *     'co_contexts_items' => int,   // live Swoole coroutine contexts
 *     'co_contexts_max'   => int,   // configured soft cap
 *     'ctx_pool_size'     => int,   // recycled context structs in the pool
 *     'ctx_pool_max'      => int,   // pool capacity
 *   ]
 */
PHP_METHOD(gene_memory, stats) {
	array_init(return_value);
	GENE_CACHE_RDLOCK();
	add_assoc_long(return_value, "cache_items",
		GENE_G(cache) ? (zend_long)zend_hash_num_elements(GENE_G(cache)) : 0);
	add_assoc_long(return_value, "cache_easy_items",
		GENE_G(cache_easy) ? (zend_long)zend_hash_num_elements(GENE_G(cache_easy)) : 0);
	GENE_CACHE_RDUNLOCK();
	add_assoc_long(return_value, "fn_cache_items",
		GENE_G(fn_cache) ? (zend_long)zend_hash_num_elements(GENE_G(fn_cache)) : 0);
	add_assoc_long(return_value, "co_contexts_items",
		GENE_G(co_contexts) ? (zend_long)zend_hash_num_elements(GENE_G(co_contexts)) : 0);
	add_assoc_long(return_value, "co_contexts_max", GENE_G(co_contexts_max));
	/* [GENE_PERF:2026-04-24] Context struct pool visibility. */
	add_assoc_long(return_value, "ctx_pool_size", GENE_G(ctx_pool_size));
	add_assoc_long(return_value, "ctx_pool_max", GENE_G(ctx_pool_max));
}
/* }}} */

/*
 * {{{ gene_memory_methods
 */
const zend_function_entry gene_memory_methods[] = {
	PHP_ME(gene_memory, __construct, gene_memory_arg_construct, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, set, gene_memory_arg_set, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, get, gene_memory_arg_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, getTime, gene_memory_arg_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, exists, gene_memory_arg_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, del, gene_memory_arg_del, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, clean, gene_memory_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, stats, gene_memory_void_arginfo, ZEND_ACC_PUBLIC)
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
#if PHP_VERSION_ID >= 80200
	gene_memory_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

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
