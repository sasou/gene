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
#include "gene_exception.h"
#include "gene_router.h"
#include "gene_view.h"

zend_class_entry * gene_exception_ce;

/** {{{ int gene_exception_error_register(zval *callback,zval *error_type TSRMLS_DC)
 */
int gene_exception_error_register(zval *callback, zval *error_type TSRMLS_DC) {
	zval ret, bak;
	zval function;
	int arg_num = 1;

	if (!callback) {
		if (GENE_G(use_namespace)) {
			ZVAL_STRING(&bak, GENE_ERROR_FUNC_NAME_NS);
		} else {
			ZVAL_STRING(&bak, GENE_ERROR_FUNC_NAME);
		}
		callback = &bak;
	}
	zval params[] = {*callback};
	if (error_type) {
		zval params[] = {*callback, *callback};
		arg_num = 2;
	}
	ZVAL_STRING(&function, "set_error_handler");
	if (call_user_function(EG(function_table), NULL, &function, &ret, arg_num, params TSRMLS_CC) == FAILURE) {
		zval_ptr_dtor(&ret);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Call to set_error_handler failed");
		return 0;
	}
	zval_ptr_dtor(&function);
	zval_ptr_dtor(&bak);
	zval_ptr_dtor(&ret);
	zval_dtor(params);
	return 1;
}

/** {{{ int gene_exception_register(zval *callback TSRMLS_DC)
 */
int gene_exception_register(zval *callback TSRMLS_DC) {
	zval ret, bak;
	zval function;
	int arg_num = 1;

	if (!callback) {
		if (GENE_G(use_namespace)) {
			ZVAL_STRING(&bak, GENE_EXCEPTION_FUNC_NAME_NS);
		} else {
			ZVAL_STRING(&bak, GENE_EXCEPTION_FUNC_NAME);
		}
		callback = &bak;
	}
	zval params[] = {*callback};
	ZVAL_STRING(&function, "set_exception_handler");
	if (call_user_function(EG(function_table), NULL, &function, &ret, arg_num, params TSRMLS_CC) == FAILURE) {
		zval_ptr_dtor(&ret);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Call to set_exception_handler failed");
		return 0;
	}
	zval_ptr_dtor(&function);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&bak);
	zval_dtor(params);
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
		gene_throw_exception(type, message TSRMLS_CC);
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
	zend_class_entry *base_exception = gene_get_exception_base(0);

	zend_throw_exception(base_exception, message, code TSRMLS_CC);
}
/* }}} */

/** {{{ public gene_exception::setErrorHandler(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
 */
PHP_METHOD(gene_exception, setErrorHandler) {
	zval *callback, *error_type = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|z", &callback,
			&error_type) == FAILURE) {
		return;
	}
	gene_exception_error_register(callback, error_type TSRMLS_CC);
	RETURN_TRUE
	;
}
/* }}} */

/** {{{ public gene_exception::setExceptionHandler(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
 */
PHP_METHOD(gene_exception, setExceptionHandler) {
	zval *callback = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z", &callback)
			== FAILURE) {
		return;
	}
	gene_exception_register(callback TSRMLS_CC);
	RETURN_TRUE
	;
}
/* }}} */

/*
 * {{{ gene_exception::doError
 */
PHP_METHOD(gene_exception, doError) {
	zend_string *msg, *file;
	zval *params = NULL;
	long code = 0, line = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "lS|Slz", &code, &msg,
			&file, &line, &params) == FAILURE) {
		RETURN_NULL()
		;
	}
	if (GENE_G(gene_error) == 1) {
		gene_trigger_error(code, "%s", ZSTR_VAL(msg));
	}
	RETURN_TRUE
	;
}
/* }}} */

/*
 * {{{ gene_exception_methods
 */
zend_function_entry gene_exception_methods[] = {
	PHP_ME(gene_exception, setErrorHandler, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, setExceptionHandler, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, doError, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3)) || (PHP_MAJOR_VERSION < 5)
	PHP_ME(gene_exception, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_exception, getPrevious, NULL, ZEND_ACC_PUBLIC)
#endif
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(exception) {
	zend_class_entry gene_exception;
	GENE_INIT_CLASS_ENTRY(gene_exception, "Gene_Exception", "Gene\\Exception",
			gene_exception_methods);
	gene_exception_ce = zend_register_internal_class_ex(&gene_exception,
			gene_get_exception_base(0));

	zend_declare_property_null(gene_exception_ce, ZEND_STRL("message"),
	ZEND_ACC_PROTECTED);
	zend_declare_property_long(gene_exception_ce, ZEND_STRL("code"), 0,
	ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_exception_ce, ZEND_STRL("previous"),
	ZEND_ACC_PROTECTED);

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
