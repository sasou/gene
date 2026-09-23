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
#include "Zend/zend_smart_str.h"
#include "ext/standard/php_string.h"
#include "ext/date/php_date.h"

#include "../gene.h"
#include "../common/common.h"
#include "../http/json.h"
#include "../tool/log.h"

zend_class_entry *gene_log_ce;

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(gene_log_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_log_message_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()

/* [GENE_FEATURE:2026-08-07] Structured context: ($message, array $context).
 * When $context is a non-empty array it is JSON-encoded and appended to the
 * log line as ` {json}`, enabling machine-parseable structured logs. */
ZEND_BEGIN_ARG_INFO_EX(gene_log_message_context_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, message)
	ZEND_ARG_INFO(0, context)
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
/* [GENE_PERF:2026-09-20 V3-2.8] Writes into the caller's stack buffer
 * instead of estrdup'ing �� one less alloc/free per log call. */
static void gene_log_get_datetime(char *datetime_str, size_t datetime_size) {
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
	snprintf(datetime_str, datetime_size, "%s.%03d", buf, (int)(tv.tv_usec / 1000));
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
		case GENE_LOG_LEVEL_CRITICAL: return "CRITICAL";
		case GENE_LOG_LEVEL_ALERT:   return "ALERT";
		case GENE_LOG_LEVEL_EMERGENCY: return "EMERGENCY";
		default: return "LOG";
	}
}
/* }}} */

/* {{{ gene_log_get_effective_level */
static zend_long gene_log_get_effective_level(void) {
	gene_request_context *ctx = gene_request_ctx();
	/* [V3-4.1] probe ctx->cold without materializing the cold block. */
	if (ctx && ctx->cold && ctx->cold->log_level_set) {
		return ctx->cold->log_level;
	}
	return GENE_LOG_LEVEL_DEBUG;
}
/* }}} */

/* {{{ gene_log_get_effective_file */
/* [GENE_PERF:2026-09-20 V3-2.8] GENE_CTX_COLD(ctx)->log_file is a zend_string* now �� the
 * error_log "file" parameter can borrow it directly (ZVAL_STR), removing
 * the per-call ZVAL_STRING copy. */
static zend_string *gene_log_get_effective_file(void) {
	gene_request_context *ctx = gene_request_ctx();
	if (ctx && ctx->cold && ctx->cold->log_file) {
		return ctx->cold->log_file;
	}
	return NULL;
}
/* }}} */

/* {{{ gene_log_get_error_log_fn �� look up error_log in current thread's
 * function_table. [GENE_FIX:2026-04-27] Previous version cached the result
 * in a process-wide static variable, which is unsafe under ZTS where
 * CG(function_table) is per-thread (cross-thread use of stale pointer
 * could crash on module unload / reload). zend_hash_str_find_ptr on a
 * pre-known interned key is cheap; do not cache.
 * [GENE_AUDIT:2026-07-03 T1#4] Now uses GENE_CG_FN_LOOKUP which caches
 * under non-ZTS (safe �� internal fn ptrs are process-constant) and does
 * a per-call lookup under ZTS. */
static inline zend_function *gene_log_get_error_log_fn(void) {
#ifndef ZTS
	static zend_function *fn = NULL;
#else
	zend_function *fn = NULL;
#endif
	return GENE_CG_FN_LOOKUP(fn, "error_log");
}
/* }}} */

/* {{{ [GENE_PERF:2026-09-21 V3-3.4] gene.log_keep_open (opt-in, default off).
 * In Swoole mode, file-target log lines are written through an unbuffered,
 * worker-owned php_stream instead of error_log($line,3,$file)'s per-call
 * open/write/close (3 syscalls per line). FPM keeps the legacy path. Every
 * gene.log_reopen_interval seconds (default 5) the path is stat'ed:
 * POSIX compares st_ino/st_dev (logrotate rename -> reopen), Windows compares
 * size shrinkage (copytruncate -> reopen). Entries carry creator_pid like
 * Pool: a forked child drops inherited handles before first write.
 * MSHUTDOWN closes everything. Falls back to error_log() on any failure. */
typedef struct _gene_log_handle {
	php_stream *stream;
	zend_long   creator_pid;
	time_t      last_check;
	zend_long   last_size;
#ifndef PHP_WIN32
	zend_long   st_dev;
	zend_long   st_ino;
#endif
} gene_log_handle;

static HashTable *gene_log_streams = NULL; /* path => gene_log_handle* (persistent) */

