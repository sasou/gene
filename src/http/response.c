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
#include "Zend/zend_smart_str.h"
#include "Zend/zend_interfaces.h"
#include "ext/standard/php_string.h"

#include "../gene.h"
#include "../http/response.h"
#include "../cache/memory.h"
#include "../di/di.h"

zend_class_entry * gene_response_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_response_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_se, 0, 0, 2)
    ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_se_data, 0, 0, 4)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, count)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_se_json, 0, 0, 3)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_redirect, 0, 0, 2)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_redirect_js, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_alert, 0, 0, 2)
    ZEND_ARG_INFO(0, text)
    ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_header, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_cookie, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_url, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_end, 0, 0, 0)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

/* {{{ gene_response_context_obj - get response object from DI */
zval *gene_response_context_obj(void) {
	if (GENE_G(runtime_type) >= 2) {
		/* [GENE_PERF:2026-05-04] Cache resolved response object in the per-request
		 * context so every header/redirect/cookie/end call skips the DI hash lookup.
		 * The context owns a refcount; cleared in gene_request_context_free_fields. */
		gene_request_context *ctx = gene_request_ctx();
		if (ctx && Z_TYPE(ctx->response_obj) == IS_OBJECT) {
			return &ctx->response_obj;
		}
		/* [GENE_FIX:2026-04-09] Use interned string to avoid repeated heap allocations. */
		static zend_string *response_key = NULL;
		if (UNEXPECTED(response_key == NULL)) {
			response_key = zend_string_init_interned("response", sizeof("response") - 1, 1);
		}
		zval *resp = gene_di_get(response_key);
		if (resp && Z_TYPE_P(resp) == IS_OBJECT) {
			if (ctx) {
				ZVAL_COPY(&ctx->response_obj, resp);
				return &ctx->response_obj;
			}
			return resp;
		}
	}
	return NULL;
}
/* }}} */

/* [GENE_PERF:2026-04-23] Per-method zend_function* cache for Swoole response
 * dispatch. Keyed by (class_entry*, cached fn*) so it invalidates if Swoole
 * is somehow reloaded inside the same process (test doubles etc.). Steady
 * state in a production worker: single load + pointer compare per call.
 * Replaces a zend_hash_str_find_ptr HashTable walk on every response.header/
 * redirect/cookie/end call — those run on the hot path of every Swoole
 * request response. */
#define GENE_SWOOLE_RESP_METHOD(ce, name_tok) \
	gene_swoole_resp_method_cached((ce), #name_tok, sizeof(#name_tok) - 1, \
		&gene_swoole_resp_cache_##name_tok##_ce, \
		&gene_swoole_resp_cache_##name_tok##_fn)

static inline zend_function *gene_swoole_resp_method_cached(
		zend_class_entry *ce, const char *name, size_t name_len,
		zend_class_entry **cached_ce, zend_function **cached_fn) {
	if (EXPECTED(*cached_ce == ce && *cached_fn != NULL)) {
		return *cached_fn;
	}
	*cached_fn = zend_hash_str_find_ptr(&ce->function_table, name, name_len);
	*cached_ce = ce;
	return *cached_fn;
}

static zend_class_entry *gene_swoole_resp_cache_redirect_ce = NULL;
static zend_function    *gene_swoole_resp_cache_redirect_fn = NULL;
static zend_class_entry *gene_swoole_resp_cache_header_ce = NULL;
static zend_function    *gene_swoole_resp_cache_header_fn = NULL;
static zend_class_entry *gene_swoole_resp_cache_cookie_ce = NULL;
static zend_function    *gene_swoole_resp_cache_cookie_fn = NULL;
static zend_class_entry *gene_swoole_resp_cache_end_ce = NULL;
static zend_function    *gene_swoole_resp_cache_end_fn = NULL;

/** {{{ void gene_response_set_redirect(char *url, zend_long code)
 */
void gene_response_set_redirect(char *url, zend_long code) {
	zval *swoole_resp = gene_response_context_obj();
	if (swoole_resp) {
		zend_function *fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), redirect);
		/* [GENE_FIX:2026-05-19] Only short-circuit when we have a callable method.
		 * If Swoole\Http\Response::redirect cannot be resolved (e.g. ext reloaded
		 * under test doubles, or the cached object is a non-Swoole stub), fall
		 * through to the sapi_header_op path so a redirect is still emitted. */
		if (EXPECTED(fn)) {
			zval retval, zurl, zcode;
			ZVAL_UNDEF(&retval);
			ZVAL_STRING(&zurl, url);
			ZVAL_LONG(&zcode, code);
			zval params[] = { zurl, zcode };
			zend_call_known_function(fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), &retval, 2, params, NULL);
			zval_ptr_dtor(&zurl);
			zval_ptr_dtor(&retval);
			return;
		}
		/* fall through to sapi fallback */
	}
	sapi_header_line ctr = { 0 };
	size_t header_len = strlen("Location:") + strlen(url) + 1;
	char header_buf[1024];
	int header_heap = 0;
	char *header_ptr = header_buf;
	if (header_len >= sizeof(header_buf)) {
		header_ptr = emalloc(header_len + 1);
		header_heap = 1;
	}
	snprintf(header_ptr, header_len + 1, "%s %s", "Location:", url);
	ctr.line = header_ptr;
	ctr.line_len = header_len;
	ctr.response_code = code;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
	if (header_heap) {
		efree(header_ptr);
	}
}
/* }}} */

