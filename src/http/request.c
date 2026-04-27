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
#include "php_globals.h"
#include "main/SAPI.h"
#include "main/php_variables.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../http/request.h"
#include "../common/common.h"
#include "../cache/memory.h"

zend_class_entry * gene_request_ce;

static zval *gene_request_attr(void) {
	gene_request_context *ctx = gene_request_ctx();
	if (UNEXPECTED(Z_TYPE(ctx->request_attr) == IS_UNDEF || Z_TYPE(ctx->request_attr) == IS_NULL)) {
		/* [GENE_FIX:2026-04-27] IS_NULL is non-refcounted; the previous
		 * zval_ptr_dtor on it was a no-op and immediately overwritten by
		 * array_init_size below. Removed the dead branch. */
		/* [GENE_PERF:2026-04-24 v5.5.8] request_attr stores at most 8 track
		 * vars (POST/GET/COOKIE/SERVER/ENV/FILES/REQUEST/HEADER, indices 0-7).
		 * Pre-size the HashTable to 8 so the first burst of getVal()/setVal()
		 * calls don't trigger rehashes (prior default init grew 0→8→... on
		 * first insert). One-time cost: a handful of extra bucket slots. */
		array_init_size(&ctx->request_attr, 8);
	}
	return &ctx->request_attr;
}

/* [GENE_PERF:2026-04-23] In the previous implementation we rebuilt a brand-
 * new HashTable by iterating $server and adding every item refcounted into
 * a fresh array. With the move away from case-normalization that's become
 * a pure shallow clone — setVal() already bumps the outer array refcount,
 * so we can simply share Swoole's original server HashTable. This removes
 * O(N) hash inserts + array allocation from every Swoole request. Behavior
 * is preserved because no write path mutates the stored array after init(). */
static void gene_request_set_server_val(zval *server) {
	setVal(3, server);

	/* Populate ctx->method and ctx->path directly from the server array so
	 * gene_ini_router() finds them preset.  This avoids the indirect
	 * getVal(TRACK_VARS_SERVER) lookup which can occasionally miss in
	 * high-concurrency Swoole scenarios.
	 * [GENE_PERF:2026-04-20] Fused copy+lowercase via single-pass loop for
	 * method, and capture leftByChar's return for path_len (avoids later strlen). */
	if (GENE_G(runtime_type) >= 2) {
		gene_request_context *ctx = gene_request_ctx();
		if (!ctx->method) {
			zval *rm = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("REQUEST_METHOD"));
			if (!rm) {
				rm = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("request_method"));
			}
			if (rm && Z_TYPE_P(rm) == IS_STRING) {
				size_t mlen = Z_STRLEN_P(rm);
				const char *msrc = Z_STRVAL_P(rm);
				char *mdst = emalloc(mlen + 1);
				size_t mi;
				unsigned char mc;
				for (mi = 0; mi < mlen; mi++) {
					mc = (unsigned char)msrc[mi];
					mdst[mi] = (mc >= 'A' && mc <= 'Z') ? (char)(mc | 0x20) : (char)mc;
				}
				mdst[mlen] = '\0';
				ctx->method = mdst;
				ctx->method_len = mlen;
			}
		}
		if (!ctx->path) {
			zval *ru = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("REQUEST_URI"));
			if (!ru) {
				ru = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("request_uri"));
			}
			if (ru && Z_TYPE_P(ru) == IS_STRING) {
				ctx->path = emalloc(Z_STRLEN_P(ru) + 1);
				ctx->path_len = leftByChar(ctx->path, Z_STRVAL_P(ru), '?');
			}
		}
	}
}

