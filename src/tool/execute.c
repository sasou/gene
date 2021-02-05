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

#include "../gene.h"
#include "../tool/execute.h"

zend_class_entry * gene_execute_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_execute_construct_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, debug)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_execute_getcode_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, php_script)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_execute_stringrun_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, php_script)
ZEND_END_ARG_INFO()

/*
 * {{{ void * gene_execite_opcodes_run(zend_op_array *op_array)
 */
void *gene_execite_opcodes_run(zend_op_array *op_array) {
	zend_op_array *orig_op_array = CG(active_op_array);
	zval ret;
	CG(active_op_array) = op_array;
	if (CG(active_op_array)) {
		zend_execute(CG(active_op_array), &ret);
		destroy_op_array(CG(active_op_array));
		efree(CG(active_op_array));
	}
	CG(active_op_array) = orig_op_array;
	efree(orig_op_array);
	return NULL;
}
/* }}} */

/*
 * {{{ gene_execute_methods
 */
PHP_METHOD(gene_execute, __construct) {
	long debug = 0;
	zval *self = getThis();
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
	if (debug) {
		zend_update_property_long(gene_execute_ce, gene_strip_obj(self), GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), debug);
	} else {
		zend_update_property_long(gene_execute_ce, gene_strip_obj(self), GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), 0);
	}
}
/* }}} */

/*
 * {{{ public gene_execute::GetOpcodes($codeString)
 */
PHP_METHOD(gene_execute, GetOpcodes) {
	zend_string *php_script;
	int i;
	zval *self = getThis(), zv, opcodes_array, *debug;
	zend_op_array *op_array;
	debug = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &php_script) == FAILURE) {
		return;
	}
	debug = zend_read_property(gene_execute_ce, gene_strip_obj(self), GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), 0, NULL);
	if (debug->value.lval) {
		php_printf(ZSTR_VAL(php_script));
	}
	ZVAL_STRINGL(&zv, ZSTR_VAL(php_script), ZSTR_LEN(php_script));
	array_init(&opcodes_array);

#if PHP_VERSION_ID < 80000
		op_array = zend_compile_string(&zv, "");
#else
        op_array = zend_compile_string(Z_STR(zv), "");
#endif
	if (!op_array) {
		return;
	}
	for (i = 0; i < op_array->cache_size; i++) {
		zend_op op = op_array->opcodes[i];
		add_index_long(&opcodes_array, i, op.lineno);
	}
	efree(op_array);
	zval_ptr_dtor(&zv);
	RETURN_ZVAL(&opcodes_array, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_execute::StringRun($codeString)
 */
PHP_METHOD(gene_execute, StringRun) {
	zend_string *php_script;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &php_script) == FAILURE) {
		return;
	}
	zend_try {
		zend_eval_stringl(ZSTR_VAL(php_script), ZSTR_LEN(php_script), NULL, "");
	} zend_catch {
		zend_bailout();
	}zend_end_try();

	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_execute_methods
 */
zend_function_entry gene_execute_methods[] = {
	PHP_ME(gene_execute, GetOpcodes, gene_execute_getcode_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_execute, StringRun, gene_execute_stringrun_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_execute, __construct, gene_execute_construct_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(execute) {
	zend_class_entry gene_execute;
	GENE_INIT_CLASS_ENTRY(gene_execute, "Gene_Execute", "Gene\\Execute", gene_execute_methods);
	gene_execute_ce = zend_register_internal_class(&gene_execute);

	//debug
	zend_declare_property_null(gene_execute_ce, GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), ZEND_ACC_PUBLIC);
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
