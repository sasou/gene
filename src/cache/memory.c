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
static void gene_memory_hash_copy_deep(HashTable *target, HashTable *source);


ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, safe)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_set, 0, 0, 3)
	ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_incr, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, step)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_del, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_mget, 0, 0, 1)
	ZEND_ARG_INFO(0, keys)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_mset, 0, 0, 1)
	ZEND_ARG_INFO(0, values)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_rate_limit, 0, 0, 3)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, max)
	ZEND_ARG_INFO(0, windowSec)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_lock, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, ttlSec)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_memory_arg_unlock, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, token)
ZEND_END_ARG_INFO()

/* }}} */

static zend_string* gene_str_persistent(const char *str, size_t len) /* {{{ */{
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
		/* [GENE_AUDIT:2026-07-13 M3] pemalloc (malloc) returns NULL on OOM and
		 * does not bailout like emalloc; skip key collection rather than crash
		 * (worst case the persistent keys leak until process exit). */
		keys = (zend_string **) pemalloc(sizeof(zend_string *) * ht->nNumUsed, 1);
		if (keys) {
			ZEND_HASH_FOREACH_STR_KEY(ht, key) {
				if (key && (GC_FLAGS(key) & (IS_STR_INTERNED | IS_STR_PERMANENT))) {
					keys[key_count++] = key;
				}
			} ZEND_HASH_FOREACH_END();
		}
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
	if (!GENE_G(cache_expiry)) {
		PALLOC_HASHTABLE(GENE_G(cache_expiry));
		zend_hash_init(GENE_G(cache_expiry), 8, NULL, NULL, 1);
	}
	return;
}
/* }}} */

/* [GENE_FIX:2026-08-07] TTL support helpers. The main persistent cache stores
 * bare values; expiry timestamps live in GENE_G(cache_expiry) (unix ts,
 * IS_LONG). All accesses happen under the same cache lock as the main table.
 * Expired keys are treated as missing and lazily deleted by the reader when
 * writes are still allowed (they are frozen after workerReady, where the
 * lock macros no-op anyway and the table is stable). */
static int gene_memory_expired_nolock(const char *keyString, size_t keyString_len) {
	zval *exp;
	/* [GENE_FIX:2026-08-07-5 N4] Most deployments never use TTL; skip the
	 * extra hash lookup on the hot read path when the expiry table is empty
	 * (one integer comparison instead of a full hash find per get). */
	if (!GENE_G(cache_expiry) || zend_hash_num_elements(GENE_G(cache_expiry)) == 0) {
		return 0;
	}
	exp = zend_hash_str_find(GENE_G(cache_expiry), keyString, keyString_len);
	return (exp && Z_TYPE_P(exp) == IS_LONG && Z_LVAL_P(exp) <= (zend_long)time(NULL)) ? 1 : 0;
}

/* Caller must hold GENE_CACHE_WRLOCK. */
static void gene_memory_set_expiry_nolock(const char *keyString, size_t keyString_len, int validity) {
	if (!GENE_G(cache_expiry)) {
		return;
	}
	if (validity > 0) {
		zval exp;
		ZVAL_LONG(&exp, (zend_long)time(NULL) + validity);
		zend_hash_str_update(GENE_G(cache_expiry), keyString, keyString_len, &exp);
	} else {
		/* 0 = permanent: drop any stale expiry from a previous TTLed set. */
		zend_hash_str_del(GENE_G(cache_expiry), keyString, keyString_len);
	}
}

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
	/* [GENE_FIX:2026-08-23 UAF-4] IS_REFERENCE must be dereferenced before the
	 * switch: dst is an uninitialized stack zval, so falling through with no
	 * matching case would store stack garbage into the persistent table and
	 * gene_memory_zval_dtor would later pefree a garbage pointer. */
	ZVAL_DEREF(source);
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
	default:
		ZVAL_NULL(dst);
		break;
	}
} /* }}} */

/** {{{ static void * gene_memory_zval_edit_persistent(zval *zvalue)
 */
static void * gene_memory_zval_edit_persistent(zval *dst, zval *source) {
	/* [GENE_FIX:2026-08-23 UAF-3] Swap-then-free: build the new persistent
	 * value first, atomically exchange it into the bucket, then free the old
	 * value. The previous order (free dst, then rebuild) left dst holding a
	 * freed pointer for the whole rebuild window — any concurrent reader that
	 * had borrowed dst's persistent zend_string/HashTable (see
	 * gene_memory_zval_local) dereferenced freed memory. */
	zval old;
	zval newv;
	ZVAL_COPY_VALUE(&old, dst);
	ZVAL_DEREF(source);
	switch (Z_TYPE_P(source)) {
	case IS_STRING:
		ZVAL_INTERNED_STR(&newv,
				gene_str_persistent(Z_STRVAL_P(source), Z_STRLEN_P(source)));
		break;
	case IS_ARRAY: {
		gene_hash_init(&newv, zend_hash_num_elements(Z_ARRVAL_P(source)));
		gene_memory_hash_copy(Z_ARRVAL_P(&newv), Z_ARRVAL_P(source));
	}
		break;
	case IS_TRUE:
	case IS_FALSE:
	case IS_DOUBLE:
	case IS_LONG:
	case IS_NULL:
		ZVAL_COPY_VALUE(&newv, source);
		break;
	case IS_RESOURCE:
	case IS_OBJECT:
		zend_error(E_ERROR, "An unsupported data type");
		break;
	default:
		ZVAL_NULL(&newv);
		break;
	}
	ZVAL_COPY_VALUE(dst, &newv);
	switch (Z_TYPE_P(&old)) {
	case IS_PTR:
	case IS_STRING:
		pefree(Z_PTR_P(&old), 1);
		break;
	case IS_ARRAY:
		gene_hash_destroy(Z_ARRVAL_P(&old));
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
		/* [GENE_FIX:2026-08-23 UAF-2] Deep-copy every value into request
		 * memory. The previous zero-copy path borrowed persistent
		 * zend_string* into the request array; a concurrent overwrite of the
		 * same persistent key (Gene\Cache business write / LRU evict / TTL
		 * delete) then freed memory the request still referenced. Bucket keys
		 * below are likewise rebuilt instead of borrowed. */
		gene_memory_zval_local_copy(&rv, element);
		if (key) {
			zend_string *str_key = zend_string_init(ZSTR_VAL(key), ZSTR_LEN(key), 0);
			zend_symtable_update(target, str_key, &rv);
			zend_string_release(str_key);
		} else {
			zend_hash_index_update(target, idx, &rv);
		}
	}ZEND_HASH_FOREACH_END();
} /* }}} */

