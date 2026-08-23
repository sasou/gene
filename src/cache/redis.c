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
#include "ext/standard/php_rand.h"
#include <string.h>


#include "../gene.h"
#include "../common/common.h"
#include "../cache/redis.h"
#include "../factory/factory.h"
#include "../cache/redis_pool.h"

zend_class_entry * gene_redis_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_redis_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_call_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_rate_limit_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, max)
	ZEND_ARG_INFO(0, windowSec)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_lock_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, ttlSec)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_unlock_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, token)
ZEND_END_ARG_INFO()

int gene_redis_connect(zval *object, zval *host, zval *port, zval *timeout) /*{{{*/
{
    zval retval;
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("connect"));
	zval params[] = { *host, *port, *timeout };
    ZVAL_UNDEF(&retval);
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), &retval, 3, params, NULL);
    }
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&retval);
    return ret;
}/*}}}*/


int gene_redis_pconnect(zval *object, zval *host, zval *port, zval *timeout, zval *persistent) /*{{{*/
{
    zval retval;
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("pconnect"));
	zval params[] = { *host, *port, *timeout, *persistent };
    ZVAL_UNDEF(&retval);
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), &retval, 4, params, NULL);
    }
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&retval);
    return ret;
}/*}}}*/

int gene_redis_setOption(zval *object, zval *key, zval *value) /*{{{*/
{
    zval retval;
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("setoption"));
	zval params[] = { *key, *value };
    ZVAL_UNDEF(&retval);
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), &retval, 2, params, NULL);
    }
    int ret =  (Z_TYPE(retval) == IS_TRUE ) ? 1 : 0;
    zval_ptr_dtor(&retval);
    return ret;
}/*}}}*/

void gene_redis_set(zval *object, zval *key, zval *value, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("set"));
	zval params[] = { *key, *value };
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 2, params, NULL);
    }
}/*}}}*/

void gene_redis_auth(zval *object, zval *value, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("auth"));
	zval params[] = { *value };
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 1, params, NULL);
    }
}/*}}}*/

void gene_redis_setEx(zval *object, zval *key, zval *ttl, zval *value, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("setex"));
	zval params[] = { *key, *ttl, *value };
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 3, params, NULL);
    }
}/*}}}*/


void gene_redis_get(zval *object, zval *key, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("get"));
	zval params[] = { *key };
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 1, params, NULL);
    }
}/*}}}*/

void gene_redis_mGet(zval *object, zval *key, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, ZEND_STRL("mget"));
	zval params[] = { *key };
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 1, params, NULL);
    }
}/*}}}*/

bool checkError (zend_object *ex) {
	zval *msg;
	zend_class_entry *ce;
	zval zv, rv;
	int i;
	/* Redis / Swoole-hooked socket disconnection error patterns.
	 * Deliberately excludes "Connection refused" / "Connection timed out":
	 * those mean the server is unreachable, so an immediate reconnect would
	 * just fail again and create an uncontrolled retry loop. */
	const char *redisErrors[] = {
		"read error on connection",   /* phpredis: socket read failure      */
		"failed with errno=10054",    /* Windows:  connection reset by peer */
		"Connection closed",          /* server closed the connection       */
		"Connection lost",            /* phpredis internal                  */
		"Failed reading data from stream", /* stream wrapper disconnect     */
		"went away"                   /* server restart detection           */
	};

	ZVAL_OBJ(&zv, ex);
	ce = Z_OBJCE(zv);

	msg = zend_read_property(ce, gene_strip_obj(&zv), ZEND_STRL("message"), 0, &rv);
	if (!msg || Z_TYPE_P(msg) != IS_STRING) {
		return 0;
	}
	for (i = 0; i < (int)(sizeof(redisErrors) / sizeof(redisErrors[0])); i++) {
		if (strstr(Z_STRVAL_P(msg), redisErrors[i]) != NULL) {
			return 1;
		}
	}
	return 0;
}

