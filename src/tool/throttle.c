/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01 S7] Atomic failure counting / throttling over cache
 * stores. Replaces the 7 read-compare-write failure counters in exchange/
 * mchat/gene_web which all race under concurrent requests.
 *
 * Semantics (fixed window, "count failures, clear on success"):
 *   hit(store, key, ttl)          atomic +1, ttl set when the window opens
 *   tooMany(store, key, max)      READ-ONLY: count >= max (gate before work)
 *   attempt(store, key, max, ttl) hit() then count <= max (send-rate limits)
 *   remaining(store, key, max)    max - count, floored at 0
 *   count / clear
 * Keys are namespaced under "gene:thr:" so they never collide with
 * business keys.
 *
 * Store dispatch (R2):
 *   Gene\Memory       → C fast path, incr-with-ttl under the cache wrlock
 *   Gene\Cache\Redis  → one Lua script (INCR + EXPIRE on create)
 *   other objects     → incr (+expire when it created the key) → add(nx)
 *                       → set(nx,ex) → incr; -1 when the store refuses
 * The earlier generic-only probe created counters via INCR without a TTL on
 * both Memory and Redis — a throttled key never expired (permanent lockout). */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../cache/memory.h"
#include "../cache/redis.h"
#include "../tool/throttle.h"

zend_class_entry *gene_throttle_ce;

#define GENE_THR_PREFIX "gene:thr:"

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
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_throttle_attempt_arginfo, 0, 0, 3)
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

static zend_string *gene_throttle_key(zend_string *key) {
	return zend_string_concat2(ZEND_STRL(GENE_THR_PREFIX), ZSTR_VAL(key), ZSTR_LEN(key));
}

/* Call $store->$method(...$params); returns retval zval (UNDEF on failure).
 * Clears any exception raised inside the driver — a missing method or a
 * signature mismatch means "try the next form", not a userland error.
 * Stores that expose their driver API through __call (Gene\Cache\Redis and
 * Gene\Cache\Memcached both do) get the magic handler invoked exactly like
 * a userland call — otherwise e.g. clear() silently no-ops on them. */
static zval gene_throttle_call(zval *store, const char *method, size_t mlen, uint32_t argc, zval *params) {
	zval ret;
	zend_function *fn;
	ZVAL_UNDEF(&ret);
	fn = zend_hash_str_find_ptr(&Z_OBJCE_P(store)->function_table, method, mlen);
	if (fn) {
		zend_call_known_function(fn, Z_OBJ_P(store), Z_OBJCE_P(store), &ret, argc, params, NULL);
	} else if ((fn = Z_OBJCE_P(store)->__call) != NULL
			&& (fn->common.fn_flags & ZEND_ACC_PUBLIC)) {
		zval cparams[2], args;
		uint32_t i;
		array_init(&args);
		for (i = 0; i < argc; i++) {
			Z_TRY_ADDREF(params[i]);
			add_next_index_zval(&args, &params[i]);
		}
		ZVAL_STRINGL(&cparams[0], method, mlen);
		ZVAL_COPY_VALUE(&cparams[1], &args);
		zend_call_known_function(fn, Z_OBJ_P(store), Z_OBJCE_P(store), &ret, 2, cparams, NULL);
		zval_ptr_dtor(&cparams[0]);
		zval_ptr_dtor(&cparams[1]);
	}
	if (EG(exception)) {
		zend_clear_exception();
		zval_ptr_dtor(&ret);
		ZVAL_UNDEF(&ret);
	}
	return ret;
}

static int gene_throttle_long(zval *ret, zend_long *out) {
	if (Z_TYPE_P(ret) == IS_LONG) {
		*out = Z_LVAL_P(ret);
		return 1;
	}
	if (Z_TYPE_P(ret) == IS_STRING && Z_STRLEN_P(ret) > 0) {
		zend_long l;
		double d;
		if (is_numeric_string(Z_STRVAL_P(ret), Z_STRLEN_P(ret), &l, &d, 0) == IS_LONG) {
			*out = l;
			return 1;
		}
	}
	return 0;
}

