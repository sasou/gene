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
#include "Zend/zend_interfaces.h"

#include "../gene.h"
#include "../app/application.h"
#include "../mvc/controller.h"
#include "../http/request.h"
#include "../http/response.h"
#include "../cache/memory.h"
#include "../router/router.h"
#include "../mvc/view.h"
#include "../di/di.h"
#include "../factory/load.h"
#include "../common/common.h"

zend_class_entry * gene_controller_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_controller_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_param_get, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_redirect, 0, 0, 2)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_redirect_js, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_alert, 0, 0, 2)
    ZEND_ARG_INFO(0, text)
    ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_display, 0, 0, 2)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, parent_file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_display_ext, 0, 0, 3)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, parent_file)
	ZEND_ARG_INFO(0, isCompile)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_url_param, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_arg_url, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_se, 0, 0, 2)
    ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_se_data, 0, 0, 4)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, count)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_se_json, 0, 0, 3)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(gene_controller_se_assign, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()


/*
 * {{{ gene_controller
 */
PHP_METHOD(gene_controller, __construct) {
	int debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_controller::get(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_controller, get, TRACK_VARS_GET);
/* }}} */

/** {{{ public gene_controller::post(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_controller, post, TRACK_VARS_POST);
/* }}} */

/** {{{ public gene_controller::request(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_controller, request, TRACK_VARS_REQUEST);
/* }}} */

/** {{{ public gene_controller::files(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_controller, files, TRACK_VARS_FILES);
/* }}} */

/** {{{ public gene_controller::cookie(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_controller, cookie, TRACK_VARS_COOKIE);
/* }}} */

/** {{{ public gene_controller::server(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_controller, server, TRACK_VARS_SERVER);
/* }}} */

/** {{{ public gene_controller::env(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_controller, env, TRACK_VARS_ENV);
/* }}} */

/** {{{ public gene_controller::isGet(void)
 */
GENE_REQUEST_IS_METHOD(gene_controller, Get);
/* }}} */

/** {{{ public gene_controller::isPost(void)
 */
GENE_REQUEST_IS_METHOD(gene_controller, Post);
/* }}} */

/** {{{ public gene_controller::isPut(void)
 */
GENE_REQUEST_IS_METHOD(gene_controller, Put);
/* }}} */

/** {{{ public gene_controller::isHead(void)
 */
GENE_REQUEST_IS_METHOD(gene_controller, Head);
/* }}} */

/** {{{ public gene_controller::isOptions(void)
 */
GENE_REQUEST_IS_METHOD(gene_controller, Options);
/* }}} */

/** {{{ public gene_controller::isOptions(void)
 */
GENE_REQUEST_IS_METHOD(gene_controller, Delete);
/* }}} */

/** {{{ public gene_controller::isCli(void)
 */
GENE_REQUEST_IS_METHOD(gene_controller, Cli);
/* }}} */

/** {{{ proto public gene_controller::isAjax()
 */
PHP_METHOD(gene_controller, isAjax) {
	zval *header = request_query(TRACK_VARS_SERVER, ZEND_STRL("HTTP_X_REQUESTED_WITH"));
	if (header && Z_TYPE_P(header) == IS_STRING && strncasecmp("XMLHttpRequest", Z_STRVAL_P(header), Z_STRLEN_P(header)) == 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_controller::getMethod()
 */
PHP_METHOD(gene_controller, getMethod) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->method) {
		RETURN_STRING(ctx->method);
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_controller::getLang()
 */
PHP_METHOD(gene_controller, getLang) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx->lang) {
		RETURN_STRING(ctx->lang);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_controller::params($name)
 */
PHP_METHOD(gene_controller, params) {
	char *name = NULL;
	zend_long name_len = 0;
	zval *params = NULL;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &name, &name_len) == FAILURE) {
		return;
	}

	ctx = gene_request_ctx();
	params = ctx->path_params;
	if (name_len == 0) {
		RETURN_ZVAL(ctx->path_params, 1, 0);
	} else {
		zval *val = zend_symtable_str_find(Z_ARRVAL_P(params), name, name_len);
		if (val) {
			RETURN_ZVAL(val, 1, 0);
		}
		RETURN_NULL();
	}
}
/* }}} */

/** {{{ public gene_response::redirect(string $url)
 */
PHP_METHOD(gene_controller, redirect) {
	zend_string *url;
	zend_long code = 302;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &url, &code) == FAILURE) {
		return;
	}
	gene_response_set_redirect(ZSTR_VAL(url), code);
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public gene_controller::redirectJs(string $url)
 */
PHP_METHOD(gene_controller, redirectJs) {
	zend_string *url;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &url) == FAILURE) {
		return;
	}
	gene_response_redirect_js(url);
}
/* }}} */