bool initRObj (zval * self, zval *config) {
	zval  *host = NULL, *port = NULL, *servers = NULL,*oneServers = NULL,*timeout = NULL, *persistent = NULL, *options = NULL, *serializer = NULL, *password = NULL;
	zval   obj_object;
	int    connected;

	if (config == NULL) {
		config =  zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_CONFIG), 1, NULL);
	}
	/* [GENE_FIX:2026-05-24] gene_lookup_class_str avoids the unsafe
	 * static zend_string* + zend_string_init_interned(...,1) pattern that
	 * dangles across requests under opcache.file_cache_only=1. Redis is an
	 * internal class registered at MINIT, so the EG(class_table) fast
	 * path inside gene_lookup_class_str is a single hash hit. */
	zend_class_entry *obj_ptr = gene_lookup_class_str(ZEND_STRL("Redis"));

	if (!obj_ptr) {
		/* E_WARNING instead of E_ERROR: E_ERROR kills the Swoole worker process */
		php_error_docref(NULL, E_WARNING, "Gene\\Cache\\Redis: Redis extension not found.");
		return 0;
	}

	object_init_ex(&obj_object, obj_ptr);

	host    = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("host"));
	port    = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("port"));
	servers = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("servers"));
	timeout = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("timeout"));

	if (!(host && port)) {
		if (servers && Z_TYPE_P(servers) == IS_ARRAY) {
			uint32_t num = zend_hash_num_elements(Z_ARRVAL_P(servers));
			uint32_t index = php_mt_rand_range(0, num - 1);
			oneServers = zend_hash_index_find(Z_ARRVAL_P(servers), index);
			if (oneServers) {
				host = zend_hash_str_find(Z_ARRVAL_P(oneServers), ZEND_STRL("host"));
				port = zend_hash_str_find(Z_ARRVAL_P(oneServers), ZEND_STRL("port"));
			}
		} else {
			php_error_docref(NULL, E_WARNING, "Gene\\Cache\\Redis: missing host/port in config.");
			zval_ptr_dtor(&obj_object);
			return 0;
		}
	}

	if (!timeout) {
		php_error_docref(NULL, E_WARNING, "Gene\\Cache\\Redis: missing timeout in config.");
		zval_ptr_dtor(&obj_object);
		return 0;
	}

	serializer = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("serializer"));
	if (serializer && Z_TYPE_P(serializer) == IS_LONG) {
		zend_update_property_long(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_SERIALIZE), Z_LVAL_P(serializer));
	}
	persistent = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("persistent"));
	options    = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("options"));

	/* In Swoole mode pconnect is forbidden: persistent connections share a
	 * single TCP socket across coroutines, causing "socket already bound to
	 * another coroutine" errors.  runtime_type >= 2 means Swoole mode. */
	if (persistent && GENE_G(runtime_type) < 2) {
		connected = gene_redis_pconnect(&obj_object, host, port, timeout, persistent);
	} else {
		connected = gene_redis_connect(&obj_object, host, port, timeout);
	}

	/* connect() can fail two ways:
	 *   a) returns false without throwing  → emit warning, bail
	 *   b) throws an exception            → leave EG(exception) for caller
	 * In both cases we must NOT store the partially-initialised Redis object
	 * in self->obj, and we must NOT call auth/setOption on it. */
	if (!connected || EG(exception)) {
		if (!connected && !EG(exception)) {
			php_error_docref(NULL, E_WARNING,
				"Gene\\Cache\\Redis: connect() failed.");
		}
		zval_ptr_dtor(&obj_object);
		return 0;
	}

	/* Auth */
	password = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("password"));
	if (password && Z_TYPE_P(password) != IS_NULL && Z_TYPE_P(password) != IS_FALSE) {
		zval authret;
		ZVAL_UNDEF(&authret);
		gene_redis_auth(&obj_object, password, &authret);
		zval_ptr_dtor(&authret);
		if (EG(exception)) {
			/* Auth failed — don't store unauthenticated connection */
			zval_ptr_dtor(&obj_object);
			return 0;
		}
	}

	/* setOption */
	if (options && Z_TYPE_P(options) == IS_ARRAY) {
		zend_string *key;
		zval *element;
		zend_long id;
		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(options), id, key, element)
		{
			zval key_v;
			ZVAL_LONG(&key_v, id);
			gene_redis_setOption(&obj_object, &key_v, element);
			zval_ptr_dtor(&key_v);
		}ZEND_HASH_FOREACH_END();
	}

	zend_update_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), &obj_object);
	zval_ptr_dtor(&obj_object);
	return 1;
}

