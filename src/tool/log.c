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
#include "Zend/zend_interfaces.h"
#include "ext/standard/php_string.h"
#include "ext/date/php_date.h"

#include "../gene.h"
#include "../tool/log.h"

zend_class_entry *gene_log_ce;

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(gene_log_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_log_message_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_log_exception_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, exception)
	ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_log_file_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_log_level_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, level)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ gene_log_get_datetime */
static void gene_log_get_datetime(char **datetime_str) {
	struct timeval tv;
	time_t now;
	struct tm *tm_info;
	char buf[64];

#ifdef PHP_WIN32
	{
		FILETIME ft;
		ULARGE_INTEGER uli;
		GetSystemTimeAsFileTime(&ft);
		uli.LowPart = ft.dwLowDateTime;
		uli.HighPart = ft.dwHighDateTime;
		/* Convert Windows FILETIME (100-ns since 1601) to Unix epoch */
		tv.tv_sec = (long)((uli.QuadPart - 116444736000000000ULL) / 10000000ULL);
		tv.tv_usec = (long)((uli.QuadPart / 10) % 1000000);
	}
#else
	gettimeofday(&tv, NULL);
#endif

	now = (time_t)tv.tv_sec;
	tm_info = php_localtime_r(&now, &(struct tm){0});
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
	spprintf(datetime_str, 0, "%s.%03d", buf, (int)(tv.tv_usec / 1000));
}
/* }}} */

/* {{{ gene_log_level_name */
static const char *gene_log_level_name(zend_long level) {
	switch (level) {
		case GENE_LOG_LEVEL_DEBUG:   return "DEBUG";
		case GENE_LOG_LEVEL_INFO:    return "INFO";
		case GENE_LOG_LEVEL_NOTICE:  return "NOTICE";
		case GENE_LOG_LEVEL_WARNING: return "WARNING";
		case GENE_LOG_LEVEL_ERROR:   return "ERROR";
		default: return "LOG";
	}
}
/* }}} */

/* {{{ gene_log_write_message */
static void gene_log_write_message(zend_long level, const char *msg) {
	zval *prop_level, *prop_file;
	char *datetime = NULL;
	char *log_line = NULL;
	const char *level_name;

	/* Check log level threshold */
	prop_level = zend_read_static_property(gene_log_ce, ZEND_STRL("level"), 1);
	if (prop_level && Z_TYPE_P(prop_level) == IS_LONG) {
		if (level < Z_LVAL_P(prop_level)) {
			return;
		}
	}

	gene_log_get_datetime(&datetime);
	level_name = gene_log_level_name(level);

	spprintf(&log_line, 0, "[%s] [Gene.%s] %s", datetime, level_name, msg);
	efree(datetime);

	/* Check if custom log file is set */
	prop_file = zend_read_static_property(gene_log_ce, ZEND_STRL("file"), 1);
	if (prop_file && Z_TYPE_P(prop_file) == IS_STRING && Z_STRLEN_P(prop_file) > 0) {
		/* Write to custom file */
		zval func_name, retval;
		zval params[3];
		ZVAL_STRING(&func_name, "error_log");
		ZVAL_STRING(&params[0], log_line);
		ZVAL_LONG(&params[1], 3);
		ZVAL_STR_COPY(&params[2], Z_STR_P(prop_file));
		call_user_function(EG(function_table), NULL, &func_name, &retval, 3, params);
		zval_ptr_dtor(&func_name);
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[2]);
		zval_ptr_dtor(&retval);
	} else {
		/* Write to default error_log */
		zval func_name, retval;
		zval params[1];
		ZVAL_STRING(&func_name, "error_log");
		ZVAL_STRING(&params[0], log_line);
		call_user_function(EG(function_table), NULL, &func_name, &retval, 1, params);
		zval_ptr_dtor(&func_name);
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&retval);
	}

	efree(log_line);
}
/* }}} */

/* {{{ proto static void Gene\Log::debug(string $message) */
PHP_METHOD(gene_log, debug) {
	zend_string *message;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &message) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_DEBUG, ZSTR_VAL(message));
}
/* }}} */

/* {{{ proto static void Gene\Log::info(string $message) */
PHP_METHOD(gene_log, info) {
	zend_string *message;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &message) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_INFO, ZSTR_VAL(message));
}
/* }}} */

