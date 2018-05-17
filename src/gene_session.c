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

#include "php_gene.h"
#include "ext/session/php_session.h"
#include "gene_session.h"
#include "gene_common.h"

zend_class_entry * gene_session_ce;

/* {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_session_get_arginfo, 0, 0, 1) ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_has_arginfo, 0, 0, 1) ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_del_arginfo, 0, 0, 1) ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_set_arginfo, 0, 0, 2) ZEND_ARG_INFO(0, name)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */


void gene_session_start() /*{{{*/
{
    zval function_name,ret;
    ZVAL_STRING(&function_name, "session_start");
    call_user_function(NULL, NULL, &function_name, &ret, 0, NULL);
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&ret);
}/*}}}*/

int gene_session_status() /*{{{*/
{
    zval function_name,ret;
    zend_long type = 0;
    ZVAL_STRING(&function_name, "session_status");
    call_user_function(NULL, NULL, &function_name, &ret, 0, NULL);
    zval_ptr_dtor(&function_name);
    if (Z_TYPE(ret) == IS_LONG) {
    	type = Z_LVAL(ret);
    }
    zval_ptr_dtor(&ret);
    return type;
}/*}}}*/

void gene_session_commit() /*{{{*/
{
    zval function_name,ret;
    ZVAL_STRING(&function_name, "session_commit");
    call_user_function(NULL, NULL, &function_name, &ret, 0, NULL);
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&ret);
}/*}}}*/


/** {{{ zval * gene_session_get_by_path(char *path TSRMLS_DC)
 */
zval * gene_session_get_by_path(char *path TSRMLS_DC) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp = NULL;
	zval *sess = NULL;

	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		tmp = Z_REFVAL_P(sess);
		if (path != NULL) {
			seg = php_strtok_r(path, "/", &ptr);
			while (seg) {
				if (Z_TYPE_P(tmp) != IS_ARRAY) {
					return NULL;
				}
				tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
				if (tmp == NULL) {
					return NULL;
				}
				seg = php_strtok_r(NULL, "/", &ptr);
			}
		}
		return tmp;
	}
	return NULL;
}
/* }}} */

/** {{{ static zval * gene_session_set_val(char *keyString, int keyString_len TSRMLS_DC)
 */
static zval * gene_session_set_val(zval *val, char *keyString, int keyString_len, zval *zvalue TSRMLS_DC) {
	zval tmp, *copyval;
	if (val == NULL) {
		return NULL;
	}

	if (zvalue == NULL) {
		copyval = zend_symtable_str_find(Z_ARRVAL_P(val), keyString, keyString_len);
		if (copyval == NULL) {
			array_init(&tmp);
			copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
			zval_ptr_dtor(&tmp);
		} else {
			if (Z_TYPE_P(copyval) != IS_ARRAY) {
				array_init(&tmp);
				copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
				zval_ptr_dtor(&tmp);
			}
		}
	} else {
		copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, zvalue);
	}
	return copyval;
}
/* }}} */

/** {{{ void gene_session_set_by_path(char *path, zval *zvalue TSRMLS_DC)
 */
void gene_session_set_by_path(char *path, zval *zvalue TSRMLS_DC) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp;
	zval *sess = NULL, ret;
	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess == NULL) {
		array_init(&ret);
		zend_symtable_str_update(GENE_G(cache), ZEND_STRL("_SESSION"), &ret);
		tmp = &ret;
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), zvalue);
				Z_TRY_ADDREF_P(zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
		zval_ptr_dtor(&ret);
	} else {
		tmp = Z_REFVAL_P(sess);
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), zvalue);
				Z_TRY_ADDREF_P(zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
	}
	return;
}
/* }}} */

/** {{{ zend_bool gene_session_del_by_path(char *path TSRMLS_DC)
 */
zend_bool gene_session_del_by_path(char *path TSRMLS_DC) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp = NULL;
	zval *sess = NULL;

	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		tmp = Z_REFVAL_P(sess);
		if (path != NULL) {
			seg = php_strtok_r(path, "/", &ptr);
			while (seg) {
				if (Z_TYPE_P(tmp) != IS_ARRAY) {
					return 0;
				}
				if (ptr && strlen(ptr) > 0) {
					tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
				} else {
					return zend_symtable_str_del(Z_ARRVAL_P(tmp), seg, strlen(seg)) == 0 ? 1 : 0;
				}
				seg = php_strtok_r(NULL, "/", &ptr);
			}
		}
		return 0;
	}
	return 0;
}
/* }}} */

/*
 * {{{ gene_session
 */
PHP_METHOD(gene_session, __construct) {

}
/* }}} */


/** {{{ public static gene_session::get($name)
 */
PHP_METHOD(gene_session, get) {
	zend_string *name = NULL;
	zval *sess = NULL;
	char *path = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|S", &name) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
		replaceAll(path, '.', '/');
	}

	sess = gene_session_get_by_path(path TSRMLS_CC);
	if (path) {
		efree(path);
	}
	gene_session_commit();
	if (sess) {
		RETURN_ZVAL(sess, 1, 0);
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public static gene_session::set($name, $value)
 */
PHP_METHOD(gene_session, set) {
	zval *value;
	zend_string *name;
	char *path = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "Sz", &name, &value) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
		replaceAll(path, '.', '/');
	}
	gene_session_set_by_path(path, value TSRMLS_CC);
	if (path) {
		efree(path);
	}
	gene_session_commit();
	RETURN_TRUE;
}
/* }}} */

/** {{{ public static gene_session::del($name)
 */
PHP_METHOD(gene_session, del) {
	zend_string *name;
	char *path = NULL;
	zend_bool ret = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &name) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
		replaceAll(path, '.', '/');
	}
	ret = gene_session_del_by_path(path TSRMLS_CC);
	if (path) {
		efree(path);
	}
	gene_session_commit();
	RETURN_BOOL(ret);
}
/* }}} */

/** {{{ public static gene_session::clear()
 */
PHP_METHOD(gene_session, clear) {
	zval *sess = NULL;
	if (gene_session_status() < 2) {
		gene_session_start();
	}
	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		zend_hash_clean(Z_ARRVAL_P(Z_REFVAL_P(sess)));
	}
	gene_session_commit();
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_session::has($name)
 */
PHP_METHOD(gene_session, has) {
	zend_string *name;
	zend_bool ret = 0;
	zval *sess = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &name) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		ret = zend_hash_exists(Z_ARRVAL_P(Z_REFVAL_P(sess)), name);
	}
	gene_session_commit();
	RETURN_BOOL(ret);

}
/* }}} */

/*
 * {{{ gene_session_methods
 */
zend_function_entry gene_session_methods[] = {
	PHP_ME(gene_session, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, clear, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_MALIAS(gene_session, __get, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __isset, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __set, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __unset, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(session) {
	zend_class_entry gene_session;
	GENE_INIT_CLASS_ENTRY(gene_session, "Gene_Session", "Gene\\Session", gene_session_methods);
	gene_session_ce = zend_register_internal_class(&gene_session TSRMLS_CC);

	//debug
	//zend_declare_property_null(gene_application_ce, GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), ZEND_ACC_PUBLIC TSRMLS_CC);
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
