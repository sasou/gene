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
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
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


static inline void gene_exception_call_method(zval *object, const char *name, size_t name_len, zval *retval) /*{{{*/
{
	zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(object)->function_table, name, name_len);
	if (EXPECTED(fn)) {
		zend_call_known_function(fn, Z_OBJ_P(object), Z_OBJCE_P(object), retval, 0, NULL, NULL);
	} else {
		ZVAL_NULL(retval);
	}
}/*}}}*/

void gene_exception_getCode(zval *object, zval *retval) /*{{{*/
{
	gene_exception_call_method(object, ZEND_STRL("getcode"), retval);
}/*}}}*/

void gene_exception_getLine(zval *object, zval *retval) /*{{{*/
{
	gene_exception_call_method(object, ZEND_STRL("getline"), retval);
}/*}}}*/

void gene_exception_getFile(zval *object, zval *retval) /*{{{*/
{
	gene_exception_call_method(object, ZEND_STRL("getfile"), retval);
}/*}}}*/

void gene_exception_getMessage(zval *object, zval *retval) /*{{{*/
{
	gene_exception_call_method(object, ZEND_STRL("getmessage"), retval);
}/*}}}*/

void gene_exception_getTrace(zval *object, zval *retval) /*{{{*/
{
	gene_exception_call_method(object, ZEND_STRL("gettrace"), retval);
}/*}}}*/

/* [GENE_FIX:2026-04-27] Removed process-wide static zend_function* caches
 * across this file: under ZTS CG(function_table) is per-thread, so a
 * pointer cached on thread A is invalid for thread B. The exception
 * display path is cold; drop the caches and look up each call. */
static void gene_html_escape(const char *input, zval *retval) /*{{{*/
{
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("htmlspecialchars"));
    ZVAL_UNDEF(retval);
    if (UNEXPECTED(!fn)) { ZVAL_STRING(retval, input); return; }
    zval zv_input, zv_flags, zv_encoding;
    ZVAL_STRING(&zv_input, input);
    ZVAL_LONG(&zv_flags, 3 /* ENT_QUOTES */);
    ZVAL_STRING(&zv_encoding, "UTF-8");
    zval params[] = { zv_input, zv_flags, zv_encoding };
    zend_call_known_function(fn, NULL, NULL, retval, 3, params, NULL);
    zval_ptr_dtor(&zv_input);
    zval_ptr_dtor(&zv_encoding);
}/*}}}*/

void gene_file_codes(zval *file, zval *retval) /*{{{*/
{
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("file"));
    ZVAL_UNDEF(retval);
    if (UNEXPECTED(!fn)) return;
    zval params[] = { *file };
    zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_file_gettype(zval *var, zval *retval) /*{{{*/
{
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("gettype"));
    ZVAL_UNDEF(retval);
    if (UNEXPECTED(!fn)) { ZVAL_EMPTY_STRING(retval); return; }
    zval params[] = { *var };
    zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_file_var_export(zval *var, zval *retval) /*{{{*/
{
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("print_r"));
    ZVAL_UNDEF(retval);
    if (UNEXPECTED(!fn)) { ZVAL_EMPTY_STRING(retval); return; }
    zval arg;
    ZVAL_TRUE(&arg);
    zval params[] = { *var, arg };
    zend_call_known_function(fn, NULL, NULL, retval, 2, params, NULL);
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
    "<code>File:<font color=\"red\">%s</font> Line:<font color=\"red\">" ZEND_LONG_FMT "</font></code>\n" \
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
    "<td>" ZEND_LONG_FMT "</td>\n"

#define HTML_EXCEPTION_TABLE_TD_SPAN  \
    "<span title=\"%s\">%s</span>\n"

#define HTML_EXCEPTION_FOOTER  \
	"</tbody>\n" \
	"</table>\n" \
	"<center class=\"foot\">Gene %s</center>\n" \
	"</div>\n" \
	"</body>\n" \
	"</html>\n"

/** {{{ int gene_exception_error_register(zval *callback,zval *error_type)
 */