zval *gene_memory_zval_local(zval *dst, zval *source) /* {{{ */
{
	/* [GENE_FIX:2026-08-23 UAF-4] Deref references before the switch so a
	 * reference-typed persistent entry cannot fall through leaving dst
	 * uninitialized (same stack-garbage hazard as gene_memory_zval_persistent). */
	ZVAL_DEREF(source);
	switch (Z_TYPE_P(source)) {
	case IS_STRING:
		/* [GENE_FIX:2026-08-23 UAF-2] Always deep-copy. The previous
		 * zero-copy branch (ZVAL_STR of the persistent interned string) was
		 * only safe while the table was strictly write-once; Gene\Cache
		 * business writes after workerReady() break that invariant and freed
		 * the borrowed pointer under in-flight requests. */
		ZVAL_NEW_STR(dst, zend_string_init(Z_STRVAL_P(source), Z_STRLEN_P(source), 0));
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
	default:
		ZVAL_NULL(dst);
		break;
	}
	return dst;
} /* }}} */

/* [GENE_FIX:2026-08-23 UAF-2] Request-scope deep copy of a persistent-cache
 * value: every string is rebuilt with zend_string_init(..., 0) and every
 * bucket key is re-created in the request heap, so the returned zval owns no
 * pointer into GENE_G(cache). Use this for Gene\Cache business reads, whose
 * entries may be overwritten (pefree'd) or evicted while a request still
 * holds the returned value. Framework metadata reads (routes/config/DI) keep
 * using the zero-copy gene_memory_zval_local above. */
static void gene_memory_hash_copy_deep(HashTable *target, HashTable *source) /* {{{ */{
	zend_string *key;
	zend_long idx;
	zval *element;
	ZEND_HASH_FOREACH_KEY_VAL(source, idx, key, element)
	{
		zval rv;
		gene_memory_zval_local_copy(&rv, element);
		if (key) {
			zend_string *str_key = zend_string_init(ZSTR_VAL(key), ZSTR_LEN(key), 0);
			zend_symtable_update(target, str_key, &rv);
			zend_string_release(str_key);
		} else {
			zend_hash_index_update(target, idx, &rv);
		}
	}ZEND_HASH_FOREACH_END();
} /* }}} */

zval *gene_memory_zval_local_copy(zval *dst, zval *source) /* {{{ */
{
	ZVAL_DEREF(source);
	switch (Z_TYPE_P(source)) {
	case IS_STRING:
		ZVAL_NEW_STR(dst, zend_string_init(Z_STRVAL_P(source), Z_STRLEN_P(source), 0));
		break;
	case IS_ARRAY:
		array_init_size(dst, zend_hash_num_elements(Z_ARRVAL_P(source)));
		gene_memory_hash_copy_deep(Z_ARRVAL_P(dst), Z_ARRVAL_P(source));
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
	default:
		ZVAL_NULL(dst);
		break;
	}
	return dst;
} /* }}} */

/* }}} */


/* {{{ M1 — persistent business-cache cap + approximate-LRU eviction.
 *
 * Problem (audit M1): GENE_G(cache) is a process-persistent (pemalloc) table
 * shared by framework metadata (routes / configs / events) AND the userland
 * Gene\Cache data layer. The framework metadata is written once at startup and
 * is read-only afterwards (in Swoole it is frozen at workerReady()), so it can
 * never grow unbounded. The Gene\Cache layer, however, is allowed to write at
 * request time (it brackets its writes with cache_layer_memory_write_depth>0 to
 * bypass the freeze), so an application that uses Gene\Cache as an in-process
 * data cache grows RSS without bound — the worker never reclaims it.
 *
 * Fix: an opt-in cap, gene.cache_max_items (default 0 = unlimited / fully
 * backward-compatible). When > 0 we track ONLY the Gene\Cache business
 * partition (depth>0 writes) in a parallel ordered set GENE_G(cache_lru) and,
 * on each business write, evict the oldest tracked entries until the partition
 * is back within the cap. Framework metadata and plain userland Gene\Memory::set
 * entries are NEVER tracked and therefore NEVER evicted — routing/config can
 * never break. Recency is updated on write (move-to-tail): a re-set key is the
 * most-recently-used, so eviction approximates LRU. We deliberately do NOT touch
 * recency on read: the read path may run lock-free after workerReady(), so a
 * write into cache_lru there would be a data race. This "lazy, write-triggered"
 * policy is exactly what the audit prescribes.
 *
 * All helpers below assume the caller already holds GENE_CACHE_WRLOCK(). */

/* Core deletion from the main persistent cache, WITHOUT taking the lock and
 * WITHOUT the write_allowed() gate. Factored out of gene_memory_del() so the
 * eviction path (which already holds the write lock) can reuse it without
 * recursive locking. Returns 1 if an entry was removed. */