/** {{{ proto public gene_controller::alert(string $text, string $url = NULL)
 */
PHP_METHOD(gene_controller, alert) {
	zend_string *text, *url = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S", &text, &url) == FAILURE) {
		return;
	}
	gene_response_alert(text, url);
}
/* }}} */

/** {{{ public gene_controller::display(string $file)
 */
PHP_METHOD(gene_controller, display) {
	zend_string *file, *parent_file = NULL;
	zend_array *table = NULL;
	zval *vars = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S", &file, &parent_file) == FAILURE) {
		return;
	}
	zval *self = getThis();
	vars = gene_view_get_vars();
	table = gene_view_build_symbol_table(vars);

	if (parent_file && ZSTR_LEN(parent_file) > 0) {
		gene_request_context *ctx = gene_request_ctx();
		if (ctx->child_views) {
			efree(ctx->child_views);
			ctx->child_views = NULL;
		}
		ctx->child_views = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display(ZSTR_VAL(parent_file), self, table);
	} else {
		gene_view_display(ZSTR_VAL(file), self, table);
	}
	if (table) {
		zend_hash_destroy(table);
		FREE_HASHTABLE(table);
		gene_view_clear_vars();
	}
}
/* }}} */

/** {{{ public gene_controller::display(string $file)
 */
PHP_METHOD(gene_controller, displayExt) {
	zend_string *file, *parent_file = NULL;
	bool isCompile = 0;
	zend_array *table = NULL;
	zval *vars = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|Sb", &file, &parent_file, &isCompile) == FAILURE) {
		return;
	}
	zval *self = getThis();
	vars = gene_view_get_vars();
	table = gene_view_build_symbol_table(vars);

	if (parent_file && ZSTR_LEN(parent_file) > 0) {
		gene_request_context *ctx = gene_request_ctx();
		if (ctx->child_views) {
			efree(ctx->child_views);
			ctx->child_views = NULL;
		}
		ctx->child_views = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display_ext(ZSTR_VAL(parent_file), isCompile, self, table);
	} else {
		gene_view_display_ext(ZSTR_VAL(file), isCompile, self, table);
	}
	if (table) {
		zend_hash_destroy(table);
		FREE_HASHTABLE(table);
		gene_view_clear_vars();
	}
}
/* }}} */

/** {{{ public gene_controller::contains(string $file)
 */
PHP_METHOD(gene_controller, contains) {
	gene_request_context *ctx = gene_request_ctx();
	if (!ctx->child_views || ctx->child_views[0] == '\0') {
		RETURN_FALSE;
	}
	gene_view_contains(ctx->child_views, return_value);
}
/* }}} */

/** {{{ public gene_controller::contains(string $file)
 */
PHP_METHOD(gene_controller, containsExt) {
	gene_request_context *ctx = gene_request_ctx();
	if (!ctx->child_views || ctx->child_views[0] == '\0') {
		RETURN_FALSE;
	}
	gene_view_contains_ext(ctx->child_views, 0, return_value);
}
/* }}} */

/** {{{ public gene_controller::url(string $path)
 *  返回带当前语言前缀的 URL，如 url("login.html") => "/en/login.html"
 */
