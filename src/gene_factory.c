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


#include "php_gene.h"
#include "gene_factory.h"
#include "gene_model.h"
#include "gene_reg.h"

zend_class_entry * gene_factory_ce;

/*
 * {{{ gene_factory
 */
PHP_METHOD(gene_factory, __construct)
{

}
/* }}} */


/*
 * {{{ public gene_factory::test($key)
 */
PHP_METHOD(gene_factory, create)
{
	zval *params = NULL, *reg = NULL, *entrys = NULL, *pzval = NULL, classObject;
	char *class;
	size_t class_len = 0;
	long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|zl", &class, &class_len, &params, &type) == FAILURE) {
		return;
	}

	reg = gene_reg_instance();
	entrys = zend_read_property(gene_reg_ce, reg, GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), 1, NULL);
	if (type) {
		if ((pzval = zend_hash_str_find(Z_ARRVAL_P(entrys), class, class_len)) != NULL) {
			RETURN_ZVAL(pzval, 1, 0);
		}
	}
	if (gene_factory(class, class_len, params, &classObject)) {
		if (type) {
			if (zend_hash_str_update(Z_ARRVAL_P(entrys), class, class_len, &classObject) != NULL) {
				Z_TRY_ADDREF_P(&classObject);
			}
		}
		RETURN_ZVAL(&classObject, 1, 0);
	}
	RETURN_NULL();
}
/* }}} */


/*
 * {{{ gene_factory_methods
 */
zend_function_entry gene_factory_methods[] = {
		PHP_ME(gene_factory, create, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_factory, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(factory)
{
    zend_class_entry gene_factory;
	GENE_INIT_CLASS_ENTRY(gene_factory, "Gene_Factory", "Gene\\Factory", gene_factory_methods);
	gene_factory_ce = zend_register_internal_class_ex(&gene_factory, NULL);

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
