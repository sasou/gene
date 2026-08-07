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
#include "main/php_streams.h"
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
    ZEND_ARG_INFO(0, expires)
    ZEND_ARG_INFO(0, path)
    ZEND_ARG_INFO(0, domain)
    ZEND_ARG_INFO(0, secure)
    ZEND_ARG_INFO(0, httponly)
    ZEND_ARG_INFO(0, samesite)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_url, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_end, 0, 0, 0)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_send_file, 0, 0, 1)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO(0, length)
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
		/* [GENE_FIX:2026-04-09] Use interned string to avoid repeated heap allocations.
		 * [GENE_FIX:2026-05-24] Routed through gene_interned_str_persistent so the
		 * cached zend_string is invalidated whenever the runtime cannot grant
		 * IS_STR_PERMANENT (opcache.file_cache_only=1, opcache disabled, or CLI),
		 * preventing dangling-pointer reads on the next request. */
		GENE_INTERNED_STR(response_key, "response");
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
		/* [GENE_PERF:2026-05-19] In Swoole mode sapi_header_op is a no-op (Swoole
		 * bypasses the PHP SAPI output layer entirely). If the Swoole method
		 * cannot be resolved here, there is no useful fallback — just return. */
		zend_function *fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), redirect);
		if (UNEXPECTED(!fn)) return;
		zval retval, zurl, zcode;
		ZVAL_UNDEF(&retval);
		ZVAL_STRING(&zurl, url);
		ZVAL_LONG(&zcode, code);
		zval params[] = { zurl, zcode };
		zend_call_known_function(fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), &retval, 2, params, NULL);
		zval_ptr_dtor(&zurl);
		/* [GENE_FEATURE:2026-08-07] Track the last status code so
		 * Response::getStatusCode() can report it in Swoole mode (Swoole's
		 * response object exposes no status getter).
		 * [GENE_FIX:2026-08-07] Only record when the redirect actually
		 * succeeded (not on exception / false return). */
		if (!EG(exception) && !Z_ISUNDEF(retval) && Z_TYPE(retval) != IS_FALSE) {
			gene_request_ctx()->response_status = code;
		}
		zval_ptr_dtor(&retval);
		return;
	}
	/* [GENE_PERF:2026-05-21 F7] FPM redirect hot path: replace
	 *   strlen("Location:") + strlen(url) + 1 + snprintf("%s %s", ...)
	 * with a compile-time-known literal length and two memcpy calls.
	 * sizeof("Location: ") - 1 is a compile-time constant (the literal
	 * already embeds the trailing space) - no runtime strlen, no snprintf
	 * format-parser overhead. snprintf is significantly slower than memcpy
	 * for short strings due to varargs/parser dispatch. */
	sapi_header_line ctr = { 0 };
	size_t url_len = strlen(url);
	size_t header_len = sizeof("Location: ") - 1 + url_len; /* 10 + url_len */
	char header_buf[1024];
	int header_heap = 0;
	char *header_ptr = header_buf;
	if (header_len + 1 > sizeof(header_buf)) {
		header_ptr = emalloc(header_len + 1);
		header_heap = 1;
	}
	memcpy(header_ptr, "Location: ", sizeof("Location: ") - 1);
	memcpy(header_ptr + sizeof("Location: ") - 1, url, url_len);
	header_ptr[header_len] = '\0';
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
		/* [GENE_PERF:2026-05-19] Swoole mode: sapi_header_op is a no-op, no fallback. */
		zend_function *fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), header);
		if (UNEXPECTED(!fn)) return;
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
		sapi_header_op(SAPI_HEADER_REPLACE, &ctr);
		efree(header_ptr);
	}
}

void gene_response_set_header(char *key, char *value) {
	gene_response_set_header_ex(key, strlen(key), value, strlen(value));
}
/* }}} */

