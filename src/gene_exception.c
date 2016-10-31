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
#include "gene_exception.h"
#include "gene_router.h"
#include "gene_view.h"

zend_class_entry * gene_exception_ce;

/** {{{ int gene_exception_error_register(zval *callback,zval *error_type TSRMLS_DC)
*/
int gene_exception_error_register(zval *callback,zval *error_type TSRMLS_DC) {
	zval ret,bak;
	zval params[2] = {0};
	zval function;
	int arg_num = 1;

	if (!callback) {
		if (GENE_G(use_namespace)) {
			ZVAL_STRING(&bak, GENE_ERROR_FUNC_NAME_NS);
		} else {
			ZVAL_STRING(&bak, GENE_ERROR_FUNC_NAME);
		}
		ZVAL_COPY(&params[0], &bak);
	} else {
		ZVAL_COPY(&params[0], callback);
	}
	if (error_type) {
		ZVAL_COPY(&params[1], error_type);
		arg_num = 2;
	}

	ZVAL_STRING(&function, "set_error_handler");
	if (call_user_function(EG(function_table), NULL, &function, &ret, arg_num, params TSRMLS_CC) == FAILURE) {
		zval_dtor(&ret);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Call to set_error_handler failed");
		return 0;
	}
	zval_dtor(&ret);
	return 1;
}

/** {{{ int gene_exception_register(zval *callback,zval *error_type TSRMLS_DC)
*/
int gene_exception_register(zval *callback,zval *error_type TSRMLS_DC) {
	zval ret,bak;
	zval params[1]	 = {0};
	zval function;
	int arg_num = 1;

	if (!callback) {
		if (GENE_G(use_namespace)) {
			ZVAL_STRING(&bak, GENE_EXCEPTION_FUNC_NAME_NS);
		} else {
			ZVAL_STRING(&bak, GENE_EXCEPTION_FUNC_NAME);
		}
		ZVAL_COPY(&params[0], &bak);
	} else {
		ZVAL_COPY(&params[0], callback);
	}

	ZVAL_STRING(&function, "set_exception_handler");
	if (call_user_function(EG(function_table), NULL, &function, &ret, arg_num, params TSRMLS_CC) == FAILURE) {
		zval_dtor(&ret);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Call to set_exception_handler failed");
		return 0;
	}
	zval_dtor(&ret);
	return 1;
}


/** {{{void gene_trigger_error(int type TSRMLS_DC, char *format, ...)
 */
void gene_trigger_error(int type TSRMLS_DC, char *format, ...) {
	va_list args;
	char *message;
	int msg_len;

	va_start(args, format);
	msg_len = vspprintf(&message, 0, format, args);
	va_end(args);

	if (GENE_G(gene_error)) {
#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3
		if (!EG(exception)) {
			gene_throw_exception(type, message TSRMLS_CC);
		}
#else
		gene_throw_exception(type, message TSRMLS_CC);
#endif
	} else {
		php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", message);
	}
	efree(message);
}
/* }}} */

/** {{{ zend_class_entry * gene_get_exception_base(int root)
*/
zend_class_entry * gene_get_exception_base(int root) {
#if can_handle_soft_dependency_on_SPL && defined(HAVE_SPL) && ((PHP_MAJOR_VERSION > 5) || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 1))
	if (!root) {
		if (!spl_ce_RuntimeException) {
			zend_class_entry **pce;

			if (zend_hash_find(CG(class_table), "runtimeexception", sizeof("RuntimeException"), (void **) &pce) == SUCCESS) {
				spl_ce_RuntimeException = *pce;
				return *pce;
			}
		} else {
			return spl_ce_RuntimeException;
		}
	}
#endif

	return zend_exception_get_default();
}
/* }}} */


/** {{{ void gene_throw_exception(long code, char *message TSRMLS_DC)
*/
void gene_throw_exception(long code, char *message TSRMLS_DC) {
	zend_class_entry *base_exception = gene_exception_ce;

	zend_throw_exception(base_exception, message, code TSRMLS_CC);
}
/* }}} */

/*
 * {{{ gene_exception
 */
PHP_METHOD(gene_exception, __construct)
{
	long debug = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &debug) == FAILURE)
    {
        RETURN_NULL();
    }
}
/* }}} */


/** {{{ public gene_exception::setErrorHandler(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
*/
PHP_METHOD(gene_exception, setErrorHandler) {
	zval *callback, *error_type = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &callback, &error_type) == FAILURE) {
		return;
	}
	gene_exception_error_register(callback, error_type TSRMLS_CC);
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_exception::setExceptionHandler(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
*/
PHP_METHOD(gene_exception, setExceptionHandler) {
	zval *callback = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &callback) == FAILURE) {
		return;
	}
	gene_exception_register(callback, NULL TSRMLS_CC);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_exception::doError
 */
PHP_METHOD(gene_exception, doError)
{
	char *msg,*file;
	zval *params = NULL;
	long code = 0,line=0,msg_len,file_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"ls|slz", &code,&msg,&msg_len,&file,&file_len,&line,&params) == FAILURE)
    {
        RETURN_NULL();
    }
    if (GENE_G(gene_error) == 1) {
    	gene_trigger_error(code, "%s",msg);
    }
    RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_exception::doException
 */
PHP_METHOD(gene_exception, doException)
{
	zval *e = NULL,*safe = NULL;
	char *run = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"z", &e) == FAILURE)
    {
        RETURN_NULL();
    }
    Z_TRY_ADDREF_P(e);
    zend_update_static_property(gene_exception_ce, GENE_EXCEPTION_EX, strlen(GENE_EXCEPTION_EX), e);
    if (GENE_G(gene_exception) == 1) {
		gene_view_display("error");
    } else {
    	if (GENE_G(show_exception) == 1) {
        	spprintf(&run, 0, "%s", HTML_ERROR_CONTENT);
        	zend_try {
        		zend_eval_stringl(run, strlen(run), NULL, "gene_error" TSRMLS_CC);
        	} zend_catch {
        		efree(run);
        		run = NULL;
        		zend_bailout();
        	} zend_end_try();
        	efree(run);
        	run = NULL;
    	}
    }
}
/* }}} */


/*
 * {{{ gene_exception::doException
 */
PHP_METHOD(gene_exception, getEx)
{
	zval *ex = zend_read_static_property(gene_exception_ce, GENE_EXCEPTION_EX, strlen(GENE_EXCEPTION_EX), 1);
	RETURN_ZVAL(ex, 1, 0);
}
/* }}} */

/*
 * {{{ gene_exception_methods
 */
zend_function_entry gene_exception_methods[] = {
		PHP_ME(gene_exception, setErrorHandler, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_exception, setExceptionHandler, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_exception, doError, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_exception, doException, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_exception, getEx, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_exception, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(exception)
{
    zend_class_entry gene_exception;
    GENE_INIT_CLASS_ENTRY(gene_exception, "Gene_Exception",  "Gene\\Exception", gene_exception_methods);
    gene_exception_ce = zend_register_internal_class_ex(&gene_exception, gene_get_exception_base(0));

	//prop
    zend_declare_property_null(gene_exception_ce, GENE_EXCEPTION_EX, strlen(GENE_EXCEPTION_EX),  ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);
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
