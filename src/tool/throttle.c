/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01 S7] Atomic throttling over generic cache stores.
 * Replaces the 7 read-compare-write failure counters in exchange/mchat/gene_web
 * which all race under concurrent requests.
 *
 * hit() algorithm (no store-type sniffing, signature probing instead):
 *   1. incr(key)            → number: done (Redis INCR creates atomically,
 *                             Memory::incr needs existing key)
 *   2. set(key,1,{nx,ex})   → truthy: count=1 (phpredis-shaped drivers)
 *   3. incr(key) again      → number: the nx-loser's path under race
 *   4. set(key,1,ttl_long)  → truthy: count=1 (Gene\Memory set(key,val,ttl))
 *   else → -1 (fail-open: callers treat -1 as allowed)
 * Every method call goes through zend_call_known_function on whatever object
 * the app passes in — Memory, Gene\Cache\Redis, Memcached, or any compatible
 * component. No driver-specific branches, no per-request allocation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../tool/throttle.h"

zend_class_entry *gene_throttle_ce;

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(gene_throttle_hit_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, store)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_throttle_many_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, store)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, max)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_throttle_basic_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, store)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()
/* }}} */

/* Call $store->$method(...$params); returns retval zval (UNDEF on failure).
 * Clears any exception raised inside the driver — a missing method or a
 * signature mismatch means "try the next form", not a userland error. */
static zval gene_throttle_call(zval *store, const char *method, size_t mlen, uint32_t argc, zval *params) {
	zval ret;
	zend_function *fn;
	ZVAL_UNDEF(&ret);
	if (!store || Z_TYPE_P(store) != IS_OBJECT) {
		return ret;
	}
	fn = zend_hash_str_find_ptr(&Z_OBJCE_P(store)->function_table, method, mlen);
	if (!fn) {
		return ret;
	}
	zend_call_known_function(fn, Z_OBJ_P(store), Z_OBJCE_P(store), &ret, argc, params, NULL);
	if (EG(exception)) {
		zend_clear_exception();
		zval_ptr_dtor(&ret);
		ZVAL_UNDEF(&ret);
	}
	return ret;
}

/* {{{ hit(): returns new count, 1 on fresh create, -1 when store refused */
static zend_long gene_throttle_hit(zval *store, zend_string *key, zend_long ttl) {
	zval params[3], ret;
	zend_long n = -1;

	/* 1. incr */
	ZVAL_STR(&params[0], key);
	ret = gene_throttle_call(store, ZEND_STRL("incr"), 1, params);
	if (Z_TYPE(ret) == IS_LONG || (Z_TYPE(ret) == IS_STRING && Z_STRLEN(ret) > 0)) {
		n = zval_get_long(&ret);
		zval_ptr_dtor(&ret);
		return n;
	}
	zval_ptr_dtor(&ret);

	/* 2. set(key, 1, {nx:1, ex:ttl}) — phpredis shape */
	ZVAL_STR(&params[0], key);
	ZVAL_LONG(&params[1], 1);
	array_init(&params[2]);
	add_assoc_bool_ex(&params[2], ZEND_STRL("nx"), 1);
	add_assoc_long_ex(&params[2], ZEND_STRL("ex"), ttl);
	ret = gene_throttle_call(store, ZEND_STRL("set"), 3, params);
	zval_ptr_dtor(&params[2]);
	if (!Z_ISUNDEF(ret) && zend_is_true(&ret)) {
		zval_ptr_dtor(&ret);
		return 1;
	}
	zval_ptr_dtor(&ret);

	/* 3. incr again — we lost the create race OR store had it all along */
	ZVAL_STR(&params[0], key);
	ret = gene_throttle_call(store, ZEND_STRL("incr"), 1, params);
	if (Z_TYPE(ret) == IS_LONG || (Z_TYPE(ret) == IS_STRING && Z_STRLEN(ret) > 0)) {
		n = zval_get_long(&ret);
		zval_ptr_dtor(&ret);
		return n;
	}
	zval_ptr_dtor(&ret);

	/* 4. set(key, 1, ttl) — Gene\Memory long-validity shape */
	ZVAL_STR(&params[0], key);
	ZVAL_LONG(&params[1], 1);
	ZVAL_LONG(&params[2], ttl);
	ret = gene_throttle_call(store, ZEND_STRL("set"), 3, params);
	if (!Z_ISUNDEF(ret) && zend_is_true(&ret)) {
		zval_ptr_dtor(&ret);
		return 1;
	}
	zval_ptr_dtor(&ret);
	return -1;
}
/* }}} */

