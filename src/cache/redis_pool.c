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
#include "ext/standard/php_rand.h"

#include "../gene.h"
#include "../factory/factory.h"
#include "../config/configs.h"
#include "../cache/memory.h"
#include "../cache/redis_pool.h"

zend_class_entry *gene_redis_pool_ce;

/* Forward declaration */
static void rpool_stop_timer(zval *self);

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(gene_redis_pool_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_pool_construct_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, config)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_pool_create_arginfo, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, configKey)
    ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_pool_get_instance_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_redis_pool_put_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, redis)
ZEND_END_ARG_INFO()
/* }}} */

/* ====================== Internal helper functions ====================== */

/*
 * Create a new phpredis Redis object and connect using the pool config.
 * Always uses connect() — pconnect() is incompatible with Swoole coroutines
 * because persistent connections share a single TCP socket across coroutines.
 * On success *retval holds the Redis object (refcount=1).
 * On failure *retval is set to IS_NULL.
 */
static void rpool_create_connection(zval *self, zval *retval)
{
    zval *config = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CONFIG), 1, NULL);
    ZVAL_NULL(retval);

    if (!config || Z_TYPE_P(config) != IS_ARRAY) {
        return;
    }

    /* Locate the phpredis extension class */
    static zend_string *redis_cls_str = NULL;
    if (UNEXPECTED(!redis_cls_str)) {
        redis_cls_str = zend_string_init_interned(ZEND_STRL("Redis"), 1);
    }
    zend_class_entry *redis_cls = zend_lookup_class(redis_cls_str);

    if (!redis_cls) {
        php_error_docref(NULL, E_WARNING,
            "Gene\\Cache\\RedisPool: Redis extension class not found.");
        return;
    }

    /* Resolve host/port — direct config first, then servers[] (random pick) */
    zval *host    = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("host"));
    zval *port    = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("port"));
    zval *timeout = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("timeout"));

    if (!(host && port)) {
        zval *servers = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("servers"));
        if (servers && Z_TYPE_P(servers) == IS_ARRAY) {
            uint32_t num = zend_hash_num_elements(Z_ARRVAL_P(servers));
            if (num > 0) {
                uint32_t idx = php_mt_rand_range(0, num - 1);
                zval *one = zend_hash_index_find(Z_ARRVAL_P(servers), idx);
                if (one && Z_TYPE_P(one) == IS_ARRAY) {
                    host = zend_hash_str_find(Z_ARRVAL_P(one), ZEND_STRL("host"));
                    port = zend_hash_str_find(Z_ARRVAL_P(one), ZEND_STRL("port"));
                }
            }
        }
    }

    if (!host || !port || !timeout) {
        php_error_docref(NULL, E_WARNING,
            "Gene\\Cache\\RedisPool: config missing host/port/timeout.");
        return;
    }

    /* Instantiate Redis */
    zval redis_obj;
    object_init_ex(&redis_obj, redis_cls);

    /* connect() — never pconnect() in pool / Swoole mode */
    {
        zval rv;
        zval params[3];
        zend_function *connect_fn = zend_hash_str_find_ptr(&Z_OBJCE(redis_obj)->function_table, ZEND_STRL("connect"));
        ZVAL_COPY(&params[0], host);
        ZVAL_COPY(&params[1], port);
        ZVAL_COPY(&params[2], timeout);
        ZVAL_UNDEF(&rv);
        if (EXPECTED(connect_fn)) {
            zend_call_known_function(connect_fn, Z_OBJ(redis_obj), Z_OBJCE(redis_obj), &rv, 3, params, NULL);
        }
        zval_ptr_dtor(&params[0]);
        zval_ptr_dtor(&params[1]);
        zval_ptr_dtor(&params[2]);
        bool connected = (Z_TYPE(rv) == IS_TRUE);
        if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);

        if (EG(exception)) {
            zend_clear_exception();
            zval_ptr_dtor(&redis_obj);
            return;
        }
        if (!connected) {
            zval_ptr_dtor(&redis_obj);
            return;
        }
    }

    /* Auth — supports string password and array [user, pass] (Redis 6 ACL) */
    zval *password = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("password"));
    if (password && Z_TYPE_P(password) != IS_NULL && Z_TYPE_P(password) != IS_FALSE) {
        zval rv;
        zval param;
        zend_function *auth_fn = zend_hash_str_find_ptr(&Z_OBJCE(redis_obj)->function_table, ZEND_STRL("auth"));
        ZVAL_COPY(&param, password);
        ZVAL_UNDEF(&rv);
        if (EXPECTED(auth_fn)) {
            zend_call_known_function(auth_fn, Z_OBJ(redis_obj), Z_OBJCE(redis_obj), &rv, 1, &param, NULL);
        }
        zval_ptr_dtor(&param);
        if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);

        if (EG(exception)) {
            zend_clear_exception();
            zval_ptr_dtor(&redis_obj);
            return;
        }
    }

    /* Set options: numeric-keyed array — Redis::OPT_* => value */
    zval *options = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("options"));
    if (options && Z_TYPE_P(options) == IS_ARRAY) {
        zend_long    opt_id;
        zend_string *opt_key;
        zval        *opt_val;
        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(options), opt_id, opt_key, opt_val) {
            if (!opt_key) { /* numeric key == option constant */
                zval rv;
                zval params[2];
                zend_function *setopt_fn = zend_hash_str_find_ptr(&Z_OBJCE(redis_obj)->function_table, ZEND_STRL("setoption"));
                ZVAL_LONG(&params[0], opt_id);
                ZVAL_COPY(&params[1], opt_val);
                ZVAL_UNDEF(&rv);
                if (EXPECTED(setopt_fn)) {
                    zend_call_known_function(setopt_fn, Z_OBJ(redis_obj), Z_OBJCE(redis_obj), &rv, 2, params, NULL);
                }
                /* params[0] is IS_LONG — no dtor needed */
                zval_ptr_dtor(&params[1]);
                if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
            }
        } ZEND_HASH_FOREACH_END();
    }

    /* Transfer ownership to caller (refcount stays at 1) */
    ZVAL_COPY_VALUE(retval, &redis_obj);
}

/*
 * Ping the connection. Returns true if alive.
 * phpredis >= 4 returns bool true; older versions return the string "+PONG".
 * Any exception means the connection is dead.
 */
