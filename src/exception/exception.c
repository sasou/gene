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
#include "../exception/exception.h"
#include "../router/router.h"
#include "../mvc/view.h"

zend_class_entry * gene_exception_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_exception_error, 0, 0, 1)
	ZEND_ARG_INFO(0, callback)
    ZEND_ARG_INFO(0, error_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_exception_exception, 0, 0, 0)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_exception_do_exception, 0, 0, 1)
	ZEND_ARG_INFO(0, ex)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_exception_do_error, 0, 0, 2)
	ZEND_ARG_INFO(0, code)
    ZEND_ARG_INFO(0, msg)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, line)
    ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()


void gene_exception_getCode(zval *object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "getCode");
    call_user_function(NULL, object, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_exception_getLine(zval *object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "getLine");
    call_user_function(NULL, object, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_exception_getFile(zval *object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "getFile");
    call_user_function(NULL, object, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_exception_getMessage(zval *object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "getMessage");
    call_user_function(NULL, object, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_exception_getTrace(zval *object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "getTrace");
    call_user_function(NULL, object, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_file_codes(zval *file, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "file");
    zval params[] = { *file };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_file_gettype(zval *var, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "gettype");
    zval params[] = { *var };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_file_var_export(zval *var, zval *retval) /*{{{*/
{
    zval function_name, arg;
    ZVAL_STRING(&function_name, "var_export");
    ZVAL_TRUE(&arg);
    zval params[] = { *var, arg };
    call_user_function(NULL, NULL, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&arg);
}/*}}}*/

#define HTML_EXCEPTION_HEADER  \
	"<!DOCTYPE html>\n" \
	"<head>\n" \
	"<meta charset=\"utf-8\">\n" \
	"<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge,chrome=1\" />\n" \
	"<title>Gene Exception</title>\n" \
	"<style>\n" \
	"   body{padding: 0;margin: 0;}\n" \
	"   .content {width: 80%;margin: 0 auto;padding:5px 10px;}\n" \
	"   h1{font-weight: bolder;color: #cc7a94;padding: 5px;}\n" \
	"   h2{font-weight: normal;color: #AF7C8C;background-color: #e9f4f5;padding: 5px;}\n" \
	"   ul.code {font-size: 13px;line-height: 20px;color: #68777d;background-color: #f2f9fc;border: 1px solid #c9e6f2;min-height: 30px;}\n" \
	"   ul.code li {width : 100%;margin:0px;white-space: pre ;list-style-type:none;font-family : monospace;}\n" \
	"   ul.code li.line {color : red;}\n" \
	"   table.trace {width : 100%;font-size: 13px;line-height: 20px;color: #247696;background-color: #c9e6f2;}\n" \
	"   table.trace thead tr {background-color: #90a9b3;color: white;}\n" \
	"   table.trace tr {background-color: #f2f9fc;}\n" \
	"   table.trace td {padding : 5px;}\n" \
	"   .foot {line-height: 20px;color: #cc7a94;margin:10px;}\n" \
	"</style>\n" \
	"</head>\n" \
	"<body>\n" \
	"<div class=\"content\">\n" \
	"<center><h1>Gene Exception</h1></center>\n" \
	"<h2>What's happened:</h2>\n"

#define HTML_EXCEPTION_MSG  \
	"<code>%s</code>\n" \
	"<h2>Where's happened:</h2>\n"

#define HTML_EXCEPTION_FILE  \
    "<code>File:<font color=\"red\">%s</font> Line:<font color=\"red\">%d</font></code>\n" \
	"<ul class=\"code\">\n"

#define HTML_EXCEPTION_CODE_LINE  \
    "<li class=\"line\">%s</li>\n"

#define HTML_EXCEPTION_CODE  \
    "<li>%s</li>\n"

#define HTML_EXCEPTION_TABLE_HEADER  \
    "</ul>\n" \
    "<h2>Stack trace:</h2>\n" \
    "<table class=\"trace\">\n" \
    "<thead><tr><td width=\"480px\">File</td><td width=\"30px\">Line</td><td width=\"200px\">Class</td><td width=\"150px\">Function</td><td>Arguments</td></tr></thead>\n" \
    "<tbody>\n"

#define HTML_EXCEPTION_TABLE_TD_STR  \
    "<td>%s</td>\n"

#define HTML_EXCEPTION_TABLE_TD_INT  \
    "<td>%d</td>\n"

#define HTML_EXCEPTION_TABLE_TD_SPAN  \
    "<span title=\"%s\">%s</span>\n"

#define HTML_EXCEPTION_FOOTER  \
	"</tbody>\n" \
	"</table>\n" \
	"<center class=\"foot\">Gene %s</center>\n" \
	"</div>\n" \
	"</body>\n" \
	"</html>\n"

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
		zval params[] = {*callback, *error_type};
		arg_num = 2;
	}
	ZVAL_STRING(&function, "set_error_handler");
	if (call_user_function(EG(function_table), NULL, &function, &ret, arg_num, params TSRMLS_CC) == FAILURE) {
		zval_ptr_dtor(&ret);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Call to set_error_handler failed");
		return 0;
	}
	zval_ptr_dtor(&function);
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
	zval_dtor(params);
	return 1;
}

/** {{{void gene_trigger_error(int type TSRMLS_DC, char *format, ...)
 */
void gene_trigger_error(int type TSRMLS_DC, char *format, ...) {
	va_list args;
	char *message;
	size_t msg_len;

	va_start(args, format);
	msg_len = vspprintf(&message, 0, format, args);
	va_end(args);

	gene_throw_exception(type, message TSRMLS_CC);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z|z", &callback, &error_type) == FAILURE) {
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
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z", &callback) == FAILURE) {
		return;
	}
	gene_exception_register(callback TSRMLS_CC);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_exception::doError
 */
PHP_METHOD(gene_exception, doError) {
	zend_string *msg, *file;
	zval *params = NULL;
	long code = 0, line = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "lS|Slz", &code, &msg, &file, &line, &params) == FAILURE) {
		RETURN_NULL();
	}
	gene_trigger_error(code, "%s", ZSTR_VAL(msg));
}
/* }}} */

void showCode(zval *file, zval *line) {
	char *out = NULL;
	zval codes;
	size_t length = 0;
	if (ZSTR_LEN(Z_STR_P(file)) > 0) {
		gene_file_codes(file, &codes);
		if (Z_TYPE(codes) == IS_ARRAY) {
			int size = zend_hash_num_elements(Z_ARRVAL(codes));
			zend_long start, end;
			zval *ele = NULL;
			start = line->value.lval - 8;
			end = line->value.lval + 8;
			start = start < 0 ? 0 : start;
			end = end > size ? size : end;
			for(start; start < end; start++) {
				ele = zend_hash_index_find(Z_ARRVAL(codes), start);
				if (start + 1 == line->value.lval) {
					length = spprintf(&out, 0, HTML_EXCEPTION_CODE_LINE, ele->value.str->val);
				} else {
					length = spprintf(&out, 0, HTML_EXCEPTION_CODE, ele->value.str->val);
				}
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
			}
		}
		zval_ptr_dtor(&codes);
	}
}

void showTrace(zval *ex) {
	char *out = NULL;
	zval trace;
	zval *element = NULL, *file = NULL, *line = NULL, *class = NULL, *func = NULL, *args = NULL, *arg = NULL;
	size_t length = 0;
	gene_exception_getTrace(ex, &trace);
	if (Z_TYPE(trace) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL(trace), element)
		{
			class = zend_hash_str_find(Z_ARRVAL_P(element), "class", 5);
			if (class != NULL) {
				file = zend_hash_str_find(Z_ARRVAL_P(element), "file", 4);
				line = zend_hash_str_find(Z_ARRVAL_P(element), "line", 4);
				func = zend_hash_str_find(Z_ARRVAL_P(element), "function", 8);
				args = zend_hash_str_find(Z_ARRVAL_P(element), "args", 4);
				PHPWRITE("<tr>\n", 6);
				length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_STR, file != NULL ? file->value.str->val : "");
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_INT, line != NULL ? line->value.lval : 0);
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_STR, class != NULL ? class->value.str->val : "");
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_STR, func != NULL ? func->value.str->val : "");
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				PHPWRITE("<td>", 4);
				if (args != NULL && Z_TYPE_P(args) == IS_ARRAY) {
					ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), arg) {
						zval export,type;
						gene_file_gettype(arg, &type);
						gene_file_var_export(arg, &export);
						length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_SPAN, Z_TYPE(export) == IS_STRING ? export.value.str->val : "", Z_TYPE(type) == IS_STRING ? type.value.str->val : "");
						PHPWRITE(out, length);
						efree(out);
						out = NULL;
						zval_ptr_dtor(&export);
						zval_ptr_dtor(&type);
					}ZEND_HASH_FOREACH_END();
				}
				PHPWRITE("</td>", 5);
				PHPWRITE("</tr>\n", 7);
			}
		}ZEND_HASH_FOREACH_END();
	}
	zval_ptr_dtor(&trace);
}
/*
 * {{{ gene_exception::doException
 */
