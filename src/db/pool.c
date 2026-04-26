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
 #include "../factory/factory.h"
 #include "../config/configs.h"
 #include "../cache/memory.h"
 #include "../db/pool.h"
  
 zend_class_entry *gene_pool_ce;
 
 /* Forward declarations */
 static void pool_stop_timer(zval *self);
 static bool pool_in_coroutine(void);

/* ====================== Timer management ====================== */

static bool pool_in_coroutine(void)
{
    /* If not in Swoole mode, definitely not in a coroutine */
    if (GENE_G(runtime_type) < 2) {
        return 0;
    }

    /* Check if Swoole\Coroutine::exists() returns true */
    static zend_string *co_class = NULL;
    if (UNEXPECTED(!co_class)) {
        co_class = zend_string_init_interned(ZEND_STRL("Swoole\\Coroutine"), 1);
    }
    zend_class_entry *co_ce = zend_lookup_class(co_class);
    if (!co_ce) {
        return 0;
    }

    /* Get current coroutine ID first */
    zend_function *fn_getcid = zend_hash_str_find_ptr(&co_ce->function_table, ZEND_STRL("getCid"));
    if (!fn_getcid) {
        return 0;
    }

    zval cid_ret;
    ZVAL_UNDEF(&cid_ret);
    zend_call_known_function(fn_getcid, NULL, co_ce, &cid_ret, 0, NULL, NULL);

    zend_long cid = (Z_TYPE(cid_ret) == IS_LONG) ? Z_LVAL(cid_ret) : -1;
    if (!Z_ISUNDEF(cid_ret)) zval_ptr_dtor(&cid_ret);

    if (cid < 0) {
        return 0;
    }

    /* Check if this coroutine exists */
    zend_function *fn_exists = zend_hash_str_find_ptr(&co_ce->function_table, ZEND_STRL("exists"));
    if (!fn_exists) {
        return 0;
    }

    zval ret, params[1];
    ZVAL_LONG(&params[0], cid);
    ZVAL_UNDEF(&ret);
    zend_call_known_function(fn_exists, NULL, co_ce, &ret, 1, params, NULL);

    bool in_co = (Z_TYPE(ret) == IS_TRUE);
    if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);

    return in_co;
}

