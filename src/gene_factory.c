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
#include "gene_di.h"

zend_class_entry * gene_factory_ce;


zend_bool gene_factory_load_class(char *className, int tmp_len, zval *classObject) {
	zend_string *c_key = NULL;
	zend_class_entry *pdo_ptr = NULL;

	c_key = zend_string_init(className, tmp_len, 0);
	pdo_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);
	if (pdo_ptr) {
		object_init_ex(classObject, pdo_ptr);
		return 1;
	}
	return 0;
}

void gene_factory_construct(zval *object, zval *param, zval *retval) /*{{{*/
{
	zval *one = NULL,*two = NULL, *three = NULL, *fro = NULL;
    zval function_name;
    ZVAL_STRING(&function_name, "__construct");
    uint32_t param_count = 0;
    zval params[] = {0};
    if (param && Z_TYPE_P(param) == IS_ARRAY) {
    	param_count = zend_hash_num_elements(Z_ARRVAL_P(param));
        switch(param_count) {
        case 1:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	params[0] = *one;
        	break;
        case 2:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	two = zend_hash_index_find(Z_ARRVAL_P(param), 1);
        	params[0] = *one;
        	params[1] = *two;
        	break;
        case 3:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	two = zend_hash_index_find(Z_ARRVAL_P(param), 1);
        	three = zend_hash_index_find(Z_ARRVAL_P(param), 2);
        	params[0] = *one;
        	params[1] = *two;
        	params[2] = *three;
        	break;
        case 4:
        	one = zend_hash_index_find(Z_ARRVAL_P(param), 0);
        	two = zend_hash_index_find(Z_ARRVAL_P(param), 1);
        	three = zend_hash_index_find(Z_ARRVAL_P(param), 2);
        	fro = zend_hash_index_find(Z_ARRVAL_P(param), 3);
        	params[0] = *one;
        	params[1] = *two;
        	params[2] = *three;
        	params[3] = *fro;
        	break;
        }
        call_user_function(NULL, object, &function_name, retval, param_count, params);
    } else {
    	call_user_function(NULL, object, &function_name, retval, param_count, NULL);
    }
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_factory_call_action(zval *object, char *action, zval *param, zval *retval) /*{{{*/
{
	zval *one = NULL,*two = NULL, *three = NULL, *fro = NULL;
    zval function_name;
    ZVAL_STRING(&function_name, action);
    uint32_t param_count = 0;
    zval params[] = {0};
    if (param && Z_TYPE_P(param) == IS_ARRAY) {
    	param_count = 1;
    	params[0] = *param;
        call_user_function(NULL, object, &function_name, retval, param_count, params);
    } else {
    	call_user_function(NULL, object, &function_name, retval, param_count, NULL);
    }
    zval_ptr_dtor(&function_name);
}/*}}}*/

zend_bool gene_factory(char *className, int tmp_len, zval *params, zval *classObject) {
	zend_string *c_key = NULL;
	zend_class_entry *pdo_ptr = NULL;
	zval ret;
	c_key = zend_string_init(className, tmp_len, 0);
	pdo_ptr = zend_lookup_class(c_key);
	zend_string_free(c_key);
	if (pdo_ptr) {
		object_init_ex(classObject, pdo_ptr);
		if (Z_TYPE_P(classObject) == IS_OBJECT
				&& zend_hash_str_exists(&(Z_OBJCE_P(classObject)->function_table), ZEND_STRL("__construct"))) {
			gene_factory_construct(classObject, params, &ret);
		}
		zval_ptr_dtor(&ret);
		return 1;
	}
	return 0;
}


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
	zval *params = NULL, *di = NULL, *entrys = NULL, *pzval = NULL, classObject;
	char *class;
	size_t class_len = 0;
	long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|zl", &class, &class_len, &params, &type) == FAILURE) {
		return;
	}

	if (type) {
		di = gene_di_instance();
		entrys = zend_read_property(gene_di_ce, di, GENE_DI_PROPERTY_REG, strlen(GENE_DI_PROPERTY_REG), 1, NULL);
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
