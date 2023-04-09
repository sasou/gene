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
  | Author: Sasou  <admin@caophp.com>                                    |
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


#include "../gene.h"
#include "../http/validate.h"
#include "../factory/factory.h"

zend_class_entry * gene_validate_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_validate_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_construct, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_init, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_name, 0, 0, 1)
	ZEND_ARG_INFO(0, field)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_filter, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_addvalidator, 0, 0, 3)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, msg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_msg, 0, 0, 1)
	ZEND_ARG_INFO(0, msg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_get_value, 0, 0, 1)
ZEND_ARG_INFO(0, field)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_get_error, 0, 0, 1)
ZEND_ARG_INFO(0, field)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_match, 0, 0, 1)
ZEND_ARG_INFO(0, regex)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_max, 0, 0, 1)
ZEND_ARG_INFO(0, max)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_min, 0, 0, 1)
ZEND_ARG_INFO(0, min)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_range, 0, 0, 2)
ZEND_ARG_INFO(0, min)
ZEND_ARG_INFO(0, max)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_length, 0, 0, 2)
ZEND_ARG_INFO(0, min)
ZEND_ARG_INFO(0, max)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_size, 0, 0, 2)
ZEND_ARG_INFO(0, min)
ZEND_ARG_INFO(0, max)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_in, 0, 0, 1)
ZEND_ARG_INFO(0, list)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_url, 0, 0, 1)
ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_datetime, 0, 0, 1)
ZEND_ARG_INFO(0, format)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_equal, 0, 0, 1)
ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_rule_equals, 0, 0, 1)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_validate_call_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

void reset_params(zval *self)
{
	zend_update_property_null(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA));
	zend_update_property_null(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_KEY));
	zend_update_property_null(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_FIELD));
	zend_update_property_null(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_METHOD));

	zval config_tmp;
	array_init(&config_tmp);
	#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(Z_ARRVAL_P(&config_tmp)) = 1;
	#else
		GC_SET_REFCOUNT(Z_ARRVAL_P(&config_tmp), 1);
	#endif
	zend_update_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CONFIG), &config_tmp);
	zval_ptr_dtor(&config_tmp);

	zval value_tmp;
	array_init(&value_tmp);
	#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(Z_ARRVAL_P(&value_tmp)) = 1;
	#else
		GC_SET_REFCOUNT(Z_ARRVAL_P(&value_tmp), 1);
	#endif
	zend_update_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_VALUE), &value_tmp);
	zval_ptr_dtor(&value_tmp);

	zval error_tmp;
	array_init(&error_tmp);
	#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(Z_ARRVAL_P(&error_tmp)) = 1;
	#else
		GC_SET_REFCOUNT(Z_ARRVAL_P(&error_tmp), 1);
	#endif
	zend_update_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_ERROR), &error_tmp);
	zval_ptr_dtor(&error_tmp);

	zval closure_tmp;
	array_init(&closure_tmp);
	#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(Z_ARRVAL_P(&closure_tmp)) = 1;
	#else
		GC_SET_REFCOUNT(Z_ARRVAL_P(&closure_tmp), 1);
	#endif
	zend_update_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CLOSURE), &closure_tmp);
	zval_ptr_dtor(&closure_tmp);
}

void gene_explode(char const *separator_str, char *string_str, zval *retval) /*{{{*/
{
    zval function_name, separator, string;
    ZVAL_STRING(&function_name, "explode");
    ZVAL_STRING(&separator, separator_str);
    ZVAL_STRING(&string, string_str);
	zval params[] = { separator, string };
    call_user_function(NULL, NULL, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&separator);
    zval_ptr_dtor(&string);
}/*}}}*/

void gene_vsprintf(char *msg, zval *args, zval *retval) /*{{{*/
{
    zval function_name, strings;
    ZVAL_STRING(&function_name, "vsprintf");
    ZVAL_STRING(&strings, msg);
	zval params[] = { strings, *args };
    call_user_function(NULL, NULL, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&strings);
}/*}}}*/

