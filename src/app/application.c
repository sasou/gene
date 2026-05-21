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
#include "standard/php_filestat.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../app/application.h"
#include "../factory/load.h"
#include "../cache/memory.h"
#include "../config/configs.h"
#include "../router/router.h"
#include "../http/request.h"
#include "../http/webscan.h"
#include "../common/common.h"
#include "../mvc/view.h"
#include "../di/di.h"
#include "../exception/exception.h"

zend_class_entry * gene_application_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_application_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, safe)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_instance, 0, 0, 0)
	ZEND_ARG_INFO(0, safe)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_load, 0, 0, 1)
	ZEND_ARG_INFO(0, file)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, validity)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_autoload, 0, 0, 1)
	ZEND_ARG_INFO(0, app_root)
	ZEND_ARG_INFO(0, auto_function)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_mode, 0, 0, 0)
	ZEND_ARG_INFO(0, error_type)
	ZEND_ARG_INFO(0, exception_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_view, 0, 0, 0)
	ZEND_ARG_INFO(0, view)
	ZEND_ARG_INFO(0, tpl_ext)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_error, 0, 0, 3)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, error_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_exception, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_run, 0, 0, 0)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, uri)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_webscan, 0, 0, 0)
	ZEND_ARG_INFO(0, webscan_switch)
	ZEND_ARG_INFO(0, webscan_white_directory)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, webscan_white_url)
	ZEND_ARG_INFO(0, webscan_get)
	ZEND_ARG_INFO(0, webscan_post)
	ZEND_ARG_INFO(0, webscan_cookie)
	ZEND_ARG_INFO(0, webscan_referer)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_method, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_wait_worker_ready, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_path, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_uri, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_lang, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_module, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_controller, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_action, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_environment, 0, 0, 0)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_environment, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_environment_name, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_runtime_type, 0, 0, 0)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_runtime_type, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_runtime_type_name, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_config, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_response, 0, 0, 1)
    ZEND_ARG_INFO(0, response)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

/* [GENE_PERF:2026-04-24 #2] Optional bool $gc argument: when true, triggers
 * a zend GC cycle collection after the request context is destroyed. Lets
 * Swoole handlers that create deep object graphs (ORM, DI graphs, etc.)
 * opt into cycle reclaim at the cleanup boundary. Defaults to false so the
 * hot path stays cycle-collector-free. */
ZEND_BEGIN_ARG_INFO_EX(gene_application_cleanup, 0, 0, 0)
    ZEND_ARG_INFO(0, gc)
ZEND_END_ARG_INFO()

/* [GENE_PERF:2026-04-24 #2] prewarmCtxPool($count = -1): populate the
 * gene_request_context struct pool up to $count (bounded by ctx_pool_max).
 * Pass -1 or omit for "fill to max". Returns the number of contexts added. */
ZEND_BEGIN_ARG_INFO_EX(gene_application_prewarm_ctx_pool, 0, 0, 0)
    ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

/*
 * {{{ void load_file(char *key, size_t key_len,char *php_script, int validity)
 */
void load_file(char *key, size_t key_len, char *php_script, int validity) {

	int import = 0;
	zend_long cur, times = 0;
	filenode *val;
	if (UNEXPECTED(!gene_memory_write_allowed("Application::load"))) {
		return;
	}
	if (key_len) {
		val = file_cache_get_easy(key, key_len);
		if (val) {
			if (val->status == 1) {
				return;
			}
			cur = time(NULL);
			times = cur - val->stime;
			if (times > val->validity) {
				/* [GENE_PERF:2026-04-17] Batch the 3 consecutive lock cycles into 2:
				 * one to publish "loading" state, one to either commit new mtime (while
				 * keeping status=1 so concurrent loaders skip) or reset status=0. */
				zend_long new_mtime;
				GENE_CACHE_WRLOCK();
				val->status = 1;
				val->stime = cur;
				val->validity = validity;
				GENE_CACHE_WRUNLOCK();
				new_mtime = gene_file_modified(php_script, 0);
				GENE_CACHE_WRLOCK();
				if (new_mtime != val->ftime) {
					import = 1;
					val->ftime = new_mtime;
					/* keep status=1 across gene_load_import */
				} else {
					val->status = 0;
				}
				GENE_CACHE_WRUNLOCK();
			}
		} else {
			import = 1;
			file_cache_set_val(key, key_len, 0, validity);
			val = file_cache_get_easy(key, key_len);
			if (val) {
				GENE_CACHE_WRLOCK();
				val->status = 1;
				val->ftime = gene_file_modified(php_script, 0);
				GENE_CACHE_WRUNLOCK();
			}
		}
		if (import) {
			if(!gene_load_import(php_script, NULL, NULL)) {
				php_error_docref(NULL, E_WARNING, "Unable to load config file %s", php_script);
			}
			if (val) {
				GENE_CACHE_WRLOCK();
				val->status = 0;
				GENE_CACHE_WRUNLOCK();
			}
		}
	}
}
/* }}} */

/** {{{ gene_file_modified
 */
zend_long gene_file_modified(char *file, zend_long ctime) {
	zend_stat_t sb;
	if (!file || VCWD_STAT(file, &sb) == -1) {
		return 0;
	}
	if (ctime != (zend_long)sb.st_mtime) {
		return (zend_long)sb.st_mtime;
	}
	return 0;
}
/* }}} */

/** {{{ gene_ini_copy_method_lower
 * [GENE_PERF:2026-04-20] Fused copy-and-lowercase for the HTTP method string.
 * Replaces estrndup()+gene_strtolower() two-pass pattern with a single pass so
 * the buffer is written exactly once (half the cache line traffic on a 3-10 byte
 * method string). Returns the emalloc'd buffer (caller owns it).
 */