/* [GENE_PERF:2026-04-23] Same shallow-share rationale as set_server_val. */
static void gene_request_set_header_val(zval *header) {
	setVal(7, header);
}

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(geme_request_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(geme_request_get_param_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(geme_request_url_param, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_request_init_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, get)
	ZEND_ARG_INFO(0, post)
	ZEND_ARG_INFO(0, cookie)
	ZEND_ARG_INFO(0, server)
	ZEND_ARG_INFO(0, env)
	ZEND_ARG_INFO(0, files)
	ZEND_ARG_INFO(0, request)
	ZEND_ARG_INFO(0, header)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_request_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_request_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */

zval * request_query(zend_ulong type, char * name, size_t len) {
	zval *carrier = NULL, *ret;
	bool jit_initialization = PG(auto_globals_jit);

	switch (type) {
	case TRACK_VARS_POST:
	case TRACK_VARS_GET:
	case TRACK_VARS_FILES:
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_COOKIE:
		/* PHP 8+ auto_globals_jit: $_COOKIE is populated on first use. Reading
		 * PG(http_globals)[TRACK_VARS_COOKIE] from C without activating the
		 * autoglobal left the array empty (e.g. Gene_Session ctor before any
		 * userland $_COOKIE access), breaking session id under php-cgi. */
		if (jit_initialization) {
			static zend_string *cookie_str = NULL;
			if (UNEXPECTED(!cookie_str)) {
				cookie_str = zend_string_init_interned("_COOKIE", sizeof("_COOKIE") - 1, 1);
			}
			zend_is_auto_global(cookie_str);
		}
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_ENV:
		if (jit_initialization) {
			static zend_string *env_str = NULL;
			if (UNEXPECTED(!env_str)) {
				env_str = zend_string_init_interned("_ENV", sizeof("_ENV") - 1, 1);
			}
			zend_is_auto_global(env_str);
		}
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_SERVER:
		if (jit_initialization) {
			static zend_string *server_str = NULL;
			if (UNEXPECTED(!server_str)) {
				server_str = zend_string_init_interned("_SERVER", sizeof("_SERVER") - 1, 1);
			}
			zend_is_auto_global(server_str);
		}
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_REQUEST:
		if (jit_initialization) {
			static zend_string *request_str = NULL;
			if (UNEXPECTED(!request_str)) {
				request_str = zend_string_init_interned("_REQUEST", sizeof("_REQUEST") - 1, 1);
			}
			zend_is_auto_global(request_str);
		}
		carrier = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_REQUEST"));
		break;
	default:
		break;
	}

	if (!carrier) {
		return NULL;
	}
	if (len <= 0) {
		return carrier;
	}
	if ((ret = zend_hash_str_find(Z_ARRVAL_P(carrier), (char *) name, len)) == NULL) {
		return NULL;
	}
	return ret;
}

/** {{{ void gene_merge_query_into_get(const char *qs, size_t qs_len)
 * Merge a raw query string into PHP's $_GET (same path as parse_str(): treat_data).
 * Direct treat_data avoids call_user_function / EG(function_table) lookup per request.
 */
void gene_merge_query_into_get(const char *qs, size_t qs_len)
{
	char *buf;
	zval *get_carrier;
	void (*treat_data)(int arg, char *str, zval *dest_array);

	if (!qs || qs_len == 0) {
		return;
	}

	get_carrier = request_query(TRACK_VARS_GET, NULL, 0);
	if (!get_carrier || Z_TYPE_P(get_carrier) != IS_ARRAY) {
		return;
	}

	buf = estrndup(qs, qs_len);
	if (UNEXPECTED(!buf)) {
		return;
	}

	treat_data = sapi_module.treat_data;
	if (UNEXPECTED(!treat_data)) {
		treat_data = php_default_treat_data;
	}
	treat_data(PARSE_STRING, buf, get_carrier);
}
/* }}} */

void setVal(zend_ulong type, zval *value) {
	zval *attr = gene_request_attr();

	if (Z_TYPE_P(attr) ==  IS_ARRAY) {
		Z_TRY_ADDREF_P(value);
		zend_hash_index_update(Z_ARRVAL_P(attr), type, value);
	}
}

zval *getVal(zend_ulong type, char *name, size_t len) {
	zval *attr = gene_request_attr();
	zval *val = NULL;

	if (EXPECTED(Z_TYPE_P(attr) == IS_ARRAY)) {
		val = zend_hash_index_find(Z_ARRVAL_P(attr), type);
		if (UNEXPECTED(val == NULL)) {
			/* [GENE_PERF:2026-04-24 v5.5.8] Slow path (first touch of this
			 * track-var per request). Use the zend_hash_index_update return
			 * value directly instead of re-querying, shaving one hash probe
			 * off every initial GET/POST/... access. */
			zval *source = request_query(type, NULL, 0);
			if (source) {
				Z_TRY_ADDREF_P(source);
				val = zend_hash_index_update(Z_ARRVAL_P(attr), type, source);
			}
		}
		if (len == 0) {
			return val;
		}
		if (val && Z_TYPE_P(val) == IS_ARRAY) {
			zval *result = zend_hash_str_find(Z_ARRVAL_P(val), name, len);
			if (result) {
				return result;
			}
			/* [GENE_PERF] For SERVER vars, try lowercase if uppercase lookup failed */
			if (type == TRACK_VARS_SERVER && len > 0 && len < 256) {
				char lower_name[256];
				memcpy(lower_name, name, len);
				lower_name[len] = '\0';
				gene_strtolower(lower_name);
				return zend_hash_str_find(Z_ARRVAL_P(val), lower_name, len);
			}
			/* [GENE_PERF] For HEADER vars, try lowercase if uppercase lookup failed */
			if (type == 7 && len > 0 && len < 256) {
				char lower_name[256];
				memcpy(lower_name, name, len);
				lower_name[len] = '\0';
				gene_strtolower(lower_name);
				return zend_hash_str_find(Z_ARRVAL_P(val), lower_name, len);
			}
			return NULL;
		}
		return NULL;
	}
	return NULL;
}

/*
 * {{{ gene_request
 */
PHP_METHOD(gene_request, __construct) {
	zend_long debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_request::get(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, get, TRACK_VARS_GET);
/* }}} */

/** {{{ public gene_request::post(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, post, TRACK_VARS_POST);
/* }}} */

/** {{{ public gene_request::request(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, request, TRACK_VARS_REQUEST);
/* }}} */

/** {{{ public gene_request::files(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, files, TRACK_VARS_FILES);
/* }}} */

/** {{{ public gene_request::cookie(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, cookie, TRACK_VARS_COOKIE);
/* }}} */

/** {{{ public gene_request::server(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, server, TRACK_VARS_SERVER);
/* }}} */

/** {{{ public gene_request::env(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, env, TRACK_VARS_ENV);
/* }}} */

/** {{{ public gene_request::isGet(void)
 */
GENE_REQUEST_IS_METHOD(gene_request, Get);
/* }}} */

/** {{{ public gene_request::isPost(void)
 */
GENE_REQUEST_IS_METHOD(gene_request, Post);
/* }}} */

/** {{{ public gene_request::isPut(void)
 */
GENE_REQUEST_IS_METHOD(gene_request, Put);
/* }}} */

/** {{{ public gene_request::isHead(void)
 */
GENE_REQUEST_IS_METHOD(gene_request, Head);
/* }}} */

/** {{{ public gene_request::isOptions(void)
 */
GENE_REQUEST_IS_METHOD(gene_request, Options);
/* }}} */

/** {{{ public gene_request::isOptions(void)
 */
GENE_REQUEST_IS_METHOD(gene_request, Delete);
/* }}} */

/** {{{ public gene_request::isCli(void)
 */
GENE_REQUEST_IS_METHOD(gene_request, Cli);
/* }}} */

/** {{{ public gene_request::header(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_request, header, 7);
/* }}} */

/** {{{ public gene_request::isAjax()
 */
PHP_METHOD(gene_request, isAjax) {
	zval *header = getVal(TRACK_VARS_SERVER, ZEND_STRL("HTTP_X_REQUESTED_WITH"));
	if (header && Z_TYPE_P(header) == IS_STRING && strncasecmp("XMLHttpRequest", Z_STRVAL_P(header), Z_STRLEN_P(header)) == 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_request::getMethod()
 */
PHP_METHOD(gene_request, getMethod) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->method) {
		/* [GENE_PERF:2026-04-20] RETURN_STRINGL with cached length avoids strlen(). */
		RETURN_STRINGL(ctx->method, ctx->method_len);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_request::params($name)
 */
PHP_METHOD(gene_request, params) {
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
 * {{{ public gene_request::init($get, $post, $cookie, $server, $env, $files, $request)
 */
PHP_METHOD(gene_request, init) {
	zval *get = NULL, *post = NULL, *cookie = NULL, *server = NULL, *env = NULL, *files = NULL, *request = NULL, *header = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|zzzzzzzz", &get, &post, &cookie, &server, &env, &files, &request, &header) == FAILURE) {
		return;
	}
	if (post && Z_TYPE_P(post) == IS_ARRAY) {
		setVal(0, post);
	}
	if (get && Z_TYPE_P(get) == IS_ARRAY) {
		setVal(1, get);
	}
	if (cookie && Z_TYPE_P(cookie) == IS_ARRAY) {
		setVal(2, cookie);
	}
	if (server && Z_TYPE_P(server) == IS_ARRAY) {
		gene_request_set_server_val(server);
	}
	if (env && Z_TYPE_P(env) == IS_ARRAY) {
		setVal(4, env);
	}
	if (files && Z_TYPE_P(files) == IS_ARRAY) {
		setVal(5, files);
	}
	if (request && Z_TYPE_P(request) == IS_ARRAY) {
		setVal(6, request);
	} else {
		zval merged;
		zend_long get_count = (get && Z_TYPE_P(get) == IS_ARRAY) ? zend_hash_num_elements(Z_ARRVAL_P(get)) : 0;
		zend_long post_count = (post && Z_TYPE_P(post) == IS_ARRAY) ? zend_hash_num_elements(Z_ARRVAL_P(post)) : 0;
		zend_long total_count = get_count + post_count;

		/* [GENE_PERF] Pre-allocate array size to avoid reallocation */
		array_init_size(&merged, total_count);

		if (get && Z_TYPE_P(get) == IS_ARRAY) {
			zend_hash_copy(Z_ARRVAL(merged), Z_ARRVAL_P(get), (copy_ctor_func_t) zval_add_ref);
		}
		if (post && Z_TYPE_P(post) == IS_ARRAY) {
			zend_hash_copy(Z_ARRVAL(merged), Z_ARRVAL_P(post), (copy_ctor_func_t) zval_add_ref);
		}
		setVal(6, &merged);
		zval_ptr_dtor(&merged);
	}
	if (header && Z_TYPE_P(header) == IS_ARRAY) {
		gene_request_set_header_val(header);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 *  {{{ public static gene_request::get($name)
 */
PHP_METHOD(gene_request, _get) {
	char *name = NULL;
	zend_long name_len = 0;
	zval *ret = NULL;
	int i = 0;
	const char *valType[8] = { "post", "get", "cookie", "server", "env", "files",
			"request", "header"};

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	for (i = 0; i < 8; i++) {
		if (strcmp(valType[i], name) == 0) {
			ret = getVal(i, NULL, 0);
			break;
		}
	}
	if (ret) {
		RETURN_ZVAL(ret, 1, 0);
	}
	RETURN_NULL();
}
/* }}} */

/*
 *  {{{ public static gene_request::set($name, $value)
 */
PHP_METHOD(gene_request, _set) {
	char *name = NULL;
	zend_long name_len = 0;
	zval *value = NULL;
	int i = 0;
	const char *valType[8] = { "post", "get", "cookie", "server", "env", "files",
			"request", "header"};
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "sz", &name, &name_len, &value) == FAILURE) {
		RETURN_NULL();
	}

	for (i = 0; i < 8; i++) {
		if (strcmp(valType[i], name) == 0) {
			setVal(i, value);
			break;
		}
	}
	RETURN_TRUE;
}
/* }}} */


/*
 * {{{ public gene_request::clear()
 */
PHP_METHOD(gene_request, clear) {
	gene_request_context *ctx = gene_request_ctx();
	if (Z_TYPE(ctx->request_attr) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->request_attr);
	}
	ZVAL_UNDEF(&ctx->request_attr);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_request_methods
 */
const zend_function_entry gene_request_methods[] = {
	PHP_ME(gene_request, __construct, geme_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_request, get, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, request, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, post, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, cookie, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, files, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, server, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, env, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, header, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isAjax, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, params, geme_request_url_param, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, getMethod, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isGet, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isPost, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isPut, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isHead, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isOptions, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isCli, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, init, gene_request_init_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, clear, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_MALIAS(gene_request, __set, _set, gene_request_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_request, __get, _get, gene_request_get_arginfo, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(request) {
	zend_class_entry gene_request;
	GENE_INIT_CLASS_ENTRY(gene_request, "Gene_Request", "Gene\\Request", gene_request_methods);
	gene_request_ce = zend_register_internal_class(&gene_request);
#if PHP_VERSION_ID >= 80200
	gene_request_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

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
