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

#include "php_gene.h"
#include "gene_application.h"
#include "gene_controller.h"
#include "gene_request.h"
#include "gene_response.h"
#include "gene_memory.h"
#include "gene_router.h"
#include "gene_view.h"
#include "gene_di.h"

zend_class_entry * gene_controller_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_controller_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_controller_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_controller_void_arginfo, 0, 0, 0) ZEND_END_ARG_INFO()
/* }}} */

/*
 * {{{ gene_controller
 */
PHP_METHOD(gene_controller, __construct) {
	int debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
	gene_ini_router(TSRMLS_C);
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
	zval *header = request_query(TRACK_VARS_SERVER, ZEND_STRL("HTTP_X_REQUESTED_WITH") TSRMLS_CC);
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
	if (GENE_G(method)) {
		RETURN_STRING(GENE_G(method));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_controller::urlParams()
 */
PHP_METHOD(gene_controller, urlParams) {
	zval *cache = NULL;
	zend_string *keyString = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|S", &keyString) == FAILURE) {
		return;
	}
	cache = gene_memory_get_by_config(PHP_GENE_URL_PARAMS, strlen(PHP_GENE_URL_PARAMS), ZSTR_VAL(keyString) TSRMLS_CC);
	if (cache) {
		ZVAL_COPY_VALUE(return_value, cache);
		return;
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_response::redirect(string $url)
 */
PHP_METHOD(gene_controller, redirect) {
	zend_string *url;
	zend_long code = 302;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &url, &code) == FAILURE) {
		return;
	}
	gene_response_set_redirect(ZSTR_VAL(url), code TSRMLS_CC);
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_controller::display(string $file)
 */
PHP_METHOD(gene_controller, display) {
	zend_string *file, *parent_file = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|S", &file, &parent_file) == FAILURE) {
		return;
	}
	if (parent_file && ZSTR_LEN(parent_file) > 0) {
		if (GENE_G(child_views)) {
			efree(GENE_G(child_views));
			GENE_G(child_views) = NULL;
		}
		GENE_G(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display(ZSTR_VAL(parent_file) TSRMLS_CC);
	} else {
		gene_view_display(ZSTR_VAL(file) TSRMLS_CC);
	}
}
/* }}} */

/** {{{ public gene_controller::display(string $file)
 */
PHP_METHOD(gene_controller, displayExt) {
	zend_string *file, *parent_file = NULL;
	zend_bool isCompile = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|Sb", &file, &parent_file, &isCompile) == FAILURE) {
		return;
	}
	if (parent_file && ZSTR_LEN(parent_file) > 0) {
		if (GENE_G(child_views)) {
			efree(GENE_G(child_views));
			GENE_G(child_views) = NULL;
		}
		GENE_G(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display_ext(ZSTR_VAL(parent_file), isCompile TSRMLS_CC);
	} else {
		gene_view_display_ext(ZSTR_VAL(file), isCompile TSRMLS_CC);
	}
}
/* }}} */

/** {{{ public gene_controller::contains(string $file)
 */
PHP_METHOD(gene_controller, contains) {
	gene_view_display(GENE_G(child_views) TSRMLS_CC);
}
/* }}} */

/** {{{ public gene_controller::contains(string $file)
 */
PHP_METHOD(gene_controller, containsExt) {
	gene_view_display_ext(GENE_G(child_views), 0 TSRMLS_CC);
}
/* }}} */

/** {{{ proto public gene_controller::success(string $text, int $code)
 */
PHP_METHOD(gene_controller, success) {
	zend_string *text;
	zend_long code = 2000;
	zval ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(&ret);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(&ret, ZEND_STRL(GENE_RESPONSE_MSG), text);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */


/** {{{ proto public gene_controller::error(string $text, int $code)
 */
PHP_METHOD(gene_controller, error) {
	zend_string *text;
	zend_long code = 4000;
	zval ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(&ret);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(&ret, ZEND_STRL(GENE_RESPONSE_MSG), text);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */

/** {{{ proto public gene_controller::data(mixed $data, int $count,string $text, int $code)
 */
PHP_METHOD(gene_controller, data) {
	zval *data = NULL;
	zend_long count = 0;
	zend_string *text = NULL;
	zend_long code = 2000;
	zval ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|lSl", &data, &count, &text, &code) == FAILURE) {
		return;
	}
	array_init(&ret);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_CODE), code);
	if (text) {
		add_assoc_str_ex(&ret, ZEND_STRL(GENE_RESPONSE_MSG), text);
	}
	add_assoc_zval_ex(&ret, ZEND_STRL(GENE_RESPONSE_DATA), data);
	add_assoc_long_ex(&ret, ZEND_STRL(GENE_RESPONSE_COUNT), count);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */


/** {{{ proto public gene_controller::json(array $json, int $code)
 */
PHP_METHOD(gene_controller, json) {
	zval *data = NULL;
	zend_string *callback = NULL;
	zend_long code = 256;
    zval ret;
    zval json_opt;
    zend_long callback_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|sl", &data, &callback, &callback_len, &code) == FAILURE) {
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

/*
 * {{{ gene_controller
 */
PHP_METHOD(gene_controller, __set)
{
	zend_string *name;
	zval *value, *props = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	props = zend_read_property(gene_controller_ce, getThis(), ZEND_STRL(GENE_CONTROLLER_ATTR), 1, NULL);
	if (props == NULL) {
		zval prop;
		array_init(&prop);
		if (zend_hash_update(Z_ARRVAL_P(props), name, value) != NULL) {
			Z_TRY_ADDREF_P(value);
		}
		zend_update_property(gene_controller_ce, getThis(), ZEND_STRL(GENE_CONTROLLER_ATTR), &prop);
		zval_ptr_dtor(&prop);
	} else {
		if (zend_hash_update(Z_ARRVAL_P(props), name, value) != NULL) {
			Z_TRY_ADDREF_P(value);
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_controller
 */
PHP_METHOD(gene_controller, __get)
{
	zval *pzval = NULL, *props = NULL,*obj = getThis(), classObject;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		props = zend_read_property(gene_controller_ce, obj, ZEND_STRL(GENE_CONTROLLER_ATTR), 1, NULL);
		if (Z_TYPE_P(props) == IS_NULL) {
			zval prop;
			array_init(&prop);
			pzval = gene_di_get(&prop, name, &classObject);
			zend_update_property(gene_controller_ce, obj, ZEND_STRL(GENE_CONTROLLER_ATTR), &prop);
			zval_ptr_dtor(&prop);
			if (pzval) {
				RETURN_ZVAL(pzval, 1, 0);
			}
			RETURN_NULL();
		} else {
			pzval = zend_hash_find(Z_ARRVAL_P(props), name);
			if (pzval == NULL) {
				pzval = gene_di_get(props, name, &classObject);
				if (pzval) {
					RETURN_ZVAL(pzval, 1, 0);
				}
				RETURN_NULL();
			}
			RETURN_ZVAL(pzval, 1, 0);
		}
	}
	RETURN_NULL();
}
/* }}} */


/*
 * {{{ gene_controller_methods
 */
zend_function_entry gene_controller_methods[] = {
	PHP_ME(gene_controller, get, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, request, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, post, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, cookie, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, files, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, server, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, env, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isAjax, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, urlParams, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, getMethod, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isGet, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isPost, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isPut, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isHead, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isOptions, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isDelete, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, isCli, gene_controller_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, redirect, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, display, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, displayExt, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_controller, contains, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, containsExt, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, success, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, error, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, data, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, json, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_controller, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
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
	gene_controller_ce = zend_register_internal_class(&gene_controller TSRMLS_CC);
	zend_declare_property_null(gene_controller_ce, ZEND_STRL(GENE_CONTROLLER_ATTR), ZEND_ACC_PUBLIC);

	return SUCCESS;
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
