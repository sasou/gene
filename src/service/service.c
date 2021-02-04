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
#include "Zend/zend_interfaces.h"
#include "zend_exceptions.h"


#include "../gene.h"
#include "../mvc/model.h"
#include "../service/service.h"
#include "../di/di.h"
#include "../factory/factory.h"
#include "../http/response.h"
#include "../common/common.h"


zend_class_entry * gene_service_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_service_arg_se, 0, 0, 1)
    ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_service_arg_se_data, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, count)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_service_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_service_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_model_arg_instance, 0, 0, 1)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()


/*
 * {{{ gene_service
 */
PHP_METHOD(gene_service, __construct)
{

}
/* }}} */

/*
 * {{{ gene_service
 */
PHP_METHOD(gene_service, __set)
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
 * {{{ gene_service
 */
PHP_METHOD(gene_service, __get)
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


/** {{{ proto public gene_service::success(string $text, int $code)
 */
PHP_METHOD(gene_service, success) {
	zend_string *text;
	zend_long code = 2000;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), text);
}
/* }}} */


/** {{{ proto public gene_service::error(string $text, int $code)
 */
PHP_METHOD(gene_service, error) {
	zend_string *text;
	zend_long code = 4000;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &text, &code) == FAILURE) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_CODE), code);
	add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), text);
}
/* }}} */

/** {{{ proto public gene_service::data(mixed $data, int $count,string $text, int $code)
 */
PHP_METHOD(gene_service, data) {
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
		add_assoc_str_ex(return_value, ZEND_STRL(GENE_RESPONSE_MSG), text);
	}
	Z_TRY_ADDREF_P(data);
	add_assoc_zval_ex(return_value, ZEND_STRL(GENE_RESPONSE_DATA), data);
	if (count >= 0) {
		add_assoc_long_ex(return_value, ZEND_STRL(GENE_RESPONSE_COUNT), count);
	}
}
/* }}} */

/*
 * {{{ public gene_service::getInstance()
 */
PHP_METHOD(gene_service, getInstance)
{
	zval obj, *params = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &params) == FAILURE) {
		return;
	}
	zval class_name;
	gene_class_name(&class_name);
	zval *ret = gene_class_instance(&obj, &class_name, params);
	zval_ptr_dtor(&class_name);
	RETURN_ZVAL(ret, 1, 0);
}
/* }}} */

/** {{{ proto public gene_service::__destruct
*/
PHP_METHOD(gene_service, __destruct) {

}
/* }}} */

/*
 * {{{ gene_service_methods
 */
zend_function_entry gene_service_methods[] = {
		PHP_ME(gene_service, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(gene_service, __destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
		PHP_ME(gene_service, __get, gene_service_get, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, __set, gene_service_set, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, success, gene_service_arg_se, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, error, gene_service_arg_se, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, data, gene_service_arg_se_data, ZEND_ACC_PUBLIC)
		PHP_ME(gene_service, getInstance, gene_model_arg_instance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(service)
{
    zend_class_entry gene_service;
	GENE_INIT_CLASS_ENTRY(gene_service, "Gene_Service", "Gene\\Service", gene_service_methods);
	gene_service_ce = zend_register_internal_class_ex(&gene_service, NULL);

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
