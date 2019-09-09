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
#include "../http/response.h"
#include "../cache/memory.h"

zend_class_entry * gene_response_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_se, 0, 0, 1)
    ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_se_data, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, count)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_se_json, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_redirect, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_alert, 0, 0, 1)
    ZEND_ARG_INFO(0, text)
    ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_response_arg_header, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

/** {{{ void gene_response_set_redirect(char *url, zend_long code TSRMLS_DC)
 */
void gene_response_set_redirect(char *url, zend_long code TSRMLS_DC) {
	sapi_header_line ctr = { 0 };

	ctr.line_len = spprintf(&(ctr.line), 0, "%s %s", "Location:", url);
	ctr.response_code = code;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);
	efree(ctr.line);
}
/* }}} */

/** {{{ void gene_response_set_header(char *key, char *value TSRMLS_DC)
 */
void gene_response_set_header(char *key, char *value TSRMLS_DC) {
	sapi_header_line ctr = { 0 };

	ctr.line_len = spprintf(&(ctr.line), 0, "%s:%s", key, value);
	ctr.response_code = 200;
	sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);
	efree(ctr.line);
}
/* }}} */

/*
 * {{{ gene_response
 */
PHP_METHOD(gene_response, __construct) {
	long debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
}
/* }}} */

/** {{{ proto public gene_response::redirect(string $url)
 */
PHP_METHOD(gene_response, redirect) {
	zend_string *url;
	zend_long code = 302;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &url, &code) == FAILURE) {
		return;
	}

	gene_response_set_redirect(ZSTR_VAL(url), code TSRMLS_CC);
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public gene_response::alert(string $text, string $url = NULL)
 */
PHP_METHOD(gene_response, alert) {
	zend_string *text, *url;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|S", &text, &url) == FAILURE) {
		return;
	}
	php_printf("\n<script type=\"text/javascript\">\nalert(\"%s\");\n", ZSTR_VAL(text));
	if (url && ZSTR_LEN(url)) {
		php_printf("window.location.href=\"%s\";\n", ZSTR_VAL(url));
	}
	php_printf("</script>\n");
}
/* }}} */

/** {{{ proto public gene_response::success(string $text, int $code)
 */
PHP_METHOD(gene_response, success) {
	zend_string *text;
	zend_long code = 2000;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), text);
}
/* }}} */


/** {{{ proto public gene_response::error(string $text, int $code)
 */
PHP_METHOD(gene_response, error) {
	zend_string *text;
	zend_long code = 4000;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), text);
}
/* }}} */

/** {{{ proto public gene_response::data(mixed $data, int $count,string $text, int $code)
 */
PHP_METHOD(gene_response, data) {
	zval *data = NULL;
	zend_long count = -1;
	zend_string *text = NULL;
	zend_long code = 2000;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|lSl", &data, &count, &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	if (text) {
		add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), text);
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
    zval ret, json_opt;
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

/** {{{ proto public gene_response::header(array $json, int $code)
 */
PHP_METHOD(gene_response, header) {
	char *key, *value;
	zend_long key_len = 0, value_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ss", &key, &key_len, &value, &value_len) == FAILURE) {
		return;
	}

	gene_response_set_header(key, value);
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
zend_function_entry gene_response_methods[] = {
	PHP_ME(gene_response, redirect, gene_response_arg_redirect, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, alert, gene_response_arg_alert, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, success, gene_response_arg_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, error, gene_response_arg_se, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, data, gene_response_arg_se_data, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, json, gene_response_arg_se_json, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, header, gene_response_arg_header, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, setJsonHeader, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, setHtmlHeader, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_response, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(response) {
	zend_class_entry gene_response;
	GENE_INIT_CLASS_ENTRY(gene_response, "Gene_Response", "Gene\\Response", gene_response_methods);
	gene_response_ce = zend_register_internal_class(&gene_response TSRMLS_CC);

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