static zend_long gene_log_current_pid(void) {
#ifdef PHP_WIN32
	return (zend_long)_getpid();
#else
	return (zend_long)getpid();
#endif
}

static void gene_log_handle_close(gene_log_handle *h) {
	if (h->stream) {
		php_stream_close(h->stream);
		h->stream = NULL;
	}
}

void gene_log_shutdown_streams(void) {
	if (gene_log_streams) {
		gene_log_handle *h;
		ZEND_HASH_FOREACH_PTR(gene_log_streams, h) {
			gene_log_handle_close(h);
			pefree(h, 1);
		} ZEND_HASH_FOREACH_END();
		zend_hash_destroy(gene_log_streams);
		pefree(gene_log_streams, 1);
		gene_log_streams = NULL;
	}
}

static void gene_log_entry_drop(zend_string *path) {
	gene_log_handle **hp, *h;
	if (!gene_log_streams) return;
	hp = (gene_log_handle **)zend_hash_find(gene_log_streams, path);
	if (hp) {
		h = *hp;
		gene_log_handle_close(h);
		zend_hash_del(gene_log_streams, path);
		pefree(h, 1);
	}
}

/* Returns 1 when the line was written through the persistent handle. */
static int gene_log_write_persistent(zend_string *path, zend_string *line) {
	gene_log_handle **hp, *h;
	zend_long pid = gene_log_current_pid();
	time_t now = time(NULL);

	if (!gene_log_streams) {
		gene_log_streams = (HashTable *)pemalloc(sizeof(HashTable), 1);
		if (UNEXPECTED(!gene_log_streams)) {
			return 0;
		}
		zend_hash_init(gene_log_streams, 8, NULL, NULL, 1);
	}

	hp = (gene_log_handle **)zend_hash_find(gene_log_streams, path);
	if (hp) {
		h = *hp;
		if (h->creator_pid != pid) {
			/* Forked child inherited fd offsets - drop inherited handles. */
			gene_log_entry_drop(path);
			h = NULL;
		}
	} else {
		h = NULL;
	}

	if (!h) {
		h = (gene_log_handle *)pemalloc(sizeof(gene_log_handle), 1);
		memset(h, 0, sizeof(*h));
		h->creator_pid = pid;
		zend_hash_str_update_ptr(gene_log_streams, ZSTR_VAL(path), ZSTR_LEN(path), h);
	}

	/* Periodic identity check for logrotate-style rotation. */
	{
		zend_long interval = GENE_G(log_reopen_interval);
		if (interval < 1) interval = 1;
		if (h->stream && (now - h->last_check) >= interval) {
			zend_stat_t sb;
			bool stale;
			h->last_check = now;
			stale = (VCWD_STAT(ZSTR_VAL(path), &sb) != 0);
			if (!stale) {
#ifndef PHP_WIN32
				stale = (sb.st_ino != (ino_t)h->st_ino || sb.st_dev != (dev_t)h->st_dev);
#else
				stale = (sb.st_size < h->last_size);
#endif
			}
			if (stale) {
				gene_log_handle_close(h);
			} else {
				h->last_size = (zend_long)sb.st_size;
			}
		}
	}

	if (!h->stream) {
		h->stream = php_stream_open_wrapper_ex(ZSTR_VAL(path), "a", REPORT_ERRORS, NULL, NULL);
		if (!h->stream) {
			return 0;
		}
		php_stream_set_option(h->stream, PHP_STREAM_OPTION_WRITE_BUFFER,
			PHP_STREAM_BUFFER_NONE, NULL);
		{
			zend_stat_t sb;
			h->last_check = now;
			if (VCWD_STAT(ZSTR_VAL(path), &sb) == 0) {
				h->last_size = (zend_long)sb.st_size;
#ifndef PHP_WIN32
				h->st_dev = (zend_long)sb.st_dev;
				h->st_ino = (zend_long)sb.st_ino;
#endif
			}
		}
	}

	if (line && ZSTR_LEN(line) &&
		php_stream_write(h->stream, ZSTR_VAL(line), ZSTR_LEN(line)) != ZSTR_LEN(line)) {
		/* Drop the bad handle; caller falls back to error_log(). */
		gene_log_entry_drop(path);
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ gene_log_call_error_log �� write via error_log function.
 * Takes ownership of log_line (released after the call); effective_file is
 * borrowed from the request context and must not be released here. */
static void gene_log_call_error_log(zend_string *log_line, zend_string *effective_file) {
	zend_function *fn;
	zval retval, params[3];
	/* [GENE_PERF:2026-09-21 V3-3.4] opt-in persistent handle path. */
	if (effective_file && GENE_G(log_keep_open) && GENE_G(runtime_type) >= 2 &&
		gene_log_write_persistent(effective_file, log_line)) {
		if (log_line) zend_string_release(log_line);
		return;
	}
	fn = gene_log_get_error_log_fn();
	if (UNEXPECTED(!fn)) {
		if (log_line) zend_string_release(log_line);
		return;
	}
	ZVAL_STR(&params[0], log_line ? log_line : ZSTR_EMPTY_ALLOC());
	if (effective_file) {
		ZVAL_LONG(&params[1], 3);
		ZVAL_STR(&params[2], effective_file);
		zend_call_known_function(fn, NULL, NULL, &retval, 3, params, NULL);
	} else {
		zend_call_known_function(fn, NULL, NULL, &retval, 1, params, NULL);
	}
	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&retval);
}
/* }}} */

/* {{{ gene_log_write_message */
/* [GENE_FEATURE:2026-08-07] Added context parameter: when non-NULL and an
 * array with at least one element, it is JSON-encoded and appended to the
 * log line as ` {json}` for structured logging. */
static void gene_log_write_message(zend_long level, const char *msg, zval *context) {
	char datetime[32];
	smart_str log_line = {0};
	const char *level_name;
	zend_string *effective_file;

	/* Check log level threshold */
	if (level < gene_log_get_effective_level()) {
		return;
	}

	gene_log_get_datetime(datetime, sizeof(datetime));
	level_name = gene_log_level_name(level);

	/* [GENE_FEATURE:2026-08-22] Merge Context.request_id into structured
	 * context. Caller-supplied request_id wins. */
	{
		gene_request_context *rctx = gene_request_ctx();
		zval *rid = NULL;
		zval merged;
		zval *ctx_use = context;
		ZVAL_UNDEF(&merged);
		if (rctx && rctx->cold && Z_TYPE(rctx->cold->user_bag) == IS_ARRAY) {
			rid = zend_hash_str_find(Z_ARRVAL(rctx->cold->user_bag), ZEND_STRL("request_id"));
		}
		if (rid) {
			array_init(&merged);
			if (context && Z_TYPE_P(context) == IS_ARRAY) {
				zend_string *mk;
				zend_ulong mi;
				zval *mv;
				ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(context), mi, mk, mv) {
					Z_TRY_ADDREF_P(mv);
					if (mk) {
						zend_hash_update(Z_ARRVAL(merged), mk, mv);
					} else {
						zend_hash_index_update(Z_ARRVAL(merged), mi, mv);
					}
				} ZEND_HASH_FOREACH_END();
			}
			if (!zend_hash_str_exists(Z_ARRVAL(merged), ZEND_STRL("request_id"))) {
				Z_TRY_ADDREF_P(rid);
				zend_hash_str_add(Z_ARRVAL(merged), ZEND_STRL("request_id"), rid);
			}
			ctx_use = &merged;
		}

		smart_str_appends(&log_line, "[");
		smart_str_appends(&log_line, datetime);
		smart_str_appends(&log_line, "] [Gene.");
		smart_str_appends(&log_line, level_name);
		smart_str_appends(&log_line, "] ");
		smart_str_appends(&log_line, msg);

		if (ctx_use && Z_TYPE_P(ctx_use) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(ctx_use)) > 0) {
			smart_str context_json = {0};
			if (gene_json_encode_buf(&context_json, ctx_use, 0) == SUCCESS) {
				smart_str_appendc(&log_line, ' ');
				smart_str_append(&log_line, context_json.s);
			}
			smart_str_free(&context_json);
		}
		if (Z_TYPE(merged) != IS_UNDEF) {
			zval_ptr_dtor(&merged);
		}
	}

	effective_file = gene_log_get_effective_file();
	gene_log_call_error_log(smart_str_extract(&log_line), effective_file);
}
/* }}} */