int gene_exception_error_register(zval *callback, zval *error_type) {
	zval ret, bak;
	int arg_num = 1;
	int used_bak = 0;
	zval params[2];

	if (!callback) {
		if (GENE_G(use_namespace)) {
			ZVAL_STRING(&bak, GENE_ERROR_FUNC_NAME_NS);
		} else {
			ZVAL_STRING(&bak, GENE_ERROR_FUNC_NAME);
		}
		callback = &bak;
		used_bak = 1;
	}
	params[0] = *callback;
	if (error_type) {
		params[1] = *error_type;
		arg_num = 2;
	}
	zend_function *seh_fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("set_error_handler"));
	ZVAL_UNDEF(&ret);
	if (EXPECTED(seh_fn)) {
		zend_call_known_function(seh_fn, NULL, NULL, &ret, arg_num, params, NULL);
	} else {
		if (used_bak) zval_ptr_dtor(&bak);
		php_error_docref(NULL, E_WARNING, "Call to set_error_handler failed");
		return 0;
	}
	zval_ptr_dtor(&ret);
	if (used_bak) zval_ptr_dtor(&bak);
	return 1;
}

/** {{{ int gene_exception_register(zval *callback)
 */
int gene_exception_register(zval *callback) {
	zval ret, bak;
	int arg_num = 1;
	int used_bak = 0;
	zval params[1];

	if (!callback) {
		if (GENE_G(use_namespace)) {
			ZVAL_STRING(&bak, GENE_EXCEPTION_FUNC_NAME_NS);
		} else {
			ZVAL_STRING(&bak, GENE_EXCEPTION_FUNC_NAME);
		}
		callback = &bak;
		used_bak = 1;
	}
	params[0] = *callback;
	zend_function *sexh_fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("set_exception_handler"));
	ZVAL_UNDEF(&ret);
	if (EXPECTED(sexh_fn)) {
		zend_call_known_function(sexh_fn, NULL, NULL, &ret, arg_num, params, NULL);
	} else {
		if (used_bak) zval_ptr_dtor(&bak);
		php_error_docref(NULL, E_WARNING, "Call to set_exception_handler failed");
		return 0;
	}
	zval_ptr_dtor(&ret);
	if (used_bak) zval_ptr_dtor(&bak);
	return 1;
}

/** {{{void gene_trigger_error(int type, char *format, ...)
 */
void gene_trigger_error(int type, char *format, ...) {
	va_list args;
	char *message;
	size_t msg_len;

	va_start(args, format);
	msg_len = vspprintf(&message, 0, format, args);
	va_end(args);

	gene_throw_exception(type, message);
	efree(message);
}
/* }}} */

/** {{{ zend_class_entry * gene_get_exception_base(int root)
 */
zend_class_entry * gene_get_exception_base(int root) {
#if PHP_VERSION_ID >= 80500
	return zend_ce_exception;
#else
	return zend_exception_get_default();
#endif
}
/* }}} */

/** {{{ void gene_throw_exception(zend_long code, char *message)
 */
void gene_throw_exception(zend_long code, char *message) {
	zend_class_entry *base_exception = gene_get_exception_base(0);

	zend_throw_exception(base_exception, message, code);
}
/* }}} */

/** {{{ public gene_exception::setErrorHandler(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
 */
PHP_METHOD(gene_exception, setErrorHandler) {
	zval *callback, *error_type = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &callback, &error_type) == FAILURE) {
		return;
	}
	gene_exception_error_register(callback, error_type);
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_exception::setExceptionHandler(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
 */
PHP_METHOD(gene_exception, setExceptionHandler) {
	zval *callback = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &callback) == FAILURE) {
		return;
	}
	gene_exception_register(callback);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_exception::doError
 */
PHP_METHOD(gene_exception, doError) {
	zend_string *msg, *file;
	zval *params = NULL;
	zend_long code = 0, line = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lS|Slz", &code, &msg, &file, &line, &params) == FAILURE) {
		RETURN_NULL();
	}
	gene_trigger_error(code, "%s", ZSTR_VAL(msg));
}
/* }}} */