void gene_preg_match(zval *regex, zval *val, zval *retval) /*{{{*/
{
	zval function_name;
	ZVAL_STRING(&function_name, "preg_match");
	zval params[] = { *regex, *val };
	call_user_function(NULL, NULL, &function_name, retval, 2, params);
	zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_mb_strlen(zval *val, char *bm, zval *retval) /*{{{*/
{
	zval function_name,bm_z;
	ZVAL_STRING(&function_name, "mb_strlen");
	ZVAL_STRING(&bm_z, bm);
	zval params[] = { *val, bm_z };
	call_user_function(NULL, NULL, &function_name, retval, 2, params);
	zval_ptr_dtor(&function_name);
	zval_ptr_dtor(&bm_z);
}/*}}}*/

void gene_in_array(zval *in, zval *array, zval *retval) /*{{{*/
{
	zval function_name;
	ZVAL_STRING(&function_name, "in_array");
	zval params[] = { *in, *array };
	call_user_function(NULL, NULL, &function_name, retval, 2, params);
	zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_preg_match_str(char *regexStr, zval *val, zval *retval) /*{{{*/
{
	zval function_name, regex;
	ZVAL_STRING(&function_name, "preg_match");
	ZVAL_STRING(&regex, regexStr);
	zval params[] = { regex, *val };
	call_user_function(NULL, NULL, &function_name, retval, 2, params);
	zval_ptr_dtor(&function_name);
	zval_ptr_dtor(&regex);
}/*}}}*/

void gene_date(zval *format, zval *time, zval *retval) /*{{{*/
{
	zval function_name;
	ZVAL_STRING(&function_name, "date");
	zval params[] = { *format, *time };
	call_user_function(NULL, NULL, &function_name, retval, 2, params);
	zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_filter(zval *value, zend_long filter_l, zend_long options_l, zval *retval) /*{{{*/
{
	zval function_name, filter;
	ZVAL_STRING(&function_name, "filter_var");
	ZVAL_LONG(&filter, filter_l);
	if (options_l > 0) {
		zval options;
		ZVAL_LONG(&options, options_l);
		zval params[] = { *value, filter, options };
		call_user_function(NULL, NULL, &function_name, retval, 3, params);
		zval_ptr_dtor(&options);
	} else {
		zval params[] = { *value, filter };
		call_user_function(NULL, NULL, &function_name, retval, 2, params);
	}
	zval_ptr_dtor(&function_name);
	zval_ptr_dtor(&filter);
}/*}}}*/


int required (zval *self){
	zval *field = NULL, *data = NULL, *val = NULL;
	field = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_FIELD), 1, NULL);
	if (field && Z_TYPE_P(field) == IS_NULL) {
		php_error_docref(NULL, E_ERROR, "Please call the name method in the first place!");
	}
	data = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA), 1, NULL);
	if (data && Z_TYPE_P(data) != IS_ARRAY) {
		return 0;
	}

	if ((val = zend_hash_str_find(Z_ARRVAL_P(data), Z_STRVAL_P(field), Z_STRLEN_P(field))) == NULL) {
		return 0;
	}
	if (val) {
		switch(Z_TYPE_P(val)) {
		case IS_NULL:
			return 0;
			break;
		case IS_STRING:
			if (Z_STRLEN_P(val) == 0) {
				return 0;
			}
			break;
		case IS_ARRAY:
			if (zend_hash_num_elements(Z_ARRVAL_P(val)) == 0) {
				return 0;
			}
			break;
		}
	}
	return 1;
}

int compareSizeMax(zval *val, long max) {
	zval a_new;
	switch(Z_TYPE_P(val)) {
	case IS_LONG:
		if (Z_LVAL_P(val) <= max) {
			return 1;
		}
		break;
	case IS_TRUE:
		if (1 <= max) {
			return 1;
		}
		break;
	case IS_FALSE:
		if (0 <= max) {
			return 1;
		}
		break;
	case IS_STRING:
		ZVAL_STRING(&a_new, Z_STRVAL_P(val));
		convert_to_long(&a_new);
		if (Z_LVAL(a_new) <= max) {
			zval_ptr_dtor(&a_new);
			return 1;
		}
		zval_ptr_dtor(&a_new);
		break;
	default:
		if (0 <= max) {
			return 1;
		}
		break;
	}
	return 0;
}

int compareSizeMin(zval *val, long min) {
	zval a_new;
	switch(Z_TYPE_P(val)) {
	case IS_LONG:
		if (Z_LVAL_P(val) >= min) {
			return 1;
		}
		break;
	case IS_TRUE:
		if (1 >= min) {
			return 1;
		}
		break;
	case IS_FALSE:
		if (0 >= min) {
			return 1;
		}
		break;
	case IS_STRING:
		ZVAL_STRING(&a_new, Z_STRVAL_P(val));
		convert_to_long(&a_new);
		if (Z_LVAL(a_new) >= min) {
			zval_ptr_dtor(&a_new);
			return 1;
		}
		zval_ptr_dtor(&a_new);
		break;
	default:
		if (0 >= min) {
			return 1;
		}
		break;
	}
	return 0;
}

/*
 * {{{ gene_validate
 */
PHP_METHOD(gene_validate, __construct)
{
	zval *self = getThis(), *data = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &data) == FAILURE) {
		return;
	}
	if (data) {
		reset_params(self);
		zend_update_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA), data);
	}
}
/* }}} */

/*
 * {{{ public gene_validate::init($data)
 */
PHP_METHOD(gene_validate, init)
{
	zval *self = getThis(), *data = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &data) == FAILURE) {
		return;
	}

	reset_params(self);
	zend_update_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA), data);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::name($field)
 */
PHP_METHOD(gene_validate, name)
{
	zval *self = getThis();
	zend_string *name = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		return;
	}

	zend_update_property_null(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_METHOD));
	zend_update_property_str(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_KEY), name);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::skipOnEmpty()
 */