static bool rpool_is_alive(zval *redis_obj)
{
    zval rv;
    zend_function *ping_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(redis_obj)->function_table, ZEND_STRL("ping"));
    ZVAL_UNDEF(&rv);
    if (EXPECTED(ping_fn)) {
        zend_call_known_function(ping_fn, Z_OBJ_P(redis_obj), Z_OBJCE_P(redis_obj), &rv, 0, NULL, NULL);
    }

    if (EG(exception)) {
        zend_clear_exception();
        if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
        return 0;
    }

    bool alive = 0;
    switch (Z_TYPE(rv)) {
        case IS_TRUE:
            alive = 1;
            break;
        case IS_STRING:
            alive = (strcasecmp(Z_STRVAL(rv), "PONG") == 0 ||
                     strcmp(Z_STRVAL(rv), "+PONG") == 0);
            break;
        default:
            alive = 0;
            break;
    }
    if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
    return alive;
}

/* ---- Swoole\Coroutine\Channel wrappers ---- */

static bool rpool_channel_push(zval *channel, zval *redis_obj)
{
    zval rv, item;
    array_init(&item);
    Z_TRY_ADDREF_P(redis_obj);
    add_assoc_zval(&item, "conn", redis_obj);
    add_assoc_long(&item, "lastUsed", (zend_long)time(NULL));

    zend_function *push_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(channel)->function_table, ZEND_STRL("push"));
    zval param;
    ZVAL_COPY_VALUE(&param, &item);
    ZVAL_UNDEF(&rv);
    if (EXPECTED(push_fn)) {
        zend_call_known_function(push_fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &rv, 1, &param, NULL);
    }
    bool ok = (Z_TYPE(rv) == IS_TRUE);
    if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
    zval_ptr_dtor(&item);
    return ok;
}

static bool rpool_channel_pop(zval *channel, double timeout, zval *result)
{
    zval rv, param;
    zend_function *pop_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(channel)->function_table, ZEND_STRL("pop"));
    ZVAL_DOUBLE(&param, timeout);
    ZVAL_UNDEF(&rv);
    if (EXPECTED(pop_fn)) {
        zend_call_known_function(pop_fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &rv, 1, &param, NULL);
    }
    /* param is IS_DOUBLE — no dtor needed */

    if (Z_ISUNDEF(rv) || Z_TYPE(rv) == IS_FALSE || Z_TYPE(rv) == IS_NULL) {
        if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
        ZVAL_NULL(result);
        return 0;
    }
    ZVAL_COPY_VALUE(result, &rv);
    return 1;
}

static bool rpool_channel_is_empty(zval *channel)
{
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(channel)->function_table, ZEND_STRL("isempty"));
    if (!fn) return 1;
    zval rv;
    ZVAL_UNDEF(&rv);
    zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &rv, 0, NULL, NULL);
    bool empty = (Z_TYPE(rv) == IS_TRUE);
    if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
    return empty;
}

static bool rpool_channel_is_full(zval *channel)
{
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(channel)->function_table, ZEND_STRL("isfull"));
    if (!fn) return 1;
    zval rv;
    ZVAL_UNDEF(&rv);
    zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &rv, 0, NULL, NULL);
    bool full = (Z_TYPE(rv) == IS_TRUE);
    if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
    return full;
}

static zend_long rpool_channel_length(zval *channel)
{
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(channel)->function_table, ZEND_STRL("length"));
    if (!fn) return 0;
    zval rv;
    ZVAL_UNDEF(&rv);
    zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &rv, 0, NULL, NULL);
    zend_long len = (Z_TYPE(rv) == IS_LONG) ? Z_LVAL(rv) : 0;
    if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
    return len;
}

/* ---- Swoole\Atomic wrappers ---- */

static void rpool_atomic_call(zval *atomic, const char *method, zend_long arg, zval *retval)
{
    zval param, ret_local;
    if (!retval) retval = &ret_local;
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(atomic)->function_table, method, strlen(method));
    ZVAL_LONG(&param, arg);
    ZVAL_UNDEF(retval);
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(atomic), Z_OBJCE_P(atomic), retval, 1, &param, NULL);
    }
    /* param is IS_LONG — no dtor needed */
    if (retval == &ret_local) {
        if (!Z_ISUNDEF(ret_local)) zval_ptr_dtor(&ret_local);
    }
}

static void rpool_decrement_count(zval *self)
{
    zval *atomic = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT), 1, NULL);
    if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
        zval ret;
        ZVAL_UNDEF(&ret);
        rpool_atomic_call(atomic, "get", 0, &ret);
        zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
        if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
        if (val > 0) {
            rpool_atomic_call(atomic, "sub", 1, NULL);
        }
    }
}

static zend_long rpool_get_count(zval *self)
{
    zval *atomic = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT), 1, NULL);
    if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
        zval ret;
        ZVAL_UNDEF(&ret);
        rpool_atomic_call(atomic, "get", 0, &ret);
        zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
        if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
        return val;
    }
    return 0;
}

static zend_long rpool_get_max(zval *self)
{
    zval *v = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_MAX), 1, NULL);
    return (v && Z_TYPE_P(v) == IS_LONG) ? Z_LVAL_P(v) : 10;
}

static zend_long rpool_get_min(zval *self)
{
    zval *v = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_MIN), 1, NULL);
    return (v && Z_TYPE_P(v) == IS_LONG) ? Z_LVAL_P(v) : 1;
}

static bool rpool_is_closed(zval *self)
{
    zval *v = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CLOSED), 1, NULL);
    return (v && Z_TYPE_P(v) == IS_TRUE);
}

static double rpool_get_wait_timeout(zval *self)
{
    zval *v = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_WAIT_TIME), 1, NULL);
    if (v && Z_TYPE_P(v) == IS_DOUBLE) return Z_DVAL_P(v);
    if (v && Z_TYPE_P(v) == IS_LONG)   return (double)Z_LVAL_P(v);
    return 3.0;
}

static void rpool_increment_count(zval *self)
{
    zval *atomic = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT), 1, NULL);
    if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
        rpool_atomic_call(atomic, "add", 1, NULL);
    }
}

/*
 * Atomic increment that returns the new value.
 * Swoole\Atomic::add(1) is atomic and returns the post-increment value,
 * enabling a TOCTOU-safe "reserve-then-check" pattern in get().
 */
static zend_long rpool_increment_count_get(zval *self)
{
    zval *atomic = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT), 1, NULL);
    if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
        zval ret;
        ZVAL_UNDEF(&ret);
        rpool_atomic_call(atomic, "add", 1, &ret);
        zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
        if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
        return val;
    }
    return 0;
}

static void rpool_set_count(zval *self, zend_long val)
{
    zval *atomic = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT), 1, NULL);
    if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
        rpool_atomic_call(atomic, "set", val, NULL);
    }
}