/** {{{ void gene_response_set_header(char *key, char *value)
 */
static void gene_response_set_header_ex(char *key, size_t key_len, char *value, size_t value_len) {
	zval *swoole_resp = gene_response_context_obj();
	if (swoole_resp) {
		zend_function *fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), header);
		/* [GENE_FIX:2026-05-19] Same fallback rationale as gene_response_set_redirect:
		 * if Swoole\Http\Response::header cannot be resolved, do not silently drop
		 * the header — fall through to the sapi_header_op path below. */
		if (EXPECTED(fn)) {
			zval retval, zkey, zval_v;
			ZVAL_UNDEF(&retval);
			ZVAL_STRINGL(&zkey, key, key_len);
			ZVAL_STRINGL(&zval_v, value, value_len);
			zval params[] = { zkey, zval_v };
			zend_call_known_function(fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), &retval, 2, params, NULL);
			zval_ptr_dtor(&zkey);
			zval_ptr_dtor(&zval_v);
			zval_ptr_dtor(&retval);
			return;
		}
		/* fall through to sapi fallback */
	}
	sapi_header_line ctr = { 0 };
	size_t header_len = key_len + value_len + 1;
	char header_buf[1024];
	if (header_len < sizeof(header_buf)) {
		memcpy(header_buf, key, key_len);
		header_buf[key_len] = ':';
		memcpy(header_buf + key_len + 1, value, value_len);
		header_buf[header_len] = '\0';
		ctr.line = header_buf;
		ctr.line_len = header_len;
		sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
	} else {
		char *header_ptr = emalloc(header_len + 1);
		memcpy(header_ptr, key, key_len);
		header_ptr[key_len] = ':';
		memcpy(header_ptr + key_len + 1, value, value_len);
		header_ptr[header_len] = '\0';
		ctr.line = header_ptr;
		ctr.line_len = header_len;
		ctr.response_code = 200;
		sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
		efree(header_ptr);
	}
}

void gene_response_set_header(char *key, char *value) {
	gene_response_set_header_ex(key, strlen(key), value, strlen(value));
}
/* }}} */

