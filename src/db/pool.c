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
  
 #include "../gene.h"
 #include "../factory/factory.h"
 #include "../config/configs.h"
 #include "../cache/memory.h"
 #include "../db/pool.h"
  
 zend_class_entry *gene_pool_ce;
 
 /* Forward declarations */
 static void pool_stop_timer(zval *self);
  
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
  
     zend_string *pdo_key = zend_string_init(ZEND_STRL("PDO"), 0);
     zend_class_entry *pdo_ce = zend_lookup_class(pdo_key);
     zend_string_release(pdo_key);
  
     if (!pdo_ce) {
         return;
     }
  
     zval pdo_object, option, func_name, construct_ret;
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
  
     ZVAL_STRING(&func_name, "__construct");
     {
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
         call_user_function(NULL, &pdo_object, &func_name, &construct_ret, 4, params);
         zval_ptr_dtor(&params[0]);
         zval_ptr_dtor(&params[1]);
         zval_ptr_dtor(&params[2]);
         zval_ptr_dtor(&params[3]);
         if (!Z_ISUNDEF(construct_ret)) {
             zval_ptr_dtor(&construct_ret);
         }
     }
     zval_ptr_dtor(&func_name);
     zval_ptr_dtor(&option);
  
     if (EG(exception)) {
         zend_clear_exception();
         zval_ptr_dtor(&pdo_object);
         return;
     }
  
     ZVAL_COPY_VALUE(retval, &pdo_object);
 }
  
 static bool pool_is_alive(zval *pdo) {
     zval func_name, retval;
     ZVAL_STRING(&func_name, "getAttribute");
     zval params[1];
     /* PDO::ATTR_SERVER_INFO = 6 */
     ZVAL_LONG(&params[0], 6);
     ZVAL_UNDEF(&retval);
     call_user_function(NULL, pdo, &func_name, &retval, 1, params);
     zval_ptr_dtor(&func_name);
  
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
     zval func_name, retval, item;
     bool success;
     array_init(&item);
     Z_TRY_ADDREF_P(pdo);
     add_assoc_zval(&item, "conn", pdo);
     add_assoc_long(&item, "lastUsed", (zend_long)time(NULL));
  
     ZVAL_STRING(&func_name, "push");
     zval params[1];
     ZVAL_COPY_VALUE(&params[0], &item);
     call_user_function(NULL, channel, &func_name, &retval, 1, params);
     zval_ptr_dtor(&func_name);
     success = (Z_TYPE(retval) == IS_TRUE);
     if (!Z_ISUNDEF(retval)) {
         zval_ptr_dtor(&retval);
     }
     zval_ptr_dtor(&item);
     return success;
 }
  
 static bool pool_channel_pop(zval *channel, double timeout, zval *result) {
     zval func_name, retval;
     ZVAL_STRING(&func_name, "pop");
     zval params[1];
     ZVAL_DOUBLE(&params[0], timeout);
     ZVAL_UNDEF(&retval);
     call_user_function(NULL, channel, &func_name, &retval, 1, params);
     zval_ptr_dtor(&func_name);
  
     if (Z_TYPE(retval) == IS_FALSE || Z_TYPE(retval) == IS_NULL) {
         zval_ptr_dtor(&retval);
         ZVAL_NULL(result);
         return 0;
     }
     ZVAL_COPY_VALUE(result, &retval);
     return 1;
 }
  
 static bool pool_channel_is_empty(zval *channel) {
     zval func_name, retval;
     ZVAL_STRING(&func_name, "isEmpty");
     ZVAL_UNDEF(&retval);
     if (call_user_function(NULL, channel, &func_name, &retval, 0, NULL) != SUCCESS) {
         zval_ptr_dtor(&func_name);
         return true;
     }
     zval_ptr_dtor(&func_name);
     bool empty = (Z_TYPE(retval) == IS_TRUE);
     zval_ptr_dtor(&retval);
     return empty;
 }
  
 static bool pool_channel_is_full(zval *channel) {
     zval func_name, retval;
     ZVAL_STRING(&func_name, "isFull");
     ZVAL_UNDEF(&retval);
     if (call_user_function(NULL, channel, &func_name, &retval, 0, NULL) != SUCCESS) {
         zval_ptr_dtor(&func_name);
         return true;
     }
     zval_ptr_dtor(&func_name);
     bool full = (Z_TYPE(retval) == IS_TRUE);
     zval_ptr_dtor(&retval);
     return full;
 }
  
 static zend_long pool_channel_length(zval *channel) {
     zval func_name, retval;
     ZVAL_STRING(&func_name, "length");
     ZVAL_UNDEF(&retval);
     if (call_user_function(NULL, channel, &func_name, &retval, 0, NULL) != SUCCESS) {
         zval_ptr_dtor(&func_name);
         return 0;
     }
     zval_ptr_dtor(&func_name);
     zend_long len = (Z_TYPE(retval) == IS_LONG) ? Z_LVAL(retval) : 0;
     zval_ptr_dtor(&retval);
     return len;
 }
  
 static void pool_atomic_call(zval *atomic, const char *method, zend_long arg, zval *retval) {
     zval func_name, params[1], ret_local;
     if (!retval) retval = &ret_local;
     ZVAL_STRING(&func_name, method);
     ZVAL_LONG(&params[0], arg);
     ZVAL_UNDEF(retval);
     call_user_function(NULL, atomic, &func_name, retval, 1, params);
     zval_ptr_dtor(&func_name);
     if (!retval || retval == &ret_local) {
         if (!Z_ISUNDEF(ret_local)) zval_ptr_dtor(&ret_local);
     }
 }
 
 static void pool_decrement_count(zval *self) {
     zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
     if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
         zval ret;
         ZVAL_UNDEF(&ret);
         pool_atomic_call(atomic, "get", 0, &ret);
         zend_long val = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : 0;
         if (!Z_ISUNDEF(ret)) zval_ptr_dtor(&ret);
         if (val > 0) {
             pool_atomic_call(atomic, "sub", 1, NULL);
         }
     }
 }
  
 static zend_long pool_get_count(zval *self) {
     zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
     if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
         zval ret;
         ZVAL_UNDEF(&ret);
         pool_atomic_call(atomic, "get", 0, &ret);
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
         pool_atomic_call(atomic, "add", 1, NULL);
     }
 }
 
 static void pool_set_count(zval *self, zend_long val) {
     zval *atomic = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 1, NULL);
     if (atomic && Z_TYPE_P(atomic) == IS_OBJECT) {
         pool_atomic_call(atomic, "set", val, NULL);
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
             pool_increment_count(self);
             pool_channel_push(channel, &conn);
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
     idle_zv = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_IDLE_TIME), 1, NULL);
     idle_timeout = (idle_zv && Z_TYPE_P(idle_zv) == IS_LONG) ? Z_LVAL_P(idle_zv) : 60;
  
     if (!channel || Z_TYPE_P(channel) != IS_OBJECT) return;
  
     now = (zend_long)time(NULL);
     size = pool_channel_length(channel);
  
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
  
         last_used_zv = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("lastUsed"));
         zend_long last_used = (last_used_zv && Z_TYPE_P(last_used_zv) == IS_LONG) ? Z_LVAL_P(last_used_zv) : now;
  
         conn_zv = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("conn"));
  
         if (pool_get_count(self) > pool_get_min(self) && (now - last_used) > idle_timeout) {
             /* Discard idle connection */
             pool_decrement_count(self);
             zval_ptr_dtor(&item);
             continue;
         }
  
         /* Connection is still needed - check alive and push back immediately */
         if (conn_zv && Z_TYPE_P(conn_zv) == IS_OBJECT) {
             if (pool_is_alive(conn_zv)) {
                 pool_channel_push(channel, conn_zv);
             } else {
                 pool_decrement_count(self);
             }
         }
         zval_ptr_dtor(&item);
     }
  
     /* Refill to minimum */
     while (pool_get_count(self) < pool_get_min(self) && !pool_channel_is_full(channel)) {
         zval conn;
         pool_create_connection(self, &conn);
         if (Z_TYPE(conn) == IS_OBJECT) {
             pool_increment_count(self);
             pool_channel_push(channel, &conn);
             zval_ptr_dtor(&conn);
         } else {
             break;
         }
     }
 }
  
 /* Timer callback - called from PHP userland via Swoole\Timer */
 static void pool_start_idle_recycler(zval *self) {
     zval *idle_zv = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_IDLE_TIME), 1, NULL);
     zend_long idle_timeout = (idle_zv && Z_TYPE_P(idle_zv) == IS_LONG) ? Z_LVAL_P(idle_zv) : 60;
  
     if (idle_timeout <= 0) return;
  
     /* We store the pool object in a closure and use Swoole\Timer::tick */
     zend_long interval_ms = idle_timeout * 500; /* half the idle timeout */
     if (interval_ms < 1000) interval_ms = 1000;
  
     /* Build: Swoole\Timer::tick($interval, function() use ($self) { $self->recycleIdle(); }) */
     zval callable, timer_ret;
     array_init(&callable);
     add_next_index_string(&callable, "Swoole\\Timer");
     add_next_index_string(&callable, "tick");
  
     /* Create closure: [$self, 'recycleIdle'] */
     zval callback;
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
         zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), Z_LVAL(timer_ret));
     }
     if (!Z_ISUNDEF(timer_ret)) {
         zval_ptr_dtor(&timer_ret);
     }
 }
  
 static bool pool_in_coroutine(void) {
     return gene_get_coroutine_id() >= 0;
 }
 
 static void pool_stop_timer(zval *self) {
     zval *timer_id = zend_read_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 1, NULL);
     if (timer_id && Z_TYPE_P(timer_id) == IS_LONG && Z_LVAL_P(timer_id) > 0) {
         zval callable, retval;
         array_init(&callable);
         add_next_index_string(&callable, "Swoole\\Timer");
         add_next_index_string(&callable, "clear");
 
         zval params[1];
         ZVAL_LONG(&params[0], Z_LVAL_P(timer_id));
         ZVAL_UNDEF(&retval);
 
         if (call_user_function(NULL, NULL, &callable, &retval, 1, params) != SUCCESS) {
             zval_ptr_dtor(&callable);
             return;
         }
         zval_ptr_dtor(&callable);
         if (!Z_ISUNDEF(retval)) {
             zval_ptr_dtor(&retval);
         }
 
         zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 0);
     }
 }
  
 /* ====================== PHP Methods ====================== */
  
 /*
  * {{{ public Gene\Pool::__construct(array $config)
  */
 PHP_METHOD(gene_pool, __construct)
 {
     zval *config = NULL, *self = getThis();
     zend_long min_val, max_val, idle_val;
     double wait_val;
  
     if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &config) == FAILURE) {
         return;
     }
  
     zval *min_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("min"));
     zval *max_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("max"));
     zval *idle_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("idleTimeout"));
     zval *wait_zv = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("waitTimeout"));
  
     min_val = (min_zv && Z_TYPE_P(min_zv) == IS_LONG) ? Z_LVAL_P(min_zv) : 1;
     max_val = (max_zv && Z_TYPE_P(max_zv) == IS_LONG) ? Z_LVAL_P(max_zv) : 10;
     idle_val = (idle_zv && Z_TYPE_P(idle_zv) == IS_LONG) ? Z_LVAL_P(idle_zv) : 60;
     if (wait_zv) {
         if (Z_TYPE_P(wait_zv) == IS_DOUBLE) wait_val = Z_DVAL_P(wait_zv);
         else if (Z_TYPE_P(wait_zv) == IS_LONG) wait_val = (double)Z_LVAL_P(wait_zv);
         else wait_val = 3.0;
     } else {
         wait_val = 3.0;
     }
  
     if (min_val < 0) min_val = 0;
     if (max_val < 1) max_val = 1;
     if (min_val > max_val) min_val = max_val;
     if (idle_val < 0) idle_val = 0;
  
     zend_update_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CONFIG), config);
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_MIN), min_val);
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_MAX), max_val);
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_IDLE_TIME), idle_val);
     zend_update_property_double(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_WAIT_TIME), wait_val);
     /* Create Swoole\Atomic for thread-safe counting */
     {
         zend_string *atomic_str = zend_string_init(ZEND_STRL("Swoole\\Atomic"), 0);
         zend_class_entry *atomic_ce = zend_lookup_class(atomic_str);
         zend_string_release(atomic_str);
         if (atomic_ce) {
             zval atomic_obj, atomic_func, atomic_ret;
             object_init_ex(&atomic_obj, atomic_ce);
             ZVAL_STRING(&atomic_func, "__construct");
             zval atomic_params[1];
             ZVAL_LONG(&atomic_params[0], 0);
             ZVAL_UNDEF(&atomic_ret);
             call_user_function(NULL, &atomic_obj, &atomic_func, &atomic_ret, 1, atomic_params);
             zval_ptr_dtor(&atomic_func);
             if (!Z_ISUNDEF(atomic_ret)) zval_ptr_dtor(&atomic_ret);
             zend_update_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), &atomic_obj);
             zval_ptr_dtor(&atomic_obj);
         } else {
             php_error_docref(NULL, E_WARNING, "Gene\\Pool: Swoole\\Atomic not found, pool counting may be unsafe under concurrency.");
             zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_COUNT), 0);
         }
     }
     zend_update_property_long(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 0);
     zend_update_property_bool(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CLOSED), 0);
  
     /* Create Swoole\Coroutine\Channel */
     {
         zend_string *ch_class_str = zend_string_init(ZEND_STRL("Swoole\\Coroutine\\Channel"), 0);
         zend_class_entry *ch_ce = zend_lookup_class(ch_class_str);
         zend_string_release(ch_class_str);
  
         if (!ch_ce) {
             php_error_docref(NULL, E_WARNING, "Gene\\Pool requires Swoole extension (Swoole\\Coroutine\\Channel not found).");
             return;
         }
  
         zval channel;
         object_init_ex(&channel, ch_ce);
  
         zval ch_func, ch_ret;
         ZVAL_STRING(&ch_func, "__construct");
         zval ch_params[1];
         ZVAL_LONG(&ch_params[0], max_val);
         call_user_function(NULL, &channel, &ch_func, &ch_ret, 1, ch_params);
         zval_ptr_dtor(&ch_func);
         if (!Z_ISUNDEF(ch_ret)) {
             zval_ptr_dtor(&ch_ret);
         }
  
         zend_update_property(gene_pool_ce, gene_strip_obj(self), ZEND_STRL(GENE_POOL_PROPERTY_CHANNEL), &channel);
         zval_ptr_dtor(&channel);
     }
  
     /* Pre-fill min connections */
     pool_fill(self);
  
     /* Start idle recycler timer */
     if (idle_val > 0) {
         pool_start_idle_recycler(self);
     }
 }
 /* }}} */
  
 /*
  * {{{ public static Gene\Pool::create(string $name, string $configKey, array $options = []): self
  *
  * Reads DB connection info (dsn, username, password, options) from the
  * persistent config cache using $configKey (e.g. 'db'), following the
  * same lookup path as Gene\Di.  The $options parameter is optional and
  * overrides pool-specific defaults (min, max, idleTimeout, waitTimeout).
  */
 PHP_METHOD(gene_pool, create)
 {
     zend_string *name;
     char *config_key;
     size_t config_key_len;
     zval *options = NULL;
     zval *instances;
 
     if (zend_parse_parameters(ZEND_NUM_ARGS(), "Ss|a", &name, &config_key, &config_key_len, &options) == FAILURE) {
         return;
     }
 
     /* --- Read DB config from persistent cache (same path as Gene\Config) --- */
     char *router_e = NULL;
     size_t router_e_len = 0;
     if (GENE_G(app_key) && strlen(GENE_G(app_key)) > 0) {
         router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(app_key), GENE_CONFIG_CACHE);
     } else if (GENE_G(app_root) && strlen(GENE_G(app_root)) > 0) {
         router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(app_root), GENE_CONFIG_CACHE);
     } else {
         router_e_len = spprintf(&router_e, 0, "%s", GENE_CONFIG_CACHE);
     }
 
     zval *di_config = gene_memory_get_by_config(router_e, router_e_len, config_key);
     efree(router_e);
     router_e = NULL;
 
     if (!di_config || Z_TYPE_P(di_config) != IS_ARRAY) {
         php_error_docref(NULL, E_WARNING,
             "Gene\\Pool::create(): config key '%s' not found in config cache", config_key);
         RETURN_NULL();
     }
 
     /* Expect di_config = ['class'=>..., 'params'=>[[dsn,username,password,...]], ...] */
     zval *params_zv = zend_hash_str_find(Z_ARRVAL_P(di_config), ZEND_STRL("params"));
     if (!params_zv || Z_TYPE_P(params_zv) != IS_ARRAY) {
         php_error_docref(NULL, E_WARNING,
             "Gene\\Pool::create(): config key '%s' has no 'params' array", config_key);
         RETURN_NULL();
     }
 
     zval *db_params = zend_hash_index_find(Z_ARRVAL_P(params_zv), 0);
     if (!db_params || Z_TYPE_P(db_params) != IS_ARRAY) {
         php_error_docref(NULL, E_WARNING,
             "Gene\\Pool::create(): config key '%s' params[0] is not an array", config_key);
         RETURN_NULL();
     }
 
     /* --- Build merged config array for __construct --- */
     zval merged_config;
     array_init(&merged_config);
 
     /* Copy dsn, username, password from persistent cache (creates local copies) */
     {
         zval *dsn = zend_hash_str_find(Z_ARRVAL_P(db_params), ZEND_STRL("dsn"));
         if (dsn && Z_TYPE_P(dsn) == IS_STRING) {
             add_assoc_stringl(&merged_config, "dsn", Z_STRVAL_P(dsn), Z_STRLEN_P(dsn));
         }
         zval *user = zend_hash_str_find(Z_ARRVAL_P(db_params), ZEND_STRL("username"));
         if (user && Z_TYPE_P(user) == IS_STRING) {
             add_assoc_stringl(&merged_config, "username", Z_STRVAL_P(user), Z_STRLEN_P(user));
         }
         zval *pass = zend_hash_str_find(Z_ARRVAL_P(db_params), ZEND_STRL("password"));
         if (pass && Z_TYPE_P(pass) == IS_STRING) {
             add_assoc_stringl(&merged_config, "password", Z_STRVAL_P(pass), Z_STRLEN_P(pass));
         }
         zval *db_opts = zend_hash_str_find(Z_ARRVAL_P(db_params), ZEND_STRL("options"));
         if (db_opts && Z_TYPE_P(db_opts) == IS_ARRAY) {
             zval opts_local;
             gene_memory_zval_local(&opts_local, db_opts);
             add_assoc_zval(&merged_config, "options", &opts_local);
         }
     }
 
     /* Apply pool options from $options, falling back to defaults */
     {
         zend_long min_val = 1, max_val = 10, idle_val = 60;
         double wait_val = 3.0;
         zval *opt;
 
         if (options) {
             if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("min"))) != NULL && Z_TYPE_P(opt) == IS_LONG) {
                 min_val = Z_LVAL_P(opt);
             }
             if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("max"))) != NULL && Z_TYPE_P(opt) == IS_LONG) {
                 max_val = Z_LVAL_P(opt);
             }
             if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("idleTimeout"))) != NULL && Z_TYPE_P(opt) == IS_LONG) {
                 idle_val = Z_LVAL_P(opt);
             }
             if ((opt = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("waitTimeout"))) != NULL) {
                 if (Z_TYPE_P(opt) == IS_DOUBLE) wait_val = Z_DVAL_P(opt);
                 else if (Z_TYPE_P(opt) == IS_LONG) wait_val = (double)Z_LVAL_P(opt);
             }
         }
 
         add_assoc_long(&merged_config, "min", min_val);
         add_assoc_long(&merged_config, "max", max_val);
         add_assoc_long(&merged_config, "idleTimeout", idle_val);
         add_assoc_double(&merged_config, "waitTimeout", wait_val);
     }
 
     /* --- Create / register pool (same logic as before) --- */
     instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
 
     /* Close existing pool with same name if any */
     if (instances && Z_TYPE_P(instances) == IS_ARRAY) {
         zval *existing = zend_hash_find(Z_ARRVAL_P(instances), name);
         if (existing && Z_TYPE_P(existing) == IS_OBJECT) {
             zval close_ret;
             gene_factory_call(existing, "close", NULL, &close_ret);
             zval_ptr_dtor(&close_ret);
         }
     } else {
         zval arr;
         array_init(&arr);
         zend_update_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), &arr);
         zval_ptr_dtor(&arr);
         instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
     }
 
     /* Create new pool object */
     zval new_pool, params_arr;
     object_init_ex(&new_pool, gene_pool_ce);
 
     array_init(&params_arr);
     Z_TRY_ADDREF(merged_config);
     add_next_index_zval(&params_arr, &merged_config);
 
     zval construct_ret;
     gene_factory_call(&new_pool, "__construct", &params_arr, &construct_ret);
     zval_ptr_dtor(&construct_ret);
     zval_ptr_dtor(&params_arr);
     zval_ptr_dtor(&merged_config);
 
     /* Register in static instances */
     Z_TRY_ADDREF(new_pool);
     zend_hash_update(Z_ARRVAL_P(instances), name, &new_pool);
 
     RETURN_ZVAL(&new_pool, 1, 1);
 }
 /* }}} */
  
 /*
  * {{{ public static Gene\Pool::getInstance(string $name): ?self
  */
 PHP_METHOD(gene_pool, getInstance)
 {
     zend_string *name;
  
     if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
         RETURN_NULL();
     }
  
     zval *instances = zend_read_static_property(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), 1);
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
  * {{{ public Gene\Pool::get(): ?PDO
  */
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
                     zval *conn = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("conn"));
                     if (conn && Z_TYPE_P(conn) == IS_OBJECT) {
                         if (pool_is_alive(conn)) {
                             RETVAL_ZVAL(conn, 1, 0);
                             zval_ptr_dtor(&item);
                             return;
                         }
                         /* Dead connection, discard */
                         pool_decrement_count(self);
                     }
                 }
                 zval_ptr_dtor(&item);
                 retries++;
                 continue;
             }
         }
  
         /* 2. Below max: optimistically increment count BEFORE creating.
          *    This prevents two coroutines from both seeing count<max and
          *    both creating, which would exceed max. */
         if (pool_get_count(self) < pool_get_max(self)) {
             pool_increment_count(self);
             zval conn;
             pool_create_connection(self, &conn);
             if (Z_TYPE(conn) == IS_OBJECT) {
                 RETURN_ZVAL(&conn, 0, 0);
             }
             /* Creation failed — roll back the optimistic increment */
             pool_decrement_count(self);
             retries++;
             continue;
         }
  
         /* 3. At max, wait for return */
         {
             zval item;
             if (pool_channel_pop(channel, pool_get_wait_timeout(self), &item)) {
                 if (Z_TYPE(item) == IS_ARRAY) {
                     zval *conn = zend_hash_str_find(Z_ARRVAL(item), ZEND_STRL("conn"));
                     if (conn && Z_TYPE_P(conn) == IS_OBJECT) {
                         if (pool_is_alive(conn)) {
                             RETVAL_ZVAL(conn, 1, 0);
                             zval_ptr_dtor(&item);
                             return;
                         }
                         pool_decrement_count(self);
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
  
     /* Validate connection is still alive before returning to pool.
      * Returning a dead connection wastes a get() caller's retry. */
     if (!pool_is_alive(pdo)) {
         pool_decrement_count(self);
         return;
     }
 
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
             zval close_func, close_ret;
             ZVAL_STRING(&close_func, "close");
             call_user_function(NULL, channel, &close_func, &close_ret, 0, NULL);
             zval_ptr_dtor(&close_func);
             if (!Z_ISUNDEF(close_ret)) {
                 zval_ptr_dtor(&close_ret);
             }
         }
         /* else: not in coroutine — channel will be freed with the pool object */
 
         /* Force-reset count: any remaining in-use connections will see
          * closed=true when returned via put() and self-discard. */
         pool_set_count(self, 0);
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
                 gene_factory_call(pool, "close", NULL, &close_ret);
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
  * {{{ public Gene\Pool::__destruct()
  */
 PHP_METHOD(gene_pool, __destruct)
 {
     zval *self = getThis();
     if (!pool_is_closed(self)) {
         zval close_ret;
         gene_factory_call(self, "close", NULL, &close_ret);
         zval_ptr_dtor(&close_ret);
     }
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
  
     /* Call Gene\Pool::getInstance($name) */
     {
         zval callable, pool_obj;
         array_init(&callable);
         if (GENE_G(use_namespace)) {
             add_next_index_string(&callable, "Gene\\Pool");
         } else {
             add_next_index_string(&callable, "Gene_Pool");
         }
         add_next_index_string(&callable, "getInstance");
  
         zval params[1];
         ZVAL_COPY(&params[0], pool_name);
         ZVAL_UNDEF(&pool_obj);
         call_user_function(NULL, NULL, &callable, &pool_obj, 1, params);
         zval_ptr_dtor(&params[0]);
         zval_ptr_dtor(&callable);
  
         if (Z_TYPE(pool_obj) == IS_OBJECT) {
             /* Call pool->get() to borrow a PDO */
             zval pdo_obj;
             gene_factory_call(&pool_obj, "get", NULL, &pdo_obj);
  
             if (Z_TYPE(pdo_obj) == IS_OBJECT) {
                 /* Store pool reference ONLY after successful get().
                  * If get() returned NULL (timeout/exhausted), we must NOT
                  * set the pool property — otherwise __destruct would try
                  * to return a standalone fallback PDO to the pool,
                  * corrupting the pool's connection count. */
                 zend_update_property(db_ce, gene_strip_obj(self), pool_key, pool_key_len, &pool_obj);
                 zend_update_property(db_ce, gene_strip_obj(self), pdo_key, pdo_key_len, &pdo_obj);
                 zval_ptr_dtor(&pdo_obj);
                 zval_ptr_dtor(&pool_obj);
                 return 1;
             }
             zval_ptr_dtor(&pdo_obj);
         }
         zval_ptr_dtor(&pool_obj);
     }
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
 
             zval retval, params;
             array_init(&params);
             add_next_index_zval(&params, &pdo_copy);
             gene_factory_call(pool, "put", &params, &retval);
             zval_ptr_dtor(&retval);
             zval_ptr_dtor(&params);
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
         zval retval;
         gene_factory_call(pool, "remove", NULL, &retval);
         zval_ptr_dtor(&retval);
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
     zend_declare_property_long(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_MAX), 10, ZEND_ACC_PROTECTED);
     zend_declare_property_long(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_IDLE_TIME), 60, ZEND_ACC_PROTECTED);
     zend_declare_property_double(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_WAIT_TIME), 3.0, ZEND_ACC_PROTECTED);
     zend_declare_property_null(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_COUNT), ZEND_ACC_PROTECTED);
     zend_declare_property_long(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_TIMER_ID), 0, ZEND_ACC_PROTECTED);
     zend_declare_property_bool(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_CLOSED), 0, ZEND_ACC_PROTECTED);
  
     /* Static property: instances registry */
     zend_declare_property_null(gene_pool_ce, ZEND_STRL(GENE_POOL_PROPERTY_INSTANCES), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
  
     return SUCCESS;
 }