/* Pre-fill the channel with min connections */
static void rpool_fill(zval *self)
{
    zval *channel = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL), 1, NULL);
    zend_long min = rpool_get_min(self);
    zend_long i;

    if (!channel || Z_TYPE_P(channel) != IS_OBJECT) return;

    for (i = 0; i < min; i++) {
        zval conn;
        rpool_create_connection(self, &conn);
        if (Z_TYPE(conn) == IS_OBJECT) {
            /* [GENE_FIX:2026-04-09] Only increment count AFTER successful push.
             * If push fails (channel full), we must NOT increment count, otherwise
             * the count will drift from the actual number of connections in the pool. */
            if (rpool_channel_push(channel, &conn)) {
                rpool_increment_count(self);
            }
            zval_ptr_dtor(&conn);
        } else {
            break; /* server unavailable — pool fills lazily on first get() */
        }
    }
}

/*
 * Idle recycler — runs inside a Swoole timer coroutine.
 * Discards connections idle longer than idleTimeout (above min),
 * verifies liveness of kept connections via PING, refills to min.
 */
static void rpool_recycle_idle(zval *self)
{
    if (rpool_is_closed(self)) {
        rpool_stop_timer(self);
        return;
    }

    zval *channel = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL), 1, NULL);
    zval *idle_zv  = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                         ZEND_STRL(GENE_REDIS_POOL_PROPERTY_IDLE_TIME), 1, NULL);
    zend_long idle_timeout = (idle_zv && Z_TYPE_P(idle_zv) == IS_LONG)
                              ? Z_LVAL_P(idle_zv) : 60;

    if (!channel || Z_TYPE_P(channel) != IS_OBJECT) return;

    zend_long now  = (zend_long)time(NULL);
    zend_long size = rpool_channel_length(channel);
    zend_long i;

    /* Pop-check-push pattern avoids starving concurrent get() callers */
    for (i = 0; i < size; i++) {
        zval item;
        ZVAL_UNDEF(&item);
        if (!rpool_channel_pop(channel, 0.001, &item)) break;

        if (Z_TYPE(item) != IS_ARRAY) {
            zval_ptr_dtor(&item);
            continue;
        }

        zval *last_used_zv = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("lastUsed"));
        zend_long last_used = (last_used_zv && Z_TYPE_P(last_used_zv) == IS_LONG)
                               ? Z_LVAL_P(last_used_zv) : now;

        zval *conn_zv = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("conn"));

        if (rpool_get_count(self) > rpool_get_min(self) &&
            (now - last_used) > idle_timeout) {
            /* Idle beyond threshold — discard */
            rpool_decrement_count(self);
            zval_ptr_dtor(&item);
            continue;
        }

        /* Keep — verify liveness then push back */
        if (conn_zv && Z_TYPE_P(conn_zv) == IS_OBJECT) {
            if (rpool_is_alive(conn_zv)) {
                rpool_channel_push(channel, conn_zv);
            } else {
                rpool_decrement_count(self);
            }
        }
        zval_ptr_dtor(&item);
    }

    /* Refill to minimum */
    while (rpool_get_count(self) < rpool_get_min(self) &&
           !rpool_channel_is_full(channel)) {
        zval conn;
        rpool_create_connection(self, &conn);
        if (Z_TYPE(conn) == IS_OBJECT) {
            rpool_increment_count(self);
            rpool_channel_push(channel, &conn);
            zval_ptr_dtor(&conn);
        } else {
            break;
        }
    }
}

/* Register Swoole\Timer::tick to call recycleIdle periodically */
static void rpool_start_idle_recycler(zval *self)
{
    zval *idle_zv = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_IDLE_TIME), 1, NULL);
    zend_long idle_timeout = (idle_zv && Z_TYPE_P(idle_zv) == IS_LONG)
                              ? Z_LVAL_P(idle_zv) : 60;
    if (idle_timeout <= 0) return;

    zend_long interval_ms = idle_timeout * 500; /* fire at half the idle timeout */
    if (interval_ms < 1000) interval_ms = 1000;

    /* Swoole\Timer::tick($interval_ms, [$self, 'recycleIdle']) */
    zval callable, callback, timer_ret;
    array_init(&callable);
    add_next_index_string(&callable, "Swoole\\Timer");
    add_next_index_string(&callable, "tick");

    array_init(&callback);
    Z_TRY_ADDREF_P(self);
    add_next_index_zval(&callback, self);
    add_next_index_string(&callback, "recycleIdle");

    zval params[2];
    ZVAL_LONG(&params[0], interval_ms);
    ZVAL_COPY(&params[1], &callback);

    ZVAL_UNDEF(&timer_ret);
    call_user_function(NULL, NULL, &callable, &timer_ret, 2, params);

    zval_ptr_dtor(&params[1]);
    zval_ptr_dtor(&callable);
    zval_ptr_dtor(&callback);

    if (Z_TYPE(timer_ret) == IS_LONG) {
        zend_update_property_long(gene_redis_pool_ce, gene_strip_obj(self),
                                   ZEND_STRL(GENE_REDIS_POOL_PROPERTY_TIMER_ID),
                                   Z_LVAL(timer_ret));
    }
    if (!Z_ISUNDEF(timer_ret)) zval_ptr_dtor(&timer_ret);
}

static bool rpool_in_coroutine(void)
{
    return gene_get_coroutine_id() >= 0;
}

static void rpool_stop_timer(zval *self)
{
    zval *timer_id = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                         ZEND_STRL(GENE_REDIS_POOL_PROPERTY_TIMER_ID), 1, NULL);
    if (timer_id && Z_TYPE_P(timer_id) == IS_LONG && Z_LVAL_P(timer_id) > 0) {
        zval callable, rv, param;
        array_init(&callable);
        add_next_index_string(&callable, "Swoole\\Timer");
        add_next_index_string(&callable, "clear");

        ZVAL_LONG(&param, Z_LVAL_P(timer_id));
        ZVAL_UNDEF(&rv);
        call_user_function(NULL, NULL, &callable, &rv, 1, &param);
        zval_ptr_dtor(&callable);
        if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);

        zend_update_property_long(gene_redis_pool_ce, gene_strip_obj(self),
                                   ZEND_STRL(GENE_REDIS_POOL_PROPERTY_TIMER_ID), 0);
    }
}

/* ====================== PHP Methods ====================== */

/*
 * {{{ public Gene\Cache\RedisPool::__construct(array $config)
 *
 * Required keys: host (or servers[]), port, timeout
 * Optional:      password, options[], min, max, idleTimeout, waitTimeout
 */