/*
 * Reconnect helper used by the error-recovery paths in get/set/__call.
 *
 * Pool mode: notifies the pool that the current connection is dead (no PING —
 * the caller already knows), clears it, then borrows a fresh one.
 * Non-pool mode: recreates a direct connection via initRObj (same as before).
 *
 * This preserves correct pool currentCount bookkeeping across reconnects:
 *   notify_remove decrements count for the dead slot,
 *   pool_get increments count for the fresh slot.
 */
static void gene_redis_reconnect(zval *self)
{
    zval *config = zend_read_property(gene_redis_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_CONFIG), 1, NULL);
    zval *pool   = zend_read_property(gene_redis_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_REF), 1, NULL);

    if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
        /* Decrement count for the dead connection (no PING needed) */
        gene_redis_pool_notify_remove(gene_redis_ce, self, ZEND_STRL(GENE_REDIS_POOL_REF));
        /* Clear the dead connection property */
        zend_update_property_null(gene_redis_ce, gene_strip_obj(self),
                                   ZEND_STRL(GENE_REDIS_OBJ));
        /* Borrow a fresh connection from the pool */
        gene_redis_pool_get(gene_redis_ce, self, config,
                             ZEND_STRL(GENE_REDIS_POOL_REF), ZEND_STRL(GENE_REDIS_OBJ));
    } else {
        /* Non-pool mode: recreate direct connection */
        initRObj(self, config);
    }
}

void redis_get(zval *object, zval *key, zval *ret) {
	if (Z_TYPE_P(key) == IS_ARRAY) {
		gene_redis_mGet(object, key, ret);
	} else {
		gene_redis_get(object, key, ret);
	}
}

void redis_set(zval *object, zval *key, zval *ttl, zval *value, zval *ret) {
	if (ttl && Z_LVAL_P(ttl) > 0) {
		gene_redis_setEx(object, key, ttl, value, ret);
	} else {
		gene_redis_set(object, key, value, ret);
	}
}

/*
 * {{{ gene_redis
 */
PHP_METHOD(gene_redis, __construct)
{
	zval *config = NULL, *self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS(),"z", &config) == FAILURE)
    {
        return;
    }

    if (Z_TYPE_P(config) == IS_ARRAY) {
        zend_update_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_CONFIG), config);
        /* Set Gene-level serializer before pool check so it is available
         * regardless of whether a pooled or direct connection is used. */
        {
            zval *serializer = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("serializer"));
            if (serializer && Z_TYPE_P(serializer) == IS_LONG) {
                zend_update_property_long(gene_redis_ce, gene_strip_obj(self),
                                           ZEND_STRL(GENE_REDIS_SERIALIZE), Z_LVAL_P(serializer));
            }
        }
        /* Pool mode: in Swoole coroutine mode, if config has a 'pool' key,
         * borrow a Redis connection from the named Gene\Cache\RedisPool instance. */
        if (!gene_redis_pool_get(gene_redis_ce, self, config,
                                  ZEND_STRL(GENE_REDIS_POOL_REF), ZEND_STRL(GENE_REDIS_OBJ))) {
            /* Not pool mode — create a direct connection */
            initRObj(self, config);
            if (EG(exception)) {
                if (checkError(EG(exception))) {
                    zend_clear_exception();
                    initRObj(self, config);
                }
            }
        }
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_redis::get()
 */