static zend_always_inline char *gene_ini_copy_method_lower(const char *src, size_t len) {
	char *dst = (char *)emalloc(len + 1);
	size_t i;
	unsigned char c;
	for (i = 0; i < len; i++) {
		c = (unsigned char)src[i];
		dst[i] = (c >= 'A' && c <= 'Z') ? (char)(c | 0x20) : (char)c;
	}
	dst[len] = '\0';
	return dst;
}
/* }}} */

/** {{{ int gene_ini_router()
 * Returns 1 on success (method + path all available), 0 on failure.
 * [GENE_PERF:2026-04-19 #2] Cache gene_request_ctx() once at entry — the previous
 * code issued ~16 GENE_REQ() expansions per request (each expands to a function call
 * or, in Swoole mode, at least several globals loads + branches). With one local
 * load, the whole routine becomes tight field accesses on `ctx->...`. Safe because
 * this function never yields (only SAPI globals + strcpy/lowercase).
 * [GENE_PERF:2026-04-20] Fused method copy+lowercase into one pass; populate
 * ctx->method_len and ctx->path_len from leftByChar's return so later dispatch
 * code can skip repeated strlen() on these strings. emalloc instead of ecalloc
 * for ctx->path since leftByChar writes its own null terminator.
 */
int gene_ini_router() {
	zval *server = NULL, *temp = NULL;
	gene_request_context *ctx = gene_request_ctx();
	if (!ctx->method || !ctx->path) {
		if (GENE_G(runtime_type) >= 2) {
			zval *attr_server = getVal(TRACK_VARS_SERVER, NULL, 0);
			if (attr_server && Z_TYPE_P(attr_server) == IS_ARRAY) {
				if (!ctx->method) {
					temp = zend_hash_str_find(Z_ARRVAL_P(attr_server), ZEND_STRL("REQUEST_METHOD"));
					if (!temp) {
						temp = zend_hash_str_find(Z_ARRVAL_P(attr_server), ZEND_STRL("request_method"));
					}
					if (temp && Z_TYPE_P(temp) == IS_STRING) {
						ctx->method = gene_ini_copy_method_lower(Z_STRVAL_P(temp), Z_STRLEN_P(temp));
						ctx->method_len = Z_STRLEN_P(temp);
					}
				}
				if (!ctx->path) {
					temp = zend_hash_str_find(Z_ARRVAL_P(attr_server), ZEND_STRL("REQUEST_URI"));
					if (!temp) {
						temp = zend_hash_str_find(Z_ARRVAL_P(attr_server), ZEND_STRL("request_uri"));
					}
					if (temp && Z_TYPE_P(temp) == IS_STRING) {
						ctx->path = emalloc(Z_STRLEN_P(temp) + 1);
						ctx->path_len = leftByChar(ctx->path, Z_STRVAL_P(temp), '?');
					}
				}
				temp = NULL;
			}
		}
		if (!ctx->method || !ctx->path) {
			server = request_query(TRACK_VARS_SERVER, NULL, 0);
			if (server) {
				if (!ctx->method
					&& (temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("REQUEST_METHOD"))) != NULL
					&& Z_TYPE_P(temp) == IS_STRING) {
					ctx->method = gene_ini_copy_method_lower(Z_STRVAL_P(temp), Z_STRLEN_P(temp));
					ctx->method_len = Z_STRLEN_P(temp);
				}
				if (!ctx->path
					&& (temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("REQUEST_URI"))) != NULL
					&& Z_TYPE_P(temp) == IS_STRING) {
					ctx->path = emalloc(Z_STRLEN_P(temp) + 1);
					ctx->path_len = leftByChar(ctx->path, Z_STRVAL_P(temp), '?');
				}
			}
			server = NULL;
			temp = NULL;
		}
	}
	if (!ctx->method || !ctx->path) {
		return 0;
	}
	return 1;
}
/* }}} */