static void pool_stop_timer(zval *self)
{
    zval *timer_id_zv = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 1, NULL);
    zend_long timer_id = (timer_id_zv && Z_TYPE_P(timer_id_zv) == IS_LONG) ? Z_LVAL_P(timer_id_zv) : 0;

    if (timer_id != 0) {
        static zend_string *timer_key = NULL;
        if (UNEXPECTED(!timer_key)) {
            timer_key = zend_string_init_interned(ZEND_STRL("Swoole\\Timer"), 1);
        }
        zend_class_entry *timer_ce = zend_lookup_class(timer_key);
        if (timer_ce) {
            zend_function *fn_clear = zend_hash_str_find_ptr(&timer_ce->function_table, ZEND_STRL("clear"));
            if (fn_clear) {
                zval params[1], ret;
                ZVAL_LONG(&params[0], timer_id);
                ZVAL_UNDEF(&ret);
                zend_call_known_function(fn_clear, NULL, timer_ce, &ret, 1, params, NULL);
                if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
            }
        }
        zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 0);
    }
}

 /* [GENE_PERF:2026-04-26] Cached zend_function pointers — avoid per-call hash
  * lookups in the hot DB borrow/return path. Internal classes never reload
  * within a process, so a one-shot lazy cache is safe. */
 static zend_function *fn_pool_getinstance = NULL;
 static zend_function *fn_pool_get = NULL;
 static zend_function *fn_pool_put = NULL;
 static zend_function *fn_pool_remove = NULL;

 static inline zend_function *pool_method(zend_function **slot, const char *name, size_t len) {
     if (UNEXPECTED(!*slot)) {
         *slot = zend_hash_str_find_ptr(&gene_pool_ce->function_table, name, len);
     }
     return *slot;
 }

 #define POOL_OBJ_METHOD_CACHED(obj_zv, slot, mname) \
     ((UNEXPECTED(!(slot))) \
         ? ((slot) = zend_hash_str_find_ptr(&Z_OBJCE_P(obj_zv)->function_table, ZEND_STRL(mname))) \
         : (slot))
  
 /* {{{ ARG_INFO */
 ZEND_BEGIN_ARG_INFO_EX(gene_pool_void_arginfo, 0, 0, 0)
 ZEND_END_ARG_INFO()
  
 ZEND_BEGIN_ARG_INFO_EX(gene_pool_construct_arginfo, 0, 0, 1)
     ZEND_ARG_INFO(0, config)
 ZEND_END_ARG_INFO()
  
 ZEND_BEGIN_ARG_INFO_EX(gene_pool_create_arginfo, 0, 0, 2)
     ZEND_ARG_INFO(0, name)
     ZEND_ARG_INFO(0, configKey)
     ZEND_ARG_INFO(0, options)
 ZEND_END_ARG_INFO()
  
 ZEND_BEGIN_ARG_INFO_EX(gene_pool_get_instance_arginfo, 0, 0, 1)
     ZEND_ARG_INFO(0, name)
 ZEND_END_ARG_INFO()
  
 ZEND_BEGIN_ARG_INFO_EX(gene_pool_put_arginfo, 0, 0, 1)
     ZEND_ARG_INFO(0, pdo)
 ZEND_END_ARG_INFO()
 /* }}} */
  
 /* ====================== Internal helper functions ====================== */
  
 static void pool_create_connection(zval *self, zval *retval) {
     zval *config = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CONFIG), 1, NULL);
     zval *dsn = NULL, *user = NULL, *pass = NULL, *options = NULL;
  
     ZVAL_NULL(retval);
  
     if (!config || Z_TYPE_P(config) != IS_ARRAY) {
         return;
     }
  
     dsn = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("dsn"));
     user = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("username"));
     pass = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("password"));
     options = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("options"));
  
     if (!dsn || Z_TYPE_P(dsn) != IS_STRING) {
         return;
     }
  
     static zend_string *pdo_key = NULL;
     if (UNEXPECTED(!pdo_key)) {
         pdo_key = zend_string_init_interned(ZEND_STRL("PDO"), 1);
     }
     zend_class_entry *pdo_ce = zend_lookup_class(pdo_key);
  
     if (!pdo_ce) {
         return;
     }
  
     zval pdo_object, option, construct_ret;
     object_init_ex(&pdo_object, pdo_ce);
  
     if (options && Z_TYPE_P(options) == IS_ARRAY) {
         ZVAL_DUP(&option, options);
     } else {
         array_init(&option);
     }
     /* PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION */
     add_index_long(&option, 3, 2);
     /* PDO::ATTR_EMULATE_PREPARES => false */
     add_index_long(&option, 20, 0);
    /* In Swoole/coroutine mode, force-disable PDO persistent connections.
     * NOTE: PDO::ATTR_PERSISTENT index is 12. */
    if (GENE_G(runtime_type) >= 2) {
        add_index_bool(&option, 12, 0);
    }
  
     zend_function *ctor = pdo_ce->constructor;
     zval params[4];
     ZVAL_COPY(&params[0], dsn);
     if (user && Z_TYPE_P(user) == IS_STRING) {
         ZVAL_COPY(&params[1], user);
     } else {
         ZVAL_STRING(&params[1], "");
     }
     if (pass && Z_TYPE_P(pass) == IS_STRING) {
         ZVAL_COPY(&params[2], pass);
     } else {
         ZVAL_STRING(&params[2], "");
     }
     ZVAL_COPY(&params[3], &option);
     ZVAL_UNDEF(&construct_ret);
     if (EXPECTED(ctor)) {
         zend_call_known_function(ctor, Z_OBJ(pdo_object), pdo_ce, &construct_ret, 4, params, NULL);
     }
     zval_ptr_dtor(&params[0]);
     zval_ptr_dtor(&params[1]);
     zval_ptr_dtor(&params[2]);
     zval_ptr_dtor(&params[3]);
     if (!Z_ISUNDEF(construct_ret)) {
         zval_ptr_dtor(&construct_ret);
     }
     zval_ptr_dtor(&option);
  
     if (EG(exception)) {
         zend_clear_exception();
         zval_ptr_dtor(&pdo_object);
         return;
     }
  
     ZVAL_COPY_VALUE(retval, &pdo_object);
 }
  
 static bool pool_is_alive(zval *pdo) {
     zval retval;
     zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo)->function_table, ZEND_STRL("getattribute"));
     zval params[1];
     /* PDO::ATTR_SERVER_INFO = 6 */
     ZVAL_LONG(&params[0], 6);
     ZVAL_UNDEF(&retval);
     if (EXPECTED(fn)) {
         zend_call_known_function(fn, Z_OBJ_P(pdo), Z_OBJCE_P(pdo), &retval, 1, params, NULL);
     }
  
     if (EG(exception)) {
         zend_clear_exception();
         if (!Z_ISUNDEF(retval)) {
             zval_ptr_dtor(&retval);
         }
         return 0;
     }
     if (!Z_ISUNDEF(retval)) {
         zval_ptr_dtor(&retval);
     }
     return 1;
 }
  
 static bool pool_channel_push(zval *channel, zval *pdo) {
     zval retval, item;
     bool success;
     static zend_function *fn_push = NULL;
     zend_function *fn;
 
     array_init_size(&item, 2);
     Z_TRY_ADDREF_P(pdo);
     add_index_zval(&item, 0, pdo);
     add_index_long(&item, 1, (zend_long)time(NULL));
 
     fn = POOL_OBJ_METHOD_CACHED(channel, fn_push, "push");
     ZVAL_FALSE(&retval);
     if (EXPECTED(fn)) {
         zval params[1];
         ZVAL_COPY_VALUE(&params[0], &item);
         zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &retval, 1, params, NULL);
     }
     success = (Z_TYPE(retval) == IS_TRUE);
     if (!Z_ISUNDEF(retval)) {
         zval_ptr_dtor(&retval);
     }
     zval_ptr_dtor(&item);
     return success;
 }
  
 static bool pool_channel_pop(zval *channel, double timeout, zval *result) {
     static zend_function *fn_pop = NULL;
     zend_function *fn = POOL_OBJ_METHOD_CACHED(channel, fn_pop, "pop");
     if (!fn) {
         ZVAL_NULL(result);
         return 0;
     }
     zval retval, params[1];
     ZVAL_DOUBLE(&params[0], timeout);
     ZVAL_UNDEF(&retval);
     zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &retval, 1, params, NULL);
  
     if (Z_TYPE(retval) == IS_FALSE || Z_TYPE(retval) == IS_NULL) {
         zval_ptr_dtor(&retval);
         ZVAL_NULL(result);
         return 0;
     }
     ZVAL_COPY_VALUE(result, &retval);
     return 1;
 }
  
 static bool pool_channel_is_empty(zval *channel) {
     static zend_function *fn_isempty = NULL;
     zend_function *fn = POOL_OBJ_METHOD_CACHED(channel, fn_isempty, "isempty");
     if (!fn) return true;
     zval retval;
     ZVAL_UNDEF(&retval);
     zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &retval, 0, NULL, NULL);
     bool empty = (Z_TYPE(retval) == IS_TRUE);
     zval_ptr_dtor(&retval);
     return empty;
 }
  
 static bool pool_channel_is_full(zval *channel) {
     static zend_function *fn_isfull = NULL;
     zend_function *fn = POOL_OBJ_METHOD_CACHED(channel, fn_isfull, "isfull");
     if (!fn) return true;
     zval retval;
     ZVAL_UNDEF(&retval);
     zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &retval, 0, NULL, NULL);
     bool full = (Z_TYPE(retval) == IS_TRUE);
     zval_ptr_dtor(&retval);
     return full;
 }
  
 static zend_long pool_channel_length(zval *channel) {
     static zend_function *fn_length = NULL;
     zend_function *fn = POOL_OBJ_METHOD_CACHED(channel, fn_length, "length");
     if (!fn) return 0;
     zval retval;
     ZVAL_UNDEF(&retval);
     zend_call_known_function(fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &retval, 0, NULL, NULL);
     zend_long len = (Z_TYPE(retval) == IS_LONG) ? Z_LVAL(retval) : 0;
     zval_ptr_dtor(&retval);
     return len;
 }
  
 /* [GENE_PERF:2026-04-26] Caller passes pre-cached zend_function* (one-shot lookup)
  * — avoids strlen(method) + hash lookup on every atomic op. */
 static void pool_atomic_call_fn(zval *atomic, zend_function *fn, zend_long arg, zval *retval) {
     zval params[1], ret_local;
     if (!retval) retval = &ret_local;
     ZVAL_LONG(&params[0], arg);
     ZVAL_UNDEF(retval);
     if (EXPECTED(fn)) {
         zend_call_known_function(fn, Z_OBJ_P(atomic), Z_OBJCE_P(atomic), retval, 1, params, NULL);
     }
     if (retval == &ret_local) {
         if (!Z_ISUNDEF(ret_local)) zval_ptr_dtor(&ret_local);
     }
 }

 #define POOL_ATOMIC_CALL(atomic, MNAME, arg, retval) do {                                  \
     static zend_function *_pa_fn = NULL;                                                    \
     if (UNEXPECTED(!_pa_fn)) {                                                              \
         _pa_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(atomic)->function_table, ZEND_STRL(MNAME)); \
     }                                                                                       \
     pool_atomic_call_fn((atomic), _pa_fn, (arg), (retval));                                  \
 } while (0)
 
 static void pool_decrement_count(zval *self) {
     zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
     if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
         zval ret;
         ZVAL_UNDEF(&ret);
         POOL_ATOMIC_CALL(atomic, "get", 0, &ret);
         zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
         if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
         if (val > 0) {
             POOL_ATOMIC_CALL(atomic, "sub", 1, NULL);
         }
     }
 }
  
 static zend_long pool_get_count(zval *self) {
     zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
     if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
         zval ret;
         ZVAL_UNDEF(&ret);
         POOL_ATOMIC_CALL(atomic, "get", 0, &ret);
         zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
         if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
         return val;
     }
     return 0;
 }
  
 static zend_long pool_get_max(zval *self) {
     zval *max_zv = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_MAX), 1, NULL);
     return (max_zv && Z_TYPE_P(max_zv) == IS_LONG) ? Z_LVAL_P(max_zv) : 10;
 }
  
 static zend_long pool_get_min(zval *self) {
     zval *min_zv = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_MIN), 1, NULL);
     return (min_zv && Z_TYPE_P(min_zv) == IS_LONG) ? Z_LVAL_P(min_zv) : 1;
 }
  
 static zend_long pool_get_idle_time(zval *self) {
     zval *idle_zv = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_IDLE_TIME), 1, NULL);
     return (idle_zv && Z_TYPE_P(idle_zv) == IS_LONG) ? Z_LVAL_P(idle_zv) : 60;
 }

 static bool pool_is_closed(zval *self) {
     zval *closed = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CLOSED), 1, NULL);
     return (closed && Z_TYPE_P(closed) == IS_TRUE);
 }
  
 static double pool_get_wait_timeout(zval *self) {
     zval *wt = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_WAIT_TIME), 1, NULL);
     if (wt && Z_TYPE_P(wt) == IS_DOUBLE) return Z_DVAL_P(wt);
     if (wt && Z_TYPE_P(wt) == IS_LONG) return (double)Z_LVAL_P(wt);
     return 3.0;
 }
  