/* {{{ proto static void Gene\Log::notice(string $message) */
PHP_METHOD(gene_log, notice) {
	zend_string *message;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &message) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_NOTICE, ZSTR_VAL(message));
}
/* }}} */

/* {{{ proto static void Gene\Log::warning(string $message) */
PHP_METHOD(gene_log, warning) {
	zend_string *message;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &message) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_WARNING, ZSTR_VAL(message));
}
/* }}} */

/* {{{ proto static void Gene\Log::error(string $message) */
PHP_METHOD(gene_log, error) {
	zend_string *message;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &message) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_ERROR, ZSTR_VAL(message));
}
/* }}} */

/* {{{ proto static void Gene\Log::exception(Throwable $exception [, string $message]) */
PHP_METHOD(gene_log, exception) {
	zval *ex = NULL;
	zend_string *extra_msg = NULL;
	char *datetime = NULL;
	char *log_line = NULL;
	zval *prop_level, *prop_file;
	zval msg_val, file_val, line_val, trace_val;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|S", &ex, &extra_msg) == FAILURE) {
		return;
	}

	if (!ex || Z_TYPE_P(ex) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(ex), zend_ce_throwable)) {
		php_error_docref(NULL, E_WARNING, "Argument #1 must be an instance of Throwable");
		return;
	}

	/* Check log level threshold (exception always logs at ERROR level) */
	prop_level = zend_read_static_property(gene_log_ce, ZEND_STRL("level"), 1);
	if (prop_level && Z_TYPE_P(prop_level) == IS_LONG) {
		if (GENE_LOG_LEVEL_ERROR < Z_LVAL_P(prop_level)) {
			return;
		}
	}

	/* Get exception info */
	{
		zval func_name;

		ZVAL_STRING(&func_name, "getMessage");
		ZVAL_UNDEF(&msg_val);
		call_user_function(NULL, ex, &func_name, &msg_val, 0, NULL);
		zval_ptr_dtor(&func_name);

		ZVAL_STRING(&func_name, "getFile");
		ZVAL_UNDEF(&file_val);
		call_user_function(NULL, ex, &func_name, &file_val, 0, NULL);
		zval_ptr_dtor(&func_name);

		ZVAL_STRING(&func_name, "getLine");
		ZVAL_UNDEF(&line_val);
		call_user_function(NULL, ex, &func_name, &line_val, 0, NULL);
		zval_ptr_dtor(&func_name);

		ZVAL_STRING(&func_name, "getTraceAsString");
		ZVAL_UNDEF(&trace_val);
		call_user_function(NULL, ex, &func_name, &trace_val, 0, NULL);
		zval_ptr_dtor(&func_name);
	}

	gene_log_get_datetime(&datetime);

	{
		const char *class_name = ZSTR_VAL(Z_OBJCE_P(ex)->name);
		const char *msg_str = (Z_TYPE(msg_val) == IS_STRING) ? Z_STRVAL(msg_val) : "";
		const char *file_str = (Z_TYPE(file_val) == IS_STRING) ? Z_STRVAL(file_val) : "";
		zend_long line_num = (Z_TYPE(line_val) == IS_LONG) ? Z_LVAL(line_val) : 0;
		const char *trace_str = (Z_TYPE(trace_val) == IS_STRING) ? Z_STRVAL(trace_val) : "";

		if (extra_msg && ZSTR_LEN(extra_msg) > 0) {
			spprintf(&log_line, 0,
				"[%s] [Gene.ERROR] [%s] %s in %s:%ld\n%s\n%s",
				datetime, class_name, msg_str, file_str, line_num, ZSTR_VAL(extra_msg), trace_str);
		} else {
			spprintf(&log_line, 0,
				"[%s] [Gene.ERROR] [%s] %s in %s:%ld\n%s",
				datetime, class_name, msg_str, file_str, line_num, trace_str);
		}
	}

	efree(datetime);

	/* Write log */
	prop_file = zend_read_static_property(gene_log_ce, ZEND_STRL("file"), 1);
	if (prop_file && Z_TYPE_P(prop_file) == IS_STRING && Z_STRLEN_P(prop_file) > 0) {
		zval func_name, retval;
		zval params[3];
		ZVAL_STRING(&func_name, "error_log");
		ZVAL_STRING(&params[0], log_line);
		ZVAL_LONG(&params[1], 3);
		ZVAL_STR_COPY(&params[2], Z_STR_P(prop_file));
		call_user_function(EG(function_table), NULL, &func_name, &retval, 3, params);
		zval_ptr_dtor(&func_name);
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[2]);
		zval_ptr_dtor(&retval);
	} else {
		zval func_name, retval;
		zval params[1];
		ZVAL_STRING(&func_name, "error_log");
		ZVAL_STRING(&params[0], log_line);
		call_user_function(EG(function_table), NULL, &func_name, &retval, 1, params);
		zval_ptr_dtor(&func_name);
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&retval);
	}

	efree(log_line);
	zval_ptr_dtor(&msg_val);
	zval_ptr_dtor(&file_val);
	zval_ptr_dtor(&line_val);
	zval_ptr_dtor(&trace_val);
}
/* }}} */