void gene_response_cookie(zval *name, zval *value, zval *expires, zval *path, zval *domain,zval *secure, zval *httponly, zval *retval) /*{{{*/
{
	zval *swoole_resp = gene_response_context_obj();
	if (swoole_resp) {
		zend_function *cookie_fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), cookie);
		if (EXPECTED(cookie_fn)) {
			zval params[7];
			int num = 1;
			params[0] = *name;
			if (value) { num = 2; params[1] = *value; }
			if (expires) { num = 3; params[2] = *expires; }
			if (path) { num = 4; params[3] = *path; }
			if (domain) { num = 5; params[4] = *domain; }
			if (secure) { num = 6; params[5] = *secure; }
			if (httponly) { num = 7; params[6] = *httponly; }
			zend_call_known_function(cookie_fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), retval, num, params, NULL);
			return;
		}
		/* [GENE_FIX:2026-05-19] Method missing on the Swoole response (e.g. stub
		 * test double). Mirror the setcookie-missing branch below: signal failure
		 * via retval=false so callers can distinguish from success. */
		if (retval) ZVAL_FALSE(retval);
		return;
	}
    /* [GENE_FIX:2026-04-27] Per-call lookup (CG(function_table) is per-thread
     * under ZTS) + NULL guard: setcookie may be disabled / unavailable, and
     * zend_call_known_function dereferences fn without checking. */
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("setcookie"));
    if (UNEXPECTED(!fn)) {
        if (retval) ZVAL_FALSE(retval);
        return;
    }
    zval params[7];
    int num = 1;
    params[0] = *name;
    if (value) {
    	num = 2;
        params[1] = *value;
    }
    if (expires) {
    	num = 3;
        params[2] = *expires;
    }
    if (path) {
    	num = 4;
        params[3] = *path;
    }
    if (domain) {
    	num = 5;
        params[4] = *domain;
    }
    if (secure) {
    	num = 6;
        params[5] = *secure;
    }
    if (httponly) {
    	num = 7;
        params[6] = *httponly;
    }
    zend_call_known_function(fn, NULL, NULL, retval, num, params, NULL);
}/*}}}*/

/*
 * {{{ gene_response
 */
PHP_METHOD(gene_response, __construct) {
	zend_long debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
}
/* }}} */

/** {{{ proto public gene_response::redirect(string $url)
 */
PHP_METHOD(gene_response, redirect) {
	zend_string *url;
	zend_long code = 302;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &url, &code) == FAILURE) {
		return;
	}

	gene_response_set_redirect(ZSTR_VAL(url), code);
	RETURN_TRUE;
}
/* }}} */

/** {{{ void gene_response_redirect_js(zend_string *url)
 */
void gene_response_redirect_js(zend_string *url) {
	zend_string *escaped_url = php_addslashes(url);
	php_printf("<script type=\"text/javascript\">\n");
	php_printf("window.location.href=\"%s\";\n", ZSTR_VAL(escaped_url));
	php_printf("</script>\n");
	zend_string_release(escaped_url);
}
/* }}} */

/** {{{ proto public gene_response::redirectJs(string $url)
 */
PHP_METHOD(gene_response, redirectJs) {
	zend_string *url;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &url) == FAILURE) {
		return;
	}
	gene_response_redirect_js(url);
}


/* }}} */

/** {{{ void gene_response_alert(zend_string *text, zend_string *url)
 */
void gene_response_alert(zend_string *text, zend_string *url) {
	zend_string *escaped_text = php_addslashes(text);
	php_printf("\n<script type=\"text/javascript\">\nalert(\"%s\");\n", ZSTR_VAL(escaped_text));
	zend_string_release(escaped_text);
	if (url && ZSTR_LEN(url)) {
		zend_string *escaped_url = php_addslashes(url);
		php_printf("window.location.href=\"%s\";\n", ZSTR_VAL(escaped_url));
		zend_string_release(escaped_url);
	}
	php_printf("</script>\n");
}
/* }}} */

/** {{{ proto public gene_response::alert(string $text, string $url = NULL)
 */
PHP_METHOD(gene_response, alert) {
	zend_string *text, *url = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S", &text, &url) == FAILURE) {
		return;
	}
	gene_response_alert(text, url);
}
/* }}} */

/** {{{ proto public gene_response::success(string $text, int $code)
 */
PHP_METHOD(gene_response, success) {
	zend_string *text;
	zend_long code = 2000;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), zend_string_copy(text));
}
/* }}} */


/** {{{ proto public gene_response::error(string $text, int $code)
 */
PHP_METHOD(gene_response, error) {
	zend_string *text;
	zend_long code = 4000;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), zend_string_copy(text));
}
/* }}} */

/** {{{ proto public gene_response::data(mixed $data, int $count,string $text, int $code)
 */