void gene_response_cookie(zval *name, zval *value, zval *expires, zval *path, zval *domain,zval *secure, zval *httponly, zval *samesite, zval *retval) /*{{{*/
{
	zval *swoole_resp = gene_response_context_obj();
	/* [GENE_FEAT] Browsers (Chrome 80+, Firefox, ...) silently drop a cookie
	 * with SameSite=None unless it is also marked Secure. Detect the None case
	 * (case-insensitive) and force Secure so the cookie is actually accepted,
	 * regardless of the caller's/session's secure flag. */
	zend_bool has_samesite = (samesite && Z_TYPE_P(samesite) == IS_STRING && Z_STRLEN_P(samesite) > 0);
	zend_bool samesite_none = (has_samesite && zend_string_equals_literal_ci(Z_STR_P(samesite), "None"));
	zval secure_forced;
	zval *secure_eff = secure;
	if (samesite_none) {
		ZVAL_TRUE(&secure_forced);
		secure_eff = &secure_forced;
	}
	if (swoole_resp) {
		zend_function *cookie_fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), cookie);
		if (EXPECTED(cookie_fn)) {
			zval params[8];
			int num = 1;
			params[0] = *name;
			if (value) { num = 2; params[1] = *value; }
			if (expires) { num = 3; params[2] = *expires; }
			if (path) { num = 4; params[3] = *path; }
			if (domain) { num = 5; params[4] = *domain; }
			if (secure_eff) { num = 6; params[5] = *secure_eff; }
			if (httponly) { num = 7; params[6] = *httponly; }
			if (has_samesite) { num = 8; params[7] = *samesite; }
			zend_call_known_function(cookie_fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), retval, num, params, NULL);
			return;
		}
		/* [GENE_FIX:2026-05-19] Method missing on the Swoole response (e.g. stub
		 * test double). Mirror the setcookie-missing branch below: signal failure
		 * via retval=false so callers can distinguish from success. */
		if (retval) ZVAL_FALSE(retval);
		return;
	}
	/* [GENE_FIX:2026-05-29] Swoole has no PHP header layer; setcookie() is a
	 * no-op / warning source when Response was not bound or was already cleared
	 * (e.g. Application::cleanup() before Session __destruct). */
	if (GENE_G(runtime_type) >= 2) {
		if (retval) {
			ZVAL_FALSE(retval);
		}
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
    /* [GENE_FEAT] The positional setcookie() signature has no 'samesite'
     * parameter; it is only available via the options-array form added in
     * PHP 7.3. When samesite is requested, build that array and call
     * setcookie(name, value, options) instead of the positional form. */
    if (has_samesite) {
        zval options;
        array_init(&options);
        if (expires && Z_TYPE_P(expires) == IS_LONG) {
            add_assoc_long_ex(&options, ZEND_STRL("expires"), Z_LVAL_P(expires));
        }
        if (path && Z_TYPE_P(path) == IS_STRING) {
            add_assoc_str_ex(&options, ZEND_STRL("path"), zend_string_copy(Z_STR_P(path)));
        }
        if (domain && Z_TYPE_P(domain) == IS_STRING) {
            add_assoc_str_ex(&options, ZEND_STRL("domain"), zend_string_copy(Z_STR_P(domain)));
        }
        if (secure_eff) {
            add_assoc_bool_ex(&options, ZEND_STRL("secure"), zend_is_true(secure_eff));
        }
        if (httponly) {
            add_assoc_bool_ex(&options, ZEND_STRL("httponly"), zend_is_true(httponly));
        }
        add_assoc_str_ex(&options, ZEND_STRL("samesite"), zend_string_copy(Z_STR_P(samesite)));

        zval opt_params[3];
        opt_params[0] = *name;
        if (value) {
            opt_params[1] = *value;
        } else {
            ZVAL_EMPTY_STRING(&opt_params[1]);
        }
        opt_params[2] = options;
        zend_call_known_function(fn, NULL, NULL, retval, 3, opt_params, NULL);
        zval_ptr_dtor(&options);
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
	zval *name = NULL, *value = NULL, *expires = NULL, *path = NULL, *domain = NULL, *secure = NULL, *httponly = NULL, *samesite = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|zzzzzzz", &name, &value, &expires, &path, &domain, &secure, &httponly, &samesite) == FAILURE) {
		return;
	}
	gene_response_cookie(name, value, expires, path, domain, secure, httponly, samesite, return_value);
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
		/* [GENE_PERF:2026-05-19] Swoole mode: php_write does not flush to the
		 * client (Swoole owns the response). If Swoole\Http\Response::end is
		 * unresolvable there is no meaningful fallback — return TRUE silently. */
		zend_function *end_fn = GENE_SWOOLE_RESP_METHOD(Z_OBJCE_P(swoole_resp), end);
		if (UNEXPECTED(!end_fn)) RETURN_TRUE;
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
	if (data && ZSTR_LEN(data) > 0) {
		php_write(ZSTR_VAL(data), ZSTR_LEN(data));
	}
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public gene_response::getStatusCode(): int
 * [GENE_FEATURE:2026-08-07] Last HTTP status code set through this layer.
 * FPM: reads SG(sapi_headers).http_response_code (set by redirect()/header()
 * via sapi_header_op; 0 when nothing was set). Swoole: returns the status
 * tracked in the request context by redirect()/status-setting calls, 0 when
 * nothing was set through Gene\Response (Swoole's response object exposes
 * no status getter).
 */
PHP_METHOD(gene_response, getStatusCode) {
	if (GENE_G(runtime_type) >= 2) {
		RETURN_LONG(gene_request_ctx()->response_status);
	}
	RETURN_LONG(SG(sapi_headers).http_response_code);
}
/* }}} */

/** {{{ proto public gene_response::isSent(): bool
 * [GENE_FEATURE:2026-08-07] Whether the response can no longer be written.
 * FPM: headers already sent (SG(headers_sent)). Swoole: the response object's
 * isWritable() (available since Swoole 4.5); when the method is unresolvable
 * or no response object is bound, reports false.
 */
PHP_METHOD(gene_response, isSent) {
	zval *swoole_resp = gene_response_context_obj();
	if (swoole_resp) {
		zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(swoole_resp)->function_table, ZEND_STRL("iswritable"));
		if (fn) {
			zval retval;
			ZVAL_UNDEF(&retval);
			zend_call_known_function(fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), &retval, 0, NULL, NULL);
			/* [GENE_FIX:2026-08-07] Accept any truthy return, not just
			 * IS_TRUE — a numeric 1 from a Swoole-compatible shim would
			 * otherwise be misread as "already sent". */
			if (!Z_ISUNDEF(retval) && zend_is_true(&retval)) {
				zval_ptr_dtor(&retval);
				RETURN_FALSE;
			}
			if (!Z_ISUNDEF(retval)) {
				zval_ptr_dtor(&retval);
			}
			RETURN_TRUE;
		}
		RETURN_FALSE;
	}
	RETURN_BOOL(SG(headers_sent));
}
/* }}} */