PHP_METHOD(gene_validate, skipOnEmpty)
{
	zval *self = getThis(), *key = NULL, *config = NULL, *keyArr = NULL, *skip = NULL;

	key = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_WARNING, "Please call the name method in the first place.");
	}

	config = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);

	if ((keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key))) == NULL) {
		zval keyArr_tmp;
		array_init(&keyArr_tmp);
		Z_TRY_ADDREF(keyArr_tmp);
		zend_hash_str_update(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key), &keyArr_tmp);
		zval_ptr_dtor(&keyArr_tmp);
		keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key));
	}

	if ((skip = zend_hash_str_find(Z_ARRVAL_P(keyArr), "skip", 4)) == NULL) {
		zval val;
		ZVAL_BOOL(&val , 1);
		Z_TRY_ADDREF(val);
		zend_hash_str_update(Z_ARRVAL_P(keyArr), "skip", 4, &val);
		zval_ptr_dtor(&val);
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::filter($method, $args)
 */
PHP_METHOD(gene_validate, filter)
{
	zval *self = getThis(), *key = NULL, *data = NULL, *args = NULL;
	zend_string *method = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|z", &method, &args) == FAILURE) {
		return;
	}

	key = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_WARNING, "Please call the name method in the first place.");
	}

	zval fieldArr;
	gene_explode(",", Z_STRVAL_P(key), &fieldArr);
	if (Z_TYPE(fieldArr) == IS_ARRAY) {
		zval *val = NULL, *key_one = NULL;
		data = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA), 1, NULL);
		if (data && Z_TYPE_P(data) == IS_ARRAY) {
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL(fieldArr), key_one) {
				if ((val = zend_hash_str_find(Z_ARRVAL_P(data), Z_STRVAL_P(key_one), Z_STRLEN_P(key_one))) != NULL) {
					zval ret;
					gene_factory_function_call(ZSTR_VAL(method), val, args, &ret);
					Z_TRY_ADDREF(ret);
					zend_hash_str_update(Z_ARRVAL_P(data),  Z_STRVAL_P(key_one), Z_STRLEN_P(key_one), &ret);
					zval_ptr_dtor(&ret);
				}
			}ZEND_HASH_FOREACH_END();
		}
	}
	zval_ptr_dtor(&fieldArr);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::addValidator($name, $callback, $msg)
 */