PHP_METHOD(gene_response, data) {
	zval *data = NULL;
	zend_long count = -1;
	zend_string *text = NULL;
	zend_long code = 2000;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|lSl", &data, &count, &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	if (text) {
		add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), zend_string_copy(text));
	}
	Z_TRY_ADDREF_P(data);
	add_assoc_zval_ex(return_value, ZEND_STRL(GENE_RESPONSE_DATA), data);
	if (count >= 0) {
		add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_COUNT), count);
	}
}
/* }}} */

/** {{{ proto public gene_response::json(array $json, int $code)
 */
PHP_METHOD(gene_response, json) {
	zval *data = NULL;
	char *callback = NULL;
	zend_long code = 256;
    zval json_opt;
    zval ret;
    zend_long callback_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|sl", &data, &callback, &callback_len, &code) == FAILURE) {
		return;
	}
	ZVAL_LONG(&json_opt, code);
	ZVAL_UNDEF(&ret);
	{
		static zend_function *json_fn = NULL;
		if (UNEXPECTED(!json_fn)) {
			json_fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("json_encode"));
		}
		if (EXPECTED(json_fn)) {
			zval params[] = { *data, json_opt };
			zend_call_known_function(json_fn, NULL, NULL, &ret, 2, params, NULL);
		}
	}
	if (Z_TYPE(ret) == IS_STRING) {
		if (callback_len) {
			php_write(callback, callback_len);
			php_write(ZEND_STRL("("));
		}
		php_write(Z_STRVAL(ret), Z_STRLEN(ret));
		if (callback_len) {
			php_write(ZEND_STRL(")"));
		}
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
    zval_ptr_dtor(&ret);
    RETURN_FALSE;
}
/* }}} */

/** {{{ proto public gene_response::header(array $json, int $code)
 */
PHP_METHOD(gene_response, header) {
	char *key, *value;
	zend_long key_len = 0, value_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &key, &key_len, &value, &value_len) == FAILURE) {
		return;
	}

	gene_response_set_header_ex(key, key_len, value, value_len);
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public gene_response::cookie(array $json, int $code)
 */
PHP_METHOD(gene_response, cookie) {
	zval *name = NULL, *value = NULL, *expires = NULL, *path = NULL, *domain = NULL, *secure = NULL, *httponly = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|zzzzzz", &name, &value, &expires, &path, &domain, &secure, &httponly) == FAILURE) {
		return;
	}
	gene_response_cookie(name, value, expires, path, domain, secure, httponly, return_value);
}
/* }}} */

/** {{{ public gene_response::url(string $path)
 *  返回带当前语言前缀的 URL，如 url("login.html") => "/en/login.html"
 */