/* {{{ proto static void Gene\Log::debug(string $message [, array $context]) */
PHP_METHOD(gene_log, debug) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_DEBUG, ZSTR_VAL(message), context);
}
/* }}} */

/* {{{ proto static void Gene\Log::info(string $message [, array $context]) */
PHP_METHOD(gene_log, info) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_INFO, ZSTR_VAL(message), context);
}
/* }}} */

/* {{{ proto static void Gene\Log::notice(string $message [, array $context]) */
PHP_METHOD(gene_log, notice) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_NOTICE, ZSTR_VAL(message), context);
}
/* }}} */

/* {{{ proto static void Gene\Log::warning(string $message [, array $context]) */
PHP_METHOD(gene_log, warning) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_WARNING, ZSTR_VAL(message), context);
}
/* }}} */

/* {{{ proto static void Gene\Log::error(string $message [, array $context]) */
PHP_METHOD(gene_log, error) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_ERROR, ZSTR_VAL(message), context);
}
/* }}} */

/* [GENE_FEATURE:2026-08-07] RFC-5424 severity levels above ERROR. */
/* {{{ proto static void Gene\Log::critical(string $message [, array $context]) */
PHP_METHOD(gene_log, critical) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_CRITICAL, ZSTR_VAL(message), context);
}
/* }}} */