PHP_METHOD(gene_validate, addValidator)
{
	zval *self = getThis(), *closure = NULL, *val = NULL, *msg = NULL;
	zend_string *name = NULL;
	zend_fcall_info callback;
	zend_fcall_info_cache fci_cache = empty_fcall_info_cache;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sfz", &name, &callback, &fci_cache, &msg) == FAILURE) {
		return;
	}

	closure = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CLOSURE), 1, NULL);
	if (closure && Z_TYPE_P(closure) == IS_ARRAY) {
		val = zend_hash_find(Z_ARRVAL_P(closure), name);
		if (val == NULL) {
			zval val_tmp;
			array_init(&val_tmp);
			Z_TRY_ADDREF(callback.function_name);
			add_assoc_zval_ex(&val_tmp, "func", 4, &callback.function_name);
			Z_TRY_ADDREF_P(msg);
			add_assoc_zval_ex(&val_tmp, "msg", 3, msg);
			Z_TRY_ADDREF(val_tmp);
			zend_hash_update(Z_ARRVAL_P(closure), name, &val_tmp);
			zval_ptr_dtor(&val_tmp);
		}
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::__call($methord, $args)
 */
PHP_METHOD(gene_validate, __call) {
	zval *self = getThis(), *val = NULL, *key = NULL, *config = NULL, *keyArr = NULL, *listArr = NULL, *methodArr = NULL;
	zend_string *method = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &method, &val) == FAILURE) {
		return;
	}

	zend_update_property_str(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_METHOD), method);
	key = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_WARNING, "Please call the name method in the first place.");
	}

	config = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);

	if ((keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key))) == NULL) {
		zval keyArr_tmp;
		array_init(&keyArr_tmp);
		Z_TRY_ADDREF(keyArr_tmp);
		zend_hash_str_update(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key), &keyArr_tmp);
		zval_ptr_dtor(&keyArr_tmp);
		keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key));
	}

	if ((listArr = zend_hash_str_find(Z_ARRVAL_P(keyArr), "list", 4)) == NULL) {
		zval listArr_tmp;
		array_init(&listArr_tmp);
		Z_TRY_ADDREF(listArr_tmp);
		zend_hash_str_update(Z_ARRVAL_P(keyArr), "list", 4, &listArr_tmp);
		zval_ptr_dtor(&listArr_tmp);
		listArr = zend_hash_str_find(Z_ARRVAL_P(keyArr), "list", 4);
	}

	if ((methodArr = zend_hash_find(Z_ARRVAL_P(listArr), method)) == NULL) {
		zval methodArr_tmp;
		array_init(&methodArr_tmp);
		Z_TRY_ADDREF_P(val);
		add_assoc_zval_ex(&methodArr_tmp, "args", 4, val);
		Z_TRY_ADDREF(methodArr_tmp);
		zend_hash_update(Z_ARRVAL_P(listArr),  method, &methodArr_tmp);
		zval_ptr_dtor(&methodArr_tmp);
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_validate::msg($msg)
 */
PHP_METHOD(gene_validate, msg)
{
	zval *self = getThis(), *key = NULL, *config = NULL, *method = NULL, *keyArr = NULL, *listArr = NULL, *methodArr = NULL, *msg = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &msg) == FAILURE) {
		return;
	}

	key = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_WARNING, "Please call the name method in the first place");
	}

	config = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);

	if ((keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key))) == NULL) {
		zval keyArr_tmp;
		array_init(&keyArr_tmp);
		Z_TRY_ADDREF(keyArr_tmp);
		zend_hash_str_update(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key), &keyArr_tmp);
		zval_ptr_dtor(&keyArr_tmp);
		keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key));
	}

	method = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_METHOD), 1, NULL);
	if (method && Z_TYPE_P(method) == IS_NULL) {
		if (zend_hash_str_exists(Z_ARRVAL_P(keyArr), "msg", 3) == 0) {
			Z_TRY_ADDREF_P(msg);
			zend_hash_str_update(Z_ARRVAL_P(keyArr), "msg", 3, msg);
		}
	} else {
		if ((listArr = zend_hash_str_find(Z_ARRVAL_P(keyArr), "list", 4)) == NULL) {
			zval listArr_tmp;
			array_init(&listArr_tmp);
			Z_TRY_ADDREF(listArr_tmp);
			zend_hash_str_update(Z_ARRVAL_P(keyArr), "list", 4, &listArr_tmp);
			zval_ptr_dtor(&listArr_tmp);
			listArr = zend_hash_str_find(Z_ARRVAL_P(keyArr), "list", 4);
		}

		if ((methodArr = zend_hash_str_find(Z_ARRVAL_P(listArr), Z_STRVAL_P(method), Z_STRLEN_P(method))) == NULL) {
			zval methodArr_tmp;
			array_init(&methodArr_tmp);
			Z_TRY_ADDREF_P(msg);
			add_assoc_zval_ex(&methodArr_tmp, "msg", 3, msg);
			Z_TRY_ADDREF(methodArr_tmp);
			zend_hash_str_update(Z_ARRVAL_P(listArr), Z_STRVAL_P(method), Z_STRLEN_P(method), &methodArr_tmp);
			zval_ptr_dtor(&methodArr_tmp);
		} else {
			if (zend_hash_str_exists(Z_ARRVAL_P(methodArr), "msg", 3) == 0) {
				Z_TRY_ADDREF_P(msg);
				zend_hash_str_update(Z_ARRVAL_P(methodArr), "msg", 3, msg);
			}
		}
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

char *getErrorMsg(char *method) {
    char* names[]={"required","match","min","max","range","length","size","in","url","email","ip","mobile","date","datetime","int","number","digit","string","equal","equals"};
    char* descs[]={"This field is required","Value is not in conformity with the regular expression","Please enter a value greater than %s","Please enter a value less than %s",
    		"Please input greater than %s, less than the value of %s","The length of the string of mistakes","The array size error",
    		"The field value is not allowed within the input range","Please input the correct url","Please enter the correct email address",
			"Please input the correct IP address","Please enter the correct phone number","Please input the correct date","Please input the correct time",
			"Please enter an integer","Please enter the Numbers","Please enter a string Numbers","Please enter a string","Please enter the same value as %s","Please enter a same value"};
	int i = 0;
    for (i = 0; i < 19; i++) {
		if (strcmp(names[i], method) == 0) {
			break;
		}
	}
    return descs[i];
}

int validCheck(zval *self, zval *date_field, zval *rules, int is_group) {
	int isValid = 1;
	zval *list = NULL;
	list = zend_hash_str_find(Z_ARRVAL_P(rules), "list", 4);
	if (list && Z_TYPE_P(list) == IS_ARRAY) {
		zval *value = NULL, *closure = NULL, *closure_arr = NULL, *data = NULL, *date_field_val = NULL, *values = NULL, *errors = NULL, *args = NULL;
		zend_string *method = NULL;
		closure = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CLOSURE), 1, NULL);
		data = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA), 1, NULL);
		if (data && Z_TYPE_P(data) == IS_ARRAY) {
			date_field_val = zend_hash_str_find(Z_ARRVAL_P(data), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field));

			values = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_VALUE), 1, NULL);
			errors = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_ERROR), 1, NULL);

			ZEND_HASH_FOREACH_STR_KEY_VAL_IND(Z_ARRVAL_P(list), method, value) {
				args = zend_hash_str_find(Z_ARRVAL_P(value), "args", 4);

				if (closure && Z_TYPE_P(closure) == IS_ARRAY) {
					closure_arr = zend_hash_str_find(Z_ARRVAL_P(closure), ZSTR_VAL(method), ZSTR_LEN(method));
				}

				if (closure_arr && Z_TYPE_P(closure_arr) == IS_ARRAY) {
					zval *func = NULL, *msg = NULL;
					func = zend_hash_str_find(Z_ARRVAL_P(closure_arr), "func", 4);
					if (func) {
						zval ret;
						if (date_field_val) {
							gene_factory_function_call_1(func, date_field_val, NULL, &ret);
						} else {
							gene_factory_function_call_1(func, NULL, NULL, &ret);
						}
						if (Z_TYPE(ret) == IS_FALSE) {
							zend_hash_str_del(Z_ARRVAL_P(values), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field));
							msg = zend_hash_str_find(Z_ARRVAL_P(value), "msg", 3);
							if (msg == NULL) {
								msg = zend_hash_str_find(Z_ARRVAL_P(rules), "msg", 3);
							}
							if (msg == NULL) {
								msg = zend_hash_str_find(Z_ARRVAL_P(closure_arr), "msg", 3);
							}
							if (msg) {
								zend_hash_str_update(Z_ARRVAL_P(errors), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field), msg);
							}
							isValid = 0;
							if (is_group == 0) {
								zval_ptr_dtor(&ret);
								return isValid;
							}
						} else {
							if (zend_hash_str_exists(Z_ARRVAL_P(errors), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field)) == 0) {
								Z_TRY_ADDREF_P(date_field_val);
								zend_hash_str_update(Z_ARRVAL_P(values), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field), date_field_val);
							}
						}
						zval_ptr_dtor(&ret);
					}
				} else {
					char *name = NULL;
					size_t name_len = 0;
					name_len = spprintf(&name, 0, "rule_%s", ZSTR_VAL(method));
					if (zend_hash_str_exists(&(Z_OBJCE_P(self)->function_table), name, name_len)) {
						zval ret, *msg = NULL;
						gene_factory_call(self, name, args, &ret);
						if (Z_TYPE(ret) == IS_FALSE) {
							zend_hash_str_del(Z_ARRVAL_P(values), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field));
							msg = zend_hash_str_find(Z_ARRVAL_P(value), "msg", 3);
							if (msg == NULL) {
								msg = zend_hash_str_find(Z_ARRVAL_P(rules), "msg", 3);
							}
							if (msg == NULL) {
								zval tmp_msg;
								gene_vsprintf(getErrorMsg(ZSTR_VAL(method)), args, &tmp_msg);
								Z_TRY_ADDREF(tmp_msg);
								zend_hash_str_update(Z_ARRVAL_P(errors), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field), &tmp_msg);
								zval_ptr_dtor(&tmp_msg);
							} else {
								zend_hash_str_update(Z_ARRVAL_P(errors), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field), msg);
							}
							isValid = 0;
							if (is_group == 0) {
								zval_ptr_dtor(&ret);
								efree(name);
								return isValid;
							}
						} else {
							if (zend_hash_str_exists(Z_ARRVAL_P(errors), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field)) == 0) {
								Z_TRY_ADDREF_P(date_field_val);
								zend_hash_str_update(Z_ARRVAL_P(values), Z_STRVAL_P(date_field), Z_STRLEN_P(date_field), date_field_val);
							}
						}
						zval_ptr_dtor(&ret);
						efree(name);
					} else {
						php_error_docref(NULL, E_WARNING, "Executed method:%s does not exist.", ZSTR_VAL(method));
						efree(name);
					}
				}
			}ZEND_HASH_FOREACH_END();
		}
	}
	return isValid;
}
/*
 * {{{ public gene_validate::valid()
 */