static int gene_application_webscan_check()
{
	zval *enabled = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_ENABLED), 1);
	zval *config = NULL, *callback = NULL;
	zend_class_entry *webscan_ce = NULL;
	zval webscan_obj, ctor_ret, check_ret;
	zval params[7];
	int i, blocked = 0;
	/* [GENE_PERF:2026-04-17] Cache class entry + function pointers per process to avoid
	 * zend_lookup_class + ZVAL_STRING method name heap-alloc on every request. */
	static zend_class_entry *cached_ce = NULL;
	static zend_function *cached_ctor = NULL;
	static zend_function *cached_check = NULL;

	if (!enabled || !zend_is_true(enabled)) {
		return 0;
	}
	config = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_CONFIG), 1);
	if (!config || Z_TYPE_P(config) != IS_ARRAY) {
		return 0;
	}
	callback = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_CALLBACK), 1);

	if (UNEXPECTED(!cached_ce)) {
		static zend_string *ws_key = NULL;
		if (!ws_key) {
			ws_key = zend_string_init_interned(ZEND_STRL("Gene\\Webscan"), 1);
		}
		webscan_ce = zend_lookup_class(ws_key);
		if (!webscan_ce) {
			php_error_docref(NULL, E_WARNING, "Unable to load security scanner class Gene\\Webscan");
			return 0;
		}
		cached_ce = webscan_ce;
		cached_ctor = webscan_ce->constructor;
		cached_check = zend_hash_str_find_ptr(&webscan_ce->function_table, "check", sizeof("check") - 1);
	}
	webscan_ce = cached_ce;

	object_init_ex(&webscan_obj, webscan_ce);
	for (i = 0; i < 7; i++) {
		zval *item = zend_hash_index_find(Z_ARRVAL_P(config), i);
		if (item) {
			/* Addref instead of deep copy: call_user_function/zend_call_known_function
			 * treats args as input and will not mutate a shared persistent config array. */
			Z_TRY_ADDREF_P(item);
			ZVAL_COPY_VALUE(&params[i], item);
		} else {
			ZVAL_NULL(&params[i]);
		}
	}

	ZVAL_NULL(&ctor_ret);
	if (EXPECTED(cached_ctor)) {
		zend_call_known_function(cached_ctor, Z_OBJ(webscan_obj), webscan_ce, &ctor_ret, 7, params, NULL);
	} else {
		zval ctor_name;
		ZVAL_STRINGL(&ctor_name, "__construct", sizeof("__construct") - 1);
		call_user_function(NULL, &webscan_obj, &ctor_name, &ctor_ret, 7, params);
		zval_ptr_dtor(&ctor_name);
	}
	zval_ptr_dtor(&ctor_ret);
	for (i = 0; i < 7; i++) {
		zval_ptr_dtor(&params[i]);
	}

	ZVAL_NULL(&check_ret);
	if (EXPECTED(cached_check)) {
		zend_call_known_function(cached_check, Z_OBJ(webscan_obj), webscan_ce, &check_ret, 0, NULL, NULL);
	} else {
		zval check_name;
		ZVAL_STRINGL(&check_name, "check", sizeof("check") - 1);
		call_user_function(NULL, &webscan_obj, &check_name, &check_ret, 0, NULL);
		zval_ptr_dtor(&check_name);
	}
	blocked = zend_is_true(&check_ret);
	zval_ptr_dtor(&check_ret);

	if (blocked) {
		if (callback && Z_TYPE_P(callback) != IS_NULL) {
			zval cb_ret;
			ZVAL_NULL(&cb_ret);
			if (call_user_function(EG(function_table), NULL, callback, &cb_ret, 0, NULL) == SUCCESS) {
				if (Z_TYPE(cb_ret) != IS_NULL) {
					zend_string *output = zval_get_string(&cb_ret);
					if (ZSTR_LEN(output) > 0) {
						PHPWRITE(ZSTR_VAL(output), ZSTR_LEN(output));
					}
					zend_string_release(output);
				}
				zval_ptr_dtor(&cb_ret);
			}
		} else {
			PHPWRITE("Illegal access", sizeof("Illegal access") - 1);
		}
	}

	zval_ptr_dtor(&webscan_obj);
	return blocked;
}

/*
 *  {{{ zval *gene_application_instance(zval *this_ptr,zval *safe)
 */
zval *gene_application_instance(zval *this_ptr, zval *safe) {
	zval *instance = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), 1);

	if (Z_TYPE_P(instance) == IS_OBJECT) {
		if (safe && !GENE_G(app_key)) {
			GENE_G(app_key) = estrndup(Z_STRVAL_P(safe), Z_STRLEN_P(safe));
			GENE_G(app_key_len) = Z_STRLEN_P(safe);
		}
		return instance;
	}
	if (this_ptr) {
		zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), this_ptr);
		instance = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), 1);
	} else {
		zval new_instance;
		object_init_ex(&new_instance, gene_application_ce);
		zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), &new_instance);
		zval_ptr_dtor(&new_instance);
		instance = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), 1);
	}

	if (safe && !GENE_G(app_key)) {
		GENE_G(app_key) = estrndup(Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		GENE_G(app_key_len) = Z_STRLEN_P(safe);
	}
	return instance;
}
/* }}} */


/*
 *  {{{ public gene_application::getInstance(void)
 */