static int gene_memory_del_core(const char *keyString, size_t keyString_len) {
	zval *stored_val;
	zend_string *stored_key;
	dtor_func_t orig_dtor;

	/* O(1) lookup mirroring gene_memory_get()'s zend_symtable_str_find — handles
	 * both string and numeric-as-index keys. The returned zval* is the first
	 * member of its Bucket, so (Bucket*)zv->key yields the stored key pointer
	 * (NULL for numeric/index entries, which need no manual free). This replaces
	 * the former O(N) scan so capped-cache eviction stays cheap under the lock. */
	stored_val = zend_symtable_str_find(GENE_G(cache), (char *)keyString, keyString_len);
	if (!stored_val) {
		return 0;
	}
	stored_key = ((Bucket *)stored_val)->key;
	gene_memory_zval_dtor(stored_val);
	orig_dtor = GENE_G(cache)->pDestructor;
	GENE_G(cache)->pDestructor = NULL;
	zend_symtable_str_del(GENE_G(cache), keyString, keyString_len);
	GENE_G(cache)->pDestructor = orig_dtor;
	if (stored_key && (GC_FLAGS(stored_key) & (IS_STR_INTERNED | IS_STR_PERMANENT))) {
		pefree(stored_key, 1);
	}
	/* [GENE_FIX:2026-08-07] Drop any TTL bookkeeping for the removed key. */
	if (GENE_G(cache_expiry)) {
		zend_hash_str_del(GENE_G(cache_expiry), keyString, keyString_len);
	}
	return 1;
}

/* [GENE_FIX:2026-08-07-5 N3] Sampling sweep for expired TTL entries.
 * Lazy delete only fires when an expired key is read again; a rotating TTL key
 * (e.g. Memory::set("rate:$ip", $v, 60)) that is never re-read would otherwise
 * keep both the value and the expiry entry alive forever in FPM's process-level
 * cache. Called from gene_memory_set() (WRLOCK held) on every 32nd TTL write;
 * scans the expiry table and drops up to 64 expired entries per pass, so the
 * worst-case growth between sweeps stays bounded. In Swoole the gate in
 * gene_memory_write_allowed() already stops TTL writes after the freeze, so
 * this only ever runs while writes are allowed. */
#define GENE_MEMORY_EXPIRY_SWEEP_INTERVAL 32
#define GENE_MEMORY_EXPIRY_SWEEP_BATCH 64
static void gene_memory_expiry_sweep_nolock(void) {
	HashTable *expiry = GENE_G(cache_expiry);
	zend_string *keys[GENE_MEMORY_EXPIRY_SWEEP_BATCH];
	uint32_t n = 0;
	zend_string *k;
	zval *zv;
	zend_long now;
	if (!expiry || zend_hash_num_elements(expiry) == 0) {
		return;
	}
	now = (zend_long)time(NULL);
	ZEND_HASH_FOREACH_STR_KEY_VAL(expiry, k, zv) {
		if (k && Z_TYPE_P(zv) == IS_LONG && Z_LVAL_P(zv) <= now) {
			keys[n++] = zend_string_copy(k);
			if (n == GENE_MEMORY_EXPIRY_SWEEP_BATCH) {
				break;
			}
		}
	} ZEND_HASH_FOREACH_END();
	while (n > 0) {
		n--;
		/* gene_memory_del_core also removes the key from the expiry table. */
		gene_memory_del_core(ZSTR_VAL(keys[n]), ZSTR_LEN(keys[n]));
		zend_string_release(keys[n]);
	}
}

/* Remove a key from the LRU tracking set (if present) and free its persistent
 * key. The tracking set stores persistent interned keys (zend_string_release is
 * a no-op for those), so we must pefree the stored key manually — same contract
 * as the main cache. No-op when tracking is not active. */
static void gene_cache_lru_remove_nolock(const char *keyString, size_t keyString_len) {
	HashTable *lru = GENE_G(cache_lru);
	zval *zv;
	zend_string *stored_key;

	if (!lru) {
		return;
	}
	/* The tracking set is always keyed by plain (non-numeric-coerced) strings,
	 * so a direct zend_hash_str_find is the right O(1) lookup. */
	zv = zend_hash_str_find(lru, keyString, keyString_len);
	if (!zv) {
		return;
	}
	stored_key = ((Bucket *)zv)->key;
	zend_hash_str_del(lru, keyString, keyString_len);
	if (stored_key && (GC_FLAGS(stored_key) & (IS_STR_INTERNED | IS_STR_PERMANENT))) {
		pefree(stored_key, 1);
	}
}

/* Mark a business key as most-recently-used: drop any existing tracking entry
 * then re-insert at the tail so iteration order = least→most recent. Lazily
 * allocates the tracking table on first use. */
static void gene_cache_lru_touch_nolock(const char *keyString, size_t keyString_len) {
	zend_string *k;
	if (!GENE_G(cache_lru)) {
		PALLOC_HASHTABLE(GENE_G(cache_lru));
		zend_hash_init(GENE_G(cache_lru), 8, NULL, NULL, 1);
	}
	gene_cache_lru_remove_nolock(keyString, keyString_len);
	k = gene_str_persistent(keyString, keyString_len);
	zend_hash_add_empty_element(GENE_G(cache_lru), k);
	/* key now owned by the tracking table; freed on removal/destroy. */
}

/* Evict oldest tracked business entries until the partition fits the cap. */
static void gene_cache_lru_evict_nolock(void) {
	HashTable *lru = GENE_G(cache_lru);
	zend_long cap = GENE_G(cache_max_items);

	if (!lru || cap <= 0) {
		return;
	}
	while ((zend_long)zend_hash_num_elements(lru) > cap) {
		zend_string *victim = NULL;
		zend_string *iter_key;
		char vbuf[256];
		char *vk;
		size_t vlen;
		int vheap = 0;

		ZEND_HASH_FOREACH_STR_KEY(lru, iter_key) {
			victim = iter_key;
			break; /* head = least-recently-used */
		} ZEND_HASH_FOREACH_END();
		if (!victim) {
			break;
		}
		/* The key string is freed by the removals below, so copy it out first. */
		vlen = ZSTR_LEN(victim);
		if (vlen < sizeof(vbuf)) {
			memcpy(vbuf, ZSTR_VAL(victim), vlen);
			vbuf[vlen] = '\0';
			vk = vbuf;
		} else {
			vk = (char *)pemalloc(vlen + 1, 1);
			memcpy(vk, ZSTR_VAL(victim), vlen);
			vk[vlen] = '\0';
			vheap = 1;
		}
		gene_memory_del_core(vk, vlen);
		gene_cache_lru_remove_nolock(vk, vlen);
		if (vheap) {
			pefree(vk, 1);
		}
	}
}