PHP_METHOD(gene_redis, get) {
	zval *self = getThis(), *object = NULL, *serializer_handler = NULL, *key = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &key) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	/* Must verify IS_OBJECT: zend_read_property never returns NULL but may
	 * return an IS_NULL zval (e.g. after release()).  Calling a method on a
	 * non-object zval would produce a misleading "undefined function" error. */
	if (object && Z_TYPE_P(object) == IS_OBJECT) {
		serializer_handler = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_SERIALIZE), 1, NULL);
		zval ret;
		ZVAL_UNDEF(&ret);
		redis_get(object, key, &ret);
		if (EG(exception)) {
			if (checkError(EG(exception))) {
				zend_clear_exception();
				/* Release any partial return value before retry to prevent
				 * a refcount leak should call_user_function have set retval
				 * before throwing. */
				if (!Z_ISUNDEF(ret)) { zval_ptr_dtor(&ret); ZVAL_UNDEF(&ret); }
				gene_redis_reconnect(self);
				object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
				if (object && Z_TYPE_P(object) == IS_OBJECT) {
					redis_get(object, key, &ret);
				}
			}
		}
		if (serializer_handler) {
			if (Z_TYPE_P(key) == IS_ARRAY) {
				/* Guard: mGet may return IS_UNDEF/IS_FALSE/IS_NULL on error.
				 * Accessing Z_ARRVAL() on a non-array zval is undefined
				 * behaviour and will crash with a NULL dereference. */
				if (Z_TYPE(ret) != IS_ARRAY) {
					RETURN_ZVAL(&ret, 1, 1);
				}
				zval *element, *value;
				zval arr;
				array_init(&arr);
				zend_long i = 0;
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(key), element)
				{
					value = zend_hash_index_find(Z_ARRVAL(ret), i);
					if (value && Z_TYPE_P(value) != IS_FALSE) {
						if (Z_TYPE_P(element) == IS_STRING) {
							zval tmp;
							ZVAL_UNDEF(&tmp);
							if (unserialize(value, &tmp, serializer_handler)) {
								add_assoc_zval_ex(&arr, Z_STRVAL_P(element), Z_STRLEN_P(element), &tmp);
							} else {
								Z_TRY_ADDREF_P(value);
								add_assoc_zval_ex(&arr, Z_STRVAL_P(element), Z_STRLEN_P(element), value);
							}
						}
					}
					i = i + 1;
				}ZEND_HASH_FOREACH_END();
				zval_ptr_dtor(&ret);
				RETURN_ZVAL(&arr, 1, 1);
			} else {
				zval arr;
				if (unserialize(&ret, &arr, serializer_handler)) {
					zval_ptr_dtor(&ret);
					RETURN_ZVAL(&arr, 1, 1);
				}
				RETURN_ZVAL(&ret, 1, 1);
			}
		}
		RETURN_ZVAL(&ret, 1, 1);
	}
	RETURN_NULL();
}

/*
 * {{{ public gene_redis::set()
 */
PHP_METHOD(gene_redis, set) {
	zval *self = getThis(),  *object = NULL, *serializer_handler = NULL, *key = NULL, *value = NULL, *ttl = NULL, *config = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz|z", &key, &value, &ttl) == FAILURE) {
		return;
	}

	if (ttl == NULL) {
		config =  zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_CONFIG), 1, NULL);
		ttl = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("ttl"));
	}
	object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	if (object && Z_TYPE_P(object) == IS_OBJECT) {
		if (Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) {
			serializer_handler = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_SERIALIZE), 1, NULL);
			if (serializer_handler && Z_LVAL_P(serializer_handler) > 0) {
				zval ret_string;
				if (serialize(value, &ret_string, serializer_handler) > 0) {
					redis_set(object, key, ttl, &ret_string, return_value);
					if (EG(exception)) {
						if (checkError(EG(exception))) {
							zend_clear_exception();
							gene_redis_reconnect(self);
							object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
							if (object && Z_TYPE_P(object) == IS_OBJECT) {
								redis_set(object, key, ttl, &ret_string, return_value);
							}
						}
					}
					zval_ptr_dtor(&ret_string);
					return;
				}
			}
		}

		redis_set(object, key, ttl, value, return_value);
		if (EG(exception)) {
			if (checkError(EG(exception))) {
				zend_clear_exception();
				gene_redis_reconnect(self);
				object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
				if (object && Z_TYPE_P(object) == IS_OBJECT) {
					redis_set(object, key, ttl, value, return_value);
				}
			}
		}
		return;
	}
	RETURN_NULL();
}

/*
 * {{{ public gene_redis::__call($codeString)
 */
PHP_METHOD(gene_redis, __call) {
	zval *self = getThis(), *object = NULL, *params = NULL, ret;
	zend_long methodlen;
	char *method;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "sz", &method, &methodlen, &params) == FAILURE) {
		RETURN_NULL();
	}
	object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	if (object && Z_TYPE_P(object) == IS_OBJECT) {
		ZVAL_UNDEF(&ret);
		gene_factory_call(object, method, methodlen, params, &ret);
		if (EG(exception)) {
			if (checkError(EG(exception))) {
				zend_clear_exception();
				/* Free any partial return value before retry */
				if (!Z_ISUNDEF(ret)) { zval_ptr_dtor(&ret); ZVAL_UNDEF(&ret); }
				gene_redis_reconnect(self);
				object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
				if (object && Z_TYPE_P(object) == IS_OBJECT) {
					gene_factory_call(object, method, methodlen, params, &ret);
				}
			}
		}
		RETURN_ZVAL(&ret, 1, 1);
	}
	RETURN_NULL();
}