PHP_METHOD(gene_controller, url) {
	zend_string *path_str;
	const char *p;
	size_t path_len;
	gene_request_context *ctx;
	const char *lang;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &path_str) == FAILURE) {
		return;
	}
	p = ZSTR_VAL(path_str);
	path_len = ZSTR_LEN(path_str);
	/* 跳过 path 前导斜杠 */
	for (; path_len > 0 && *p == '/'; p++, path_len--) {}
	ctx = gene_request_ctx();
	lang = (ctx->lang && ctx->lang[0] != '\0') ? ctx->lang : NULL;
	if (path_len == 0) {
		/* 如果只有斜杠，也加上语言前缀 */
		if (lang) {
			size_t lang_len = strlen(lang);
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
		size_t lang_len = strlen(lang);
		size_t out_len = lang_len + path_len + 2;
		char out_buf[512];
		char *out_ptr = out_buf;
		int out_heap = 0;
		if (out_len >= sizeof(out_buf)) {
			out_ptr = emalloc(out_len + 1);
			out_heap = 1;
		}
		snprintf(out_ptr, out_len + 1, "/%s/%.*s", lang, (int)path_len, p);
		RETVAL_STRING(out_ptr);
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
		snprintf(out_ptr, out_len + 1, "/%.*s", (int)path_len, p);
		RETVAL_STRING(out_ptr);
		if (out_heap) {
			efree(out_ptr);
		}
	}
}
/* }}} */

/** {{{ proto public gene_controller::success(string $text, int $code)
 */
PHP_METHOD(gene_controller, success) {
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


/** {{{ proto public gene_controller::error(string $text, int $code)
 */
PHP_METHOD(gene_controller, error) {
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

/** {{{ proto public gene_controller::data(mixed $data, int $count,string $text, int $code)
 */
PHP_METHOD(gene_controller, data) {
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


/** {{{ proto public gene_controller::json(array $json, int $code)
 */
PHP_METHOD(gene_controller, json) {
	zval *data = NULL;
	char *callback = NULL;
	zend_long code = 256;
    zval ret, json_opt;
    zend_long callback_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|sl", &data, &callback, &callback_len, &code) == FAILURE) {
		return;
	}
	ZVAL_LONG(&json_opt, code);
	zend_call_method_with_2_params(NULL, NULL, NULL, "json_encode", &ret, data, &json_opt);
	zval_ptr_dtor(&json_opt);
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

PHP_METHOD(gene_controller, assign) {
	zval *value;
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	gene_view_set_vars(name, value);
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_controller
 */
PHP_METHOD(gene_controller, __set)
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
 * {{{ gene_controller
 */
PHP_METHOD(gene_controller, __get)
{
	zval *pzval = NULL;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
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
 * {{{ gene_controller_methods
 */
const zend_function_entry gene_controller_methods[] = {
	PHP_ME(gene_controller, __construct, gene_controller_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, get, gene_controller_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, request, gene_controller_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, post, gene_controller_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, cookie, gene_controller_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, files, gene_controller_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, server, gene_controller_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, env, gene_controller_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isAjax, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, params, gene_controller_url_param, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, getMethod, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, getLang, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isGet, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isPost, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isPut, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isHead, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isOptions, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isDelete, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isCli, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, redirect, gene_controller_redirect, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, redirectJs, gene_controller_redirect_js, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, alert, gene_controller_alert, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, display, gene_controller_display, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, displayExt, gene_controller_display_ext, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, contains, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, containsExt, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, url, gene_controller_arg_url, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, success, gene_controller_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, error, gene_controller_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, data, gene_controller_se_data, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, json, gene_controller_se_json, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, assign, gene_controller_se_assign, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, __get, gene_controller_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, __set, gene_controller_set, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(controller) {
	zend_class_entry gene_controller;
	GENE_INIT_CLASS_ENTRY(gene_controller, "Gene_Controller", "Gene\\Controller", gene_controller_methods);
	gene_controller_ce = zend_register_internal_class(&gene_controller);
#if PHP_VERSION_ID >= 80200
	gene_controller_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
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