/** {{{ proto public gene_response::sendFile(string $file [, int $offset = 0 [, int $length = 0]]): bool
 * [GENE_FEATURE:2026-08-07] Stream a file as the response body.
 * Swoole: delegates to Swoole\Http\Response::sendfile (kernel sendfile).
 * FPM/CLI: streams the file through php_stream in 8KB chunks + php_write;
 * $offset/$length select a byte range (0 length = to EOF). Returns
 * false when the file cannot be opened.
 */
PHP_METHOD(gene_response, sendFile) {
	zend_string *file;
	zend_long offset = 0, length = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|ll", &file, &offset, &length) == FAILURE) {
		return;
	}
	if (offset < 0 || length < 0) {
		php_error_docref(NULL, E_WARNING, "offset and length must be >= 0");
		RETURN_FALSE;
	}

	zval *swoole_resp = gene_response_context_obj();
	if (swoole_resp) {
		zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(swoole_resp)->function_table, ZEND_STRL("sendfile"));
		if (UNEXPECTED(!fn)) RETURN_FALSE;
		zval retval, zfile, zoffset, zlength;
		ZVAL_UNDEF(&retval);
		ZVAL_STR_COPY(&zfile, file);
		ZVAL_LONG(&zoffset, offset);
		ZVAL_LONG(&zlength, length);
		zval params[] = { zfile, zoffset, zlength };
		zend_call_known_function(fn, Z_OBJ_P(swoole_resp), Z_OBJCE_P(swoole_resp), &retval, 3, params, NULL);
		zval_ptr_dtor(&zfile);
		if (Z_TYPE(retval) == IS_FALSE) {
			zval_ptr_dtor(&retval);
			RETURN_FALSE;
		}
		if (!Z_ISUNDEF(retval)) {
			zval_ptr_dtor(&retval);
		}
		RETURN_TRUE;
	}

	{
		/* [GENE_FIX:2026-08-07] REPORT_PATH does not exist (compile error);
		 * and the FPM path used to read the whole file into one zend_string,
		 * which OOMs on large files — stream in 8KB chunks instead.
		 * [GENE_FIX:2026-08-07-5] plain files only (EX_USE_URL / wrappers rejected) to
		 * close the SSRF surface when $file is derived from user input.
		 * REPORT_ERRORS alone does not gate the wrapper — reject anything that
		 * is not the plain files wrapper (php://filter, data://, and http://
		 * under allow_url_fopen=On would otherwise pass through). */
		php_stream_wrapper *wrapper = php_stream_locate_url_wrapper(ZSTR_VAL(file), NULL, STREAM_LOCATE_WRAPPERS_ONLY);
		if (!wrapper || wrapper != &php_plain_files_wrapper) {
			php_error_docref(NULL, E_WARNING, "sendFile() only accepts local file paths");
			RETURN_FALSE;
		}
		php_stream *stream = php_stream_open_wrapper_ex(ZSTR_VAL(file), "rb", REPORT_ERRORS, NULL, NULL);
		if (!stream) {
			RETURN_FALSE;
		}
		if (offset > 0) {
			if (php_stream_seek(stream, offset, SEEK_SET) != 0) {
				php_stream_close(stream);
				RETURN_FALSE;
			}
			/* [GENE_FIX:2026-08-07-5] regular files happily seek past EOF and return 0;
			 * verify the resulting position so out-of-range offsets fail instead of
			 * silently returning true with an empty body. */
			if ((zend_long)php_stream_tell(stream) != offset) {
				php_stream_close(stream);
				RETURN_FALSE;
			}
		}
		{
			char buf[8192];
			zend_long remaining = length; /* 0 = until EOF */
			while (!php_stream_eof(stream) && (length == 0 || remaining > 0)) {
				size_t want = sizeof(buf);
				if (length > 0 && (size_t)remaining < want) {
					want = (size_t)remaining;
				}
				/* [GENE_FIX:2026-08-07-5] php_stream_read returns ssize_t in PHP 8:
				 * -1 on failure must not be widened to size_t. */
				ssize_t got = php_stream_read(stream, buf, want);
				if (got <= 0) {
					break;
				}
				php_write(buf, (size_t)got);
				if (length > 0) {
					remaining -= (zend_long)got;
				}
			}
		}
		php_stream_close(stream);
		RETURN_TRUE;
	}
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
	/* [GENE_FEATURE:2026-08-07] Status introspection + file streaming. */
	PHP_ME(gene_response, getStatusCode, gene_response_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, isSent, gene_response_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, sendFile, gene_response_arg_send_file, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