static void pool_increment_count(zval *self) {
    zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
    if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
        POOL_ATOMIC_CALL(atomic, "add", 1, NULL);
    }
}

/* [GENE_AUDIT:2026-03-25] Atomic increment that returns the new value.
 * Swoole\Atomic::add() is atomic and returns the post-increment value,
 * enabling check-after-reserve pattern to reduce the TOCTOU race window
 * in pool::get(). Two coroutines may both call add(1), but only one will
 * see new_value <= max; the other rolls back with sub(1). */
static zend_long pool_increment_count_get(zval *self) {
    zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
    if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
        zval ret;
        ZVAL_UNDEF(&ret);
        POOL_ATOMIC_CALL(atomic, "add", 1, &ret);
        zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
        if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
        return val;
    }
    return 0;
}
 
 static void pool_set_count(zval *self, zend_long val) {
     zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
     if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
         POOL_ATOMIC_CALL(atomic, "set", val, NULL);
     }
 }
 
 static void pool_fill(zval *self) {
     zval *channel = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), 1, NULL);
     zend_long min = pool_get_min(self);
     zend_long i;
 
     if (!channel || Z_TYPE_P(channel) != IS_OBJECT) return;
 
     for (i = 0; i < min; i++) {
         zval conn;
         pool_create_connection(self, &conn);
         if (Z_TYPE(conn) == IS_OBJECT) {
             /* [GENE_FIX:2026-04-09] Only increment count AFTER successful push.
              * If push fails (channel full), we must NOT increment count, otherwise
              * the count will drift from the actual number of connections in the pool. */
             if (pool_channel_push(channel, &conn)) {
                 pool_increment_count(self);
             }
             zval_ptr_dtor(&conn);
         } else {
             break;
         }
     }
 }
  
 static void pool_recycle_idle(zval *self) {
     zval *channel, *idle_zv;
     zend_long now, idle_timeout, size, i;
     zval item, *conn_zv, *last_used_zv;
  
     if (pool_is_closed(self)) {
         pool_stop_timer(self);
         return;
     }
 
     channel = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), 1, NULL);

    if (!channel || Z_TYPE_P(channel) != IS_OBJECT) return;

    now = (zend_long)time(NULL);
    size = pool_channel_length(channel);
    /* [GENE_PERF:2026-04-26] Cache min and idle_timeout outside loop — they cannot change while we iterate. */
    zend_long min_cached = pool_get_min(self);
    idle_timeout = pool_get_idle_time(self);

    /* Pop-check-push one at a time to avoid draining the channel.
     * This prevents starving concurrent get() callers. */
    for (i = 0; i < size; i++) {
        ZVAL_UNDEF(&item);
        if (!pool_channel_pop(channel, 0.001, &item)) {
            break;
        }
        if (Z_TYPE(item) != IS_ARRAY) {
            zval_ptr_dtor(&item);
            continue;
        }

        /* [GENE_PERF:2026-04-26] Items are now packed indexed arrays:
         * idx 0 = conn (PDO), idx 1 = lastUsed (long). */
        conn_zv      = zend_hash_index_find(Z_ARRVAL(item), 0);
        last_used_zv = zend_hash_index_find(Z_ARRVAL(item), 1);
        zend_long last_used = (last_used_zv && Z_TYPE_P(last_used_zv) == IS_LONG) ? Z_LVAL_P(last_used_zv) : now;

        if (pool_get_count(self) > min_cached && (now - last_used) > idle_timeout) {
            /* Discard idle connection */
            pool_decrement_count(self);
            zval_ptr_dtor(&item);
            continue;
        }

        /* Connection is still needed - check alive and push back immediately */
        if (conn_zv && Z_TYPE_P(conn_zv) == IS_OBJECT) {
            if (pool_is_alive(conn_zv)) {
                if (!pool_channel_push(channel, conn_zv)) {
                    pool_decrement_count(self);
                }
            } else {
                pool_decrement_count(self);
            }
        }
        zval_ptr_dtor(&item);
    }

    /* Refill to minimum */
    while (pool_get_count(self) < min_cached && !pool_channel_is_full(channel)) {
        zval conn;
        pool_create_connection(self, &conn);
        if (Z_TYPE(conn) == IS_OBJECT) {
            if (pool_channel_push(channel, &conn)) {
                pool_increment_count(self);
            }
            zval_ptr_dtor(&conn);
        } else {
            break;
        }
    }
}