/* Tear down the LRU tracking set, freeing all persistent keys. Mirrors
 * gene_hash_destroy() but the values are IS_NULL placeholders (no dtor). */
void gene_cache_lru_destroy(void) {
	HashTable *lru = GENE_G(cache_lru);
	zend_string **keys = NULL;
	zend_string *key;
	uint32_t key_count = 0;
	uint32_t i;

	if (!lru) {
		return;
	}
	if (lru->nNumUsed > 0) {
		/* [GENE_AUDIT:2026-07-13 M3] pemalloc returns NULL on OOM (no bailout);
		 * skip key collection rather than crash. */
		keys = (zend_string **) pemalloc(sizeof(zend_string *) * lru->nNumUsed, 1);
		if (keys) {
			ZEND_HASH_FOREACH_STR_KEY(lru, key) {
				if (key && (GC_FLAGS(key) & (IS_STR_INTERNED | IS_STR_PERMANENT))) {
					keys[key_count++] = key;
				}
			} ZEND_HASH_FOREACH_END();
		}
	}
	zend_hash_destroy(lru);
	for (i = 0; i < key_count; i++) {
		pefree(keys[i], 1);
	}
	if (keys) {
		pefree(keys, 1);
	}
	pefree(lru, 1);
	GENE_G(cache_lru) = NULL;
}
/* }}} */

/** {{{ zend_long gene_cache_effective_reserve(void)
 * [GENE_FIX:2026-08-23 AUTO-RESERVE] cache_reserve is internal headroom for
 * the frozen bucket array, not a user-tuned capacity — a value <=
 * cache_max_items is structurally contradictory (the table fills before the
 * LRU cap is reached, eviction never triggers, new business keys are refused)
 * and can never be intentional. Auto-correct UPWARD to max_items + margin
 * (cost: extra memory only); never lower max_items, which would silently
 * change eviction semantics. The user-visible warning lives in
 * Application::workerReady(); this function only computes the value actually
 * used for the pre-extend. */
zend_long gene_cache_effective_reserve(void) {
	zend_long reserve = GENE_G(cache_reserve);
	zend_long max_items = GENE_G(cache_max_items);

	if (max_items > 0 && reserve <= max_items) {
		zend_long margin = max_items / 4;
		if (margin < 64) {
			margin = 64;
		}
		return max_items + margin;
	}
	return reserve;
}
/* }}} */

/** {{{ void gene_memory_reserve(void)
 * [GENE_FIX:2026-08-23 UAF-1] Called from Application::workerReady() at the
 * freeze boundary. Pre-extends GENE_G(cache) by the effective reserve (see
 * gene_cache_effective_reserve) so post-freeze business inserts (Gene\Cache
 * layer) fit without resizing the bucket array. Combined with the insert
 * guard in gene_memory_set(), this keeps the arData address constant after
 * the freeze, which is the invariant the lock-free read path and the
 * borrowed-pointer readers rely on. */
void gene_memory_reserve(void) {
	zend_long reserve = gene_cache_effective_reserve();
	/* [GENE_FIX:2026-08-23 IDEMPOTENT] Never extend once the freeze flag is
	 * set: a post-freeze zend_hash_extend can pemalloc+move arData, breaking
	 * the very invariant this function exists to protect. workerReady() now
	 * early-returns on repeat calls; this guard protects against any future
	 * caller reaching here after the freeze. */
	if (!GENE_G(cache) || reserve <= 0 || GENE_G(worker_ready)) {
		return;
	}
	GENE_CACHE_WRLOCK();
	zend_hash_extend(GENE_G(cache),
			GENE_G(cache)->nNumUsed + (uint32_t)reserve, 0);
	GENE_CACHE_WRUNLOCK();
}
/* }}} */

/* [GENE_FIX:2026-08-23 P2-1] Recursive full scan for unsupported types.
 * The UAF-4 pre-check only inspected the outermost zval, but Gene\Cache
 * payloads are nested ({data: ..., version: ...}) — an object (DateTime /
 * ORM Model / closure) at any depth previously reached
 * gene_memory_zval_persistent()'s zend_error(E_ERROR) *after*
 * GENE_CACHE_WRLOCK() was already held: the bailout skipped
 * GENE_CACHE_WRUNLOCK() (permanent write-lock leak) and longjmp'd on the
 * Swoole coroutine stack. Scan the whole payload here, BEFORE any lock is
 * taken; on a hit the caller refuses the write instead of bailing out.
 * Self-referencing (recursive) arrays are likewise refused — they cannot be
 * persisted and would infinitely recurse in the copy path. */
static int gene_memory_zval_is_supported(zval *zv) {
	HashTable *ht;
	zval *element;
	int ok = 1;

	ZVAL_DEREF(zv);
	if (Z_TYPE_P(zv) != IS_ARRAY) {
		return Z_TYPE_P(zv) != IS_OBJECT && Z_TYPE_P(zv) != IS_RESOURCE;
	}
	ht = Z_ARRVAL_P(zv);
	if (GC_IS_RECURSIVE(ht)) {
		return 0;
	}
	GC_TRY_PROTECT_RECURSION(ht);
	ZEND_HASH_FOREACH_VAL(ht, element) {
		if (!gene_memory_zval_is_supported(element)) {
			ok = 0;
			break;
		}
	} ZEND_HASH_FOREACH_END();
	GC_TRY_UNPROTECT_RECURSION(ht);
	return ok;
}

/** {{{ void gene_memory_set(char *keyString,int keyString_len,zval *zvalue, int validity)
 */
