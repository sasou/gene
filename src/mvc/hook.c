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
#include "Zend/zend_smart_str.h"
#include "Zend/zend_interfaces.h"

#include "../gene.h"
#include "../app/application.h"
#include "../mvc/hook.h"
#include "../http/request.h"
#include "../http/response.h"
#include "../cache/memory.h"
#include "../router/router.h"
#include "../mvc/view.h"
#include "../di/di.h"
#include "../factory/load.h"
#include "../common/common.h"

zend_class_entry * gene_hook_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_hook_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_param_get, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_redirect, 0, 0, 2)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_display, 0, 0, 2)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, parent_file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_display_ext, 0, 0, 3)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, parent_file)
	ZEND_ARG_INFO(0, isCompile)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_url_param, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_arg_url, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_se, 0, 0, 2)
    ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_se_data, 0, 0, 4)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, count)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_se_json, 0, 0, 3)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_se_assign, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_before_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_after_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_hook_handle_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()


/*
 * {{{ gene_hook
 */
PHP_METHOD(gene_hook, __construct) {
	int debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_hook::get(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_hook, get, TRACK_VARS_GET);
/* }}} */

/** {{{ public gene_hook::post(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_hook, post, TRACK_VARS_POST);
/* }}} */

/** {{{ public gene_hook::request(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_hook, request, TRACK_VARS_REQUEST);
/* }}} */

/** {{{ public gene_hook::files(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_hook, files, TRACK_VARS_FILES);
/* }}} */

/** {{{ public gene_hook::cookie(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_hook, cookie, TRACK_VARS_COOKIE);
/* }}} */

/** {{{ public gene_hook::server(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_hook, server, TRACK_VARS_SERVER);
/* }}} */

/** {{{ public gene_hook::env(mixed $name, mixed $default = NULL)
 */
GENE_REQUEST_METHOD(gene_hook, env, TRACK_VARS_ENV);
/* }}} */

/** {{{ public gene_hook::isGet(void)
 */
GENE_REQUEST_IS_METHOD(gene_hook, Get);
/* }}} */

/** {{{ public gene_hook::isPost(void)
 */
GENE_REQUEST_IS_METHOD(gene_hook, Post);
/* }}} */

/** {{{ public gene_hook::isPut(void)
 */
GENE_REQUEST_IS_METHOD(gene_hook, Put);
/* }}} */

/** {{{ public gene_hook::isHead(void)
 */
GENE_REQUEST_IS_METHOD(gene_hook, Head);
/* }}} */

/** {{{ public gene_hook::isOptions(void)
 */
GENE_REQUEST_IS_METHOD(gene_hook, Options);
/* }}} */

/** {{{ public gene_hook::isDelete(void)
 */
GENE_REQUEST_IS_METHOD(gene_hook, Delete);
/* }}} */

/** {{{ public gene_hook::isCli(void)
 */
GENE_REQUEST_IS_METHOD(gene_hook, Cli);
/* }}} */

/** {{{ proto public gene_hook::isAjax()
 */
PHP_METHOD(gene_hook, isAjax) {
	zval *header = request_query(TRACK_VARS_SERVER, ZEND_STRL("HTTP_X_REQUESTED_WITH"));
	if (header && Z_TYPE_P(header) == IS_STRING && strncasecmp("XMLHttpRequest", Z_STRVAL_P(header), Z_STRLEN_P(header)) == 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_hook::getMethod()
 */
PHP_METHOD(gene_hook, getMethod) {
	if (GENE_REQ(method)) {
		RETURN_STRING(GENE_REQ(method));
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_hook::getLang()
 */
PHP_METHOD(gene_hook, getLang) {
	if (GENE_REQ(lang)) {
		RETURN_STRING(GENE_REQ(lang));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_hook::params($name)
 */
PHP_METHOD(gene_hook, params) {
	char *name = NULL;
	zend_long name_len = 0;
	zval *params = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &name, &name_len) == FAILURE) {
		return;
	}

	params = GENE_REQ(path_params);
	if (name_len == 0) {
		RETURN_ZVAL(GENE_REQ(path_params), 1, 0);
	} else {
		zval *val = zend_symtable_str_find(Z_ARRVAL_P(params), name, name_len);
		if (val) {
			RETURN_ZVAL(val, 1, 0);
		}
		RETURN_NULL();
	}
}
/* }}} */

/** {{{ public gene_hook::redirect(string $url)
 */
PHP_METHOD(gene_hook, redirect) {
	zend_string *url;
	zend_long code = 302;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &url, &code) == FAILURE) {
		return;
	}
	gene_response_set_redirect(ZSTR_VAL(url), code);
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_hook::display(string $file)
 */
PHP_METHOD(gene_hook, display) {
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
		if (GENE_REQ(child_views)) {
			efree(GENE_REQ(child_views));
			GENE_REQ(child_views) = NULL;
		}
		GENE_REQ(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
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

/** {{{ public gene_hook::displayExt(string $file)
 */
PHP_METHOD(gene_hook, displayExt) {
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
		if (GENE_REQ(child_views)) {
			efree(GENE_REQ(child_views));
			GENE_REQ(child_views) = NULL;
		}
		GENE_REQ(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
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

/** {{{ public gene_hook::contains()
 */
PHP_METHOD(gene_hook, contains) {
	if (!GENE_REQ(child_views) || GENE_REQ(child_views)[0] == '\0') {
		RETURN_FALSE;
	}
	gene_view_contains(GENE_REQ(child_views), return_value);
}
/* }}} */

/** {{{ public gene_hook::containsExt()
 */
PHP_METHOD(gene_hook, containsExt) {
	if (!GENE_REQ(child_views) || GENE_REQ(child_views)[0] == '\0') {
		RETURN_FALSE;
	}
	gene_view_contains_ext(GENE_REQ(child_views), 0, return_value);
}
/* }}} */

/** {{{ public gene_hook::url(string $path)
 */
PHP_METHOD(gene_hook, url) {
	zend_string *path_str;
	char *out = NULL;
	const char *p;
	size_t path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &path_str) == FAILURE) {
		return;
	}
	p = ZSTR_VAL(path_str);
	path_len = ZSTR_LEN(path_str);
	for (; path_len > 0 && *p == '/'; p++, path_len--) {}
	if (path_len == 0) {
		if (GENE_REQ(lang) && GENE_REQ(lang)[0] != '\0') {
			spprintf(&out, 0, "/%s/", GENE_REQ(lang));
			RETVAL_STRING(out);
			efree(out);
		} else {
			RETURN_STRING("/");
		}
		return;
	}
	if (GENE_REQ(lang) && GENE_REQ(lang)[0] != '\0') {
		spprintf(&out, 0, "/%s/%.*s", GENE_REQ(lang), (int)path_len, p);
	} else {
		spprintf(&out, 0, "/%.*s", (int)path_len, p);
	}
	RETVAL_STRING(out);
	efree(out);
}
/* }}} */

/** {{{ proto public gene_hook::success(string $text, int $code)
 */
PHP_METHOD(gene_hook, success) {
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


/** {{{ proto public gene_hook::error(string $text, int $code)
 */
PHP_METHOD(gene_hook, error) {
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

/** {{{ proto public gene_hook::data(mixed $data, int $count, string $text, int $code)
 */
PHP_METHOD(gene_hook, data) {
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


/** {{{ proto public gene_hook::json(array $json, int $code)
 */
PHP_METHOD(gene_hook, json) {
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

PHP_METHOD(gene_hook, assign) {
	zval *value;
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	gene_view_set_vars(name, value);
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_hook::before()
 *  Default before hook - subclasses override this.
 *  Return false/0 to abort the request, return true/null to continue.
 */
PHP_METHOD(gene_hook, before) {
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_hook::after($params)
 *  Default after hook - subclasses override this.
 *  Receives the dispatch result as a parameter.
 */
PHP_METHOD(gene_hook, after) {
	zval *params = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &params) == FAILURE) {
		return;
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_hook::handle()
 *  Default named hook handler - subclasses override this.
 *  Return false/0 to abort the request, return true/null to continue.
 */
PHP_METHOD(gene_hook, handle) {
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_hook
 */
PHP_METHOD(gene_hook, __set)
{
	zend_string *name;
	zval *value;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	zval class_name;
	gene_class_name(&class_name);
	if (gene_di_set_class(Z_STR(class_name), name, value)) {
		zval_ptr_dtor(&class_name);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&class_name);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_hook
 */
PHP_METHOD(gene_hook, __get)
{
	zval *pzval = NULL;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		zval class_name;
		gene_class_name(&class_name);
		pzval = gene_di_get_class(Z_STR(class_name), name);
		if (pzval) {
			zval_ptr_dtor(&class_name);
			RETURN_ZVAL(pzval, 1, 0);
		}
		zval_ptr_dtor(&class_name);
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */


/*
 * {{{ gene_hook_methods
 */
const zend_function_entry gene_hook_methods[] = {
	PHP_ME(gene_hook, __construct, gene_hook_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, get, gene_hook_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, request, gene_hook_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, post, gene_hook_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, cookie, gene_hook_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, files, gene_hook_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, server, gene_hook_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, env, gene_hook_param_get, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isAjax, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, params, gene_hook_url_param, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, getMethod, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, getLang, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isGet, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isPost, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isPut, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isHead, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isOptions, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isDelete, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, isCli, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, redirect, gene_hook_redirect, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, display, gene_hook_display, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, displayExt, gene_hook_display_ext, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, contains, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, containsExt, gene_hook_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, url, gene_hook_arg_url, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, success, gene_hook_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, error, gene_hook_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, data, gene_hook_se_data, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, json, gene_hook_se_json, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, assign, gene_hook_se_assign, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_hook, before, gene_hook_before_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, after, gene_hook_after_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, handle, gene_hook_handle_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, __get, gene_hook_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_hook, __set, gene_hook_set, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(hook) {
	zend_class_entry gene_hook;
	GENE_INIT_CLASS_ENTRY(gene_hook, "Gene_Hook", "Gene\\Hook", gene_hook_methods);
	gene_hook_ce = zend_register_internal_class(&gene_hook);
#if PHP_VERSION_ID >= 80200
	gene_hook_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
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