static const char GENE_REDIS_RATE_LIMIT_LUA[] =
	"local n = redis.call('INCR', KEYS[1])\n"
	"if n == 1 then redis.call('EXPIRE', KEYS[1], ARGV[1]) end\n"
	"if n > tonumber(ARGV[2]) then return 0 end\n"
	"return 1";

static const char GENE_REDIS_UNLOCK_LUA[] =
	"if redis.call('GET', KEYS[1]) == ARGV[1] then return redis.call('DEL', KEYS[1]) else return 0 end";

static zend_string *gene_redis_random_token(void) {
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
	ZVAL_UNDEF(&hex);
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("bin2hex"));
	if (UNEXPECTED(!fn)) {
		zval_ptr_dtor(&raw);
		return NULL;
	}
	zend_call_known_function(fn, NULL, NULL, &hex, 1, &raw, NULL);
	zval_ptr_dtor(&raw);
	if (Z_TYPE(hex) != IS_STRING) {
		zval_ptr_dtor(&hex);
		return NULL;
	}
	return Z_STR(hex);
}

static zend_string *gene_redis_sha_rate = NULL;
static zend_string *gene_redis_sha_unlock = NULL;

static int gene_redis_ex_contains(zend_object *ex, const char *needle) {
	zval *msg, rv, zv;
	int hit;
	ZVAL_OBJ(&zv, ex);
	ZVAL_UNDEF(&rv);
	msg = zend_read_property(Z_OBJCE(zv), gene_strip_obj(&zv), ZEND_STRL("message"), 1, &rv);
	if (!msg || Z_TYPE_P(msg) != IS_STRING) {
		if (!Z_ISUNDEF(rv)) {
			zval_ptr_dtor(&rv);
		}
		return 0;
	}
	hit = strstr(Z_STRVAL_P(msg), needle) != NULL;
	if (!Z_ISUNDEF(rv)) {
		zval_ptr_dtor(&rv);
	}
	return hit;
}

static zend_string *gene_redis_persistent_sha1(const char *script) {
	zval in, out;
	zend_function *fn;
	zend_string *sha;
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("sha1"));
	if (!fn) {
		return NULL;
	}
	ZVAL_STRING(&in, script);
	ZVAL_UNDEF(&out);
	zend_call_known_function(fn, NULL, NULL, &out, 1, &in, NULL);
	zval_ptr_dtor(&in);
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		return NULL;
	}
	sha = zend_string_init(Z_STRVAL(out), Z_STRLEN(out), 1);
	zval_ptr_dtor(&out);
	return sha;
}

static void gene_redis_eval_call(zval *object, const char *method, size_t method_len,
		zend_string *payload, zval *args, zend_long nkeys, zval *retval) {
	zval params;
	array_init(&params);
	add_next_index_str(&params, zend_string_copy(payload));
	Z_TRY_ADDREF_P(args);
	add_next_index_zval(&params, args);
	add_next_index_long(&params, nkeys);
	gene_factory_call(object, (char *)method, method_len, &params, retval);
	zval_ptr_dtor(&params);
}