void gene_memory_set(char *keyString, size_t keyString_len, zval *zvalue,
		int validity) {
	zval *copyval, ret;
	zend_string *key;
	/* [GENE_MEM:2026-06-19 M1] Only the Gene\Cache data layer (writes bracketed
	 * with cache_layer_memory_write_depth>0) participates in the capped/evictable
	 * business partition; routes/config/userland Memory::set are never tracked. */
	int is_business = (GENE_G(cache_max_items) > 0
		&& GENE_G(cache_layer_memory_write_depth) > 0);
	if (zvalue) {
		/* [GENE_FIX:2026-08-23 P2-1] Reject unsupported types at ANY depth,
		 * BEFORE taking the write lock: the UAF-4 top-level-only check let
		 * nested objects slip through to gene_memory_zval_persistent()'s
		 * E_ERROR, which bails out with GENE_CACHE_WRLOCK() held — a
		 * permanent write-lock leak plus a longjmp on the Swoole coroutine
		 * stack. A refused write degrades to a cache miss; the E_ERROR
		 * branches inside the copy helpers are now unreachable from this
		 * path and remain only as defense for the router/startup writers. */
		if (UNEXPECTED(!gene_memory_zval_is_supported(zvalue))) {
			php_error_docref(NULL, E_WARNING,
				"Gene memory cache does not support object/resource values at any nesting depth; set refused");
			return;
		}
		if (UNEXPECTED(!gene_memory_write_allowed("Memory::set"))) {
			return;
		}
		GENE_CACHE_WRLOCK();
		/* [GENE_FIX:2026-08-07] validity was previously accepted and silently
		 * dropped; record the expiry timestamp alongside the value. */
		gene_memory_set_expiry_nolock(keyString, keyString_len, validity);
		/* [GENE_FIX:2026-08-07-5 N3] Periodically reclaim expired TTL entries
		 * that were written but never read again (closes the FPM growth path). */
		if (validity > 0 && ++GENE_G(memory_expiry_sweep_ctr) % GENE_MEMORY_EXPIRY_SWEEP_INTERVAL == 0) {
			gene_memory_expiry_sweep_nolock();
		}
	copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (copyval == NULL) {
		/* [GENE_FIX:2026-08-23 UAF-1] After the workerReady() freeze the bucket
		 * array address must stay constant: router/DI/config readers hold raw
		 * zval* into it without a lock. A new-key insert that would trigger a
		 * resize (perealloc) is therefore refused — the caller simply gets a
		 * cache miss instead of a SIGSEGV. workerReady() pre-extends the table
		 * by gene.cache_reserve so normal business churn still fits. */
		if (UNEXPECTED(GENE_G(runtime_type) >= 2 && GENE_G(worker_ready)
				&& GENE_G(cache)->nNumUsed >= GENE_G(cache)->nTableSize)) {
			GENE_G(cache_insert_refused)++;
			GENE_CACHE_WRUNLOCK();
			return;
		}
		gene_memory_zval_persistent(&ret, zvalue);
		key = gene_str_persistent(keyString, keyString_len);
		gene_symtable_update(GENE_G(cache), key, &ret);
			/* key is now owned by the hash table; do not free here.
			 * zend_string_release is a no-op for interned strings. */
			if (is_business) {
				gene_cache_lru_touch_nolock(keyString, keyString_len);
				gene_cache_lru_evict_nolock();
			}
			GENE_CACHE_WRUNLOCK();
			return;
		}
		gene_memory_zval_edit_persistent(copyval, zvalue);
		if (is_business) {
			gene_cache_lru_touch_nolock(keyString, keyString_len);
			gene_cache_lru_evict_nolock();
		}
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
	if (UNEXPECTED(gene_memory_expired_nolock(keyString, keyString_len))) {
		GENE_CACHE_RDUNLOCK();
		/* [GENE_FIX:2026-08-07] Lazy delete: only when writes are still
		 * allowed (FPM / pre-workerReady); after the freeze the entry stays
		 * but reads keep reporting a miss, which is the correct TTL semantic. */
		if (!(GENE_G(runtime_type) >= 2 && GENE_G(worker_ready))) {
			gene_memory_del(keyString, keyString_len);
		}
		return NULL;
	}
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
	/* [GENE_FIX:2026-08-07] Apply the same TTL semantics as gene_memory_get:
	 * an expired key must read as missing here too, otherwise the two read
	 * paths disagree. The empty-expiry-table short-circuit in
	 * gene_memory_expired_nolock keeps this free for non-TTL deployments. */
	if (out1) {
		*out1 = (k1 && k1_len && !gene_memory_expired_nolock(k1, k1_len))
			? zend_symtable_str_find(ht, (char *)k1, k1_len) : NULL;
	}
	if (out2) {
		*out2 = (k2 && k2_len && !gene_memory_expired_nolock(k2, k2_len))
			? zend_symtable_str_find(ht, (char *)k2, k2_len) : NULL;
	}
	if (out3) {
		*out3 = (k3 && k3_len && !gene_memory_expired_nolock(k3, k3_len))
			? zend_symtable_str_find(ht, (char *)k3, k3_len) : NULL;
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
 * O(depth) hash lookups. For a 3-4 segment config path (e.g. db/mysql/host)
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
	/* [GENE_FIX:2026-08-07] Honor TTL: expired entries are not "existing". */
	if (result && gene_memory_expired_nolock(keyString, keyString_len)) {
		result = 0;
	}
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
	int ret;

	if (UNEXPECTED(!gene_memory_write_allowed("Memory::del"))) {
		return 0;
	}
	GENE_CACHE_WRLOCK();
	/* gene_memory_del_core() handles the persistent-key free dance (keys are
	 * IS_STR_INTERNED|IS_STR_PERMANENT, so zend_string_release is a no-op and
	 * we must pefree them manually) plus the numeric-index fallback. */
	ret = gene_memory_del_core(keyString, keyString_len);
	if (ret) {
		/* [GENE_MEM:2026-06-19 M1] Keep the LRU tracking set in sync so a
		 * later re-set doesn't see a stale entry / mis-count the partition.
		 * No-op when tracking is inactive (cache_lru == NULL). */
		gene_cache_lru_remove_nolock(keyString, keyString_len);
	}
	GENE_CACHE_WRUNLOCK();
	return ret;
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
	/* [GENE_FEATURE:2026-08-06 F1-7] Userland Memory::get() hit/miss
	 * telemetry (memory_cache_hit/memory_cache_miss in Gene\Monitor::stats).
	 * Only the userland entry point is counted — the internal router/DI
	 * lookups on the hot dispatch path are deliberately excluded. */
	if (zvalue) {
		GENE_G(memory_cache_hit)++;
		gene_memory_zval_local(return_value, zvalue);
		return;
	}
	GENE_G(memory_cache_miss)++;
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

/* [GENE_FEATURE:2026-08-06 F1-2] Build the safe-prefixed cache key used by
 * all Memory methods. Only used by incr/decr for now; the older methods keep
 * their inlined copies untouched. */
static char *gene_memory_build_key(zval *safe, zend_string *keyString, char *stack_buf, size_t stack_size, size_t *out_len, int *out_heap) {
	char *router_e = stack_buf;
	size_t router_e_len;
	*out_heap = 0;
	if (safe && Z_TYPE_P(safe) == IS_STRING && Z_STRLEN_P(safe)) {
		router_e_len = Z_STRLEN_P(safe) + 1 + ZSTR_LEN(keyString);
		if (router_e_len >= stack_size) {
			router_e = emalloc(router_e_len + 1);
			*out_heap = 1;
		}
		memcpy(router_e, Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		router_e[Z_STRLEN_P(safe)] = ':';
		memcpy(router_e + Z_STRLEN_P(safe) + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
	} else {
		router_e_len = 1 + ZSTR_LEN(keyString);
		if (router_e_len >= stack_size) {
			router_e = emalloc(router_e_len + 1);
			*out_heap = 1;
		}
		router_e[0] = ':';
		memcpy(router_e + 1, ZSTR_VAL(keyString), ZSTR_LEN(keyString));
	}
	router_e[router_e_len] = '\0';
	*out_len = router_e_len;
	return router_e;
}

/* [GENE_FEATURE:2026-08-06 F1-2] Atomic-in-lock numeric adjust (incr/decr).
 * The read-modify-write happens entirely inside GENE_CACHE_WRLOCK — never a
 * PHP-level get+set pair, which would race between coroutines. A missing key
 * is created with the stepped value (Redis INCR semantics); an existing
 * non-long value fails with *ok=0 to avoid silently corrupting payloads.
 * Subject to the same workerReady() freeze as Memory::set — under Swoole the
 * process-level cache is read-mostly after boot, so counters meant for the
 * request runtime belong in Gene\Cache (redis/memcached) instead. */
static zend_long gene_memory_adjust(const char *keyString, size_t keyString_len, zend_long step, zend_bool *ok) {
	zval *copyval, ret;
	zend_string *key;
	zend_long result = 0;

	*ok = 0;
	if (UNEXPECTED(!gene_memory_write_allowed("Memory::incr/decr"))) {
		return 0;
	}
	GENE_CACHE_WRLOCK();
	copyval = zend_symtable_str_find(GENE_G(cache), keyString, keyString_len);
	if (copyval == NULL) {
		ZVAL_LONG(&ret, step);
		key = gene_str_persistent(keyString, keyString_len);
		gene_symtable_update(GENE_G(cache), key, &ret);
		/* key is now owned by the hash table; do not free here. */
		*ok = 1;
		result = step;
	} else if (Z_TYPE_P(copyval) == IS_LONG) {
		/* Stored longs are persistent scalars (no refcounted payload), so an
		 * in-place edit under the write lock is safe. */
		Z_LVAL_P(copyval) += step;
		*ok = 1;
		result = Z_LVAL_P(copyval);
	}
	GENE_CACHE_WRUNLOCK();
	return result;
}

/*
 * {{{ public gene_memory::incr(string $key, int $step = 1): int|false
 */
PHP_METHOD(gene_memory, incr) {
	zend_string *keyString;
	zend_long step = 1;
	char stack_buf[256];
	char *router_e;
	size_t router_e_len;
	int router_e_heap = 0;
	zend_bool ok;
	zend_long result;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &keyString, &step) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	router_e = gene_memory_build_key(safe, keyString, stack_buf, sizeof(stack_buf), &router_e_len, &router_e_heap);
	result = gene_memory_adjust(router_e, router_e_len, step, &ok);
	if (router_e_heap) efree(router_e);
	if (!ok) {
		RETURN_FALSE;
	}
	RETURN_LONG(result);
}
/* }}} */

/*
 * {{{ public gene_memory::decr(string $key, int $step = 1): int|false
 */
PHP_METHOD(gene_memory, decr) {
	zend_string *keyString;
	zend_long step = 1;
	char stack_buf[256];
	char *router_e;
	size_t router_e_len;
	int router_e_heap = 0;
	zend_bool ok;
	zend_long result;
	zval *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &keyString, &step) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	router_e = gene_memory_build_key(safe, keyString, stack_buf, sizeof(stack_buf), &router_e_len, &router_e_heap);
	result = gene_memory_adjust(router_e, router_e_len, -step, &ok);
	if (router_e_heap) efree(router_e);
	if (!ok) {
		RETURN_FALSE;
	}
	RETURN_LONG(result);
}
/* }}} */

static zend_string *gene_memory_random_token(void) {
	zval n, raw, hex;
	zend_function *fn;
	ZVAL_LONG(&n, 16);
	ZVAL_UNDEF(&raw);
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("random_bytes"));
	if (UNEXPECTED(!fn)) {
		return NULL;
	}
	zend_call_known_function(fn, NULL, NULL, &raw, 1, &n, NULL);
	if (Z_TYPE(raw) != IS_STRING) {
		zval_ptr_dtor(&raw);
		return NULL;
	}
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("bin2hex"));
	if (UNEXPECTED(!fn)) {
		zval_ptr_dtor(&raw);
		return NULL;
	}
	ZVAL_UNDEF(&hex);
	zend_call_known_function(fn, NULL, NULL, &hex, 1, &raw, NULL);
	zval_ptr_dtor(&raw);
	if (Z_TYPE(hex) != IS_STRING) {
		zval_ptr_dtor(&hex);
		return NULL;
	}
	return Z_STR(hex);
}

/*
 * {{{ public gene_memory::rateLimit(string $key, int $max, int $windowSec): bool
 * Single-process / single-worker only. After Swoole workerReady() Memory is
 * frozen — use Gene\Cache\Redis::rateLimit instead.
 */
PHP_METHOD(gene_memory, rateLimit) {
	zend_string *keyString;
	zend_long max, window;
	char stack_buf[256];
	char *router_e;
	size_t router_e_len;
	int router_e_heap = 0;
	zval *safe, *copyval, one;
	zend_string *pkey;
	zend_long n;
	int allowed = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sll", &keyString, &max, &window) == FAILURE) {
		return;
	}
	if (max < 1 || window < 1) {
		RETURN_FALSE;
	}
	if (UNEXPECTED(!gene_memory_write_allowed("Memory::rateLimit"))) {
		RETURN_FALSE;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	router_e = gene_memory_build_key(safe, keyString, stack_buf, sizeof(stack_buf), &router_e_len, &router_e_heap);
	GENE_CACHE_WRLOCK();
	if (gene_memory_expired_nolock(router_e, router_e_len)) {
		gene_memory_del_core(router_e, router_e_len);
	}
	copyval = zend_symtable_str_find(GENE_G(cache), router_e, router_e_len);
	if (copyval == NULL) {
		ZVAL_LONG(&one, 1);
		pkey = gene_str_persistent(router_e, router_e_len);
		gene_symtable_update(GENE_G(cache), pkey, &one);
		gene_memory_set_expiry_nolock(router_e, router_e_len, (int)window);
		allowed = 1;
	} else if (Z_TYPE_P(copyval) == IS_LONG) {
		n = Z_LVAL_P(copyval);
		if (n < max) {
			Z_LVAL_P(copyval) = n + 1;
			allowed = 1;
		}
	}
	GENE_CACHE_WRUNLOCK();
	if (router_e_heap) efree(router_e);
	RETURN_BOOL(allowed);
}
/* }}} */

/*
 * {{{ public gene_memory::lock(string $key, int $ttlSec): string|false
 */
PHP_METHOD(gene_memory, lock) {
	zend_string *keyString, *token;
	zend_long ttl;
	char stack_buf[256];
	char *router_e;
	size_t router_e_len;
	int router_e_heap = 0;
	zval *safe, *copyval, tok;
	zend_string *pkey;
	int ok = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sl", &keyString, &ttl) == FAILURE) {
		return;
	}
	if (ttl < 1) {
		RETURN_FALSE;
	}
	if (UNEXPECTED(!gene_memory_write_allowed("Memory::lock"))) {
		RETURN_FALSE;
	}
	token = gene_memory_random_token();
	if (!token) {
		RETURN_FALSE;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	router_e = gene_memory_build_key(safe, keyString, stack_buf, sizeof(stack_buf), &router_e_len, &router_e_heap);
	GENE_CACHE_WRLOCK();
	if (gene_memory_expired_nolock(router_e, router_e_len)) {
		gene_memory_del_core(router_e, router_e_len);
	}
	copyval = zend_symtable_str_find(GENE_G(cache), router_e, router_e_len);
	if (copyval == NULL) {
		zval src;
		ZVAL_STR(&src, token); /* borrow; gene_memory_zval_persistent copies */
		gene_memory_zval_persistent(&tok, &src);
		pkey = gene_str_persistent(router_e, router_e_len);
		gene_symtable_update(GENE_G(cache), pkey, &tok);
		gene_memory_set_expiry_nolock(router_e, router_e_len, (int)ttl);
		ok = 1;
	}
	GENE_CACHE_WRUNLOCK();
	if (router_e_heap) efree(router_e);
	if (ok) {
		RETURN_STR(token);
	}
	zend_string_release(token);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_memory::unlock(string $key, string $token): bool
 */
PHP_METHOD(gene_memory, unlock) {
	zend_string *keyString, *token;
	char stack_buf[256];
	char *router_e;
	size_t router_e_len;
	int router_e_heap = 0;
	zval *safe, *copyval;
	int ok = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &keyString, &token) == FAILURE) {
		return;
	}
	if (UNEXPECTED(!gene_memory_write_allowed("Memory::unlock"))) {
		RETURN_FALSE;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	router_e = gene_memory_build_key(safe, keyString, stack_buf, sizeof(stack_buf), &router_e_len, &router_e_heap);
	GENE_CACHE_WRLOCK();
	if (gene_memory_expired_nolock(router_e, router_e_len)) {
		gene_memory_del_core(router_e, router_e_len);
	} else {
		copyval = zend_symtable_str_find(GENE_G(cache), router_e, router_e_len);
		if (copyval && Z_TYPE_P(copyval) == IS_STRING
			&& Z_STRLEN_P(copyval) == ZSTR_LEN(token)
			&& memcmp(Z_STRVAL_P(copyval), ZSTR_VAL(token), ZSTR_LEN(token)) == 0) {
			ok = gene_memory_del_core(router_e, router_e_len);
		}
	}
	GENE_CACHE_WRUNLOCK();
	if (router_e_heap) efree(router_e);
	RETURN_BOOL(ok);
}
/* }}} */

/*
 * {{{ public gene_memory::mget(array $keys): array
 * [GENE_FEATURE:2026-08-07] Batch read. Returns an assoc array mapping each
 * requested key to its value (null on miss), preserving request order.
 * Hit/miss telemetry is counted per key, same as get().
 */
PHP_METHOD(gene_memory, mget) {
	zval *keys, *entry, *safe;
	zend_string *orig_key;
	zend_ulong num_idx;
	char stack_buf[256];
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &keys) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	array_init(return_value);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(keys), num_idx, orig_key, entry) {
		char *router_e;
		size_t router_e_len;
		int router_e_heap = 0;
		zval *zvalue, item;
		if (!orig_key) {
			/* Numeric-keyed list of key names: the entry value is the key. */
			if (Z_TYPE_P(entry) != IS_STRING) {
				continue;
			}
			orig_key = Z_STR_P(entry);
		} else if (ZSTR_LEN(orig_key) == 0) {
			continue;
		}
		router_e = gene_memory_build_key(safe, orig_key, stack_buf, sizeof(stack_buf), &router_e_len, &router_e_heap);
		zvalue = gene_memory_get(router_e, router_e_len);
		if (router_e_heap) efree(router_e);
		if (zvalue) {
			GENE_G(memory_cache_hit)++;
			gene_memory_zval_local(&item, zvalue);
		} else {
			GENE_G(memory_cache_miss)++;
			ZVAL_NULL(&item);
		}
		zend_hash_update(Z_ARRVAL_P(return_value), orig_key, &item);
	} ZEND_HASH_FOREACH_END();
}
/* }}} */