PHP_METHOD(gene_redis_pool, __construct)
{
    zval *config = NULL, *self = getThis();
    zend_long min_val, max_val, idle_val;
    double    wait_val;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &config) == FAILURE) {
        return;
    }

    zval *min_zv  = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("min"));
    zval *max_zv  = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("max"));
    zval *idle_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("idleTimeout"));
    zval *wait_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("waitTimeout"));

    min_val  = (min_zv  && Z_TYPE_P(min_zv)  == IS_LONG) ? Z_LVAL_P(min_zv)  : 1;
    max_val  = (max_zv  && Z_TYPE_P(max_zv)  == IS_LONG) ? Z_LVAL_P(max_zv)  : 64;
    idle_val = (idle_zv && Z_TYPE_P(idle_zv) == IS_LONG) ? Z_LVAL_P(idle_zv) : 60;

    if (wait_zv) {
        if      (Z_TYPE_P(wait_zv) == IS_DOUBLE) wait_val = Z_DVAL_P(wait_zv);
        else if (Z_TYPE_P(wait_zv) == IS_LONG)   wait_val = (double)Z_LVAL_P(wait_zv);
        else                                      wait_val = 3.0;
    } else {
        wait_val = 3.0;
    }

    if (min_val < 0)       min_val = 0;
    if (max_val < 1)       max_val = 1;
    if (min_val > max_val) min_val = max_val;
    if (idle_val < 0)      idle_val = 0;

    zend_update_property(gene_redis_pool_ce, gene_strip_obj(self),
                          ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CONFIG), config);
    zend_update_property_long(gene_redis_pool_ce, gene_strip_obj(self),
                               ZEND_STRL(GENE_REDIS_POOL_PROPERTY_MIN), min_val);
    zend_update_property_long(gene_redis_pool_ce, gene_strip_obj(self),
                               ZEND_STRL(GENE_REDIS_POOL_PROPERTY_MAX), max_val);
    zend_update_property_long(gene_redis_pool_ce, gene_strip_obj(self),
                               ZEND_STRL(GENE_REDIS_POOL_PROPERTY_IDLE_TIME), idle_val);
    zend_update_property_double(gene_redis_pool_ce, gene_strip_obj(self),
                                 ZEND_STRL(GENE_REDIS_POOL_PROPERTY_WAIT_TIME), wait_val);

    /* Swoole\Atomic for thread-safe connection counting */
    {
        static zend_string *atomic_str = NULL;
        if (UNEXPECTED(!atomic_str)) {
            atomic_str = zend_string_init_interned(ZEND_STRL("Swoole\\Atomic"), 1);
        }
        zend_class_entry *atomic_ce = zend_lookup_class(atomic_str);

        if (atomic_ce) {
            zval atomic_obj, atomic_ret, atomic_param;
            object_init_ex(&atomic_obj, atomic_ce);
            ZVAL_LONG(&atomic_param, 0);
            ZVAL_UNDEF(&atomic_ret);
            if (EXPECTED(atomic_ce->constructor)) {
                zend_call_known_function(atomic_ce->constructor, Z_OBJ(atomic_obj), atomic_ce, &atomic_ret, 1, &atomic_param, NULL);
            }
            if (!Z_ISUNDEF(atomic_ret)) zval_ptr_dtor(&atomic_ret);
            zend_update_property(gene_redis_pool_ce, gene_strip_obj(self),
                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT), &atomic_obj);
            zval_ptr_dtor(&atomic_obj);
        } else {
            php_error_docref(NULL, E_WARNING,
                "Gene\\Cache\\RedisPool: Swoole\\Atomic not found. "
                "Connection counting may be unsafe under concurrency.");
            zend_update_property_long(gene_redis_pool_ce, gene_strip_obj(self),
                                       ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT), 0);
        }
    }

    zend_update_property_long(gene_redis_pool_ce, gene_strip_obj(self),
                               ZEND_STRL(GENE_REDIS_POOL_PROPERTY_TIMER_ID), 0);
    zend_update_property_bool(gene_redis_pool_ce, gene_strip_obj(self),
                               ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CLOSED), 0);

    /* Swoole\Coroutine\Channel as the idle connection queue */
    {
        static zend_string *ch_str = NULL;
        if (UNEXPECTED(!ch_str)) {
            ch_str = zend_string_init_interned(ZEND_STRL("Swoole\\Coroutine\\Channel"), 1);
        }
        zend_class_entry *ch_ce = zend_lookup_class(ch_str);

        if (!ch_ce) {
            php_error_docref(NULL, E_WARNING,
                "Gene\\Cache\\RedisPool requires Swoole "
                "(Swoole\\Coroutine\\Channel not found).");
            return;
        }

        zval channel, ch_ret, ch_param;
        object_init_ex(&channel, ch_ce);
        ZVAL_LONG(&ch_param, max_val);
        ZVAL_UNDEF(&ch_ret);
        if (EXPECTED(ch_ce->constructor)) {
            zend_call_known_function(ch_ce->constructor, Z_OBJ(channel), ch_ce, &ch_ret, 1, &ch_param, NULL);
        }
        if (!Z_ISUNDEF(ch_ret)) zval_ptr_dtor(&ch_ret);

        zend_update_property(gene_redis_pool_ce, gene_strip_obj(self),
                              ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL), &channel);
        zval_ptr_dtor(&channel);
    }

    /* Pre-fill pool with min connections */
    rpool_fill(self);

    /* Start idle recycler timer */
    if (idle_val > 0) {
        rpool_start_idle_recycler(self);
    }
}
/* }}} */

/*
 * {{{ public static Gene\Cache\RedisPool::create(string $name, string $configKey, array $options=[]): self
 *
 * Reads Redis connection params from the Gene persistent config cache under
 * $configKey (Gene\Di format: ['class'=>..., 'params'=>[[host, port, ...]]]).
 * $options overrides pool defaults (min, max, idleTimeout, waitTimeout).
 * Registers the pool in the static instances registry under $name.
 */
