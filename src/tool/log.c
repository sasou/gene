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
	char datetime_buf[32];

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
	snprintf(datetime_buf, sizeof(datetime_buf), "%s.%03d", buf, (int)(tv.tv_usec / 1000));
	*datetime_str = estrdup(datetime_buf);
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

/* {{{ gene_log_get_effective_level */
static zend_long gene_log_get_effective_level(void) {
	if (GENE_G(runtime_type) >= 2) {
		gene_request_context *ctx = gene_request_ctx();
		if (ctx && ctx->log_level_set) {
			return ctx->log_level;
		}
	}
	zval *prop_level = zend_read_static_property(gene_log_ce, ZEND_STRL("level"), 1);
	if (prop_level && Z_TYPE_P(prop_level) == IS_LONG) {
		return Z_LVAL_P(prop_level);
	}
	return GENE_LOG_LEVEL_DEBUG;
}
/* }}} */

/* {{{ gene_log_get_effective_file */
static const char *gene_log_get_effective_file(void) {
	if (GENE_G(runtime_type) >= 2) {
		gene_request_context *ctx = gene_request_ctx();
		if (ctx && ctx->log_file) {
			return ctx->log_file;
		}
	}
	zval *prop_file = zend_read_static_property(gene_log_ce, ZEND_STRL("file"), 1);
	if (prop_file && Z_TYPE_P(prop_file) == IS_STRING && Z_STRLEN_P(prop_file) > 0) {
		return Z_STRVAL_P(prop_file);
	}
	return NULL;
}
/* }}} */

/* {{{ gene_log_get_error_log_fn — cached error_log function pointer */
static inline zend_function *gene_log_get_error_log_fn(void) {
	static zend_function *fn = NULL;
	if (UNEXPECTED(!fn)) {
		fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("error_log"));
	}
	return fn;
}
/* }}} */

/* {{{ gene_log_call_error_log — write via cached error_log function */
static void gene_log_call_error_log(const char *log_line, const char *effective_file) {
	zend_function *fn = gene_log_get_error_log_fn();
	if (UNEXPECTED(!fn)) return;
	zval retval, params[3];
	ZVAL_STRING(&params[0], log_line);
	if (effective_file) {
		ZVAL_LONG(&params[1], 3);
		ZVAL_STRING(&params[2], effective_file);
		zend_call_known_function(fn, NULL, NULL, &retval, 3, params, NULL);
		zval_ptr_dtor(&params[2]);
	} else {
		zend_call_known_function(fn, NULL, NULL, &retval, 1, params, NULL);
	}
	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&retval);
}
/* }}} */

/* {{{ gene_log_write_message */
static void gene_log_write_message(zend_long level, const char *msg) {
	char *datetime = NULL;
	char *log_line = NULL;
	const char *level_name;
	const char *effective_file;

	/* Check log level threshold */
	if (level < gene_log_get_effective_level()) {
		return;
	}

	gene_log_get_datetime(&datetime);
	level_name = gene_log_level_name(level);

	spprintf(&log_line, 0, "[%s] [Gene.%s] %s", datetime, level_name, msg);
	efree(datetime);

	/* Check if custom log file is set */
	effective_file = gene_log_get_effective_file();
	gene_log_call_error_log(log_line, effective_file);

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
	zval msg_val, file_val, line_val, trace_val;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|S", &ex, &extra_msg) == FAILURE) {
		return;
	}

	if (!ex || Z_TYPE_P(ex) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(ex), zend_ce_throwable)) {
		php_error_docref(NULL, E_WARNING, "Argument #1 must be an instance of Throwable");
		return;
	}

	/* Check log level threshold (exception always logs at ERROR level) */
	if (GENE_LOG_LEVEL_ERROR < gene_log_get_effective_level()) {
		return;
	}

	/* Get exception info via direct property reads (avoids 3× call_user_function overhead).
	 * message, file, line are standard Throwable properties readable via zend_read_property.
	 * getTraceAsString() must remain a method call (it computes the string). */
	{
		zval rv1, rv2, rv3;
		zval *msg_prop  = zend_read_property(Z_OBJCE_P(ex), gene_strip_obj(ex), ZEND_STRL("message"), 1, &rv1);
		zval *file_prop = zend_read_property(Z_OBJCE_P(ex), gene_strip_obj(ex), ZEND_STRL("file"), 1, &rv2);
		zval *line_prop = zend_read_property(Z_OBJCE_P(ex), gene_strip_obj(ex), ZEND_STRL("line"), 1, &rv3);

		if (msg_prop && Z_TYPE_P(msg_prop) == IS_STRING) { ZVAL_COPY(&msg_val, msg_prop); } else { ZVAL_EMPTY_STRING(&msg_val); }
		if (file_prop && Z_TYPE_P(file_prop) == IS_STRING) { ZVAL_COPY(&file_val, file_prop); } else { ZVAL_EMPTY_STRING(&file_val); }
		if (line_prop && Z_TYPE_P(line_prop) == IS_LONG) { ZVAL_COPY(&line_val, line_prop); } else { ZVAL_LONG(&line_val, 0); }

		ZVAL_UNDEF(&trace_val);
		zend_function *trace_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(ex)->function_table, ZEND_STRL("gettraceasstring"));
		if (EXPECTED(trace_fn)) {
			zend_call_known_function(trace_fn, Z_OBJ_P(ex), Z_OBJCE_P(ex), &trace_val, 0, NULL, NULL);
		}
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
	gene_log_call_error_log(log_line, gene_log_get_effective_file());

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
	if (GENE_G(runtime_type) >= 2) {
		gene_request_context *ctx = gene_request_ctx();
		if (ctx) {
			if (ctx->log_file) { efree(ctx->log_file); }
			ctx->log_file = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
			return;
		}
	}
	zend_update_static_property_string(gene_log_ce, ZEND_STRL("file"), ZSTR_VAL(file));
}
/* }}} */

/* {{{ proto static string|null Gene\Log::getFile() */
PHP_METHOD(gene_log, getFile) {
	const char *effective = gene_log_get_effective_file();
	if (effective) {
		RETURN_STRING(effective);
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
	if (GENE_G(runtime_type) >= 2) {
		gene_request_context *ctx = gene_request_ctx();
		if (ctx) {
			ctx->log_level = level;
			ctx->log_level_set = 1;
			return;
		}
	}
	zend_update_static_property_long(gene_log_ce, ZEND_STRL("level"), level);
}
/* }}} */

/* {{{ proto static int Gene\Log::getLevel() */
PHP_METHOD(gene_log, getLevel) {
	RETURN_LONG(gene_log_get_effective_level());
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