/*
 * {{{ public gene_memory::mset(array $values, int $ttl = 0): bool
 * [GENE_FEATURE:2026-08-07] Batch write. Each assoc entry is stored under its
 * (safe-prefixed) key via the same gene_memory_set() path as set(), including
 * the workerReady() freeze guard. Non-string keys are skipped.
 */
PHP_METHOD(gene_memory, mset) {
	zval *values, *entry, *safe;
	zend_string *orig_key;
	zend_ulong num_idx;
	zend_long validity = 0;
	char stack_buf[256];
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|l", &values, &validity) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_memory_ce, gene_strip_obj(getThis()), GENE_MEMORY_SAFE, strlen(GENE_MEMORY_SAFE), 1, NULL);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(values), num_idx, orig_key, entry) {
		char *router_e;
		size_t router_e_len;
		int router_e_heap = 0;
		if (!orig_key || ZSTR_LEN(orig_key) == 0) {
			continue;
		}
		router_e = gene_memory_build_key(safe, orig_key, stack_buf, sizeof(stack_buf), &router_e_len, &router_e_heap);
		gene_memory_set(router_e, router_e_len, entry, (int)validity);
		if (router_e_heap) efree(router_e);
	} ZEND_HASH_FOREACH_END();
	RETURN_TRUE;
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
	/* [GENE_FIX:2026-08-07] Wipe TTL bookkeeping together with the cache. */
	if (GENE_G(cache_expiry)) {
		zend_hash_destroy(GENE_G(cache_expiry));
		pefree(GENE_G(cache_expiry), 1);
		GENE_G(cache_expiry) = NULL;
	}
	/* [GENE_MEM:2026-06-19 M1] clean() wipes the whole persistent cache, so the
	 * LRU tracking set's keys now point at freed entries — drop it too. It will
	 * be lazily re-created on the next business write. */
	gene_cache_lru_destroy();
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
 *     'closure_src_cache_items' => int,
 *     'route_pc_items'    => int,
 *     'co_contexts_*'     => int,   // high-water and sweep telemetry
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
	add_assoc_long(return_value, "co_contexts_watermark", (zend_long)GENE_G(co_contexts_watermark));
	add_assoc_long(return_value, "co_contexts_sweep_count", (zend_long)GENE_G(co_contexts_sweep_count));
	add_assoc_long(return_value, "co_contexts_sweep_scanned", (zend_long)GENE_G(co_contexts_sweep_scanned));
	add_assoc_long(return_value, "co_contexts_sweep_us", (zend_long)GENE_G(co_contexts_sweep_us));
	/* [GENE_PERF:2026-07-30 M1] Cap triggers suppressed by the sweep
	 * cooldown (cap/4 new allocations or table growth past the last-sweep
	 * mark re-arms the next sweep). */
	add_assoc_long(return_value, "co_contexts_sweep_skipped", (zend_long)GENE_G(co_contexts_sweep_skipped));
	/* [GENE_PERF:2026-04-24] Context struct pool visibility. */
	add_assoc_long(return_value, "ctx_pool_size", GENE_G(ctx_pool_size));
	add_assoc_long(return_value, "ctx_pool_max", GENE_G(ctx_pool_max));
	add_assoc_long(return_value, "ctx_pool_hit", (zend_long)GENE_G(ctx_pool_hit));
	add_assoc_long(return_value, "ctx_pool_miss", (zend_long)GENE_G(ctx_pool_miss));
	add_assoc_long(return_value, "cache_business_items",
		GENE_G(cache_lru) ? (zend_long)zend_hash_num_elements(GENE_G(cache_lru)) : 0);
	add_assoc_long(return_value, "route_pc_items",
		GENE_G(route_pc) ? (zend_long)zend_hash_num_elements(GENE_G(route_pc)) : 0);
	add_assoc_long(return_value, "closure_src_cache_items", gene_closure_src_cache_items());
	add_assoc_long(return_value, "closure_src_cache_flushes", (zend_long)GENE_G(closure_src_cache_flushes));
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
	PHP_ME(gene_memory, incr, gene_memory_arg_incr, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, decr, gene_memory_arg_incr, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, rateLimit, gene_memory_arg_rate_limit, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, lock, gene_memory_arg_lock, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, unlock, gene_memory_arg_unlock, ZEND_ACC_PUBLIC)
	/* [GENE_FEATURE:2026-08-07] Batch read/write. */
	PHP_ME(gene_memory, mget, gene_memory_arg_mget, ZEND_ACC_PUBLIC)
	PHP_ME(gene_memory, mset, gene_memory_arg_mset, ZEND_ACC_PUBLIC)
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