PHP_METHOD(gene_redis_pool, create)
{
    zend_string *name;
    char        *config_key;
    size_t       config_key_len;
    zval        *options   = NULL;
    zval        *instances = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Ss|a",
                               &name, &config_key, &config_key_len, &options) == FAILURE) {
        return;
    }

    /* Build the persistent config cache key */
    char  *cache_key     = NULL;
    size_t cache_key_len = 0;
    char cache_key_buf[256];
    int cache_key_heap = 0;

    if (GENE_G(app_key) && strlen(GENE_G(app_key)) > 0) {
        cache_key_len = strlen(GENE_G(app_key)) + strlen(GENE_CONFIG_CACHE);
        if (cache_key_len >= sizeof(cache_key_buf)) {
            cache_key = emalloc(cache_key_len + 1);
            cache_key_heap = 1;
        } else {
            cache_key = cache_key_buf;
        }
        snprintf(cache_key, cache_key_len + 1, "%s%s", GENE_G(app_key), GENE_CONFIG_CACHE);
    } else if (GENE_G(app_root) && strlen(GENE_G(app_root)) > 0) {
        cache_key_len = strlen(GENE_G(app_root)) + strlen(GENE_CONFIG_CACHE);
        if (cache_key_len >= sizeof(cache_key_buf)) {
            cache_key = emalloc(cache_key_len + 1);
            cache_key_heap = 1;
        } else {
            cache_key = cache_key_buf;
        }
        snprintf(cache_key, cache_key_len + 1, "%s%s", GENE_G(app_root), GENE_CONFIG_CACHE);
    } else {
        cache_key_len = strlen(GENE_CONFIG_CACHE);
        if (cache_key_len >= sizeof(cache_key_buf)) {
            cache_key = emalloc(cache_key_len + 1);
            cache_key_heap = 1;
        } else {
            cache_key = cache_key_buf;
        }
        snprintf(cache_key, cache_key_len + 1, "%s", GENE_CONFIG_CACHE);
    }

    zval *di_config = gene_memory_get_by_config(cache_key, cache_key_len, config_key);
    if (cache_key_heap) {
        efree(cache_key);
    }
    cache_key = NULL;

    if (!di_config || Z_TYPE_P(di_config) != IS_ARRAY) {
        php_error_docref(NULL, E_WARNING,
            "Gene\\Cache\\RedisPool::create(): config key '%s' not found in config cache",
            config_key);
        RETURN_NULL();
    }

    /* Expect: ['class'=>..., 'params'=>[[host, port, timeout, ...]]] */
    zval *params_zv = zend_hash_str_find(Z_ARRVAL_P(di_config), ZEND_STRL("params"));
    if (!params_zv || Z_TYPE_P(params_zv) != IS_ARRAY) {
        php_error_docref(NULL, E_WARNING,
            "Gene\\Cache\\RedisPool::create(): config key '%s' has no 'params' array",
            config_key);
        RETURN_NULL();
    }

    zval *redis_params = zend_hash_index_find(Z_ARRVAL_P(params_zv), 0);
    if (!redis_params || Z_TYPE_P(redis_params) != IS_ARRAY) {
        php_error_docref(NULL, E_WARNING,
            "Gene\\Cache\\RedisPool::create(): config key '%s' params[0] is not an array",
            config_key);
        RETURN_NULL();
    }

    /* Build merged config: connection params (local copies) + pool options */
    zval merged_config;
    array_init(&merged_config);

    {
        zval *v;

        v = zend_hash_str_find(Z_ARRVAL_P(redis_params), ZEND_STRL("host"));
        if (v && Z_TYPE_P(v) == IS_STRING) {
            add_assoc_stringl(&merged_config, "host", Z_STRVAL_P(v), Z_STRLEN_P(v));
        }

        v = zend_hash_str_find(Z_ARRVAL_P(redis_params), ZEND_STRL("port"));
        if (v) {
            if (Z_TYPE_P(v) == IS_LONG)        add_assoc_long(&merged_config, "port", Z_LVAL_P(v));
            else if (Z_TYPE_P(v) == IS_STRING)
                add_assoc_stringl(&merged_config, "port", Z_STRVAL_P(v), Z_STRLEN_P(v));
        }

        v = zend_hash_str_find(Z_ARRVAL_P(redis_params), ZEND_STRL("timeout"));
        if (v) {
            if      (Z_TYPE_P(v) == IS_DOUBLE) add_assoc_double(&merged_config, "timeout", Z_DVAL_P(v));
            else if (Z_TYPE_P(v) == IS_LONG)   add_assoc_long(&merged_config, "timeout", Z_LVAL_P(v));
            else if (Z_TYPE_P(v) == IS_STRING)
                add_assoc_stringl(&merged_config, "timeout", Z_STRVAL_P(v), Z_STRLEN_P(v));
        }

        v = zend_hash_str_find(Z_ARRVAL_P(redis_params), ZEND_STRL("password"));
        if (v && Z_TYPE_P(v) == IS_STRING) {
            add_assoc_stringl(&merged_config, "password", Z_STRVAL_P(v), Z_STRLEN_P(v));
        }

        /* options[]: deep-copy from persistent memory */
        v = zend_hash_str_find(Z_ARRVAL_P(redis_params), ZEND_STRL("options"));
        if (v && Z_TYPE_P(v) == IS_ARRAY) {
            zval opts_local;
            gene_memory_zval_local(&opts_local, v);
            add_assoc_zval(&merged_config, "options", &opts_local);
        }

        /* servers[]: deep-copy from persistent memory */
        v = zend_hash_str_find(Z_ARRVAL_P(redis_params), ZEND_STRL("servers"));
        if (v && Z_TYPE_P(v) == IS_ARRAY) {
            zval servers_local;
            gene_memory_zval_local(&servers_local, v);
            add_assoc_zval(&merged_config, "servers", &servers_local);
        }
    }

    /* Pool-specific options (with defaults) */
    {
        zend_long min_val = 1, max_val = 10, idle_val = 60;
        double    wait_val = 3.0;
        zval     *opt;

        if (options) {
            if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("min"))) &&
                Z_TYPE_P(opt) == IS_LONG)   min_val  = Z_LVAL_P(opt);
            if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("max"))) &&
                Z_TYPE_P(opt) == IS_LONG)   max_val  = Z_LVAL_P(opt);
            if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("idleTimeout"))) &&
                Z_TYPE_P(opt) == IS_LONG)   idle_val = Z_LVAL_P(opt);
            if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("waitTimeout")))) {
                if      (Z_TYPE_P(opt) == IS_DOUBLE) wait_val = Z_DVAL_P(opt);
                else if (Z_TYPE_P(opt) == IS_LONG)   wait_val = (double)Z_LVAL_P(opt);
            }
        }

        add_assoc_long(&merged_config,   "min",         min_val);
        add_assoc_long(&merged_config,   "max",         max_val);
        add_assoc_long(&merged_config,   "idleTimeout", idle_val);
        add_assoc_double(&merged_config, "waitTimeout", wait_val);
    }

    /* Get or initialise the static instances registry */
    instances = zend_read_static_property(gene_redis_pool_ce,
                                           ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), 1);

    if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
        /* Close the existing pool with this name if present */
        zval *existing = zend_hash_find(Z_ARRVAL_P(instances), name);
        if (existing && Z_TYPE_P(existing) == IS_OBJECT) {
            zval close_ret;
            gene_factory_call(existing, "close", sizeof("close") - 1, NULL, &close_ret);
            zval_ptr_dtor(&close_ret);
        }
    } else {
        zval arr;
        array_init(&arr);
        zend_update_static_property(gene_redis_pool_ce,
                                     ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), &arr);
        zval_ptr_dtor(&arr);
        instances = zend_read_static_property(gene_redis_pool_ce,
                                               ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), 1);
    }

    /* Construct the new pool object */
    zval new_pool, params_arr, construct_ret;
    object_init_ex(&new_pool, gene_redis_pool_ce);

    array_init(&params_arr);
    Z_TRY_ADDREF(merged_config);
    add_next_index_zval(&params_arr, &merged_config);

    gene_factory_call(&new_pool, "__construct", sizeof("__construct") - 1, &params_arr, &construct_ret);
    zval_ptr_dtor(&construct_ret);
    zval_ptr_dtor(&params_arr);
    zval_ptr_dtor(&merged_config);

    /* Register in the static instances registry */
    Z_TRY_ADDREF(new_pool);
    zend_hash_update(Z_ARRVAL_P(instances), name, &new_pool);

    RETURN_ZVAL(&new_pool, 1, 1);
}
/* }}} */

