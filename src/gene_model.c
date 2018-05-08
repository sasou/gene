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


#include "php_gene.h"
#include "gene_model.h"
#include "gene_reg.h"


zend_class_entry * gene_model_ce;


ZEND_BEGIN_ARG_INFO_EX(gene_model_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_model_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()


zend_bool gene_factory_load_class(char *className, int tmp_len, zval *classObject) {
	zend_string *c_key = NULL;
	zend_class_entry *pdo_ptr = NULL;

	c_key = zend_string_init(className, tmp_len, 0);
	pdo_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);
	if (pdo_ptr) {
		object_init_ex(classObject, pdo_ptr);
		return TRUE;
	}
	return FALSE;
}

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
	zval *pzval, *props, classObject;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_ZVAL(getThis(), 1, 0);
	} else {
		props = zend_read_property(gene_model_ce, getThis(), ZEND_STRL(GENE_MODEL_ATTR), 1, NULL);
		if ((pzval = zend_hash_find(Z_ARRVAL_P(props), name)) == NULL) {
			char *tmp = NULL;
			int tmp_len = 0;
			tmp_len = spprintf(&tmp, 0, "Models\\User");
			if (gene_factory_load_class(tmp, tmp_len, &classObject)) {
				if (zend_hash_update(Z_ARRVAL_P(props), name, &classObject) != NULL) {
					Z_TRY_ADDREF_P(&classObject);
					RETURN_ZVAL(&classObject, 1, 0);
				}
			}
			efree(tmp);
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
	zval *ppzval, *reg, *entrys;
	zval ret;
	char *name;
	int name_len = 0;
	zend_call_method_with_0_params(NULL, NULL, NULL, "get_called_class", &ret);
	name_len = spprintf(&name, 0, "%s", Z_STRVAL(ret));
	zval_ptr_dtor(&ret);
	reg = gene_reg_instance();
	entrys = zend_read_property(gene_reg_ce, reg, GENE_REG_PROPERTY_REG,
			strlen(GENE_REG_PROPERTY_REG), 1, NULL);
	if ((ppzval = zend_hash_str_find(Z_ARRVAL_P(entrys), name, name_len)) != NULL) {
		efree(name);
		RETURN_ZVAL(ppzval, 1, 0);
	} else {
		zval rv;
		if (gene_factory_load_class(name, name_len, &rv)) {
			if (zend_hash_str_update(Z_ARRVAL_P(entrys), name, name_len, &rv) != NULL) {
				Z_TRY_ADDREF_P(&rv);
			}
			efree(name);
			RETURN_ZVAL(&rv, 1, 0);
		}
	}
	efree(name);
	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto public gene_model::__destruct
*/
PHP_METHOD(gene_model, __destruct) {

}
/* }}} */

/*
 * {{{ gene_model_methods
 */
zend_function_entry gene_model_methods[] = {
		PHP_ME(gene_model, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(gene_model, __destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
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
	//gene_model_ce->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;
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