PHP_METHOD(gene_application, getInstance) {
	zval *safe = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_ZVAL(gene_application_instance(NULL, safe), 1, 0);
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __construct) {
	zval *safe = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_ZVAL(gene_application_instance(NULL, safe), 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::load($key,$validity)
 */
PHP_METHOD(gene_application, load) {
	zval *self = getThis();
	zend_string *file = NULL, *path = NULL;
	char *cache_key;
	int validity = 10;
	size_t cache_key_len;
	char cache_key_buf[512];
	int cache_key_heap = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|Sl", &file, &path, &validity) == FAILURE) {
		return;
	}
	if (path && ZSTR_LEN(path) > 0) {
		cache_key_len = ZSTR_LEN(path) + 1 + ZSTR_LEN(file);
		if (cache_key_len >= sizeof(cache_key_buf)) {
			cache_key = emalloc(cache_key_len + 1);
			cache_key_heap = 1;
		} else {
			cache_key = cache_key_buf;
		}
		snprintf(cache_key, cache_key_len + 1, "%s/%s", ZSTR_VAL(path), ZSTR_VAL(file));
	} else if (GENE_G(app_root) && GENE_G(app_root)[0] != '\0') {
		cache_key_len = GENE_G(app_root_len) + sizeof("/Config/") - 1 + ZSTR_LEN(file);
		if (cache_key_len >= sizeof(cache_key_buf)) {
			cache_key = emalloc(cache_key_len + 1);
			cache_key_heap = 1;
		} else {
			cache_key = cache_key_buf;
		}
		snprintf(cache_key, cache_key_len + 1, "%s/Config/%s", GENE_G(app_root), ZSTR_VAL(file));
	} else {
		php_error_docref(NULL, E_WARNING, "app_root is not set, call autoload() first or pass an explicit path");
		RETURN_ZVAL(self, 1, 0);
	}
	load_file(cache_key, cache_key_len, cache_key, validity);
	if (cache_key_heap) {
		efree(cache_key);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::getMethod()
 * [GENE_PERF:2026-04-20] RETURN_STRINGL with cached length avoids the strlen()
 * call inside RETURN_STRING — same optimization applies to all context getters below.
 */
PHP_METHOD(gene_application, getMethod) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->method) {
		RETURN_STRINGL(ctx->method, ctx->method_len);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getPath()
 */
PHP_METHOD(gene_application, getPath) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->path) {
		RETURN_STRINGL(ctx->path, ctx->path_len);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getModule()
 */
PHP_METHOD(gene_application, getModule) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->module) {
		RETURN_STRINGL(ctx->module, ctx->module_len);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getController()
 */
PHP_METHOD(gene_application, getController) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->controller) {
		RETURN_STRINGL(ctx->controller, ctx->controller_len);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getAction()
 */
PHP_METHOD(gene_application, getAction) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->action) {
		RETURN_STRINGL(ctx->action, ctx->action_len);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getLang()
 */
PHP_METHOD(gene_application, getLang) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->lang) {
		RETURN_STRINGL(ctx->lang, ctx->lang_len);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getRouterUri()
 * [GENE_PERF:2026-04-20] Use cached module_len/controller_len/action_len so
 * each strreplace_fast skips a per-length strlen(). router_path_len is also
 * cached; copy via emalloc+memcpy instead of str_init (avoids one extra strlen).
 */
PHP_METHOD(gene_application, getRouterUri) {
	char *path = NULL, *new_path = NULL;
	size_t path_len;
	gene_request_context *ctx = gene_request_ctx();
	if (!ctx->router_path) {
		RETURN_NULL();
	}

	path_len = ctx->router_path_len;
	path = emalloc(path_len + 1);
	memcpy(path, ctx->router_path, path_len + 1);
	if (ctx->module != NULL) {
		new_path = gene_strreplace_fast(path, path_len, ":m", 2, ctx->module, ctx->module_len, &path_len);
		if (new_path) {
			efree(path);
			path = new_path;
		}
	}
	if (ctx->controller != NULL) {
		new_path = gene_strreplace_fast(path, path_len, ":c", 2, ctx->controller, ctx->controller_len, &path_len);
		if (new_path) {
			efree(path);
			path = new_path;
		}
	}
	if (ctx->action != NULL) {
		new_path = gene_strreplace_fast(path, path_len, ":a", 2, ctx->action, ctx->action_len, &path_len);
		if (new_path) {
			efree(path);
			path = new_path;
		}
	}
	gene_strtolower(path);
	RETVAL_STRINGL(path, path_len);
	efree(path);
}
/* }}} */

/*
 * {{{ public gene_application::getEnvironment()
 */
PHP_METHOD(gene_application, getEnvironment) {
	RETURN_LONG(GENE_G(run_environment));
}
/* }}} */

/*
 * {{{ public gene_application::getEnvironmentName()
 */
PHP_METHOD(gene_application, getEnvironmentName) {
	switch (GENE_G(run_environment)) {
		case 2:
			RETURN_STRING("prod");
		case 1:
			RETURN_STRING("test");
		case 0:
		default:
			RETURN_STRING("dev");
	}
}
/* }}} */

/*
 * {{{ public gene_application::getRuntimeType()
 */
PHP_METHOD(gene_application, getRuntimeType) {
	RETURN_LONG(GENE_G(runtime_type));
}
/* }}} */

/*
 * {{{ public gene_application::getRuntimeTypeName()
 */
PHP_METHOD(gene_application, getRuntimeTypeName) {
	switch (GENE_G(runtime_type)) {
		case 2:
			RETURN_STRING("swoole");
		case 3:
			RETURN_STRING("coroutine");
		case 1:
		default:
			RETURN_STRING("fpm");
	}
}
/* }}} */


/*
 * {{{ public gene_application::params($key)
 */
PHP_METHOD(gene_application, params) {
	char *name = NULL;
	zend_long name_len = 0;
	zval *params = NULL;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &name, &name_len) == FAILURE) {
		return;
	}

	ctx = gene_request_ctx();
	params = &ctx->path_params;
	if (name_len == 0) {
		RETURN_ZVAL(&ctx->path_params, 1, 0);
	} else {
		zval *val = zend_symtable_str_find(Z_ARRVAL_P(params), name, name_len);
		if (val) {
			RETURN_ZVAL(val, 1, 0);
		}
		RETURN_NULL();
	}
}
/* }}} */

/*
 * {{{ public gene_application::setEnvironment($type)
 */
PHP_METHOD(gene_application, setEnvironment) {
	zval *self = getThis();
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &type) == FAILURE) {
		return;
	}
	GENE_G(run_environment) = type;
	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::setRuntimeType($type)
 */