/*
 * {{{ public static Gene\Cache\RedisPool::getInstance(string $name): ?self
 */
PHP_METHOD(gene_redis_pool, getInstance)
{
    zend_string *name;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
        RETURN_NULL();
    }
    zval *instances = zend_read_static_property(gene_redis_pool_ce,
                                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), 1);
    if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
        zval *pool = zend_hash_find(Z_ARRVAL_P(instances), name);
        if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
            RETURN_ZVAL(pool, 1, 0);
        }
    }
    RETURN_NULL();
}
/* }}} */

/*
 * {{{ public Gene\Cache\RedisPool::get(): ?Redis
 *
 * Borrow a Redis connection:
 *   1. Non-blocking pop from idle channel — PING verify.
 *   2. currentCount < max  → atomically reserve slot, create new connection.
 *   3. At max              → block up to waitTimeout for a return.
 *   4. Timeout             → create overflow connection (count > max).
 *      Overflow connections are auto-discarded in put(), shrinking back to max.
 * Returns null only when overflow creation itself fails.
 */
PHP_METHOD(gene_redis_pool, get)
{
    zval *self    = getThis();
    zval *channel = NULL;
    zend_long retries    = 0;
    zend_long max_retries;

    if (rpool_is_closed(self)) {
        RETURN_NULL();
    }

    channel = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL), 1, NULL);
    if (!channel || Z_TYPE_P(channel) != IS_OBJECT) {
        RETURN_NULL();
    }

    max_retries = rpool_get_max(self) + 2;

    while (retries < max_retries) {

        /* 1. Non-blocking pop from idle queue */
        {
            zval item;
            if (rpool_channel_pop(channel, 0.001, &item)) {
                if (Z_TYPE(item) == IS_ARRAY) {
                    zval *conn = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("conn"));
                    if (conn && Z_TYPE_P(conn) == IS_OBJECT) {
                        /* Skip liveness check — dead connections are handled
                         * by the caller (catch → remove → re-get) and by
                         * the periodic recycleIdle() timer. */
                        RETVAL_ZVAL(conn, 1, 0);
                        zval_ptr_dtor(&item);
                        return;
                    }
                }
                zval_ptr_dtor(&item);
                retries++;
                continue;
            }
        }

        /* 2. Below max: atomically reserve a slot then create */
        {
            zend_long new_count = rpool_increment_count_get(self);
            if (new_count <= rpool_get_max(self)) {
                zval conn;
                rpool_create_connection(self, &conn);
                if (Z_TYPE(conn) == IS_OBJECT) {
                    RETURN_ZVAL(&conn, 0, 0);
                }
                rpool_decrement_count(self);
                retries++;
                continue;
            }
            /* Over max — roll back the reservation */
            rpool_decrement_count(self);
        }

        /* 3. At max: block until a connection is returned or timeout */
        {
            zval item;
            if (rpool_channel_pop(channel, rpool_get_wait_timeout(self), &item)) {
                if (Z_TYPE(item) == IS_ARRAY) {
                    zval *conn = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("conn"));
                    if (conn && Z_TYPE_P(conn) == IS_OBJECT) {
                        RETVAL_ZVAL(conn, 1, 0);
                        zval_ptr_dtor(&item);
                        return;
                    }
                }
                zval_ptr_dtor(&item);
                retries++;
                continue;
            }

            /* Timeout — create overflow connection to prevent caller exception */
            rpool_increment_count(self);
            {
                zval overflow;
                rpool_create_connection(self, &overflow);
                if (Z_TYPE(overflow) == IS_OBJECT) {
                    php_error_docref(NULL, E_NOTICE,
                        "Gene\\Cache\\RedisPool: pool exhausted "
                        "(max=%ld, current=%ld), overflow connection created",
                        (long)rpool_get_max(self), (long)rpool_get_count(self));
                    RETURN_ZVAL(&overflow, 0, 0);
                }
                rpool_decrement_count(self);
            }
            RETURN_NULL();
        }
    }

    RETURN_NULL();
}
/* }}} */

/*
 * {{{ public Gene\Cache\RedisPool::put(Redis $redis): void
 *
 * Return a borrowed connection to the pool.
 * Handles: pool closed, dead connection, overflow shrink, normal return.
 */
PHP_METHOD(gene_redis_pool, put)
{
    zval *redis = NULL, *self = getThis();
    zval *channel;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "o", &redis) == FAILURE) {
        return;
    }

    if (rpool_is_closed(self)) {
        rpool_decrement_count(self);
        return;
    }

    channel = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL), 1, NULL);
    if (!channel || Z_TYPE_P(channel) != IS_OBJECT) {
        rpool_decrement_count(self);
        return;
    }

    /* Skip liveness check — dead connections are caught by recycleIdle().
     * Avoiding Redis::ping() saves one network RT per put(). */

    /* Auto-shrink overflow: discard connections above max */
    if (rpool_get_count(self) > rpool_get_max(self)) {
        rpool_decrement_count(self);
        return;
    }

    if (!rpool_channel_push(channel, redis)) {
        /* Push failed (e.g. channel already closed) */
        rpool_decrement_count(self);
    }
}
/* }}} */

/*
 * {{{ public Gene\Cache\RedisPool::remove(): void
 * Caller discarded a borrowed connection (e.g. it died mid-use).
 * Decrements the pool count without a PING — the caller already knows it's dead.
 */
PHP_METHOD(gene_redis_pool, remove)
{
    rpool_decrement_count(getThis());
}
/* }}} */

/*
 * {{{ public Gene\Cache\RedisPool::close(): void
 *
 * Two-phase shutdown:
 *   Mark closed + stop timer.
 *   (Coroutine only)
 *     a) drain idle connections;
 *     b) wait briefly for in-flight connections;
 *     c) Channel::close() — wakes blocked get() callers with false;
 *     d) force-reset count.
 */