PHP_METHOD(gene_pool, get)
{
    zval *self = getThis();
    zval *channel;
    zend_long retries = 0;
    zend_long max_retries;

    if (pool_is_closed(self)) {
        RETURN_NULL();
    }

    channel = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), 1, NULL);
    if (!channel || Z_TYPE_P(channel) != IS_OBJECT) {
        RETURN_NULL();
    }

    max_retries = pool_get_max(self) + 2;

    while (retries < max_retries) {
        /* 1. Try to pop from idle queue (non-blocking).
         *    Skip isEmpty() check to avoid TOCTOU race — just pop directly. */
        {
            zval item;
            if (pool_channel_pop(channel, 0.001, &item)) {
                if (Z_TYPE(item) == IS_ARRAY) {
                    /* [GENE_PERF:2026-04-26] packed indexed array: idx 0 = conn */
                    zval *conn = zend_hash_index_find(Z_ARRVAL(item), 0);
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

        /* 2. Below max: atomically increment THEN check the new value.
         *    Swoole\Atomic::add(1) returns the post-increment value atomically,
         *    so two coroutines cannot both see count<=max for the same slot. */
        {
            zend_long new_count = pool_increment_count_get(self);
            if (new_count <= pool_get_max(self)) {
                zval conn;
                pool_create_connection(self, &conn);
                if (Z_TYPE(conn) == IS_OBJECT) {
                    RETURN_ZVAL(&conn, 0, 0);
                }
                pool_decrement_count(self);
                retries++;
                continue;
            }
            pool_decrement_count(self);
        }

        /* 3. At max, wait for return */
        {
            zval item;
            if (pool_channel_pop(channel, pool_get_wait_timeout(self), &item)) {
                if (Z_TYPE(item) == IS_ARRAY) {
                    /* [GENE_PERF:2026-04-26] packed indexed array: idx 0 = conn */
                    zval *conn = zend_hash_index_find(Z_ARRVAL(item), 0);
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
            /* Timeout — create an overflow connection to prevent caller exception.
             * The overflow is tracked by currentCount (exceeds max).
             * When returned via put(), excess connections are auto-discarded
             * to shrink the pool back to max. */
            pool_increment_count(self);
            {
                zval overflow_conn;
                pool_create_connection(self, &overflow_conn);
                if (Z_TYPE(overflow_conn) == IS_OBJECT) {
                    php_error_docref(NULL, E_NOTICE,
                        "Gene\\Pool: pool exhausted (max=%ld, current=%ld), created overflow connection",
                        (long)pool_get_max(self), (long)pool_get_count(self));
                    RETURN_ZVAL(&overflow_conn, 0, 0);
                }
                /* Overflow creation also failed — roll back */
                pool_decrement_count(self);
            }
            RETURN_NULL();
        }
    }

    RETURN_NULL();
}
/* }}} */

/*
 * {{{ public Gene\Pool::put(PDO $pdo): void
 */
 PHP_METHOD(gene_pool, put)
 {
     zval *pdo = NULL, *self = getThis();
     zval *channel;
  
     if (zend_parse_parameters(ZEND_NUM_ARGS(), "o", &pdo) == FAILURE) {
         return;
     }
  
     if (pool_is_closed(self)) {
         pool_decrement_count(self);
         return;
     }
  
     channel = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), 1, NULL);
     if (!channel || Z_TYPE_P(channel) != IS_OBJECT) {
         pool_decrement_count(self);
         return;
     }
  
     /* Skip liveness check — dead connections are caught by recycleIdle().
     * Avoiding PDO::getAttribute() saves one network RT per put(). */

    /* Auto-shrink overflow: if currentCount exceeds max, discard this
      * connection instead of pushing it back. This naturally heals the
      * pool after overflow connections (created when pool was exhausted)
      * are returned. */
     if (pool_get_count(self) > pool_get_max(self)) {
         pool_decrement_count(self);
         return;
     }
 
     if (!pool_channel_push(channel, pdo)) {
         /* Channel is full or push failed */
         pool_decrement_count(self);
     }
 }
 /* }}} */
 
 /*
  * {{{ public Gene\Pool::remove(): void
  */
 PHP_METHOD(gene_pool, remove)
 {
     pool_decrement_count(getThis());
 }
 /* }}} */
 /*
  * {{{ public Gene\Pool::close(): void
  */
 PHP_METHOD(gene_pool, close)
 {
     zval *self = getThis();
 
     /* Mark as closed and stop timer (idempotent — safe to call multiple times).
      * closeAll() marks pools closed before calling close(), so we must NOT
      * bail out early when already closed; the channel still needs draining. */
     if (!pool_is_closed(self)) {
         zend_update_property_bool(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CLOSED), 1);
         pool_stop_timer(self);
     }
 
     /* Channel operations (pop/push/isEmpty/close) require Swoole coroutine
      * context.  When called from workerStop (no coroutine), skip the drain
      * and just force-reset.  The channel and its PDO objects will be freed
      * when the pool object is destroyed at worker shutdown. */
     zval *channel = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), 1, NULL);
     if (channel && Z_TYPE_P(channel) == IS_OBJECT) {
         if (pool_in_coroutine()) {
             /* Two-phase drain:
              * Phase 1: drain immediately available idle connections.
              * Phase 2: briefly wait for in-flight connections being returned
              *          by concurrent coroutines (up to waitTimeout total).
              * After drain, close the channel and force-reset count. */
 
             /* Phase 1: drain idle connections (non-blocking) */
             while (!pool_channel_is_empty(channel)) {
                 zval item;
                 if (!pool_channel_pop(channel, 0.001, &item)) {
                     break;
                 }
                 pool_decrement_count(self);
                 zval_ptr_dtor(&item);
             }
 
             /* Phase 2: if connections are still in-use, wait briefly for returns. */
             if (pool_get_count(self) > 0) {
                 double wait = pool_get_wait_timeout(self);
                 if (wait > 3.0) wait = 3.0;
                 if (wait < 0.1) wait = 0.1;
                 zend_long rounds = 0;
                 zend_long max_rounds = (zend_long)(wait / 0.05);
                 if (max_rounds < 1) max_rounds = 1;
 
                 while (pool_get_count(self) > 0 && rounds < max_rounds) {
                     zval item;
                     if (pool_channel_pop(channel, 0.05, &item)) {
                         pool_decrement_count(self);
                         zval_ptr_dtor(&item);
                     } else {
                         rounds++;
                     }
                 }
             }
 
             /* Close channel — wakes any remaining blocked coroutines with false */
             static zend_function *fn_ch_close = NULL;
             zend_function *close_fn = POOL_OBJ_METHOD_CACHED(channel, fn_ch_close, "close");
             zval close_ret;
             ZVAL_UNDEF(&close_ret);
             if (EXPECTED(close_fn)) {
                 zend_call_known_function(close_fn, Z_OBJ_P(channel), Z_OBJCE_P(channel), &close_ret, 0, NULL, NULL);
             }
             if (!Z_ISUNDEF(close_ret)) {
                 zval_ptr_dtor(&close_ret);
             }
         }
         /* else: not in coroutine — channel will be freed with the pool object */
 
         /* Force-reset count: any remaining in-use connections will see
          * closed=true when returned via put() and self-discard. */
         pool_set_count(self, 0);
 
         /* [GENE_FIX:2026-04-26] Null the channel property NOW so its refcount
          * drops to 0 and any remaining PDO objects inside it are released within
          * the worker's lifetime, not at process exit. Critical when close() is
          * called from non-coroutine workerStop where channel drain was skipped. */
         zend_update_property_null(gene_pool_ce, gene_strip_obj(self),
             ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL));
     }
 }
 /* }}} */
  
 /*
  * {{{ public Gene\Pool::recycleIdle(): void (called by timer)
  */
 PHP_METHOD(gene_pool, recycleIdle)
 {
     pool_recycle_idle(getThis());
 }
 /* }}} */
  
 /*
  * {{{ public static Gene\Pool::closeAll(): void
  */
 PHP_METHOD(gene_pool, closeAll)
 {
     zval *instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
     if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
         zval *pool;
 
         /* Phase 1: mark ALL pools as closed and stop timers first.
          * This prevents new get() borrows during the shutdown sequence,
          * avoiding a race where pool A's close() yields (during channel drain)
          * and a coroutine borrows from pool B before B is closed. */
         ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(instances), pool) {
             if (Z_TYPE_P(pool) == IS_OBJECT && !pool_is_closed(pool)) {
                 zend_update_property_bool(gene_pool_ce, gene_strip_obj(pool),
                     ZEND_STRL(GENE_POOL_PROPERTY_CLOSED), 1);
                 pool_stop_timer(pool);
             }
         } ZEND_HASH_FOREACH_END();
 
         /* Phase 2: drain and close all pools (already marked closed). */
         ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(instances), pool) {
             if (Z_TYPE_P(pool) == IS_OBJECT) {
                 zval close_ret;
                 gene_factory_call(pool, "close", sizeof("close") - 1, NULL, &close_ret);
                 zval_ptr_dtor(&close_ret);
             }
         } ZEND_HASH_FOREACH_END();
     }
 
     /* Clear the static instances registry */
     zval empty_arr;
     array_init(&empty_arr);
     zend_update_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), &empty_arr);
     zval_ptr_dtor(&empty_arr);
 }
 /* }}} */
  
 /*
  * {{{ public static Gene\Pool::stopTimers(): void
  *
  * Lightweight method that only clears pool idle-recycler timers without
  * draining channels.  Designed to be called from Swoole's onWorkerExit
  * callback so the event loop can exit cleanly.  closeAll() (which also
  * drains channels) should still be called from onWorkerStop afterwards.
  */
 PHP_METHOD(gene_pool, stopTimers)
 {
     zval *instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
     if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
         zval *pool;
         ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(instances), pool) {
             if (Z_TYPE_P(pool) == IS_OBJECT) {
                 pool_stop_timer(pool);
             }
         } ZEND_HASH_FOREACH_END();
     }
 }
 /* }}} */
 
 /*
  * {{{ public Gene\Pool::stats(): array
  */
 PHP_METHOD(gene_pool, stats)
 {
     zval *self = getThis();
     zval *channel = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), 1, NULL);
     zend_long count = pool_get_count(self);
     zend_long idle = (channel && Z_TYPE_P(channel) == IS_OBJECT) ? pool_channel_length(channel) : 0;
  
     zend_long max = pool_get_max(self);
     zend_long overflow = (count > max) ? (count - max) : 0;
 
     array_init(return_value);
     add_assoc_long(return_value, "total", count);
     add_assoc_long(return_value, "idle", idle);
     add_assoc_long(return_value, "using", count - idle);
     add_assoc_long(return_value, "overflow", overflow);
     add_assoc_long(return_value, "min", pool_get_min(self));
     add_assoc_long(return_value, "max", max);
     add_assoc_bool(return_value, "closed", pool_is_closed(self));
 }
 /* }}} */
  
 /*
  * {{{ public Gene\Pool::__construct(array $config)
  */
 PHP_METHOD(gene_pool, __construct)
 {
     zval *config = NULL, *self = getThis();
     zval channel, atomic, timer_id;

     if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &config) == FAILURE) {
         return;
     }

     /* Initialize properties from config options */
     zend_long min = 1, max = 64, idle_time = 60;
     double wait_time = 3.0;

     zval *opt_min = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("min"));
     if (opt_min && Z_TYPE_P(opt_min) == IS_LONG) {
         min = Z_LVAL_P(opt_min);
     }

     zval *opt_max = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("max"));
     if (opt_max && Z_TYPE_P(opt_max) == IS_LONG) {
         max = Z_LVAL_P(opt_max);
     }

     zval *opt_idle = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("idleTimeout"));
     if (opt_idle && Z_TYPE_P(opt_idle) == IS_LONG) {
         idle_time = Z_LVAL_P(opt_idle);
     }

     zval *opt_wait = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("waitTimeout"));
     if (opt_wait) {
         if (Z_TYPE_P(opt_wait) == IS_DOUBLE) {
             wait_time = Z_DVAL_P(opt_wait);
         } else if (Z_TYPE_P(opt_wait) == IS_LONG) {
             wait_time = (double)Z_LVAL_P(opt_wait);
         }
     }

     /* Store config */
     zend_update_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CONFIG), config);

     /* Create Swoole\Coroutine\Channel for connection queue */
     static zend_string *channel_key = NULL;
     if (UNEXPECTED(!channel_key)) {
         channel_key = zend_string_init_interned(ZEND_STRL("Swoole\\Coroutine\\Channel"), 1);
     }
     zend_class_entry *channel_ce = zend_lookup_class(channel_key);
     if (channel_ce) {
         zval params[1];
         ZVAL_LONG(&params[0], max);
         object_init_ex(&channel, channel_ce);
         zend_call_known_function(channel_ce->constructor, Z_OBJ(channel), channel_ce, &timer_id, 1, params, NULL);
         zend_update_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), &channel);
         zval_ptr_dtor(&channel);
     }

     /* Create Swoole\Atomic for connection count */
     static zend_string *atomic_key = NULL;
     if (UNEXPECTED(!atomic_key)) {
         atomic_key = zend_string_init_interned(ZEND_STRL("Swoole\\Atomic"), 1);
     }
     zend_class_entry *atomic_ce = zend_lookup_class(atomic_key);
     if (atomic_ce) {
         zval params[1];
         ZVAL_LONG(&params[0], 0);
         object_init_ex(&atomic, atomic_ce);
         zend_call_known_function(atomic_ce->constructor, Z_OBJ(atomic), atomic_ce, &timer_id, 1, params, NULL);
         zend_update_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), &atomic);
         zval_ptr_dtor(&atomic);
     }

     /* Store numeric properties */
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_MIN), min);
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_MAX), max);
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_IDLE_TIME), idle_time);
     zend_update_property_double(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_WAIT_TIME), wait_time);
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 0);
     zend_update_property_bool(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CLOSED), 0);
 }
 /* }}} */

 /*
  * {{{ public Gene\Pool::__destruct()
  */
 PHP_METHOD(gene_pool, __destruct)
 {
     zval *self = getThis();
     if (!pool_is_closed(self)) {
         zval close_ret;
         gene_factory_call(self, "close", sizeof("close") - 1, NULL, &close_ret);
         zval_ptr_dtor(&close_ret);
     }
 }
 /* }}} */

 /*
  * {{{ public static Gene\Pool::create(string $name, string $configKey, array $options = []): self
  */
 PHP_METHOD(gene_pool, create)
 {
    zend_string *name = NULL, *configKey = NULL;
    zval *options = NULL;
    zval *instances, pool_config, *config_data;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|a!", &name, &configKey, &options) == FAILURE) {
        return;
    }

    /* Build pool config from options */
    array_init(&pool_config);
    if (options && Z_TYPE_P(options) == IS_ARRAY) {
        zval *opt;
        if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("min"))) != NULL) {
            Z_TRY_ADDREF_P(opt);
            add_assoc_zval(&pool_config, "min", opt);
        }
        if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("max"))) != NULL) {
            Z_TRY_ADDREF_P(opt);
            add_assoc_zval(&pool_config, "max", opt);
        }
        if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("idleTimeout"))) != NULL) {
            Z_TRY_ADDREF_P(opt);
            add_assoc_zval(&pool_config, "idleTimeout", opt);
        }
        if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("waitTimeout"))) != NULL) {
            Z_TRY_ADDREF_P(opt);
            add_assoc_zval(&pool_config, "waitTimeout", opt);
        }
    }

    /* Read DB config from persistent cache using configKey */
    char cache_key[256];
    size_t cache_key_len = ZSTR_LEN(configKey) + sizeof(GENE_CONFIG_CACHE) - 1;
    if (cache_key_len < sizeof(cache_key)) {
        memcpy(cache_key, ZSTR_VAL(configKey), ZSTR_LEN(configKey));
        memcpy(cache_key + ZSTR_LEN(configKey), GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE) - 1);
        config_data = gene_memory_get(cache_key, cache_key_len);
        if (config_data && Z_TYPE_P(config_data) == IS_ARRAY) {
            zval *params = zend_hash_str_find(Z_ARRVAL_P(config_data), ZEND_STRL("params"));
            if (params && Z_TYPE_P(params) == IS_ARRAY) {
                zval *first_param = zend_hash_index_find(Z_ARRVAL_P(params), 0);
                if (first_param && Z_TYPE_P(first_param) == IS_ARRAY) {
                    zval *dsn = zend_hash_index_find(Z_ARRVAL_P(first_param), 0);
                    zval *username = zend_hash_index_find(Z_ARRVAL_P(first_param), 1);
                    zval *password = zend_hash_index_find(Z_ARRVAL_P(first_param), 2);
                    zval *db_options = zend_hash_index_find(Z_ARRVAL_P(first_param), 3);

                    if (dsn && Z_TYPE_P(dsn) == IS_STRING) {
                        zend_string_addref(Z_STR_P(dsn));
                        add_assoc_str_ex(&pool_config, ZEND_STRL("dsn"), Z_STR_P(dsn));
                    }
                    if (username && Z_TYPE_P(username) == IS_STRING) {
                        zend_string_addref(Z_STR_P(username));
                        add_assoc_str_ex(&pool_config, ZEND_STRL("username"), Z_STR_P(username));
                    }
                    if (password && Z_TYPE_P(password) == IS_STRING) {
                        zend_string_addref(Z_STR_P(password));
                        add_assoc_str_ex(&pool_config, ZEND_STRL("password"), Z_STR_P(password));
                    }
                    if (db_options && Z_TYPE_P(db_options) == IS_ARRAY) {
                        Z_TRY_ADDREF_P(db_options);
                        add_assoc_zval(&pool_config, "options", db_options);
                    }
                }
            }
        }
    }

    object_init_ex(return_value, gene_pool_ce);
    {
        zval ctor_params[1], ctor_ret;
        ZVAL_COPY_VALUE(&ctor_params[0], &pool_config);
        ZVAL_UNDEF(&ctor_ret);
        zend_call_known_function(gene_pool_ce->constructor,
            Z_OBJ_P(return_value), gene_pool_ce, &ctor_ret,
            1, ctor_params, NULL);
        if (!Z_ISUNDEF(ctor_ret)) {
            zval_ptr_dtor(&ctor_ret);
        }
    }
    zval_ptr_dtor(&pool_config);

    /* Register in instances static property */
    instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
    if (!instances || Z_TYPE_P(instances) != IS_ARRAY) {
        zval new_instances;
        array_init(&new_instances);
        zend_update_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), &new_instances);
        zval_ptr_dtor(&new_instances);
        instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
    }
    if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
        Z_TRY_ADDREF_P(return_value);
        zend_hash_update(Z_ARRVAL_P(instances), name, return_value);
    }

    /* Fill pool to minimum size */
    pool_fill(return_value);
 }
 /* }}} */

 /*
  * {{{ public static Gene\Pool::getInstance(string $name): ?self
  */
 PHP_METHOD(gene_pool, getInstance)
 {
    zend_string *name = NULL;
    zval *instances;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
        return;
    }

    instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
    if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
        zval *pool = zend_hash_find(Z_ARRVAL_P(instances), name);
        if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
            RETURN_ZVAL(pool, 1, 0);
        }
    }

    RETURN_NULL();
 }
 /* }}} */

 /* ====================== Shared DB pool helpers ====================== */
 
 bool gene_pool_get_pdo(zend_class_entry *db_ce, zval *self, zval *config,
     const char *pool_key, size_t pool_key_len,
     const char *pdo_key, size_t pdo_key_len)
 {
     zval *pool_name = NULL;
  
     if (config == NULL || Z_TYPE_P(config) != IS_ARRAY) {
         return 0;
     }
  
     pool_name = zend_hash_str_find(Z_ARRVAL_P(config), pool_key, pool_key_len);
     if (!pool_name || Z_TYPE_P(pool_name) != IS_STRING || GENE_G(runtime_type) < 2) {
         return 0;
     }
 
     /* [GENE_PERF:2026-04-26] Direct method dispatch via cached zend_function*
      * — avoids array_init+add_next_index_string×2+call_user_function and the
      * gene_factory_call name lookup on every DB query. */
     zend_function *fn_gi = pool_method(&fn_pool_getinstance, ZEND_STRL("getinstance"));
     if (UNEXPECTED(!fn_gi)) {
         return 0;
     }
 
     zval pool_obj, gi_args[1];
     ZVAL_COPY(&gi_args[0], pool_name);
     ZVAL_UNDEF(&pool_obj);
     zend_call_known_function(fn_gi, NULL, gene_pool_ce, &pool_obj, 1, gi_args, NULL);
     zval_ptr_dtor(&gi_args[0]);
 
     if (Z_TYPE(pool_obj) == IS_OBJECT) {
         zend_function *fn_get = pool_method(&fn_pool_get, ZEND_STRL("get"));
         zval pdo_obj;
         ZVAL_UNDEF(&pdo_obj);
         if (EXPECTED(fn_get)) {
             zend_call_known_function(fn_get, Z_OBJ(pool_obj), gene_pool_ce, &pdo_obj, 0, NULL, NULL);
         }
 
         if (Z_TYPE(pdo_obj) == IS_OBJECT) {
             zend_update_property(db_ce, gene_strip_obj(self), pool_key, pool_key_len, &pool_obj);
             zend_update_property(db_ce, gene_strip_obj(self), pdo_key, pdo_key_len, &pdo_obj);
             zval_ptr_dtor(&pdo_obj);
             zval_ptr_dtor(&pool_obj);
             return 1;
         }
         if (!Z_ISUNDEF(pdo_obj)) zval_ptr_dtor(&pdo_obj);
     }
     zval_ptr_dtor(&pool_obj);
     return 0;
 }
  
 void gene_pool_return_pdo(zend_class_entry *db_ce, zval *self,
     const char *pool_key, size_t pool_key_len,
     const char *pdo_key, size_t pdo_key_len)
 {
     zval *pool = zend_read_property(db_ce, gene_strip_obj(self), pool_key, pool_key_len, 1, NULL);
     if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
         zval *pdo = zend_read_property(db_ce, gene_strip_obj(self), pdo_key, pdo_key_len, 1, NULL);
         if (pdo && Z_TYPE_P(pdo) == IS_OBJECT) {
             zval pdo_copy;
             ZVAL_COPY(&pdo_copy, pdo);
             /* Null the property BEFORE put() to prevent re-use during
              * coroutine yield inside channel->push() */
             zend_update_property_null(db_ce, gene_strip_obj(self), pdo_key, pdo_key_len);
 
             /* [GENE_PERF:2026-04-26] Direct call via cached fn ptr. */
             zend_function *fn_put = pool_method(&fn_pool_put, ZEND_STRL("put"));
             zval retval, args[1];
             ZVAL_COPY_VALUE(&args[0], &pdo_copy);
             ZVAL_UNDEF(&retval);
             if (EXPECTED(fn_put)) {
                 zend_call_known_function(fn_put, Z_OBJ_P(pool), gene_pool_ce, &retval, 1, args, NULL);
             }
             zval_ptr_dtor(&pdo_copy);
             if (!Z_ISUNDEF(retval)) zval_ptr_dtor(&retval);
         } else {
             zend_update_property_null(db_ce, gene_strip_obj(self), pdo_key, pdo_key_len);
         }
     }
 }
  
 void gene_pool_notify_remove(zend_class_entry *db_ce, zval *self,
     const char *pool_key, size_t pool_key_len)
 {
     zval *pool = zend_read_property(db_ce, gene_strip_obj(self), pool_key, pool_key_len, 1, NULL);
     if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
         /* [GENE_PERF:2026-04-26] Direct call via cached fn ptr. */
         zend_function *fn_rm = pool_method(&fn_pool_remove, ZEND_STRL("remove"));
         zval retval;
         ZVAL_UNDEF(&retval);
         if (EXPECTED(fn_rm)) {
             zend_call_known_function(fn_rm, Z_OBJ_P(pool), gene_pool_ce, &retval, 0, NULL, NULL);
         }
         if (!Z_ISUNDEF(retval)) zval_ptr_dtor(&retval);
     }
 }
  
 /* ====================== Method table ====================== */
  
 const zend_function_entry gene_pool_methods[] = {
     PHP_ME(gene_pool, __construct, gene_pool_construct_arginfo, ZEND_ACC_PUBLIC)
     PHP_ME(gene_pool, __destruct, gene_pool_void_arginfo, ZEND_ACC_PUBLIC)
     PHP_ME(gene_pool, create, gene_pool_create_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
     PHP_ME(gene_pool, getInstance, gene_pool_get_instance_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
     PHP_ME(gene_pool, get, gene_pool_void_arginfo, ZEND_ACC_PUBLIC)
     PHP_ME(gene_pool, put, gene_pool_put_arginfo, ZEND_ACC_PUBLIC)
     PHP_ME(gene_pool, remove, gene_pool_void_arginfo, ZEND_ACC_PUBLIC)
     PHP_ME(gene_pool, close, gene_pool_void_arginfo, ZEND_ACC_PUBLIC)
     PHP_ME(gene_pool, closeAll, gene_pool_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
     PHP_ME(gene_pool, stopTimers, gene_pool_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
     PHP_ME(gene_pool, recycleIdle, gene_pool_void_arginfo, ZEND_ACC_PUBLIC)
     PHP_ME(gene_pool, stats, gene_pool_void_arginfo, ZEND_ACC_PUBLIC)
     {NULL, NULL, NULL}
 };
  
 /* ====================== MINIT ====================== */
  
 GENE_MINIT_FUNCTION(pool)
 {
     zend_class_entry gene_pool;
     GENE_INIT_CLASS_ENTRY(gene_pool, "Gene_Pool", "Gene\\Pool", gene_pool_methods);
     gene_pool_ce = zend_register_internal_class_ex(&gene_pool, NULL);
     gene_pool_ce->ce_flags |= ZEND_ACC_FINAL;
 #if PHP_VERSION_ID >= 80200
     gene_pool_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
 #endif
  
     zend_declare_property_null(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), ZEND_ACC_PROTECTED);
     zend_declare_property_null(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_CONFIG), ZEND_ACC_PROTECTED);
     zend_declare_property_long(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_MIN), 1, ZEND_ACC_PROTECTED);
     zend_declare_property_long(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_MAX), 64, ZEND_ACC_PROTECTED);
     zend_declare_property_long(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_IDLE_TIME), 60, ZEND_ACC_PROTECTED);
     zend_declare_property_double(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_WAIT_TIME), 3.0, ZEND_ACC_PROTECTED);
     zend_declare_property_null(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_COUNT), ZEND_ACC_PROTECTED);
     zend_declare_property_long(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 0, ZEND_ACC_PROTECTED);
     zend_declare_property_bool(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_CLOSED), 0, ZEND_ACC_PROTECTED);
  
     /* Static property: instances registry */
     zend_declare_property_null(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
  
     return SUCCESS;
 }