/* {{{ hit core on a prefixed key: new count, -1 when the store refused */
static zend_long gene_throttle_hit_raw(zval *store, zend_string *key, zend_long ttl) {
	zval params[3], ret;
	zend_long n = -1;

	if (instanceof_function(Z_OBJCE_P(store), gene_memory_ce)) {
		zend_bool ok = 0;
		n = gene_memory_incr_ttl(store, key, 1, ttl, &ok);
		return ok ? n : -1;
	}
	if (instanceof_function(Z_OBJCE_P(store), gene_redis_ce)) {
		return gene_redis_hit(store, key, ttl);
	}

	/* 1. incr — when it created the key (n == 1) attach the window ttl */
	ZVAL_STR(&params[0], key);
	ret = gene_throttle_call(store, ZEND_STRL("incr"), 1, params);
	if (gene_throttle_long(&ret, &n) && n > 0) {
		zval_ptr_dtor(&ret);
		if (n == 1) {
			ZVAL_LONG(&params[1], ttl);
			ret = gene_throttle_call(store, ZEND_STRL("expire"), 2, params);
			zval_ptr_dtor(&ret);
		}
		return n;
	}
	zval_ptr_dtor(&ret);

	/* 2. add(key, 1, ttl) — memcached-shaped atomic create */
	ZVAL_LONG(&params[1], 1);
	ZVAL_LONG(&params[2], ttl);
	ret = gene_throttle_call(store, ZEND_STRL("add"), 3, params);
	if (!Z_ISUNDEF(ret) && zend_is_true(&ret)) {
		zval_ptr_dtor(&ret);
		return 1;
	}
	zval_ptr_dtor(&ret);

	/* 3. set(key, 1, ['nx', 'ex' => ttl]) — phpredis-shaped atomic create */
	array_init(&params[2]);
	add_next_index_string(&params[2], "nx");
	add_assoc_long_ex(&params[2], ZEND_STRL("ex"), ttl);
	ret = gene_throttle_call(store, ZEND_STRL("set"), 3, params);
	zval_ptr_dtor(&params[2]);
	if (!Z_ISUNDEF(ret) && zend_is_true(&ret)) {
		zval_ptr_dtor(&ret);
		return 1;
	}
	zval_ptr_dtor(&ret);

	/* 4. incr again — lost the create race to a concurrent request */
	ret = gene_throttle_call(store, ZEND_STRL("incr"), 1, params);
	if (gene_throttle_long(&ret, &n) && n > 0) {
		zval_ptr_dtor(&ret);
		return n;
	}
	zval_ptr_dtor(&ret);
	return -1;
}
/* }}} */

static zend_long gene_throttle_hit(zval *store, zend_string *key, zend_long ttl) {
	zend_string *k = gene_throttle_key(key);
	zend_long n = gene_throttle_hit_raw(store, k, ttl);
	zend_string_release(k);
	return n;
}

static zend_long gene_throttle_count(zval *store, zend_string *key) {
	zval params[1], ret;
	zend_long n = 0;
	zend_string *k = gene_throttle_key(key);
	ZVAL_STR(&params[0], k);
	ret = gene_throttle_call(store, ZEND_STRL("get"), 1, params);
	if (Z_TYPE(ret) == IS_DOUBLE) {
		n = (zend_long)Z_DVAL(ret);
	} else if (!gene_throttle_long(&ret, &n)) {
		n = 0;
	}
	zval_ptr_dtor(&ret);
	zend_string_release(k);
	return n < 0 ? 0 : n;
}

/* {{{ proto static int Gene\Throttle::hit(object $store, string $key [, int $ttl = 3600])
 * Atomic +1; ttl applies when the window opens. → new count | -1 on store failure */
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

/* {{{ proto static bool Gene\Throttle::tooMany(object $store, string $key, int $max)
 * Read-only gate: true when the recorded count already reached $max.
 * Pattern: if (tooMany) reject; ...; $ok ? clear() : hit(). */
PHP_METHOD(gene_throttle, tooMany) {
	zval *store;
	zend_string *key;
	zend_long max;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oSl", &store, &key, &max) == FAILURE) {
		return;
	}
	if (max < 1) {
		RETURN_TRUE;
	}
	RETURN_BOOL(gene_throttle_count(store, key) >= max);
}
/* }}} */

/* {{{ proto static bool Gene\Throttle::attempt(object $store, string $key, int $max [, int $ttl = 3600])
 * Counts THIS attempt atomically. → true while within budget. */
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
		RETURN_TRUE; /* fail-open: store outage must not lock everyone out */
	}
	RETURN_BOOL(n <= max);
}
/* }}} */

/* {{{ proto static int Gene\Throttle::count(object $store, string $key) */
PHP_METHOD(gene_throttle, count) {
	zval *store;
	zend_string *key;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oS", &store, &key) == FAILURE) {
		return;
	}
	RETURN_LONG(gene_throttle_count(store, key));
}
/* }}} */

/* {{{ proto static int Gene\Throttle::remaining(object $store, string $key, int $max) */
PHP_METHOD(gene_throttle, remaining) {
	zval *store;
	zend_string *key;
	zend_long max, n;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oSl", &store, &key, &max) == FAILURE) {
		return;
	}
	n = max - gene_throttle_count(store, key);
	RETURN_LONG(n < 0 ? 0 : n);
}
/* }}} */

/* {{{ proto static bool Gene\Throttle::clear(object $store, string $key)
 * delete(key) → del(key) fallbacks. */
PHP_METHOD(gene_throttle, clear) {
	zval *store;
	zend_string *key, *k;
	zval params[1], ret;
	zend_bool done = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "oS", &store, &key) == FAILURE) {
		return;
	}
	k = gene_throttle_key(key);
	ZVAL_STR(&params[0], k);
	ret = gene_throttle_call(store, ZEND_STRL("delete"), 1, params);
	done = !Z_ISUNDEF(ret);
	zval_ptr_dtor(&ret);
	if (!done) {
		ret = gene_throttle_call(store, ZEND_STRL("del"), 1, params);
		done = !Z_ISUNDEF(ret);
		zval_ptr_dtor(&ret);
	}
	zend_string_release(k);
	RETURN_BOOL(done);
}
/* }}} */

const zend_function_entry gene_throttle_methods[] = {
	PHP_ME(gene_throttle, hit, gene_throttle_hit_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, tooMany, gene_throttle_many_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, attempt, gene_throttle_attempt_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, count, gene_throttle_basic_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_throttle, remaining, gene_throttle_many_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