/* {{{ proto static void Gene\Log::alert(string $message [, array $context]) */
PHP_METHOD(gene_log, alert) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_ALERT, ZSTR_VAL(message), context);
}
/* }}} */

/* {{{ proto static void Gene\Log::emergency(string $message [, array $context]) */
PHP_METHOD(gene_log, emergency) {
	zend_string *message;
	zval *context = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a", &message, &context) == FAILURE) {
		return;
	}
	gene_log_write_message(GENE_LOG_LEVEL_EMERGENCY, ZSTR_VAL(message), context);
}
/* }}} */

/* {{{ proto static void Gene\Log::exception(Throwable $exception [, string $message]) */
PHP_METHOD(gene_log, exception) {
	zval *ex = NULL;
	zend_string *extra_msg = NULL;
	char datetime[32];
	smart_str log_line = {0};
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

	/* Get exception info via direct property reads (avoids 3�� call_user_function overhead).
	 * message, file, line are standard Throwable properties readable via zend_read_property.
	 * getTraceAsString() must remain a method call (it computes the string). */
	{
		/* [GENE_AUDIT:2026-07-30 L4] rv slots must be UNDEF-initialized and
		 * dtor'd after use. message/file/line are real declared Throwable
		 * properties, so normally no temporary is written into the rv �� but
		 * a Throwable subclass with a magic __get would have its temporary
		 * return value placed into the rv slot, and never dtor'ing it is a
		 * deterministic leak (not merely a fragile convention). */
		zval rv1, rv2, rv3;
		ZVAL_UNDEF(&rv1);
		ZVAL_UNDEF(&rv2);
		ZVAL_UNDEF(&rv3);
		zval *msg_prop  = zend_read_property(Z_OBJCE_P(ex), gene_strip_obj(ex), ZEND_STRL("message"), 1, &rv1);
		zval *file_prop = zend_read_property(Z_OBJCE_P(ex), gene_strip_obj(ex), ZEND_STRL("file"), 1, &rv2);
		zval *line_prop = zend_read_property(Z_OBJCE_P(ex), gene_strip_obj(ex), ZEND_STRL("line"), 1, &rv3);

		if (msg_prop && Z_TYPE_P(msg_prop) == IS_STRING) { ZVAL_COPY(&msg_val, msg_prop); } else { ZVAL_EMPTY_STRING(&msg_val); }
		if (file_prop && Z_TYPE_P(file_prop) == IS_STRING) { ZVAL_COPY(&file_val, file_prop); } else { ZVAL_EMPTY_STRING(&file_val); }
		if (line_prop && Z_TYPE_P(line_prop) == IS_LONG) { ZVAL_COPY(&line_val, line_prop); } else { ZVAL_LONG(&line_val, 0); }
		if (!Z_ISUNDEF(rv1)) { zval_ptr_dtor(&rv1); }
		if (!Z_ISUNDEF(rv2)) { zval_ptr_dtor(&rv2); }
		if (!Z_ISUNDEF(rv3)) { zval_ptr_dtor(&rv3); }

		ZVAL_UNDEF(&trace_val);
		zend_function *trace_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(ex)->function_table, ZEND_STRL("gettraceasstring"));
		if (EXPECTED(trace_fn)) {
			zend_call_known_function(trace_fn, Z_OBJ_P(ex), Z_OBJCE_P(ex), &trace_val, 0, NULL, NULL);
			/* [GENE_FIX:2026-04-27] If getTraceAsString itself threw, swallow it
			 * here �� we are already in the logging path for an existing
			 * exception; leaking a second pending exception into the rest of
			 * the call chain (error_log invocation, etc.) is undefined. */
			if (UNEXPECTED(EG(exception))) {
				zend_clear_exception();
				if (Z_TYPE(trace_val) != IS_STRING) {
					zval_ptr_dtor(&trace_val);
					ZVAL_EMPTY_STRING(&trace_val);
				}
			}
		}
	}

	gene_log_get_datetime(datetime, sizeof(datetime));

	{
		const char *class_name = ZSTR_VAL(Z_OBJCE_P(ex)->name);
		const char *msg_str = (Z_TYPE(msg_val) == IS_STRING) ? Z_STRVAL(msg_val) : "";
		const char *file_str = (Z_TYPE(file_val) == IS_STRING) ? Z_STRVAL(file_val) : "";
		zend_long line_num = (Z_TYPE(line_val) == IS_LONG) ? Z_LVAL(line_val) : 0;
		const char *trace_str = (Z_TYPE(trace_val) == IS_STRING) ? Z_STRVAL(trace_val) : "";

		smart_str_appends(&log_line, "[");
		smart_str_appends(&log_line, datetime);
		smart_str_appends(&log_line, "] [Gene.ERROR] [");
		smart_str_appends(&log_line, class_name);
		smart_str_appends(&log_line, "] ");
		smart_str_appends(&log_line, msg_str);
		smart_str_appends(&log_line, " in ");
		smart_str_appends(&log_line, file_str);
		smart_str_appendc(&log_line, ':');
		smart_str_append_long(&log_line, line_num);
		smart_str_appendc(&log_line, '\n');
		if (extra_msg && ZSTR_LEN(extra_msg) > 0) {
			smart_str_appendl(&log_line, ZSTR_VAL(extra_msg), ZSTR_LEN(extra_msg));
			smart_str_appendc(&log_line, '\n');
		}
		smart_str_appends(&log_line, trace_str);

		gene_log_call_error_log(smart_str_extract(&log_line), gene_log_get_effective_file());
	}

	zval_ptr_dtor(&msg_val);
	zval_ptr_dtor(&file_val);
	zval_ptr_dtor(&line_val);
	zval_ptr_dtor(&trace_val);
}
/* }}} */

