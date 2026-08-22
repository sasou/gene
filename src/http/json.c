/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../common/common.h"
#include "../http/json.h"

zend_class_entry *gene_json_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_json_encode_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_json_decode_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, json)
ZEND_END_ARG_INFO()

/* {{{ gene_json_last_error */
static zend_long gene_json_last_error(void) {
	zend_function *fn;
	zval ret;
	ZVAL_UNDEF(&ret);
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("json_last_error"));
	if (UNEXPECTED(!fn)) {
		return 0;
	}
	zend_call_known_function(fn, NULL, NULL, &ret, 0, NULL, NULL);
	if (Z_TYPE(ret) == IS_LONG) {
		zend_long code = Z_LVAL(ret);
		zval_ptr_dtor(&ret);
		return code;
	}
	zval_ptr_dtor(&ret);
	return 0;
}
/* }}} */

/* {{{ gene_json_last_error_msg */
static zend_string *gene_json_last_error_msg(void) {
	zend_function *fn;
	zval ret;
	ZVAL_UNDEF(&ret);
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("json_last_error_msg"));
	if (UNEXPECTED(!fn)) {
		return zend_string_init("JSON error", sizeof("JSON error") - 1, 0);
	}
	zend_call_known_function(fn, NULL, NULL, &ret, 0, NULL, NULL);
	if (Z_TYPE(ret) == IS_STRING) {
		return Z_STR(ret); /* steal */
	}
	zval_ptr_dtor(&ret);
	return zend_string_init("JSON error", sizeof("JSON error") - 1, 0);
}
/* }}} */

int gene_json_encode_throw(zval *value, zval *retval) {
	zval opts;
	zend_function *fn;
	ZVAL_UNDEF(retval);
	ZVAL_LONG(&opts, GENE_JSON_ENCODE_FLAGS);
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("json_encode"));
	if (UNEXPECTED(!fn)) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Json requires the json extension");
		return FAILURE;
	}
	{
		zval params[] = { *value, opts };
		zend_call_known_function(fn, NULL, NULL, retval, 2, params, NULL);
	}
	if (EG(exception)) {
		zval_ptr_dtor(retval);
		ZVAL_UNDEF(retval);
		return FAILURE;
	}
	if (Z_TYPE_P(retval) != IS_STRING) {
		zend_string *msg = gene_json_last_error_msg();
		zval_ptr_dtor(retval);
		ZVAL_UNDEF(retval);
		zend_throw_exception_ex(NULL, 0, "json_encode failed: %s", ZSTR_VAL(msg));
		zend_string_release(msg);
		return FAILURE;
	}
	return SUCCESS;
}

int gene_json_decode_throw(zend_string *str, zval *retval) {
	zval zstr, assoc, depth, flags;
	zend_function *fn;
	ZVAL_UNDEF(retval);
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("json_decode"));
	if (UNEXPECTED(!fn)) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Json requires the json extension");
		return FAILURE;
	}
	ZVAL_STR(&zstr, str); /* borrow */
	ZVAL_TRUE(&assoc);
	ZVAL_LONG(&depth, 512);
	ZVAL_LONG(&flags, 0);
	{
		zval params[] = { zstr, assoc, depth, flags };
		zend_call_known_function(fn, NULL, NULL, retval, 4, params, NULL);
	}
	if (EG(exception)) {
		zval_ptr_dtor(retval);
		ZVAL_UNDEF(retval);
		return FAILURE;
	}
	/* json_decode returns null both for JSON `null` and for errors. */
	if (Z_TYPE_P(retval) == IS_NULL && gene_json_last_error() != 0) {
		zend_string *msg = gene_json_last_error_msg();
		zval_ptr_dtor(retval);
		ZVAL_UNDEF(retval);
		zend_throw_exception_ex(NULL, 0, "json_decode failed: %s", ZSTR_VAL(msg));
		zend_string_release(msg);
		return FAILURE;
	}
	return SUCCESS;
}

/* {{{ proto static string Gene\Json::encode(mixed $data) */
PHP_METHOD(gene_json, encode) {
	zval *data;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &data) == FAILURE) {
		return;
	}
	if (gene_json_encode_throw(data, return_value) != SUCCESS) {
		RETURN_THROWS();
	}
}
/* }}} */

/* {{{ proto static mixed Gene\Json::decode(string $json) */
PHP_METHOD(gene_json, decode) {
	zend_string *json;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &json) == FAILURE) {
		return;
	}
	if (gene_json_decode_throw(json, return_value) != SUCCESS) {
		RETURN_THROWS();
	}
}
/* }}} */

const zend_function_entry gene_json_methods[] = {
	PHP_ME(gene_json, encode, gene_json_encode_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_json, decode, gene_json_decode_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(json) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Json", "Gene\\Json", gene_json_methods);
	gene_json_ce = zend_register_internal_class(&ce);
	gene_json_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