PHP_METHOD(gene_validate, valid)
{
	zval *self = getThis(), *config = NULL, *field = NULL, *field_value = NULL;

	config = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	if (config && Z_TYPE_P(config) == IS_ARRAY) {
		zval *rules = NULL, *skip = NULL;
		zend_string *key = NULL;

		ZEND_HASH_FOREACH_STR_KEY_VAL_IND(Z_ARRVAL_P(config), key, rules) {
			zval fieldArr;
			gene_explode(",", ZSTR_VAL(key), &fieldArr);
			if (Z_TYPE(fieldArr) == IS_ARRAY) {
				zval *v = NULL;
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL(fieldArr), v) {
					zend_update_property_string(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_FIELD), Z_STRVAL_P(v));
					skip = zend_hash_str_find(Z_ARRVAL_P(rules), "skip", 4);
					if (skip && Z_TYPE_P(skip) == IS_TRUE && required (self) == 0) {
						continue;
					}
					if (validCheck(self, v, rules, 0) == 0) {
						zval_ptr_dtor(&fieldArr);
						RETURN_FALSE;
					}
				}ZEND_HASH_FOREACH_END();
			}
			zval_ptr_dtor(&fieldArr);

		}ZEND_HASH_FOREACH_END();
	}

	RETURN_TRUE;
}
/* }}} */


/*
 * {{{ public gene_validate::groupValid()
 */