PHP_METHOD(gene_redis_pool, close)
{
    zval *self = getThis();

    if (!rpool_is_closed(self)) {
        zend_update_property_bool(gene_redis_pool_ce, gene_strip_obj(self),
                                   ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CLOSED), 1);
        rpool_stop_timer(self);
    }

    zval *channel = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL), 1, NULL);
    if (channel && Z_TYPE_P(channel) == IS_OBJECT) {
        if (rpool_in_coroutine()) {
            /* a) drain idle connections */
            while (!rpool_channel_is_empty(channel)) {
                zval item;
                if (!rpool_channel_pop(channel, 0.001, &item)) break;
                rpool_decrement_count(self);
                zval_ptr_dtor(&item);
            }

            /* b) wait briefly for in-flight connections */
            if (rpool_get_count(self) > 0) {
                double    wait      = rpool_get_wait_timeout(self);
                if (wait > 3.0) wait = 3.0;
                if (wait < 0.1) wait = 0.1;
                zend_long rounds     = 0;
                zend_long max_rounds = (zend_long)(wait / 0.05);
                if (max_rounds < 1) max_rounds = 1;

                while (rpool_get_count(self) > 0 && rounds < max_rounds) {
                    zval item;
                    if (rpool_channel_pop(channel, 0.05, &item)) {
                        rpool_decrement_count(self);
                        zval_ptr_dtor(&item);
                    } else {
                        rounds++;
                    }
                }
            }

            /* c) close channel — wakes any remaining blocked get() coroutines */
            zval close_rv;
            zend_function *close_func = zend_hash_str_find_ptr(&Z_OBJCE_P(channel)->function_table, ZEND_STRL("close"));
            ZVAL_UNDEF(&close_rv);
            if (EXPECTED(close_func)) {
                zend_call_known_function(close_func, Z_OBJ_P(channel), Z_OBJCE_P(channel), &close_rv, 0, NULL, NULL);
            }
            if (!Z_ISUNDEF(close_rv)) zval_ptr_dtor(&close_rv);
        }
        /* d) force-reset count; in-use connections self-discard via put() */
        rpool_set_count(self, 0);
    }
}
/* }}} */

/*
 * {{{ public Gene\Cache\RedisPool::recycleIdle(): void
 * Timer callback — runs inside a Swoole coroutine.
 */
PHP_METHOD(gene_redis_pool, recycleIdle)
{
    rpool_recycle_idle(getThis());
}
/* }}} */

/*
 * {{{ public static Gene\Cache\RedisPool::closeAll(): void
 *
 * Two-phase shutdown of every named pool:
 *   Phase 1: mark all pools closed + stop all timers (prevents new borrows).
 *   Phase 2: drain and close each pool's channel.
 * Clears the static instances registry.
 */
PHP_METHOD(gene_redis_pool, closeAll)
{
    zval *instances = zend_read_static_property(gene_redis_pool_ce,
                                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), 1);
    if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
        zval *pool;

        /* Phase 1 */
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(instances), pool) {
            if (Z_TYPE_P(pool) == IS_OBJECT && !rpool_is_closed(pool)) {
                zend_update_property_bool(gene_redis_pool_ce, gene_strip_obj(pool),
                                           ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CLOSED), 1);
                rpool_stop_timer(pool);
            }
        } ZEND_HASH_FOREACH_END();

        /* Phase 2 */
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(instances), pool) {
            if (Z_TYPE_P(pool) == IS_OBJECT) {
                zval close_ret;
                gene_factory_call(pool, "close", sizeof("close") - 1, NULL, &close_ret);
                zval_ptr_dtor(&close_ret);
            }
        } ZEND_HASH_FOREACH_END();
    }

    zval empty;
    array_init(&empty);
    zend_update_static_property(gene_redis_pool_ce,
                                 ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), &empty);
    zval_ptr_dtor(&empty);
}
/* }}} */

/*
 * {{{ public static Gene\Cache\RedisPool::stopTimers(): void
 *
 * Lightweight: only clears idle-recycler timers without draining channels.
 * Designed for Swoole's onWorkerExit so the event loop can exit cleanly.
 * closeAll() should still be called from onWorkerStop for full cleanup.
 */
PHP_METHOD(gene_redis_pool, stopTimers)
{
    zval *instances = zend_read_static_property(gene_redis_pool_ce,
                                                  ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES), 1);
    if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
        zval *pool;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(instances), pool) {
            if (Z_TYPE_P(pool) == IS_OBJECT) {
                rpool_stop_timer(pool);
            }
        } ZEND_HASH_FOREACH_END();
    }
}
/* }}} */

/*
 * {{{ public Gene\Cache\RedisPool::stats(): array
 */
PHP_METHOD(gene_redis_pool, stats)
{
    zval *self    = getThis();
    zval *channel = zend_read_property(gene_redis_pool_ce, gene_strip_obj(self),
                                        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL), 1, NULL);
    zend_long count = rpool_get_count(self);
    zend_long idle  = (channel && Z_TYPE_P(channel) == IS_OBJECT)
                      ? rpool_channel_length(channel) : 0;
    zend_long max      = rpool_get_max(self);
    zend_long overflow = (count > max) ? (count - max) : 0;

    array_init(return_value);
    add_assoc_long(return_value,  "total",    count);
    add_assoc_long(return_value,  "idle",     idle);
    add_assoc_long(return_value,  "using",    count - idle);
    add_assoc_long(return_value,  "overflow", overflow);
    add_assoc_long(return_value,  "min",      rpool_get_min(self));
    add_assoc_long(return_value,  "max",      max);
    add_assoc_bool(return_value,  "closed",   rpool_is_closed(self));
}
/* }}} */

/*
 * {{{ public Gene\Cache\RedisPool::__destruct()
 */
PHP_METHOD(gene_redis_pool, __destruct)
{
    zval *self = getThis();
    if (!rpool_is_closed(self)) {
        zval close_ret;
        gene_factory_call(self, "close", sizeof("close") - 1, NULL, &close_ret);
        zval_ptr_dtor(&close_ret);
    }
}
/* }}} */

/* ====================== Shared helpers used by Gene\Cache\Redis ====================== */

/*
 * Borrow a Redis connection from a named RedisPool instance and store it on $self.
 *
 * Reads config[$pool_key] as the pool name, calls RedisPool::getInstance($name),
 * then pool->get().  On success stores pool ref in $self->$pool_key and the
 * borrowed Redis object in $self->$obj_key.
 *
 * Returns true  → pool mode active, connection stored on self.
 * Returns false → not pool mode (config key absent / not Swoole) or pool not found.
 */
