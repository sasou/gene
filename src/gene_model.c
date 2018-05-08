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


#include "php_gene.h"
#include "gene_model.h"

zend_class_entry * gene_model_ce;


ZEND_BEGIN_ARG_INFO_EX(gene_model_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_model_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

/*
 * {{{ gene_model
 */
PHP_METHOD(gene_model, __construct)
{
	zval prop;
	array_init(&prop);
	zend_update_property(gene_model_ce, getThis(), ZEND_STRL(GENE_MODEL_ATTR), &prop);
	zval_ptr_dtor(&prop);
}
/* }}} */

/*
 * {{{ gene_model
 */
PHP_METHOD(gene_model, __set)
{
	zend_string *name;
	zval *value, *props;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	props = zend_read_property(gene_model_ce, getThis(), ZEND_STRL(GENE_MODEL_ATTR), 1, NULL);
	if (zend_hash_update(Z_ARRVAL_P(props), name, value) != NULL) {
		Z_TRY_ADDREF_P(value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */


/*
 * {{{ gene_model
 */
PHP_METHOD(gene_model, __get)
{
	zval *pzval, *props, db_object;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_ZVAL(getThis(), 1, 0);
	} else {
		props = zend_read_property(gene_model_ce, getThis(), ZEND_STRL(GENE_MODEL_ATTR), 1, NULL);
		if ((pzval = zend_hash_find(Z_ARRVAL_P(props), name)) == NULL) {
			zend_string *c_key = zend_string_init(ZEND_STRL("\\Ext\\Model"), 0);
			zend_class_entry *pdo_ptr = zend_lookup_class(c_key);
			zend_string_free(c_key);
			if (pdo_ptr) {
				object_init_ex(&db_object, pdo_ptr);
				RETURN_ZVAL(&db_object, 1, 0);
			}

			RETURN_FALSE;
		}
		RETURN_ZVAL(pzval, 1, 0);
	}
	RETURN_FALSE;
}
/* }}} */


/*
 * {{{ public gene_model::getInstance()
 */
PHP_METHOD(gene_model, getInstance)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */



/*
 * {{{ gene_model_methods
 */
zend_function_entry gene_model_methods[] = {
		PHP_ME(gene_model, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(gene_model, __get, gene_model_get, ZEND_ACC_PUBLIC)
		PHP_ME(gene_model, __set, gene_model_set, ZEND_ACC_PUBLIC)
		PHP_ME(gene_model, getInstance, NULL, ZEND_ACC_PUBLIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(model)
{
    zend_class_entry gene_model;
	GENE_INIT_CLASS_ENTRY(gene_model, "Gene_Model", "Gene\\Model", gene_model_methods);
	gene_model_ce = zend_register_internal_class_ex(&gene_model, NULL);
	zend_declare_property_null(gene_model_ce, ZEND_STRL(GENE_MODEL_ATTR), ZEND_ACC_PROTECTED TSRMLS_CC);

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