PHP_METHOD(gene_validate, groupValid)
{
	zval *self = getThis(), *config = NULL, *field = NULL, *field_value = NULL;
	int isValid = 1;
	config = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	if (config && Z_TYPE_P(config) == IS_ARRAY) {
		zval *rules = NULL, *skip = NULL;
		zend_string *key = NULL;

		ZEND_HASH_FOREACH_STR_KEY_VAL_IND(Z_ARRVAL_P(config), key, rules) {
			zval fieldArr;
			gene_explode(",", ZSTR_VAL(key), &fieldArr);
			if (Z_TYPE(fieldArr) == IS_ARRAY) {
				zval *v = NULL;
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL(fieldArr), v) {
					zend_update_property_string(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_FIELD), Z_STRVAL_P(v));
					skip = zend_hash_str_find(Z_ARRVAL_P(rules), "skip", 4);
					if (skip && Z_TYPE_P(skip) == IS_TRUE && required (self) == 0) {
						continue;
					}
					if (validCheck(self, v, rules, 1) == 0) {
						isValid = 0;
					}
				}ZEND_HASH_FOREACH_END();
			}
			zval_ptr_dtor(&fieldArr);

		}ZEND_HASH_FOREACH_END();
	}

	if (isValid) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::getValue($field)
 */
PHP_METHOD(gene_validate, getValue)
{
	zval *self = getThis(), *value = NULL, *field = NULL, *field_value = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &field) == FAILURE) {
		return;
	}

	value = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_VALUE), 1, NULL);
	if (value && Z_TYPE_P(value) == IS_NULL) {
		RETURN_NULL();
	}

	if (field && Z_TYPE_P(field) == IS_STRING) {
		if ((field_value = zend_hash_str_find(Z_ARRVAL_P(value), Z_STRVAL_P(field), Z_STRLEN_P(field))) != NULL) {
			RETURN_ZVAL(field_value, 1, 0);
		}
	}
	RETURN_ZVAL(value, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::error()
 */
PHP_METHOD(gene_validate, error)
{
	zval *self = getThis(), *error = NULL;

	error = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_ERROR), 1, NULL);
	if (error && Z_TYPE_P(error) != IS_ARRAY) {
		RETURN_NULL();
	}

	zval *v = NULL;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(error), v) {
		RETURN_ZVAL(v, 1, 0);
		break;
	}ZEND_HASH_FOREACH_END();

	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_validate::getError($field)
 */
PHP_METHOD(gene_validate, getError)
{
	zval *self = getThis(), *error = NULL, *field = NULL, *field_value = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &field) == FAILURE) {
		return;
	}

	error = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_ERROR), 1, NULL);
	if (error && Z_TYPE_P(error) == IS_NULL) {
		RETURN_NULL();
	}

	if (field && Z_TYPE_P(field) == IS_STRING) {
		if ((field_value = zend_hash_str_find(Z_ARRVAL_P(error), Z_STRVAL_P(field), Z_STRLEN_P(field))) != NULL) {
			RETURN_ZVAL(field_value, 1, 0);
		}
	}
	RETURN_ZVAL(error, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::rule_required()
 */
PHP_METHOD(gene_validate, rule_required)
{
	zval *self = getThis();
	if (required (self)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

zval *getFieldVal(zval *self) {
	zval * field = NULL, *data = NULL;
	field = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_FIELD), 1, NULL);

	if (field && Z_TYPE_P(field) == IS_NULL) {
		php_error_docref(NULL, E_WARNING, "Please call the name method in the first place.");
	}

	data = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA), 1, NULL);
	if (data && Z_TYPE_P(data) != IS_ARRAY) {
		return NULL;
	}
	return zend_hash_str_find(Z_ARRVAL_P(data), Z_STRVAL_P(field), Z_STRLEN_P(field));
}

zval *getFieldVal_1(zval *self, zval *field) {
	zval *data = NULL;
	data = zend_read_property(gene_validate_ce, gene_strip_obj(self), ZEND_STRL(GENE_VALIDATE_DATA), 1, NULL);

	if (data && Z_TYPE_P(data) != IS_ARRAY) {
		return NULL;
	}
	return zend_hash_str_find(Z_ARRVAL_P(data), Z_STRVAL_P(field), Z_STRLEN_P(field));
}

/*
 * {{{ public gene_validate::rule_match($regex)
 */
