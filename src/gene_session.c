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


#include "php_gene.h"
#include "gene_session.h"

zend_class_entry * gene_session_ce;

/* {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_session_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_has_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_del_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ bool session_start(void)
   Begin session - reinitializes freezed variables, registers browsers etc */
static session_start(TSRMLS_D)
{
	php_session_start(TSRMLS_C);
}

/* {{{ void session_write_close(void)
   Write session data and end session */
static session_write_close(TSRMLS_D)
{
	php_session_flush(TSRMLS_C);
}

/*
 * {{{ gene_session
 */
PHP_METHOD(gene_session, __construct)
{
	long debug = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &debug) == FAILURE)
    {
        RETURN_NULL();
    }
}
/* }}} */

/** {{{ public static gene_session::get($name)
*/
PHP_METHOD(gene_session, get) {
	zend_string *name 	= NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|S", &name) == FAILURE) {
		WRONG_PARAM_COUNT;
	} else {
		zval *ret,*sess = NULL;
		sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
		if (sess == NULL || Z_TYPE_P(sess) != IS_REFERENCE || Z_TYPE_P(Z_REFVAL_P(sess)) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Session is not started");
			RETURN_NULL();
		}
		if (name) {
			ret = zend_hash_find(Z_ARRVAL_P(Z_REFVAL_P(sess)), name);
			if (ret) {
				RETURN_ZVAL(ret, 1, 0);
			}
			RETURN_NULL();
		}
		RETURN_ZVAL(sess, 1, 0);
	}
}
/* }}} */

/** {{{ public static gene_session::set($name, $value)
*/
PHP_METHOD(gene_session, set) {
	zval *value;
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz", &name, &value) == FAILURE) {
		return;
	} else {
		zval *sess = NULL;
		sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
		if (sess == NULL || Z_TYPE_P(sess) != IS_REFERENCE || Z_TYPE_P(Z_REFVAL_P(sess)) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Session is not started");
			RETURN_NULL();
		}

		if (zend_hash_update(Z_ARRVAL_P(Z_REFVAL_P(sess)), name, value) != NULL) {
			Z_TRY_ADDREF_P(value);
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}
/* }}} */

/** {{{ public static gene_session::del($name)
*/
PHP_METHOD(gene_session, del) {
	zend_string *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &name) == FAILURE) {
		return;
	} else {
		zval *sess = NULL;
		sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
		if (sess == NULL || Z_TYPE_P(sess) != IS_REFERENCE || Z_TYPE_P(Z_REFVAL_P(sess)) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Session is not started");
			RETURN_NULL();
		}
		if (zend_hash_del(Z_ARRVAL_P(Z_REFVAL_P(sess)), name) == SUCCESS) {
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}
/* }}} */

/** {{{ public static gene_session::clear()
*/
PHP_METHOD(gene_session, clear) {
	zval *sess = NULL;
	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess == NULL || Z_TYPE_P(sess) != IS_REFERENCE || Z_TYPE_P(Z_REFVAL_P(sess)) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Session is not started");
		RETURN_NULL();
	}
	zend_hash_clean(Z_ARRVAL_P(Z_REFVAL_P(sess)));
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_session::has($name)
*/
PHP_METHOD(gene_session, has) {
	zend_string *name;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &name) == FAILURE) {
		return;
	} else {
		zval *sess = NULL;
		sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
		if (sess == NULL || Z_TYPE_P(sess) != IS_REFERENCE || Z_TYPE_P(Z_REFVAL_P(sess)) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Session is not started");
			RETURN_NULL();
		}
		RETURN_BOOL(zend_hash_exists(Z_ARRVAL_P(Z_REFVAL_P(sess)), name));
	}

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
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(session)
{
    zend_class_entry gene_session;
    GENE_INIT_CLASS_ENTRY(gene_session, "gene_session",  "gene\\session", gene_session_methods);
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