PHP_METHOD(gene_exception, doException) {
	zval *ex = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "z", &ex) == FAILURE) {
		RETURN_NULL();
	}
	char *out = NULL;
	size_t length = 0;

	zval msg;
	gene_exception_getMessage(ex, &msg);
	PHPWRITE(HTML_EXCEPTION_HEADER, sizeof(HTML_EXCEPTION_HEADER) - 1);

	length = spprintf(&out, 0, HTML_EXCEPTION_MSG, msg.value.str->val);
	zval_ptr_dtor(&msg);
	PHPWRITE(out, length);
	efree(out);
	out = NULL;

	zval file, line;
	gene_exception_getFile(ex, &file);
	gene_exception_getLine(ex, &line);
	length = spprintf(&out, 0, HTML_EXCEPTION_FILE, file.value.str->val, line.value.lval);
	PHPWRITE(out, length);
	efree(out);
	out = NULL;

	showCode(&file, &line);
	zval_ptr_dtor(&file);
	zval_ptr_dtor(&line);

	PHPWRITE(HTML_EXCEPTION_TABLE_HEADER, sizeof(HTML_EXCEPTION_TABLE_HEADER) - 1);

	showTrace(ex);

	length = spprintf(&out, 0, HTML_EXCEPTION_FOOTER, PHP_GENE_VERSION);
	PHPWRITE(out, length);
	efree(out);
	out = NULL;
}
/* }}} */

/*
 * {{{ gene_exception_methods
 */
zend_function_entry gene_exception_methods[] = {
	PHP_ME(gene_exception, setErrorHandler, gene_exception_error, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, setExceptionHandler, gene_exception_exception, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, doException, gene_exception_do_exception, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, doError, gene_exception_do_error, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
	GENE_INIT_CLASS_ENTRY(gene_exception, "Gene_Exception", "Gene\\Exception", gene_exception_methods);
	gene_exception_ce = zend_register_internal_class_ex(&gene_exception, gene_get_exception_base(0));

	zend_declare_property_null(gene_exception_ce, ZEND_STRL("message"), ZEND_ACC_PROTECTED);
	zend_declare_property_long(gene_exception_ce, ZEND_STRL("code"), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_exception_ce, ZEND_STRL("previous"), ZEND_ACC_PROTECTED);
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