PHP_METHOD(gene_validate, rule_match)
{
	zval *self = getThis(), *regex = NULL, *val = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &regex) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}
	zval retval;
	gene_preg_match(regex, val, &retval);
	if (Z_TYPE(retval) == IS_LONG && Z_LVAL(retval) > 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_max($max)
 */
PHP_METHOD(gene_validate, rule_max)
{
	zval *self = getThis(), *val = NULL;
	long max = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &max) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (compareSizeMax(val, max)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_min($min)
 */
PHP_METHOD(gene_validate, rule_min)
{
	zval *self = getThis(), *val = NULL;
	long min = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &min) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (compareSizeMin(val, min)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_range($min, $max)
 */
PHP_METHOD(gene_validate, rule_range)
{
	zval *self = getThis(), *val = NULL;
	long min = 0, max = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ll", &min, &max) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (compareSizeMin(val, min) && compareSizeMax(val, max)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */


/*
 * {{{ public gene_validate::rule_length($min, $max)
 */
PHP_METHOD(gene_validate, rule_length)
{
	zval *self = getThis(), *val = NULL;
	long min = 0, max = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ll", &min, &max) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(val) != IS_STRING) {
		RETURN_FALSE;
	}

	zval ret;
	gene_mb_strlen(val, "UTF8", &ret);
	if (Z_TYPE(ret) == IS_LONG && Z_LVAL(ret) >= min && Z_LVAL(ret) <= max) {
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_size($min, $max)
 */
PHP_METHOD(gene_validate, rule_size)
{
	zval *self = getThis(), *val = NULL;
	long min = 0, max = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ll", &min, &max) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(val) != IS_ARRAY) {
		RETURN_FALSE;
	}

	int num = zend_hash_num_elements(Z_ARRVAL_P(val));
	if (num >= min && num <= max) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_in()
 */
PHP_METHOD(gene_validate, rule_in)
{
	zval *self = getThis(), *list = NULL, *val = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &list) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(list) != IS_ARRAY) {
		RETURN_FALSE;
	}

	zval ret;
	gene_in_array(val, list, &ret);
	if (Z_TYPE(ret) == IS_TRUE) {
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_url($flags)
 */
PHP_METHOD(gene_validate, rule_url)
{
	zval *self = getThis(), *val = NULL;
	zend_long flags_l = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &flags_l) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	zval retval;
	if (flags_l > 0) {
		gene_filter(val, 273, flags_l, &retval);
	} else {
		gene_filter(val, 273, 131072, &retval);
	}
	if (Z_TYPE(retval) == IS_FALSE) {
		zval_ptr_dtor(&retval);
		RETURN_FALSE;
	}
	zval_ptr_dtor(&retval);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_email()
 */
PHP_METHOD(gene_validate, rule_email)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	zval retval;
	gene_filter(val, 274, 0, &retval);
	if (Z_TYPE(retval) == IS_FALSE) {
		zval_ptr_dtor(&retval);
		RETURN_FALSE;
	}
	zval_ptr_dtor(&retval);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_ip()
 */
PHP_METHOD(gene_validate, rule_ip)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(val) != IS_STRING) {
		RETURN_FALSE;
	}

	zval ipint;
	gene_factory_call_2("ip2long", val, &ipint);
	if (Z_TYPE(ipint) != IS_LONG) {
		zval_ptr_dtor(&ipint);
		RETURN_FALSE;
	}

	zval ipstr;
	gene_factory_call_2("long2ip", &ipint, &ipstr);
	if (Z_TYPE(ipstr) != IS_STRING) {
		zval_ptr_dtor(&ipstr);
		zval_ptr_dtor(&ipint);
		RETURN_FALSE;
	}

	if (strcmp(Z_STRVAL_P(val), Z_STRVAL(ipstr)) == 0) {;
		zval_ptr_dtor(&ipstr);
		zval_ptr_dtor(&ipint);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ipstr);
	zval_ptr_dtor(&ipint);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_mobile()
 */
PHP_METHOD(gene_validate, rule_mobile)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	zval retval;
	gene_preg_match_str(GENE_VALIDATE_MOBILE, val, &retval);
	if (Z_TYPE(retval) == IS_LONG && Z_LVAL(retval) > 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_date()
 */
PHP_METHOD(gene_validate, rule_date)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	zval retval;
	gene_preg_match_str(GENE_VALIDATE_DATE, val, &retval);
	if (Z_TYPE(retval) == IS_LONG && Z_LVAL(retval) > 0) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_datetime()
 */
PHP_METHOD(gene_validate, rule_datetime)
{
	zval *self = getThis(), *val = NULL;
	char *format_str = NULL;
	int format_str_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &format_str, &format_str_len) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(val) != IS_STRING) {
		RETURN_FALSE;
	}

	zval time;
	gene_factory_call_2("strtotime", val, &time);
	if (Z_TYPE(time) != IS_LONG) {
		zval_ptr_dtor(&time);
		RETURN_FALSE;
	}

	zval format;
	if (format_str_len > 0) {
		ZVAL_STRING(&format, format_str);
	} else {
		ZVAL_STRING(&format, "Y-m-d H:i:s");
	}
	zval datetime;
	gene_date(&format, &time, &datetime);
	if (strcmp(Z_STRVAL_P(val), Z_STRVAL(datetime)) == 0) {
		zval_ptr_dtor(&datetime);
		zval_ptr_dtor(&format);
		zval_ptr_dtor(&time);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&datetime);
	zval_ptr_dtor(&format);
	zval_ptr_dtor(&time);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_number()
 */
PHP_METHOD(gene_validate, rule_number)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	zval ret;
	gene_factory_call_2("is_numeric", val, &ret);
	if (Z_TYPE(ret) == IS_TRUE) {
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_int()
 */
PHP_METHOD(gene_validate, rule_int)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	zval ret;
	gene_factory_call_2("is_int", val, &ret);
	if (Z_TYPE(ret) == IS_TRUE) {
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_digit()
 */
PHP_METHOD(gene_validate, rule_digit)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}
	zval ret1,ret2;
	gene_factory_call_2("is_int", val, &ret1);
	gene_factory_call_2("ctype_digit", val, &ret2);
	if (Z_TYPE(ret1) == IS_TRUE || Z_TYPE(ret2) == IS_TRUE) {
		zval_ptr_dtor(&ret1);
		zval_ptr_dtor(&ret2);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret1);
	zval_ptr_dtor(&ret2);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_string()
 */
PHP_METHOD(gene_validate, rule_string)
{
	zval *self = getThis(), *val = NULL;

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}
	zval ret;
	gene_factory_call_2("is_string", val, &ret);
	if (Z_TYPE(ret) == IS_TRUE) {
		zval_ptr_dtor(&ret);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&ret);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_equal()
 */
PHP_METHOD(gene_validate, rule_equal)
{
	zval *self = getThis(), *val = NULL, *name = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &name) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	zval *nameVal = NULL;
	if ((nameVal = getFieldVal_1(self, name)) == NULL) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(val) != Z_TYPE_P(nameVal)) {
		RETURN_FALSE;
	}

	switch(Z_TYPE_P(val)) {
	case IS_NULL:
		RETURN_TRUE;
		break;
	case IS_TRUE:
		RETURN_TRUE;
		break;
	case IS_FALSE:
		RETURN_TRUE;
		break;
	case IS_LONG:
		if (Z_LVAL_P(val) == Z_LVAL_P(nameVal)) {
			RETURN_TRUE;
		}
		break;
	case IS_DOUBLE:
		if (Z_DVAL_P(val) == Z_DVAL_P(nameVal)) {
			RETURN_TRUE;
		}
		break;
	case IS_STRING:
		if (strcmp(Z_STRVAL_P(val), Z_STRVAL_P(nameVal)) == 0) {
			RETURN_TRUE;
		}
		break;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_validate::rule_equals()
 */
PHP_METHOD(gene_validate, rule_equals)
{
	zval *self = getThis(), *val = NULL, *value = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &value) == FAILURE) {
		return;
	}

	if ((val = getFieldVal(self)) == NULL) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(val) != Z_TYPE_P(value)) {
		RETURN_FALSE;
	}

	switch(Z_TYPE_P(val)) {
	case IS_NULL:
		RETURN_TRUE;
		break;
	case IS_TRUE:
		RETURN_TRUE;
		break;
	case IS_FALSE:
		RETURN_TRUE;
		break;
	case IS_LONG:
		if (Z_LVAL_P(val) == Z_LVAL_P(value)) {
			RETURN_TRUE;
		}
		break;
	case IS_DOUBLE:
		if (Z_DVAL_P(val) == Z_DVAL_P(value)) {
			RETURN_TRUE;
		}
		break;
	case IS_STRING:
		if (strcmp(Z_STRVAL_P(val), Z_STRVAL_P(value)) == 0) {
			RETURN_TRUE;
		}
		break;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_validate_methods
 */
zend_function_entry gene_validate_methods[] = {
		PHP_ME(gene_validate, init, gene_validate_init, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, name, gene_validate_name, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, skipOnEmpty, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, filter, gene_validate_filter, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, addValidator, gene_validate_addvalidator, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, msg, gene_validate_msg, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, valid, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, groupValid, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, error, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, getValue, gene_validate_get_value, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, getError, gene_validate_get_error, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_required, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_match, gene_validate_rule_match, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_max, gene_validate_rule_max, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_min, gene_validate_rule_min, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_range, gene_validate_rule_range, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_length, gene_validate_rule_length, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_size, gene_validate_rule_size, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_in, gene_validate_rule_in, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_url, gene_validate_rule_url, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_email, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_ip, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_mobile, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_date, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_datetime, gene_validate_rule_datetime, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_number, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_int, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_digit, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_string, gene_validate_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_equal, gene_validate_rule_equal, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, rule_equals, gene_validate_rule_equals, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, __construct, gene_validate_construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(gene_validate, __call, gene_validate_call_arginfo, ZEND_ACC_PUBLIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(validate)
{
    zend_class_entry gene_validate;
    INIT_CLASS_ENTRY(gene_validate,"gene_validate",gene_validate_methods);
    GENE_INIT_CLASS_ENTRY(gene_validate, "Gene_Validate", "Gene\\Validate", gene_validate_methods);
    gene_validate_ce = zend_register_internal_class(&gene_validate);


    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_DATA), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_KEY), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_FIELD), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_METHOD), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_CONFIG), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_VALUE), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_ERROR), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_CLOSURE), ZEND_ACC_PUBLIC);
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