void showCode(zval *file, zval *line) {
	char *out = NULL;
	zval codes;
	size_t length = 0;
	if (Z_TYPE_P(file) == IS_STRING && Z_STRLEN_P(file) > 0
		&& Z_TYPE_P(line) == IS_LONG) {
		gene_file_codes(file, &codes);
		if (Z_TYPE(codes) == IS_ARRAY) {
			int size = zend_hash_num_elements(Z_ARRVAL(codes));
			zend_long start, end;
			zval *ele = NULL;
			start = Z_LVAL_P(line) - 8;
			end = Z_LVAL_P(line) + 8;
			start = start < 0 ? 0 : start;
			end = end > size ? size : end;
			for (; start < end; start++) {
				ele = zend_hash_index_find(Z_ARRVAL(codes), start);
				if (!ele || Z_TYPE_P(ele) != IS_STRING) continue;
				if (start + 1 == Z_LVAL_P(line)) {
					length = spprintf(&out, 0, HTML_EXCEPTION_CODE_LINE, Z_STRVAL_P(ele));
				} else {
					length = spprintf(&out, 0, HTML_EXCEPTION_CODE, Z_STRVAL_P(ele));
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
				PHPWRITE("<tr>\n", 5);
				{
					zval esc;
					gene_html_escape((file != NULL && Z_TYPE_P(file) == IS_STRING) ? Z_STRVAL_P(file) : "", &esc);
					length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_STR, Z_TYPE(esc) == IS_STRING ? Z_STRVAL(esc) : "");
					zval_ptr_dtor(&esc);
				}
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_INT, (zend_long)((line != NULL && Z_TYPE_P(line) == IS_LONG) ? Z_LVAL_P(line) : 0));
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				{
					zval esc;
					gene_html_escape((class != NULL && Z_TYPE_P(class) == IS_STRING) ? Z_STRVAL_P(class) : "", &esc);
					length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_STR, Z_TYPE(esc) == IS_STRING ? Z_STRVAL(esc) : "");
					zval_ptr_dtor(&esc);
				}
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				{
					zval esc;
					gene_html_escape((func != NULL && Z_TYPE_P(func) == IS_STRING) ? Z_STRVAL_P(func) : "", &esc);
					length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_STR, Z_TYPE(esc) == IS_STRING ? Z_STRVAL(esc) : "");
					zval_ptr_dtor(&esc);
				}
				PHPWRITE(out, length);
				efree(out);
				out = NULL;
				PHPWRITE("<td>", 4);
				if (args != NULL && Z_TYPE_P(args) == IS_ARRAY) {
					ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), arg) {
						zval export,type,esc_export,esc_type;
						gene_file_gettype(arg, &type);
						gene_file_var_export(arg, &export);
						gene_html_escape(Z_TYPE(export) == IS_STRING ? Z_STRVAL(export) : "", &esc_export);
						gene_html_escape(Z_TYPE(type) == IS_STRING ? Z_STRVAL(type) : "", &esc_type);
						length = spprintf(&out, 0, HTML_EXCEPTION_TABLE_TD_SPAN, Z_TYPE(esc_export) == IS_STRING ? Z_STRVAL(esc_export) : "", Z_TYPE(esc_type) == IS_STRING ? Z_STRVAL(esc_type) : "");
						PHPWRITE(out, length);
						efree(out);
						out = NULL;
						zval_ptr_dtor(&export);
						zval_ptr_dtor(&type);
						zval_ptr_dtor(&esc_export);
						zval_ptr_dtor(&esc_type);
					}ZEND_HASH_FOREACH_END();
				}
				PHPWRITE("</td>", 5);
				PHPWRITE("</tr>\n", 6);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &ex) == FAILURE) {
		RETURN_NULL();
	}
	char *out = NULL;
	size_t length = 0;

	zval msg, msg_escaped;
	gene_exception_getMessage(ex, &msg);
	PHPWRITE(HTML_EXCEPTION_HEADER, sizeof(HTML_EXCEPTION_HEADER) - 1);

	gene_html_escape(Z_TYPE(msg) == IS_STRING ? Z_STRVAL(msg) : "", &msg_escaped);
	zval_ptr_dtor(&msg);
	length = spprintf(&out, 0, HTML_EXCEPTION_MSG, Z_TYPE(msg_escaped) == IS_STRING ? Z_STRVAL(msg_escaped) : "");
	zval_ptr_dtor(&msg_escaped);
	PHPWRITE(out, length);
	efree(out);
	out = NULL;

	zval file, line, file_escaped;
	gene_exception_getFile(ex, &file);
	gene_exception_getLine(ex, &line);
	gene_html_escape(Z_TYPE(file) == IS_STRING ? Z_STRVAL(file) : "", &file_escaped);
	length = spprintf(&out, 0, HTML_EXCEPTION_FILE,
		Z_TYPE(file_escaped) == IS_STRING ? Z_STRVAL(file_escaped) : "",
		(zend_long)(Z_TYPE(line) == IS_LONG ? Z_LVAL(line) : 0));
	zval_ptr_dtor(&file_escaped);
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
const zend_function_entry gene_exception_methods[] = {
	PHP_ME(gene_exception, setErrorHandler, gene_exception_error, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, setExceptionHandler, gene_exception_exception, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, doException, gene_exception_do_exception, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_exception, doError, gene_exception_do_error, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
#if PHP_VERSION_ID >= 80200
	gene_exception_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

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