/* {{{ proto static int Gene\Throttle::hit(object $store, string $key [, int $ttl = 3600])
 * Atomic increment with ttl on first write. → new count | -1 on store failure */
PHP_METHOD(gene_throttle, hit) {
	zval *store;
	zend_string *key;
	zend_long ttl = 3600;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oS|l", &store, &key, &ttl) == FAILURE) {
		return;
	}
	if (ttl < 1) {
		zend_argument_value_error(3, "must be >= 1");
		RETURN_THROWS();
	}
	RETURN_LONG(gene_throttle_hit(store, key, ttl));
}
/* }}} */

/* {{{ proto static bool Gene\Throttle::tooMany(object $store, string $key, int $max [, int $ttl = 3600])
 * Counts THIS attempt atomically, then compares. → true when over max. */
PHP_METHOD(gene_throttle, tooMany) {
	zval *store;
	zend_string *key;
	zend_long max, ttl = 3600;
	zend_long n;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oSl|l", &store, &key, &max, &ttl) == FAILURE) {
		return;
	}
	if (max < 1) {
		RETURN_FALSE;
	}
	n = gene_throttle_hit(store, key, ttl < 1 ? 3600 : ttl);
	if (n < 0) {
		RETURN_FALSE; /* fail-open */
	}
	RETURN_BOOL(n > max);
}
/* }}} */

/* {{{ proto static bool Gene\Throttle::attempt(object $store, string $key, int $max [, int $ttl = 3600])
 * Counts THIS attempt. → true when within budget (inverse of tooMany). */
PHP_METHOD(gene_throttle, attempt) {
	zval *store;
	zend_string *key;
	zend_long max, ttl = 3600;
	zend_long n;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oSl|l", &store, &key, &max, &ttl) == FAILURE) {
		return;
	}
	if (max < 1) {
		RETURN_FALSE;
	}
	n = gene_throttle_hit(store, key, ttl < 1 ? 3600 : ttl);
	if (n < 0) {
		RETURN_TRUE; /* fail-open */
	}
	RETURN_BOOL(n <= max);
}
/* }}} */

/* {{{ proto static int Gene\Throttle::count(object $store, string $key)
 * Read-only: current counter value, 0 when missing/unreadable. */
PHP_METHOD(gene_throttle, count) {
	zval *store;
	zend_string *key;
	zval params[1], ret;
	zend_long n = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oS", &store, &key) == FAILURE) {
		return;
	}
	ZVAL_STR(&params[0], key);
	ret = gene_throttle_call(store, ZEND_STRL("get"), 1, params);
	if (Z_TYPE(ret) == IS_LONG || Z_TYPE(ret) == IS_DOUBLE
		|| (Z_TYPE(ret) == IS_STRING && Z_STRLEN(ret) > 0)) {
		n = zval_get_long(&ret);
	}
	zval_ptr_dtor(&ret);
	RETURN_LONG(n);
}
/* }}} */

/* {{{ proto static bool Gene\Throttle::clear(object $store, string $key)
 * delete(key) → del(key) → set(key,0,1) fallbacks. */
PHP_METHOD(gene_throttle, clear) {
	zval *store;
	zend_string *key;
	zval params[3], ret;
	zend_bool done = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oS", &store, &key) == FAILURE) {
		return;
	}
	ZVAL_STR(&params[0], key);
	ret = gene_throttle_call(store, ZEND_STRL("delete"), 1, params);
	if (!Z_ISUNDEF(ret) && zend_is_true(&ret)) done = 1;
	zval_ptr_dtor(&ret);
	if (!done) {
		ret = gene_throttle_call(store, ZEND_STRL("del"), 1, params);
		if (!Z_ISUNDEF(ret) && zend_is_true(&ret)) done = 1;
		zval_ptr_dtor(&ret);
	}
	if (!done) {
		ZVAL_LONG(&params[1], 0);
		ZVAL_LONG(&params[2], 1);
		ret = gene_throttle_call(store, ZEND_STRL("set"), 3, params);
		if (!Z_ISUNDEF(ret) && zend_is_true(&ret)) done = 1;
		zval_ptr_dtor(&ret);
	}
	RETURN_BOOL(done);
}
/* }}} */

const zend_function_entry gene_throttle_methods[] = {
	PHP_ME(gene_throttle, hit, gene_throttle_hit_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, tooMany, gene_throttle_many_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, attempt, gene_throttle_many_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, count, gene_throttle_basic_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, clear, gene_throttle_basic_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(throttle) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Throttle", "Gene\\Throttle", gene_throttle_methods);
	gene_throttle_ce = zend_register_internal_class(&ce);
	gene_throttle_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
/* }}} */
