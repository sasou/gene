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
#include "gene_reg.h"

zend_class_entry *gene_reg_ce;


/* {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_reg_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_reg_has_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_reg_del_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_reg_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */


/** {{{ proto private gene_reg::__construct(void)
*/
PHP_METHOD(gene_reg, __construct) {
}
/* }}} */

/** {{{ proto private gene_reg::__sleep(void)
*/
PHP_METHOD(gene_reg, __sleep) {
}
/* }}} */

/** {{{ proto private gene_reg::__wakeup(void)
*/
PHP_METHOD(gene_reg, __wakeup) {
}
/* }}} */

/** {{{ proto private gene_reg::__clone(void)
*/
PHP_METHOD(gene_reg, __clone) {
}
/* }}} */


/*
 *  {{{ zval *gene_reg_instance(zval *this_ptr TSRMLS_DC)
 */
zval *gene_reg_instance(zval *this_ptr)
{
	zval *instance = zend_read_static_property(gene_reg_ce, GENE_REG_PROPERTY_INSTANCE, strlen(GENE_REG_PROPERTY_INSTANCE), 1);

	if (UNEXPECTED(Z_TYPE_P(instance) != IS_OBJECT ||
		!instanceof_function(Z_OBJCE_P(instance), gene_reg_ce))) {
		zval regs;

		object_init_ex(this_ptr, gene_reg_ce);

		array_init(&regs);
		zend_update_property(gene_reg_ce, this_ptr, GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), &regs);
		zend_update_static_property(gene_reg_ce, GENE_REG_PROPERTY_INSTANCE, strlen(GENE_REG_PROPERTY_INSTANCE), this_ptr);
		zval_ptr_dtor(&regs);
		zval_ptr_dtor(this_ptr);

		instance = this_ptr;
	}

	return instance;
}
/* }}} */


/*
 *  {{{ public static gene_reg::get($name)
 */
PHP_METHOD(gene_reg, get) {
	zend_string *name;
	zval *ppzval,*reg,*entrys,rv;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &name) == FAILURE) {
		RETURN_NULL();
	}
	reg = gene_reg_instance(&rv);
	entrys	 = zend_read_property(gene_reg_ce, reg, GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), 1, NULL);
	if ((ppzval = zend_hash_find(Z_ARRVAL_P(entrys), name)) != NULL) {
		RETURN_ZVAL(ppzval, 1, 0);
	}
	RETURN_NULL();
}
/* }}} */

/*
 *  {{{ public static gene_reg::set($name, $value)
 */
PHP_METHOD(gene_reg, set) {
	zval *value,*reg,*entrys,rv;
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz", &name, &value) == FAILURE) {
		RETURN_NULL();
	}
	reg = gene_reg_instance(&rv);
	entrys 	 = zend_read_property(gene_reg_ce, reg, GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), 1 , NULL);
	if (zend_hash_update(Z_ARRVAL_P(entrys), name, value) != NULL) {
		Z_TRY_ADDREF_P(value);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 *  {{{ public static gene_reg::del($name)
 */
PHP_METHOD(gene_reg, del) {
	zend_string *name;
	zval *reg,*entrys,rv;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &name) == FAILURE) {
		RETURN_NULL();
	}
	reg = gene_reg_instance(&rv);
	entrys	 = zend_read_property(gene_reg_ce, reg,GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), 1 , NULL);
	zend_hash_del(Z_ARRVAL_P(entrys), name);
	RETURN_TRUE;
}
/* }}} */

/*
 *  {{{ public gene_reg::has($name)
 */
PHP_METHOD(gene_reg, has) {
	zend_string *name;
	zval *reg,*entrys,rv;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}
	reg = gene_reg_instance(&rv);
	entrys	 = zend_read_property(gene_reg_ce, reg,GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG), 1 , NULL);
	if (zend_hash_exists(Z_ARRVAL_P(entrys), name) == 1) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 *  {{{ public gene_reg::getInstance(void)
 */
PHP_METHOD(gene_reg, getInstance)
{
	zval *reg,rv;
	reg = gene_reg_instance(&rv);
	RETURN_ZVAL(reg, 1, 0);
}
/* }}} */

/*
 * {{{ gene_reg_methods
 */
zend_function_entry gene_reg_methods[] = {
		PHP_ME(gene_reg, __construct, 	NULL, ZEND_ACC_CTOR|ZEND_ACC_PRIVATE)
		PHP_ME(gene_reg, __clone, 		NULL, ZEND_ACC_CLONE|ZEND_ACC_PRIVATE)
		PHP_ME(gene_reg, getInstance, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_reg, get, gene_reg_get_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_reg, has, gene_reg_has_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_reg, set, gene_reg_set_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_reg, del, gene_reg_del_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(reg)
{
    zend_class_entry gene_reg;
    GENE_INIT_CLASS_ENTRY(gene_reg, "Gene_Reg",  "Gene\\Reg", gene_reg_methods);
    gene_reg_ce = zend_register_internal_class_ex(&gene_reg, NULL);
    gene_reg_ce->ce_flags |= ZEND_ACC_FINAL;

	//static
    zend_declare_property_null(gene_reg_ce, GENE_REG_PROPERTY_INSTANCE, strlen(GENE_REG_PROPERTY_INSTANCE),  ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);
    zend_declare_property_null(gene_reg_ce, GENE_REG_PROPERTY_REG, strlen(GENE_REG_PROPERTY_REG),  ZEND_ACC_PROTECTED);
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