static void gene_redis_eval(zval *self, const char *script, zend_string **sha_slot,
		zval *args, zend_long nkeys, zval *retval) {
	zval *object;
	ZVAL_UNDEF(retval);
	object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	if (!object || Z_TYPE_P(object) != IS_OBJECT) {
		ZVAL_FALSE(retval);
		return;
	}
	if (!*sha_slot) {
		*sha_slot = gene_redis_persistent_sha1(script);
	}
	if (*sha_slot) {
		gene_redis_eval_call(object, "evalsha", sizeof("evalsha") - 1, *sha_slot, args, nkeys, retval);
		if (EG(exception) && gene_redis_ex_contains(EG(exception), "NOSCRIPT")) {
			zend_clear_exception();
			if (!Z_ISUNDEF_P(retval)) {
				zval_ptr_dtor(retval);
				ZVAL_UNDEF(retval);
			}
			{
				zend_string *src = zend_string_init(script, strlen(script), 0);
				gene_redis_eval_call(object, "eval", sizeof("eval") - 1, src, args, nkeys, retval);
				zend_string_release(src);
			}
		}
	} else {
		zend_string *src = zend_string_init(script, strlen(script), 0);
		gene_redis_eval_call(object, "eval", sizeof("eval") - 1, src, args, nkeys, retval);
		zend_string_release(src);
	}
	if (EG(exception) && checkError(EG(exception))) {
		zend_clear_exception();
		if (!Z_ISUNDEF_P(retval)) {
			zval_ptr_dtor(retval);
			ZVAL_UNDEF(retval);
		}
		gene_redis_reconnect(self);
		object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
		if (object && Z_TYPE_P(object) == IS_OBJECT) {
			if (*sha_slot) {
				gene_redis_eval_call(object, "evalsha", sizeof("evalsha") - 1, *sha_slot, args, nkeys, retval);
				if (EG(exception) && gene_redis_ex_contains(EG(exception), "NOSCRIPT")) {
					zend_clear_exception();
					if (!Z_ISUNDEF_P(retval)) {
						zval_ptr_dtor(retval);
						ZVAL_UNDEF(retval);
					}
					{
						zend_string *src = zend_string_init(script, strlen(script), 0);
						gene_redis_eval_call(object, "eval", sizeof("eval") - 1, src, args, nkeys, retval);
						zend_string_release(src);
					}
				}
			} else {
				zend_string *src = zend_string_init(script, strlen(script), 0);
				gene_redis_eval_call(object, "eval", sizeof("eval") - 1, src, args, nkeys, retval);
				zend_string_release(src);
			}
		}
	}
}

PHP_METHOD(gene_redis, rateLimit) {
	zend_string *key;
	zend_long max, window;
	zval args, ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sll", &key, &max, &window) == FAILURE) {
		return;
	}
	if (max < 1 || window < 1) {
		RETURN_FALSE;
	}
	array_init(&args);
	add_next_index_str(&args, zend_string_copy(key));
	add_next_index_long(&args, window);
	add_next_index_long(&args, max);
	gene_redis_eval(getThis(), GENE_REDIS_RATE_LIMIT_LUA, &gene_redis_sha_rate, &args, 1, &ret);
	zval_ptr_dtor(&args);
	if (Z_TYPE(ret) == IS_LONG) {
		RETVAL_BOOL(Z_LVAL(ret) == 1);
		zval_ptr_dtor(&ret);
		return;
	}
	if (Z_TYPE(ret) == IS_TRUE) {
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret);
	RETURN_FALSE;
}

PHP_METHOD(gene_redis, lock) {
	zend_string *key, *token;
	zend_long ttl;
	zval *self = getThis(), *object;
	zval params, opts, ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sl", &key, &ttl) == FAILURE) {
		return;
	}
	if (ttl < 1) {
		RETURN_FALSE;
	}
	token = gene_redis_random_token();
	if (!token) {
		RETURN_FALSE;
	}
	array_init(&opts);
	add_next_index_string(&opts, "nx");
	add_assoc_long_ex(&opts, ZEND_STRL("ex"), ttl);
	array_init(&params);
	add_next_index_str(&params, zend_string_copy(key));
	add_next_index_str(&params, zend_string_copy(token));
	add_next_index_zval(&params, &opts);
	object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
	ZVAL_UNDEF(&ret);
	if (object && Z_TYPE_P(object) == IS_OBJECT) {
		gene_factory_call(object, "set", sizeof("set") - 1, &params, &ret);
		if (EG(exception) && checkError(EG(exception))) {
			zend_clear_exception();
			if (!Z_ISUNDEF(ret)) {
				zval_ptr_dtor(&ret);
				ZVAL_UNDEF(&ret);
			}
			gene_redis_reconnect(self);
			object = zend_read_property(gene_redis_ce, gene_strip_obj(self), ZEND_STRL(GENE_REDIS_OBJ), 1, NULL);
			if (object && Z_TYPE_P(object) == IS_OBJECT) {
				gene_factory_call(object, "set", sizeof("set") - 1, &params, &ret);
			}
		}
	}
	zval_ptr_dtor(&params);
	if (Z_TYPE(ret) == IS_TRUE) {
		zval_ptr_dtor(&ret);
		RETURN_STR(token);
	}
	zval_ptr_dtor(&ret);
	zend_string_release(token);
	RETURN_FALSE;
}

