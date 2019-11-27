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

ZEND_BEGIN_ARG_INFO_EX(gene_validate_call_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

void reset_params(zval *self)
{
	zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_DATA));
	zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_KEY));
	zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_FIELD));
	zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_METHOD));
	zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG));
	zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_VALUE));
	zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_ERROR));
    zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CLOSURE));
}

/*
 * {{{ gene_validate
 */
PHP_METHOD(gene_validate, __construct)
{
	zval *self = getThis(), *data = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &data) == FAILURE) {
		return;
	}
	if (data) {
		reset_params(self);
		zend_update_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_DATA), data);
	}
}
/* }}} */

/*
 * {{{ public gene_validate::init($data)
 */
PHP_METHOD(gene_validate, init)
{
	zval *self = getThis(), *data = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &data) == FAILURE) {
		return;
	}

	reset_params(self);
	zend_update_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_DATA), data);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::name($field)
 */
PHP_METHOD(gene_validate, name)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &php_script, &php_script_len) == FAILURE) {
		return;
	}

	if (php_script_len) {
		zend_update_property_null(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_METHOD));
		zend_update_property_string(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_KEY), php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::skipOnEmpty()
 */
PHP_METHOD(gene_validate, skipOnEmpty)
{
	zval *self = getThis(), *key = NULL, *config = NULL, *keyArr = NULL, *skip = NULL;

	key = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_ERROR, "Please call the name method in the first place!");
	}

	config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	if (config && Z_TYPE_P(config) == IS_NULL) {
		zval config_tmp;
		array_init(&config_tmp);
		Z_TRY_ADDREF(config_tmp);
		zend_update_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), &config_tmp);
		zval_ptr_dtor(&config_tmp);
		config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	}

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
		ZVAL_BOOL(&val , TRUE);
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
	zval *self = getThis(), *key = NULL, *data = NULL, *val = NULL, *args = NULL;
	char *method = NULL;
	int method_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &method, &method_len, &args) == FAILURE) {
		return;
	}

	key = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_ERROR, "Please call the name method in the first place!");
	}

	data = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_DATA), 1, NULL);
	if (data && Z_TYPE_P(data) != IS_NULL) {
		if ((val = zend_hash_str_find(Z_ARRVAL_P(data), Z_STRVAL_P(key), Z_STRLEN_P(key))) != NULL) {
			zval ret;
			gene_factory_function_call(method, val, args, &ret);
			Z_TRY_ADDREF(ret);
			zend_hash_str_update(Z_ARRVAL_P(data),  Z_STRVAL_P(key), Z_STRLEN_P(key), &ret);
			zval_ptr_dtor(&ret);
		}
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::addValidator($name, $callback, $msg)
 */
PHP_METHOD(gene_validate, addValidator)
{
	zval *self = getThis(), *closure = NULL, *val = NULL, *msg = NULL;
	char *name = NULL;
	int name_len = 0;
	zend_fcall_info callback;
	zend_fcall_info_cache fci_cache = empty_fcall_info_cache;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sfz", &name, &name_len, &callback, &fci_cache, &msg) == FAILURE) {
		return;
	}

	closure = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CLOSURE), 1, NULL);
	if (closure && Z_TYPE_P(closure) == IS_NULL) {
		zval closure_tmp;
		array_init(&closure_tmp);
		Z_TRY_ADDREF(closure_tmp);
		zend_update_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CLOSURE), &closure_tmp);
		zval_ptr_dtor(&closure_tmp);
		closure = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CLOSURE), 1, NULL);
	}

	if ((val = zend_hash_str_find(Z_ARRVAL_P(closure), name, name_len)) == NULL) {
		zval val_tmp;
		array_init(&val_tmp);
		Z_TRY_ADDREF(callback.function_name);
		add_assoc_zval_ex(&val_tmp, "func", 4, &callback.function_name);
		Z_TRY_ADDREF_P(msg);
		add_assoc_zval_ex(&val_tmp, "msg", 3, msg);
		Z_TRY_ADDREF(val_tmp);
		zend_hash_str_update(Z_ARRVAL_P(closure), name, name_len, &val_tmp);
		zval_ptr_dtor(&val_tmp);
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_validate::__call($methord, $args)
 */
PHP_METHOD(gene_validate, __call) {
	zval *self = getThis(), *val = NULL, *key = NULL, *config = NULL, *keyArr = NULL, *listArr = NULL, *methodArr = NULL;
	int methodlen = 0;
	char *method = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "sz", &method, &methodlen, &val) == FAILURE) {
		return;
	}

	zend_update_property_string(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_METHOD), method);
	key = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_ERROR, "Please call the name method in the first place!");
	}

	config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	if (config && Z_TYPE_P(config) == IS_NULL) {
		zval config_tmp;
		array_init(&config_tmp);
		Z_TRY_ADDREF(config_tmp);
		zend_update_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), &config_tmp);
		zval_ptr_dtor(&config_tmp);
		config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	}

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

	if ((methodArr = zend_hash_str_find(Z_ARRVAL_P(listArr), method, methodlen)) == NULL) {
		zval methodArr_tmp;
		array_init(&methodArr_tmp);
		Z_TRY_ADDREF_P(val);
		add_assoc_zval_ex(&methodArr_tmp, "args", 4, val);
		Z_TRY_ADDREF(methodArr_tmp);
		zend_hash_str_update(Z_ARRVAL_P(listArr),  method, methodlen, &methodArr_tmp);
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

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &msg) == FAILURE) {
		return;
	}

	key = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_KEY), 1, NULL);
	if (key && Z_TYPE_P(key) == IS_NULL) {
		php_error_docref(NULL, E_ERROR, "Please call the name method in the first place!");
	}

	config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	if (config && Z_TYPE_P(config) == IS_NULL) {
		zval config_tmp;
		array_init(&config_tmp);
		Z_TRY_ADDREF(config_tmp);
		zend_update_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), &config_tmp);
		zval_ptr_dtor(&config_tmp);
		config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	}

	if ((keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key))) == NULL) {
		zval keyArr_tmp;
		array_init(&keyArr_tmp);
		Z_TRY_ADDREF(keyArr_tmp);
		zend_hash_str_update(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key), &keyArr_tmp);
		zval_ptr_dtor(&keyArr_tmp);
		keyArr = zend_hash_str_find(Z_ARRVAL_P(config), Z_STRVAL_P(key), Z_STRLEN_P(key));
	}

	method = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_METHOD), 1, NULL);
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