PHP_METHOD(gene_response, url) {
	zend_string *path_str;
	const char *p;
	size_t path_len;
	gene_request_context *ctx;
	const char *lang;
	size_t lang_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &path_str) == FAILURE) {
		return;
	}
	p = ZSTR_VAL(path_str);
	path_len = ZSTR_LEN(path_str);
	/* 跳过 path 前导斜杠 */
	for (; path_len > 0 && *p == '/'; p++, path_len--) {}
	ctx = gene_request_ctx();
	/* [GENE_PERF:2026-04-27] Use cached ctx->lang_len; eliminates strlen per call. */
	if (ctx->lang && ctx->lang[0] != '\0') {
		lang = ctx->lang;
		lang_len = ctx->lang_len;
	} else {
		lang = NULL;
		lang_len = 0;
	}
	if (path_len == 0) {
		/* 如果只有斜杠，也加上语言前缀 */
		if (lang) {
			size_t out_len = lang_len + 2;
			char out_buf[256];
			char *out_ptr = out_buf;
			int out_heap = 0;
			if (out_len >= sizeof(out_buf)) {
				out_ptr = emalloc(out_len + 1);
				out_heap = 1;
			}
			out_ptr[0] = '/';
			memcpy(out_ptr + 1, lang, lang_len);
			out_ptr[lang_len + 1] = '/';
			out_ptr[lang_len + 2] = '\0';
			RETVAL_STRINGL(out_ptr, out_len);
			if (out_heap) {
				efree(out_ptr);
			}
		} else {
			RETURN_STRING("/");
		}
		return;
	}
	if (lang) {
		/* [GENE_PERF:2026-04-27] memcpy + RETVAL_STRINGL — same pattern as Gene\Controller::url(). */
		size_t out_len = lang_len + path_len + 2;
		char out_buf[512];
		char *out_ptr = out_buf;
		int out_heap = 0;
		if (out_len >= sizeof(out_buf)) {
			out_ptr = emalloc(out_len + 1);
			out_heap = 1;
		}
		out_ptr[0] = '/';
		memcpy(out_ptr + 1, lang, lang_len);
		out_ptr[lang_len + 1] = '/';
		memcpy(out_ptr + lang_len + 2, p, path_len);
		out_ptr[out_len] = '\0';
		RETVAL_STRINGL(out_ptr, out_len);
		if (out_heap) {
			efree(out_ptr);
		}
	} else {
		size_t out_len = path_len + 1;
		char out_buf[512];
		char *out_ptr = out_buf;
		int out_heap = 0;
		if (out_len >= sizeof(out_buf)) {
			out_ptr = emalloc(out_len + 1);
			out_heap = 1;
		}
		out_ptr[0] = '/';
		memcpy(out_ptr + 1, p, path_len);
		out_ptr[out_len] = '\0';
		RETVAL_STRINGL(out_ptr, out_len);
		if (out_heap) {
			efree(out_ptr);
		}
	}
}
/* }}} */

/** {{{ proto public gene_response::end(string $data)
 */
PHP_METHOD(gene_response, end) {
	zend_string *data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &data) == FAILURE) {
		return;
	}

	zval *swoole_resp = gene_response_context_obj();
	if (swoole_resp) {
		zend_function *end_fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), end);
		/* [GENE_FIX:2026-05-19] If Swoole\Http\Response::end is unresolvable,
		 * fall through to php_write so the body is still flushed via the SAPI. */
		if (EXPECTED(end_fn)) {
			zval retval;
			ZVAL_UNDEF(&retval);
			if (data && ZSTR_LEN(data) > 0) {
				zval zdata;
				ZVAL_STR_COPY(&zdata, data);
				zval params[] = { zdata };
				zend_call_known_function(end_fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), &retval, 1, params, NULL);
				zval_ptr_dtor(&zdata);
			} else {
				zend_call_known_function(end_fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), &retval, 0, NULL, NULL);
			}
			zval_ptr_dtor(&retval);
			RETURN_TRUE;
		}
		/* fall through to php_write fallback below */
	}
	if (data && ZSTR_LEN(data) > 0) {
		php_write(ZSTR_VAL(data), ZSTR_LEN(data));
	}
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public gene_response::setJsonHeader()
 */
PHP_METHOD(gene_response, setJsonHeader) {
	gene_response_set_header("Content-Type", "application/json; charset=UTF-8");
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public gene_response::setHtmlHeader(array $json, int $code)
 */
PHP_METHOD(gene_response, setHtmlHeader) {
	gene_response_set_header("Content-Type", "text/html; charset=UTF-8");
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_response_methods
 */
const zend_function_entry gene_response_methods[] = {
	PHP_ME(gene_response, redirect, gene_response_arg_redirect, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, redirectJs, gene_response_arg_redirect_js, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, alert, gene_response_arg_alert, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, success, gene_response_arg_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, error, gene_response_arg_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, data, gene_response_arg_se_data, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, json, gene_response_arg_se_json, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, header, gene_response_arg_header, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, cookie, gene_response_arg_cookie, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, url, gene_response_arg_url, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, end, gene_response_arg_end, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, setJsonHeader, gene_response_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, setHtmlHeader, gene_response_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, __construct, gene_response_void_arginfo, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(response) {
	zend_class_entry gene_response;
	GENE_INIT_CLASS_ENTRY(gene_response, "Gene_Response", "Gene\\Response", gene_response_methods);
	gene_response_ce = zend_register_internal_class(&gene_response);
#if PHP_VERSION_ID >= 80200
	gene_response_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

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