PHP_METHOD(gene_redis, unlock) {
	zend_string *key, *token;
	zval args, ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &key, &token) == FAILURE) {
		return;
	}
	array_init(&args);
	add_next_index_str(&args, zend_string_copy(key));
	add_next_index_str(&args, zend_string_copy(token));
	gene_redis_eval(getThis(), GENE_REDIS_UNLOCK_LUA, &gene_redis_sha_unlock, &args, 1, &ret);
	zval_ptr_dtor(&args);
	if (Z_TYPE(ret) == IS_LONG) {
		RETVAL_BOOL(Z_LVAL(ret) > 0);
		zval_ptr_dtor(&ret);
		return;
	}
	if (Z_TYPE(ret) == IS_TRUE) {
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret);
	RETURN_FALSE;
}

/*
 * {{{ public gene_redis::release()
 *
 * Explicitly return the pool-borrowed Redis connection back to the pool.
 * Designed for long-lived objects or explicit resource management patterns.
 */
PHP_METHOD(gene_redis, release)
{
    zval *self = getThis();
    gene_redis_pool_return(gene_redis_ce, self,
                            ZEND_STRL(GENE_REDIS_POOL_REF), ZEND_STRL(GENE_REDIS_OBJ));
    RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_redis::free()
 *
 * Pool mode:     return the connection to the pool.
 * Non-pool mode: null out the internal Redis object (close the connection).
 */
PHP_METHOD(gene_redis, free)
{
    zval *self = getThis();
    zval *pool = zend_read_property(gene_redis_ce, gene_strip_obj(self),
                                     ZEND_STRL(GENE_REDIS_POOL_REF), 1, NULL);
    if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
        gene_redis_pool_return(gene_redis_ce, self,
                                ZEND_STRL(GENE_REDIS_POOL_REF), ZEND_STRL(GENE_REDIS_OBJ));
    } else {
        zend_update_property_null(gene_redis_ce, gene_strip_obj(self),
                                   ZEND_STRL(GENE_REDIS_OBJ));
    }
    RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_redis::__destruct()
 *
 * In pool mode, automatically return the borrowed connection to the pool when
 * the Gene\Cache\Redis object is destroyed (end of coroutine scope, etc.).
 * Double-return is safe: gene_redis_pool_return checks obj_key == null.
 */
PHP_METHOD(gene_redis, __destruct)
{
    zval *self = getThis();
    zval *pool = zend_read_property(gene_redis_ce, gene_strip_obj(self),
                                     ZEND_STRL(GENE_REDIS_POOL_REF), 1, NULL);
    if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
        gene_redis_pool_return(gene_redis_ce, self,
                                ZEND_STRL(GENE_REDIS_POOL_REF), ZEND_STRL(GENE_REDIS_OBJ));
    }
}
/* }}} */

/*
 * {{{ gene_redis_methods
 */
const zend_function_entry gene_redis_methods[] = {
		PHP_ME(gene_redis, set,         gene_redis_set_arginfo,  ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, get,         gene_redis_get_arginfo,  ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, rateLimit,   gene_redis_rate_limit_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, lock,        gene_redis_lock_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, unlock,      gene_redis_unlock_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, __call,      gene_redis_call_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, __construct, gene_redis_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, release,     gene_redis_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, free,        gene_redis_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_redis, __destruct,  gene_redis_void_arginfo, ZEND_ACC_PUBLIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(redis)
{
    zend_class_entry gene_redis;
    GENE_INIT_CLASS_ENTRY(gene_redis, "Gene_Cache_Redis", "Gene\\Cache\\Redis", gene_redis_methods);
    gene_redis_ce = zend_register_internal_class(&gene_redis);
#if PHP_VERSION_ID >= 80200
    gene_redis_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

    zend_declare_property_null(gene_redis_ce, ZEND_STRL(GENE_REDIS_CONFIG),   ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_redis_ce, ZEND_STRL(GENE_REDIS_OBJ),      ZEND_ACC_PUBLIC);
    zend_declare_property_long(gene_redis_ce, ZEND_STRL(GENE_REDIS_SERIALIZE), 1, ZEND_ACC_PUBLIC);
    /* Pool reference — set when a Gene\Cache\RedisPool connection is borrowed */
    zend_declare_property_null(gene_redis_ce, ZEND_STRL(GENE_REDIS_POOL_REF), ZEND_ACC_PUBLIC);
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