int required (char *name){

	return 1;
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

/*
 * {{{ public gene_validate::valid()
 */
PHP_METHOD(gene_validate, valid)
{
	zval *self = getThis(), *config = NULL, *field = NULL, *field_value = NULL;

	config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	if (config && Z_TYPE_P(config) == IS_ARRAY) {
		zval *element = NULL, *skip = NULL;
		zend_string *key = NULL;

		ZEND_HASH_FOREACH_STR_KEY_VAL_IND(Z_ARRVAL_P(config), key, element) {
			zval fieldArr;
			gene_explode(",", ZSTR_VAL(key), &fieldArr);
			if (Z_TYPE(fieldArr) == IS_ARRAY) {
				zval *v = NULL;
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL(fieldArr), v) {
					zend_update_property_string(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_FIELD), Z_STRVAL_P(v));
					skip = zend_hash_str_find(Z_ARRVAL_P(element), "skip", 4);
					if (skip && Z_TYPE_P(skip) == IS_TRUE) {
						continue;
					}

				}ZEND_HASH_FOREACH_END();
			}
			zval_ptr_dtor(&fieldArr);

		}ZEND_HASH_FOREACH_END();
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_validate::groupValid()
 */
PHP_METHOD(gene_validate, groupValid)
{
	zval *self = getThis(), *config = NULL, *field = NULL, *field_value = NULL;

	config = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_CONFIG), 1, NULL);
	if (config && Z_TYPE_P(config) == IS_NULL) {

	}

	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_validate::getValue($field)
 */
PHP_METHOD(gene_validate, getValue)
{
	zval *self = getThis(), *value = NULL, *field = NULL, *field_value = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &field) == FAILURE) {
		return;
	}

	value = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_VALUE), 1, NULL);
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
 * {{{ public gene_validate::getError($field)
 */
PHP_METHOD(gene_validate, getError)
{
	zval *self = getThis(), *error = NULL, *field = NULL, *field_value = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &field) == FAILURE) {
		return;
	}

	error = zend_read_property(gene_validate_ce, self, ZEND_STRL(GENE_VALIDATE_ERROR), 1, NULL);
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
 * {{{ gene_validate_methods
 */
zend_function_entry gene_validate_methods[] = {
		PHP_ME(gene_validate, init, gene_validate_init, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, name, gene_validate_name, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, skipOnEmpty, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, filter, gene_validate_filter, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, addValidator, gene_validate_addvalidator, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, msg, gene_validate_msg, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, valid, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, groupValid, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, getValue, gene_validate_get_value, ZEND_ACC_PUBLIC)
		PHP_ME(gene_validate, getError, gene_validate_get_error, ZEND_ACC_PUBLIC)
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
    gene_validate_ce = zend_register_internal_class(&gene_validate TSRMLS_CC);


    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_DATA), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_KEY), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_FIELD), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_METHOD), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_CONFIG), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_VALUE), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_ERROR), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_validate_ce, ZEND_STRL(GENE_VALIDATE_CLOSURE), ZEND_ACC_PUBLIC TSRMLS_CC);
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
