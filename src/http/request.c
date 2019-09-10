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

#include "../gene.h"
#include "../http/request.h"
#include "../cache/memory.h"

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

ZEND_BEGIN_ARG_INFO_EX(gene_request_init_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, get)
	ZEND_ARG_INFO(0, post)
	ZEND_ARG_INFO(0, cookie)
	ZEND_ARG_INFO(0, server)
	ZEND_ARG_INFO(0, env)
	ZEND_ARG_INFO(0, files)
	ZEND_ARG_INFO(0, request)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_request_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_request_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
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

void setVal(int type, zval *value) {
	zval *attr = zend_read_static_property(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), 1);
    zval *val = NULL;

    if (Z_TYPE_P(attr) == IS_NULL) {
    	zval tmp;
    	array_init(&tmp);
    	zend_update_static_property(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), &tmp);
    	zval_ptr_dtor(&tmp);
    	attr = zend_read_static_property(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), 1);
    }

	if (Z_TYPE_P(attr) ==  IS_ARRAY) {
		val =  zend_hash_index_find(Z_ARRVAL_P(attr), type);
		if (val == NULL) {
			zend_hash_index_update(Z_ARRVAL_P(attr), type, value);
		}
	}
}

zval *getVal(int type, char *name, int len) {
	zval *attr = zend_read_static_property(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), 1);
    zval *val = NULL;

    if (Z_TYPE_P(attr) == IS_NULL) {
    	zval tmp;
    	array_init(&tmp);
    	zend_update_static_property(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), &tmp);
    	zval_ptr_dtor(&tmp);
    	attr = zend_read_static_property(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), 1);
    }

	if (Z_TYPE_P(attr) ==  IS_ARRAY) {
		val =  zend_hash_index_find(Z_ARRVAL_P(attr), type);
		if (val == NULL) {
			val = request_query(type, NULL, 0 TSRMLS_CC);
			zend_hash_index_update(Z_ARRVAL_P(attr), type, val);
		}
		if (len == 0) {
			return val;
		}
		return zend_hash_str_find(Z_ARRVAL_P(val), name, len);
	}
    return NULL;
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
	if (GENE_G(method)) {
		RETURN_STRING(GENE_G(method));
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
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|s", &name, &name_len) == FAILURE) {
		return;
	}

	params = GENE_G(path_params);
	if (name_len == 0) {
		RETURN_ZVAL(GENE_G(path_params), 1, 0);
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
	zval *get = NULL, *post = NULL, *cookie = NULL, *server = NULL, *env = NULL, *files = NULL, *request = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|zzzzzzz", &get, &post, &cookie, &server, &env, &files, &request) == FAILURE) {
		return;
	}
	if (post && Z_TYPE_P(post) == IS_ARRAY) {
		setVal(0, post);
		Z_TRY_ADDREF_P(post);
	}
	if (get && Z_TYPE_P(get) == IS_ARRAY) {
		setVal(1, get);
		Z_TRY_ADDREF_P(get);
	}
	if (cookie && Z_TYPE_P(cookie) == IS_ARRAY) {
		setVal(2, cookie);
		Z_TRY_ADDREF_P(cookie);
	}
	if (server && Z_TYPE_P(server) == IS_ARRAY) {
		setVal(3, server);
		Z_TRY_ADDREF_P(server);
	}
	if (env && Z_TYPE_P(env) == IS_ARRAY) {
		setVal(4, env);
		Z_TRY_ADDREF_P(env);
	}
	if (files && Z_TYPE_P(files) == IS_ARRAY) {
		setVal(5, files);
		Z_TRY_ADDREF_P(files);
	}
	if (request && Z_TYPE_P(request) == IS_ARRAY) {
		setVal(6, request);
		Z_TRY_ADDREF_P(request);
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
	const char *valType[7] = { "post", "get", "cookie", "server", "env", "files",
			"request"};

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "s", &name, &name_len) == FAILURE) {
		RETURN_NULL();
	}

	for (i = 0; i < 7; i++) {
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
	const char *valType[7] = { "post", "get", "cookie", "server", "env", "files",
			"request"};
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "sz", &name, &name_len, &value) == FAILURE) {
		RETURN_NULL();
	}

	for (i = 0; i < 7; i++) {
		if (strcmp(valType[i], name) == 0) {
			setVal(i, value);
			break;
		}
	}
	RETURN_TRUE;
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
	PHP_ME(gene_request, params, geme_request_url_param, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, getMethod, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isGet, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isPost, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isPut, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isHead, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isOptions, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, isCli, geme_request_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_request, init, gene_request_init_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
	gene_request_ce = zend_register_internal_class(&gene_request TSRMLS_CC);

	zend_declare_property_null(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
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
