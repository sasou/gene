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
#include "php_globals.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "php_gene.h"
#include "http/request.h"
#include "cache/memory.h"

zend_class_entry * gene_request_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(geme_request_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(geme_request_get_param_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(geme_request_url_param, 0, 0, 0)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()
/* }}} */

zval * request_query(int type, char * name, int len TSRMLS_DC) {
	zval *carrier = NULL, *ret;
	zend_bool jit_initialization = PG(auto_globals_jit);

	switch (type) {
	case TRACK_VARS_POST:
	case TRACK_VARS_GET:
	case TRACK_VARS_FILES:
	case TRACK_VARS_COOKIE:
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_ENV:
		if (jit_initialization) {
			zend_string *env_str = zend_string_init("_ENV", sizeof("_ENV") - 1,
					0);
			zend_is_auto_global(env_str);
			zend_string_release(env_str);
		}
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_SERVER:
		if (jit_initialization) {
			zend_string *server_str = zend_string_init("_SERVER",
					sizeof("_SERVER") - 1, 0);
			zend_is_auto_global(server_str);
			zend_string_release(server_str);
		}
		carrier = &PG(http_globals)[type];
		break;
	case TRACK_VARS_REQUEST:
		if (jit_initialization) {
			zend_string *request_str = zend_string_init("_REQUEST",
					sizeof("_REQUEST") - 1, 0);
			zend_is_auto_global(request_str);
			zend_string_release(request_str);
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

/*
 * {{{ gene_request
 */
PHP_METHOD(gene_request, __construct) {
	long debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|l", &debug) == FAILURE) {
		return;
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

/** {{{ public gene_request::isAjax()
 */
PHP_METHOD(gene_request, isAjax) {
	zval *header = request_query(TRACK_VARS_SERVER, ZEND_STRL("HTTP_X_REQUESTED_WITH") TSRMLS_CC);
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
	if (GENE_G(method)) {
		RETURN_STRING(GENE_G(method));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_request::urlParams()
 */
PHP_METHOD(gene_request, urlParams) {
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

/*
 * {{{ gene_request_methods
 */
zend_function_entry gene_request_methods[] = {
	PHP_ME(gene_request, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_request, get, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, request, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, post, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, cookie, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, files, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, server, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, env, geme_request_get_param_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isAjax, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, urlParams, geme_request_url_param, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, getMethod, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isGet, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isPost, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isPut, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isHead, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isOptions, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isCli, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(request) {
	zend_class_entry gene_request;
	GENE_INIT_CLASS_ENTRY(gene_request, "Gene_Request", "Gene\\Request", gene_request_methods);
	gene_request_ce = zend_register_internal_class(&gene_request TSRMLS_CC);

	//
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