/* {{{ proto static void Gene\Log::setFile(string $file) */
PHP_METHOD(gene_log, setFile) {
	zend_string *file;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &file) == FAILURE) {
		return;
	}
	ctx = gene_request_ctx();
	if (ctx) {
		if (GENE_CTX_COLD(ctx)->log_file) {
			zend_string_release(GENE_CTX_COLD(ctx)->log_file);
		}
		GENE_CTX_COLD(ctx)->log_file = zend_string_copy(file);
	}
}
/* }}} */

/* {{{ proto static string|null Gene\Log::getFile() */
PHP_METHOD(gene_log, getFile) {
	zend_string *effective = gene_log_get_effective_file();
	if (effective) {
		RETURN_STR_COPY(effective);
	}
	RETURN_NULL();
}
/* }}} */

/* {{{ proto static void Gene\Log::setLevel(int $level) */
PHP_METHOD(gene_log, setLevel) {
	zend_long level;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &level) == FAILURE) {
		return;
	}
	if (level < GENE_LOG_LEVEL_DEBUG || level > GENE_LOG_LEVEL_EMERGENCY) {
		php_error_docref(NULL, E_WARNING, "Log level must be between %d and %d", GENE_LOG_LEVEL_DEBUG, GENE_LOG_LEVEL_EMERGENCY);
		return;
	}
	ctx = gene_request_ctx();
	if (ctx) {
		GENE_CTX_COLD(ctx)->log_level = level;
		GENE_CTX_COLD(ctx)->log_level_set = 1;
	}
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
	PHP_ME(gene_log, debug,     gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, info,      gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, notice,    gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, warning,   gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, error,     gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, critical,  gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, alert,     gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_log, emergency, gene_log_message_context_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
	/* [GENE_FEATURE:2026-08-07] RFC-5424 severity levels above ERROR. */
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_CRITICAL"), GENE_LOG_LEVEL_CRITICAL);
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_ALERT"),    GENE_LOG_LEVEL_ALERT);
	zend_declare_class_constant_long(gene_log_ce, ZEND_STRL("LEVEL_EMERGENCY"), GENE_LOG_LEVEL_EMERGENCY);

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