PHP_METHOD(gene_application, setRuntimeType) {
	zval *self = getThis(), *type = NULL;
	zend_long runtime = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &type) == FAILURE) {
		return;
	}
	if (type == NULL || Z_TYPE_P(type) == IS_NULL) {
		RETURN_FALSE;
	}
	if (Z_TYPE_P(type) == IS_LONG) {
		runtime = Z_LVAL_P(type);
	} else if (Z_TYPE_P(type) == IS_STRING) {
		char *name = Z_STRVAL_P(type);
		size_t len = Z_STRLEN_P(type);
		if ((len == 3 && strncasecmp(name, "fpm", 3) == 0)
		|| (len == 7 && strncasecmp(name, "php-fpm", 7) == 0)) {
			runtime = 1;
		} else if ((len == 6 && strncasecmp(name, "swoole", 6) == 0)
		|| (len == 8 && strncasecmp(name, "resident", 8) == 0)
		|| (len == 6 && strncasecmp(name, "daemon", 6) == 0)) {
			runtime = 2;
		} else if ((len == 9 && strncasecmp(name, "coroutine", 9) == 0)
		|| (len == 2 && strncasecmp(name, "co", 2) == 0)) {
			runtime = 3;
		} else {
			php_error_docref(NULL, E_WARNING, "Unknown runtime type '%s', fallback to fpm.", name);
			runtime = 1;
		}
	} else {
		php_error_docref(NULL, E_WARNING, "setRuntimeType type must be int or string.");
		RETURN_FALSE;
	}

	if (runtime < 1) {
		runtime = 1;
	}
	GENE_G(runtime_type) = runtime;
	if (runtime >= 2) {
		gene_init_co_contexts();
	}
	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ void gene_clear_request_state()
 * [GENE_PERF:2026-04-19 #2] Fast path: when the current vm_stack still matches
 * the cached snapshot, we're in the same coroutine as current_ctx — use it
 * directly and skip the expensive Swoole getcid() PHP call plus the hash lookup.
 * This is the common case in a normal Swoole request flow where clearState() is
 * invoked at end of request handler (no yield between dispatch and clear).
 */
static void gene_clear_request_state() {
	if (GENE_G(runtime_type) >= 2 && GENE_G(co_contexts)) {
		zend_long cid;
		gene_request_context *ctx;
		if (EXPECTED(GENE_G(current_ctx) != NULL
				&& GENE_G(current_cid) >= 0
				&& GENE_G(current_vm_stack) == (void *)EG(vm_stack))) {
			gene_request_context_reset(GENE_G(current_ctx));
			return;
		}
		cid = gene_get_coroutine_id();
		if (cid >= 0) {
			ctx = zend_hash_index_find_ptr(GENE_G(co_contexts), (zend_ulong)cid);
			if (ctx) {
				gene_request_context_reset(ctx);
			}
			return;
		}
	}
	gene_request_context_reset(gene_request_ctx());
}
/* }}} */

/*
 * {{{ static void gene_application_wait_worker_ready_once()
 */
static void gene_application_wait_worker_ready_once() {
	if (GENE_G(runtime_type) >= 2 && !GENE_G(worker_ready)) {
		zend_long waited_us = 0;
		zval func_name, retval, param;
		ZVAL_STRING(&func_name, "usleep");
		ZVAL_LONG(&param, GENE_WORKER_READY_WAIT_INTERVAL_US);
		while (!GENE_G(worker_ready) && waited_us < GENE_WORKER_READY_WAIT_MAX_US) {
			ZVAL_UNDEF(&retval);
			call_user_function(EG(function_table), NULL, &func_name, &retval, 1, &param);
			zval_ptr_dtor(&retval);
			waited_us += GENE_WORKER_READY_WAIT_INTERVAL_US;
		}
		zval_ptr_dtor(&func_name);
		if (!GENE_G(worker_ready)) {
			php_error_docref(NULL, E_WARNING, "Gene: worker not ready after %lds timeout",
				(long)(GENE_WORKER_READY_WAIT_MAX_US / 1000000));
		}
	}
}
/* }}} */

/*
 * {{{ public gene_application::clearState()
 */
PHP_METHOD(gene_application, clearState) {
	zval *self = getThis();
	gene_clear_request_state();
	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::waitWorkerReady()
 */
PHP_METHOD(gene_application, waitWorkerReady) {
	zval *self = getThis();
	gene_application_wait_worker_ready_once();
	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::destroyContext()
 * [GENE_PERF:2026-04-19 #2] Fast path: when vm_stack still matches the cached
 * snapshot, we know current_cid is valid for the active coroutine — skip the
 * Swoole getcid() PHP call entirely.
 */
PHP_METHOD(gene_application, destroyContext) {
	if (GENE_G(runtime_type) >= 2 && GENE_G(co_contexts)) {
		zend_long cid;
		if (EXPECTED(GENE_G(current_ctx) != NULL
				&& GENE_G(current_cid) >= 0
				&& GENE_G(current_vm_stack) == (void *)EG(vm_stack))) {
			cid = GENE_G(current_cid);
			GENE_G(current_ctx) = NULL;
			GENE_G(current_cid) = -1;
			GENE_G(current_vm_stack) = NULL;
			zend_hash_index_del(GENE_G(co_contexts), (zend_ulong)cid);
			RETURN_TRUE;
		}
		cid = gene_get_coroutine_id();
		if (cid < 0) {
			GENE_G(current_ctx) = NULL;
			GENE_G(current_cid) = -1;
			GENE_G(current_vm_stack) = NULL;
			if (GENE_G(resident_ctx)) {
				gene_request_context *tmp = GENE_G(resident_ctx);
				GENE_G(resident_ctx) = NULL;
				gene_request_context_destroy(tmp);
				/* [GENE_PERF:2026-04-24] Recycle through the pool. */
				gene_request_context_pool_release(tmp);
			}
			RETURN_TRUE;
		}
		if (GENE_G(current_cid) == cid) {
			GENE_G(current_ctx) = NULL;
			GENE_G(current_cid) = -1;
			GENE_G(current_vm_stack) = NULL;
		}
		zend_hash_index_del(GENE_G(co_contexts), (zend_ulong)cid);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::cleanup([bool $gc = false])
 * Combined clearState + destroyContext for Swoole mode.
 * Phase 1: destroy context fields (free user data, triggers object destructors
 *          while context struct is still alive).
 * Phase 2: remove the now-empty context from co_contexts. The dtor
 *          [GENE_PERF:2026-04-24] now recycles the struct through the
 *          bounded struct pool (gene_request_context_pool_release) instead
 *          of efree'ing — so the next coroutine spawn skips ecalloc entirely.
 * Phase 3 ([GENE_PERF:2026-04-24 #2]): if $gc truthy AND runtime_type >= 2,
 *          trigger gc_collect_cycles() so that cyclic graphs built by the
 *          handler (DI → Service → Model → DI back-refs; ORM lazy loaders)
 *          are reclaimed at the request boundary instead of waiting for the
 *          GC threshold to trip mid-next-request. Defaults to false (caller
 *          opts in): the collector walks all live zvals and costs μs-ms
 *          depending on live-heap size — only worth it for handlers that
 *          demonstrably leak RSS without it.
 * In FPM mode: behaves identically to clearState() alone; $gc is ignored
 *              because the SAPI already frees the request arena next RSHUTDOWN.
 */
PHP_METHOD(gene_application, cleanup) {
	zend_bool gc = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &gc) == FAILURE) {
		RETURN_FALSE;
	}
	if (GENE_G(runtime_type) >= 2 && GENE_G(co_contexts)) {
		zend_long cid;
		/* [GENE_PERF:2026-04-19 #2] Fast path: vm_stack match ⇒ same coroutine
		 * as current_ctx; skip getcid() and hash lookup, invalidate + delete. */
		if (EXPECTED(GENE_G(current_ctx) != NULL
				&& GENE_G(current_cid) >= 0
				&& GENE_G(current_vm_stack) == (void *)EG(vm_stack))) {
			cid = GENE_G(current_cid);
			GENE_G(current_ctx) = NULL;
			GENE_G(current_cid) = -1;
			GENE_G(current_vm_stack) = NULL;
			zend_hash_index_del(GENE_G(co_contexts), (zend_ulong)cid);
			goto maybe_gc;
		}
		cid = gene_get_coroutine_id();
		if (cid >= 0) {
			if (GENE_G(current_cid) == cid) {
				GENE_G(current_ctx) = NULL;
				GENE_G(current_cid) = -1;
				GENE_G(current_vm_stack) = NULL;
			}
			zend_hash_index_del(GENE_G(co_contexts), (zend_ulong)cid);
		} else {
			GENE_G(current_ctx) = NULL;
			GENE_G(current_cid) = -1;
			GENE_G(current_vm_stack) = NULL;
			if (GENE_G(resident_ctx)) {
				gene_request_context *tmp = GENE_G(resident_ctx);
				GENE_G(resident_ctx) = NULL;
				gene_request_context_destroy(tmp);
				/* [GENE_PERF:2026-04-24] Recycle through the pool. */
				gene_request_context_pool_release(tmp);
			}
		}
maybe_gc:
		if (gc) {
			/* [GENE_PERF:2026-04-24 #2] Opt-in cycle collector after all
			 * handler-scoped zvals have been released above. GC is disabled
			 * by default (gc_enabled()==1 by default, but gc_collect_cycles()
			 * is safe to call unconditionally — it respects gc_disabled()). */
			gc_collect_cycles();
		}
	} else {
		gene_request_context_reset(gene_request_ctx());
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::prewarmCtxPool([int $count = -1])
 * [GENE_PERF:2026-04-24 #2] Populate the internal gene_request_context struct
 * pool so the first N Swoole coroutine spawns find a ready-to-use context
 * without hitting ZMM's ecalloc. Typically invoked from the Swoole server's
 * onWorkerStart callback:
 *
 *     $server->on('WorkerStart', function () {
 *         \Gene\Application::getInstance()->prewarmCtxPool();
 *     });
 *
 * Bounded at ctx_pool_max (INI gene.ctx_pool_max, default 256). Returns the
 * number of contexts actually added (0 if FPM / already warmed / count<=0).
 */
PHP_METHOD(gene_application, prewarmCtxPool) {
	zend_long count = -1;
	zend_long added;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &count) == FAILURE) {
		RETURN_LONG(0);
	}
	added = gene_request_context_pool_prewarm(count);
	RETURN_LONG(added);
}
/* }}} */

/*
 * {{{ public gene_application::setResponse($response)
 */
PHP_METHOD(gene_application, setResponse) {
	zval *resp;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &resp) == FAILURE) {
		return;
	}
	zval *entrys = gene_di_regs();
	static zend_string *resp_key = NULL;
	if (UNEXPECTED(!resp_key)) {
		resp_key = zend_string_init_interned("response", sizeof("response") - 1, 1);
	}
	Z_TRY_ADDREF_P(resp);
	zend_hash_update(Z_ARRVAL_P(entrys), resp_key, resp);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::config()
 */
PHP_METHOD(gene_application, config) {
	zval *cache = NULL;
	zend_string *keyString;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}

	if (GENE_G(config_cache_key)) {
		cache = gene_memory_get_by_config(GENE_G(config_cache_key), GENE_G(config_cache_key_len), ZSTR_VAL(keyString));
	} else {
		const char *prefix = NULL;
		size_t prefix_len = 0;
		if (GENE_G(app_key) && GENE_G(app_key)[0] != '\0') {
			prefix = GENE_G(app_key);
			prefix_len = GENE_G(app_key_len);
		} else if (GENE_G(app_root) && GENE_G(app_root)[0] != '\0') {
			prefix = GENE_G(app_root);
			prefix_len = GENE_G(app_root_len);
		}
		if (prefix) {
			size_t key_len = prefix_len + sizeof(GENE_CONFIG_CACHE) - 1;
			char stack_buf[256];
			char *key_buf;
			int key_heap = 0;
			if (key_len >= sizeof(stack_buf)) {
				key_buf = emalloc(key_len + 1);
				key_heap = 1;
			} else {
				key_buf = stack_buf;
			}
			memcpy(key_buf, prefix, prefix_len);
			memcpy(key_buf + prefix_len, GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE));
			cache = gene_memory_get_by_config(key_buf, key_len, ZSTR_VAL(keyString));
			if (key_heap) efree(key_buf);
		} else {
			cache = gene_memory_get_by_config(GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE) - 1, ZSTR_VAL(keyString));
		}
	}
	if (cache) {
		gene_memory_zval_local(return_value, cache);
		return;
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::autoload($key)
 */
PHP_METHOD(gene_application, autoload) {
	zend_string *fileName = NULL, *app_root = NULL;
	zval *self = getThis();
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &app_root, &fileName) == FAILURE) {
		return;
	}
	if (app_root && ZSTR_LEN(app_root) > 0) {
		if (GENE_G(app_root)) {
			efree(GENE_G(app_root));
		}
		GENE_G(app_root) = estrndup(ZSTR_VAL(app_root), ZSTR_LEN(app_root));
		GENE_G(app_root_len) = ZSTR_LEN(app_root);
	}
	if (fileName && ZSTR_LEN(fileName) > 0) {
		if (GENE_G(auto_load_fun)) {
			efree(GENE_G(auto_load_fun));
		}
		GENE_G(auto_load_fun) = estrndup(ZSTR_VAL(fileName), ZSTR_LEN(fileName));
	}
	/* Pre-compute config cache key to avoid repeated concatenation in hot path */
	{
		const char *prefix = NULL;
		size_t prefix_len = 0;
		if (GENE_G(app_key) && GENE_G(app_key)[0] != '\0') {
			prefix = GENE_G(app_key);
			prefix_len = GENE_G(app_key_len);
		} else if (GENE_G(app_root) && GENE_G(app_root)[0] != '\0') {
			prefix = GENE_G(app_root);
			prefix_len = GENE_G(app_root_len);
		}
		if (GENE_G(config_cache_key)) {
			efree(GENE_G(config_cache_key));
		}
		if (prefix) {
			GENE_G(config_cache_key_len) = prefix_len + sizeof(GENE_CONFIG_CACHE) - 1;
			GENE_G(config_cache_key) = emalloc(GENE_G(config_cache_key_len) + 1);
			memcpy(GENE_G(config_cache_key), prefix, prefix_len);
			memcpy(GENE_G(config_cache_key) + prefix_len, GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE));
		} else {
			GENE_G(config_cache_key_len) = sizeof(GENE_CONFIG_CACHE) - 1;
			GENE_G(config_cache_key) = emalloc(GENE_G(config_cache_key_len) + 1);
			memcpy(GENE_G(config_cache_key), GENE_CONFIG_CACHE, sizeof(GENE_CONFIG_CACHE));
		}
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::error($callback, $type)
 */
PHP_METHOD(gene_application, error) {
	zval *callback = NULL, *error_type = NULL, *self = getThis();
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|zz", &type, &callback, &error_type) == FAILURE) {
		return;
	}
	if (type > 0) {
		gene_exception_error_register(callback, error_type);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public gene_application::exception(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
 */
PHP_METHOD(gene_application, exception) {
	zval *callback = NULL, *error_type = NULL, *self = getThis();
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|z", &type, &callback) == FAILURE) {
		return;
	}
	if (type > 0) {
		gene_exception_register(callback);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::setMode($error_type, $exception_type)
 */
PHP_METHOD(gene_application, setMode) {
	zval *self = getThis();
	zend_long error_type = 0, exception_type = 0;
	zval *error_callback = NULL, *ex_callback = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|llzz", &error_type, &exception_type, &ex_callback, &error_callback) == FAILURE) {
		return;
	}
	if (error_type > 0) {
		gene_exception_error_register(error_callback, NULL);
	}
	if (exception_type > 0) {
		gene_exception_register(ex_callback);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::setView($view, $ext)
 */
PHP_METHOD(gene_application, setView) {
	zval *self = getThis();
	zend_string *view = NULL, *tpl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &view, &tpl) == FAILURE) {
		return;
	}
	if (GENE_G(app_view)) {
		efree(GENE_G(app_view));
	}
	if (view && ZSTR_LEN(view)) {
		GENE_G(app_view) = estrndup(ZSTR_VAL(view), ZSTR_LEN(view));
		GENE_G(app_view_len) = ZSTR_LEN(view);
	} else {
		GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, GENE_VIEW_VIEW_LEN);
		GENE_G(app_view_len) = GENE_VIEW_VIEW_LEN;
	}
	if (GENE_G(app_ext)) {
		efree(GENE_G(app_ext));
	}
	if (tpl && ZSTR_LEN(tpl)) {
		GENE_G(app_ext) = estrndup(ZSTR_VAL(tpl), ZSTR_LEN(tpl));
		GENE_G(app_ext_len) = ZSTR_LEN(tpl);
	} else {
		GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, GENE_VIEW_EXT_LEN);
		GENE_G(app_ext_len) = GENE_VIEW_EXT_LEN;
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */
/*
 * {{{ public gene_application::webscan()
 */
PHP_METHOD(gene_application, webscan) {
    zval *callback = NULL, *webscan_white_url = NULL, *self = getThis();
    zend_long webscan_switch = 1, webscan_get = 1, webscan_post = 1, webscan_cookie = 1, webscan_referer = 1;
    zend_string *webscan_white_directory = NULL;
    zval config, enabled, null_val;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|lSzallll",
        &webscan_switch,
        &webscan_white_directory,
        &callback,
        &webscan_white_url,
        &webscan_get,
        &webscan_post,
        &webscan_cookie,
        &webscan_referer) == FAILURE) {
        return;
    }

    array_init_size(&config, 7);
    add_index_long(&config, 0, webscan_switch);
    if (webscan_white_directory) {
        add_index_str(&config, 1, zend_string_copy(webscan_white_directory));
    } else {
        add_index_string(&config, 1, "");
    }
    if (webscan_white_url) {
        zval tmp;
        ZVAL_COPY(&tmp, webscan_white_url);
        add_index_zval(&config, 2, &tmp);
    } else {
        zval tmp;
        array_init(&tmp);
        add_index_zval(&config, 2, &tmp);
    }
    add_index_long(&config, 3, webscan_get);
    add_index_long(&config, 4, webscan_post);
    add_index_long(&config, 5, webscan_cookie);
    add_index_long(&config, 6, webscan_referer);
    zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_CONFIG), &config);
    zval_ptr_dtor(&config);

    ZVAL_LONG(&enabled, webscan_switch > 0 ? 1 : 0);
    zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_ENABLED), &enabled);

    if (callback) {
        zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_CALLBACK), callback);
    } else {
        ZVAL_NULL(&null_val);
        zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_CALLBACK), &null_val);
    }

    RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_application::workerReady()
 * [GENE_PERF:2026-04-24 v5.5.8] In addition to flipping the read-side lock-
 * skip flag, workerReady() now also auto-populates the context struct pool
 * when running in Swoole / coroutine mode (runtime_type >= 2) and the pool
 * is currently empty. This collapses the canonical "Swoole bootstrap"
 * idiom into a single call:
 *
 *     $server->on('WorkerStart', function () {
 *         \Gene\Application::getInstance()->workerReady();
 *     });
 *
 * Without this, users had to pair workerReady() + prewarmCtxPool() or rely
 * on gene.ctx_pool_prewarm INI. The auto-fill respects ctx_pool_max and is
 * guarded on pool_size == 0 so repeated workerReady() calls stay idempotent.
 * FPM/cgi path: this is a no-op (runtime_type<2 + pool isn't used there).
 */
PHP_METHOD(gene_application, workerReady) {
	zval *self = getThis();
	GENE_G(worker_ready) = 1;
	if (GENE_G(runtime_type) >= 2 && GENE_G(ctx_pool_size) == 0) {
		gene_request_context_pool_prewarm(-1);
	}
	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::run($method,$path)
 * $path may include a query string, e.g. /cli/test/run?mode=1 (routed path only;
 * query is merged into $_GET inside the router).
 */
PHP_METHOD(gene_application, run) {
	zend_string *methodin = NULL, *pathin = NULL;
	zval *self = getThis();
	char *min = NULL, *pin = NULL;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &methodin, &pathin) == FAILURE) {
		RETURN_NULL();
	}

	gene_ini_router();
	ctx = gene_request_ctx();
	if (methodin && ZSTR_LEN(methodin)) {
		min = ZSTR_VAL(methodin);
	}
	if (pathin && ZSTR_LEN(pathin)) {
		pin = ZSTR_VAL(pathin);
	}
	if (!min && !ctx->method) {
		php_error_docref(NULL, E_WARNING, "Gene: unable to resolve REQUEST_METHOD or REQUEST_URI from server context");
		RETURN_ZVAL(self, 1, 0);
	}
	gene_loader_register();
	if (gene_application_webscan_check()) {
		RETURN_ZVAL(self, 1, 0);
	}

	{
		/* [GENE_PERF:2026-04-19 #2] safe_str length is resolved from pre-cached
		 * globals (app_key_len / app_root_len) populated in autoload(), avoiding a
		 * strlen() on every request. */
		const char *safe_str = NULL;
		size_t safe_len = 0;
		if (GENE_G(app_key)) {
			safe_str = GENE_G(app_key);
			safe_len = GENE_G(app_key_len);
		} else if (GENE_G(app_root)) {
			safe_str = GENE_G(app_root);
			safe_len = GENE_G(app_root_len);
		}
		get_router_content_run(min, pin, safe_str, safe_len);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __set)
{
	zend_string *name;
	zval *value;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	zend_string *class_name = gene_get_class_name_fast();
	if (!class_name) { RETURN_FALSE; }
	if (gene_di_set_class(class_name, name, value)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __get)
{
	zval *pzval = NULL;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		zend_string *class_name = gene_get_class_name_fast();
		if (!class_name) { RETURN_NULL(); }
		pzval = gene_di_get_class(class_name, name);
		if (pzval) {
			RETURN_ZVAL(pzval, 1, 0);
		}
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_application_methods
 */
const zend_function_entry gene_application_methods[] = {
	PHP_ME(gene_application, __construct, gene_application_construct, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, getInstance, gene_application_get_instance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, load, gene_application_load, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, autoload, gene_application_autoload, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, setMode, gene_application_set_mode, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, setView, gene_application_set_view, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, error, gene_application_error, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, exception, gene_application_exception, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, webscan, gene_application_webscan, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, run, gene_application_run, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, workerReady, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, waitWorkerReady, gene_application_wait_worker_ready, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, clearState, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, destroyContext, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, cleanup, gene_application_cleanup, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, prewarmCtxPool, gene_application_prewarm_ctx_pool, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, setResponse, gene_application_set_response, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getMethod, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getPath, gene_application_get_path, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getRouterUri, gene_application_get_uri, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getLang, gene_application_get_lang, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getModule, gene_application_get_module, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getController, gene_application_get_controller, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getAction, gene_application_get_action, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, setEnvironment, gene_application_set_environment, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getEnvironment, gene_application_get_environment, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getEnvironmentName, gene_application_get_environment_name, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, setRuntimeType, gene_application_set_runtime_type, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getRuntimeType, gene_application_get_runtime_type, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getRuntimeTypeName, gene_application_get_runtime_type_name, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, config, gene_application_config, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, params, gene_application_config, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, __get, gene_application_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, __set, gene_application_set, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(application) {
	zend_class_entry gene_application;
	GENE_INIT_CLASS_ENTRY(gene_application, "Gene_Application", "Gene\\Application", gene_application_methods);
	gene_application_ce = zend_register_internal_class( &gene_application);
	gene_application_ce->ce_flags |= ZEND_ACC_FINAL;
#if PHP_VERSION_ID >= 80200
	gene_application_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

	//static
	zend_declare_property_null(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_long(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_ENABLED), 0, ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_null(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_CONFIG), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_null(gene_application_ce, ZEND_STRL(GENE_APPLICATION_WEBSCAN_CALLBACK), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);

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