/* {{{ proto static void Gene\Log::setFile(string $file) */
PHP_METHOD(gene_log, setFile) {
	zend_string *file;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &file) == FAILURE) {
		return;
	}
	zend_update_static_property_str(gene_log_ce, ZEND_STRL("file"), file);
}
/* }}} */

/* {{{ proto static string|null Gene\Log::getFile() */
PHP_METHOD(gene_log, getFile) {
	zval *prop;
	prop = zend_read_static_property(gene_log_ce, ZEND_STRL("file"), 1);
	if (prop && Z_TYPE_P(prop) == IS_STRING && Z_STRLEN_P(prop) > 0) {
		RETURN_STR_COPY(Z_STR_P(prop));
	}
	RETURN_NULL();
}
/* }}} */

/* {{{ proto static void Gene\Log::setLevel(int $level) */
PHP_METHOD(gene_log, setLevel) {
	zend_long level;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &level) == FAILURE) {
		return;
	}
	if (level < GENE_LOG_LEVEL_DEBUG || level > GENE_LOG_LEVEL_ERROR) {
		php_error_docref(NULL, E_WARNING, "Log level must be between %d and %d", GENE_LOG_LEVEL_DEBUG, GENE_LOG_LEVEL_ERROR);
		return;
	}
	zend_update_static_property_long(gene_log_ce, ZEND_STRL("level"), level);
}
/* }}} */

/* {{{ proto static int Gene\Log::getLevel() */
PHP_METHOD(gene_log, getLevel) {
	zval *prop;
	prop = zend_read_static_property(gene_log_ce, ZEND_STRL("level"), 1);
	if (prop && Z_TYPE_P(prop) == IS_LONG) {
		RETURN_LONG(Z_LVAL_P(prop));
	}
	RETURN_LONG(GENE_LOG_LEVEL_DEBUG);
}
/* }}} */

/*
 * {{{ gene_log_methods
 */
const zend_function_entry gene_log_methods[] = {
	PHP_ME(gene_log, debug,     gene_log_message_arginfo,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, info,      gene_log_message_arginfo,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, notice,    gene_log_message_arginfo,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, warning,   gene_log_message_arginfo,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, error,     gene_log_message_arginfo,   ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, exception, gene_log_exception_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, setFile,   gene_log_file_arginfo,      ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, getFile,   gene_log_void_arginfo,      ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, setLevel,  gene_log_level_arginfo,     ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, getLevel,  gene_log_void_arginfo,      ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(log)
{
	zend_class_entry gene_log;
	GENE_INIT_CLASS_ENTRY(gene_log, "Gene_Log", "Gene\\Log", gene_log_methods);
	gene_log_ce = zend_register_internal_class_ex(&gene_log, NULL);
	gene_log_ce->ce_flags |= ZEND_ACC_FINAL;
#if PHP_VERSION_ID >= 80200
	gene_log_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

	/* static properties */
	zend_declare_property_null(gene_log_ce, ZEND_STRL("file"), ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);
	zend_declare_property_long(gene_log_ce, ZEND_STRL("level"), GENE_LOG_LEVEL_DEBUG, ZEND_ACC_PROTECTED|ZEND_ACC_STATIC);

	/* class constants for log levels */
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_DEBUG"),   GENE_LOG_LEVEL_DEBUG);
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_INFO"),    GENE_LOG_LEVEL_INFO);
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_NOTICE"),  GENE_LOG_LEVEL_NOTICE);
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_WARNING"), GENE_LOG_LEVEL_WARNING);
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_ERROR"),   GENE_LOG_LEVEL_ERROR);

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