bool gene_redis_pool_get(zend_class_entry *redis_ce, zval *self, zval *config,
    const char *pool_key, size_t pool_key_len,
    const char *obj_key,  size_t obj_key_len)
{
    if (config == NULL || Z_TYPE_P(config) != IS_ARRAY) return 0;

    zval *pool_name = zend_hash_str_find(Z_ARRVAL_P(config), pool_key, pool_key_len);
    if (!pool_name || Z_TYPE_P(pool_name) != IS_STRING || GENE_G(runtime_type) < 2) {
        return 0;
    }

    /* Gene\Cache\RedisPool::getInstance($name) */
    zval callable, pool_obj;
    array_init(&callable);
    if (GENE_G(use_namespace)) {
        add_next_index_string(&callable, "Gene\\Cache\\RedisPool");
    } else {
        add_next_index_string(&callable, "Gene_Cache_RedisPool");
    }
    add_next_index_string(&callable, "getInstance");

    zval param;
    ZVAL_COPY(&param, pool_name);
    ZVAL_UNDEF(&pool_obj);
    call_user_function(NULL, NULL, &callable, &pool_obj, 1, &param);
    zval_ptr_dtor(&param);
    zval_ptr_dtor(&callable);

    if (Z_TYPE(pool_obj) == IS_OBJECT) {
        zend_ulong obj_handle = self ? (zend_ulong)Z_OBJ_HANDLE_P(self) : 0;
        zval redis_conn;
        ZVAL_UNDEF(&redis_conn);
        gene_factory_call(&pool_obj, "get", sizeof("get") - 1, NULL, &redis_conn);

        if (Z_TYPE(redis_conn) == IS_OBJECT) {
            /* Store pool ref ONLY after successful get().
             * Storing it before would make __destruct try to return a null
             * connection to the pool, corrupting the connection count. */
            zend_update_property(redis_ce, gene_strip_obj(self),
                                  pool_key, pool_key_len, &pool_obj);
            zend_update_property(redis_ce, gene_strip_obj(self),
                                  obj_key, obj_key_len, &redis_conn);
            zval_ptr_dtor(&redis_conn);
            zval_ptr_dtor(&pool_obj);
            return 1;
        }
        zval_ptr_dtor(&redis_conn);
    }
    zval_ptr_dtor(&pool_obj);
    return 0;
}
/*
 * re-use of the connection during the coroutine yield inside channel->push().
 * The pool property ($pool_key) is intentionally kept so that:
 *   - subsequent commands can detect pool mode and re-borrow;
 *   - __destruct double-return is prevented by checking obj_key == null.
 */
void gene_redis_pool_return(zend_class_entry *redis_ce, zval *self,
    const char *pool_key, size_t pool_key_len,
    const char *obj_key,  size_t obj_key_len)
{
    zval *pool = zend_read_property(redis_ce, gene_strip_obj(self),
                                     pool_key, pool_key_len, 1, NULL);
    zend_ulong obj_handle = self ? (zend_ulong)Z_OBJ_HANDLE_P(self) : 0;
    if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
        zval *redis_obj = zend_read_property(redis_ce, gene_strip_obj(self),
                                              obj_key, obj_key_len, 1, NULL);
        if (redis_obj && Z_TYPE_P(redis_obj) == IS_OBJECT) {
            zval obj_copy;
            ZVAL_COPY(&obj_copy, redis_obj);
            /* Null BEFORE put() — prevents re-use during coroutine yield */
            zend_update_property_null(redis_ce, gene_strip_obj(self),
                                       obj_key, obj_key_len);

            zval retval, params;
            array_init(&params);
            add_next_index_zval(&params, &obj_copy);
            gene_factory_call(pool, "put", sizeof("put") - 1, &params, &retval);
            zval_ptr_dtor(&retval);
            zval_ptr_dtor(&params);
        } else {
            /* Already returned — just ensure property is null */
            zend_update_property_null(redis_ce, gene_strip_obj(self),
                                       obj_key, obj_key_len);
        }
    }
}

/*
 * Notify the pool that a dead connection is being discarded (no PING check).
 * Decrements the pool count so a new connection slot becomes available.
 * Does NOT clear the pool reference property — caller clears obj_key separately.
 */
void gene_redis_pool_notify_remove(zend_class_entry *redis_ce, zval *self,
    const char *pool_key, size_t pool_key_len)
{
    zval *pool = zend_read_property(redis_ce, gene_strip_obj(self),
                                     pool_key, pool_key_len, 1, NULL);
    if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
        zval retval;
        gene_factory_call(pool, "remove", sizeof("remove") - 1, NULL, &retval);
        zval_ptr_dtor(&retval);
    }
}

/* ====================== Method table ====================== */

const zend_function_entry gene_redis_pool_methods[] = {
    PHP_ME(gene_redis_pool, __construct,  gene_redis_pool_construct_arginfo,     ZEND_ACC_PUBLIC)
    PHP_ME(gene_redis_pool, __destruct,   gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC)
    PHP_ME(gene_redis_pool, create,       gene_redis_pool_create_arginfo,        ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(gene_redis_pool, getInstance,  gene_redis_pool_get_instance_arginfo,  ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(gene_redis_pool, get,          gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC)
    PHP_ME(gene_redis_pool, put,          gene_redis_pool_put_arginfo,           ZEND_ACC_PUBLIC)
    PHP_ME(gene_redis_pool, remove,       gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC)
    PHP_ME(gene_redis_pool, close,        gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC)
    PHP_ME(gene_redis_pool, closeAll,     gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(gene_redis_pool, stopTimers,   gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(gene_redis_pool, recycleIdle,  gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC)
    PHP_ME(gene_redis_pool, stats,        gene_redis_pool_void_arginfo,          ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/* ====================== MINIT ====================== */

GENE_MINIT_FUNCTION(redis_pool)
{
    zend_class_entry redis_pool_ce;
    GENE_INIT_CLASS_ENTRY(redis_pool_ce,
                           "Gene_Cache_RedisPool",
                           "Gene\\Cache\\RedisPool",
                           gene_redis_pool_methods);
    gene_redis_pool_ce = zend_register_internal_class_ex(&redis_pool_ce, NULL);
    gene_redis_pool_ce->ce_flags |= ZEND_ACC_FINAL;
#if PHP_VERSION_ID >= 80200
    gene_redis_pool_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

    zend_declare_property_null(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CHANNEL),   ZEND_ACC_PROTECTED);
    zend_declare_property_null(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CONFIG),    ZEND_ACC_PROTECTED);
    zend_declare_property_long(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_MIN),    1,   ZEND_ACC_PROTECTED);
    zend_declare_property_long(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_MAX),    64,  ZEND_ACC_PROTECTED);
    zend_declare_property_long(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_IDLE_TIME), 60, ZEND_ACC_PROTECTED);
    zend_declare_property_double(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_WAIT_TIME), 3.0, ZEND_ACC_PROTECTED);
    zend_declare_property_null(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_COUNT),     ZEND_ACC_PROTECTED);
    zend_declare_property_long(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_TIMER_ID), 0, ZEND_ACC_PROTECTED);
    zend_declare_property_bool(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_CLOSED),  0, ZEND_ACC_PROTECTED);

    /* Static instances registry */
    zend_declare_property_null(gene_redis_pool_ce,
        ZEND_STRL(GENE_REDIS_POOL_PROPERTY_INSTANCES),
        ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);

    return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */