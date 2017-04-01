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
#include "gene_execute.h"

zend_class_entry * gene_execute_ce;

/*
 * {{{ void * gene_execite_opcodes_run(zend_op_array *op_array TSRMLS_DC)
 */
void *gene_execite_opcodes_run(zend_op_array *op_array TSRMLS_DC){
	zend_op_array *orig_op_array = CG(active_op_array);
	zval ret;
	CG(active_op_array) = op_array;
	if (CG(active_op_array)) {
		zend_execute(CG(active_op_array), &ret);
		destroy_op_array(CG(active_op_array) TSRMLS_CC);
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
PHP_METHOD(gene_execute, __construct)
{
	long debug = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &debug) == FAILURE)
    {
        RETURN_NULL();
    }
    if(debug) {
    	zend_update_property_long(gene_execute_ce, getThis(), GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), debug TSRMLS_CC);
    } else {
    	zend_update_property_long(gene_execute_ce, getThis(), GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), 0 TSRMLS_CC);
    }
}
/* }}} */

/*
 * {{{ public gene_execute::GetOpcodes($codeString)
 */
PHP_METHOD(gene_execute, GetOpcodes)
{
	zend_string *php_script;
	int i;
	zval zv, opcodes_array,*debug;
	zend_op_array *op_array;
	debug = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &php_script) == FAILURE) {
		return;
	}
	debug = zend_read_property(gene_execute_ce, getThis(), GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), 0, NULL);
	if(debug->value.lval){
		php_printf(ZSTR_VAL(php_script));
	}
	ZVAL_STRINGL(&zv, ZSTR_VAL(php_script), ZSTR_LEN(php_script));
	array_init(&opcodes_array);

	op_array = zend_compile_string(&zv, "");
	if (! op_array) {
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
PHP_METHOD(gene_execute, StringRun)
{
	zend_string *php_script;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &php_script) == FAILURE) {
		return;
	}
	zend_try {
		zend_eval_stringl(ZSTR_VAL(php_script), ZSTR_LEN(php_script), NULL, "" TSRMLS_CC);
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
		PHP_ME(gene_execute, GetOpcodes, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_execute, StringRun, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_execute, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(execute)
{
    zend_class_entry gene_execute;
    GENE_INIT_CLASS_ENTRY(gene_execute, "Gene_Execute",  "Gene\\Execute", gene_execute_methods);
    gene_execute_ce = zend_register_internal_class(&gene_execute TSRMLS_CC);

	//debug
    zend_declare_property_null(gene_execute_ce, GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), ZEND_ACC_PUBLIC TSRMLS_CC);
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
