/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include "zend_smart_str.h"
#include <string.h>
#include <stdio.h>

#include "../gene.h"
#include "../common/common.h"
#include "../factory/factory.h"
#include "../http/json.h"
#include "../http/http.h"
#include "../http/response.h"

#define GENE_HTTP_SSE_MAX_BUF 1048576
#define GENE_HTTP_MULTI_MAX 64

static void gene_http_invoke_stream(zend_string *chunk);

zend_class_entry *gene_http_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_http_request_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_http_multi_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, requests, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_http_write_fn_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, ch)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

static int gene_http_has_sse(gene_request_context *ctx) {
	return ctx && (ctx->http_sse_leftover != NULL);
}

static void gene_http_sse_emit(gene_request_context *ctx, zend_string *event, zval *data) {
	zval retval, args[2];
	zend_function *fn;

	if (!ctx || ctx->http_sse_done) {
		return;
	}
	if (ctx->http_sse_forward) {
		fn = zend_hash_str_find_ptr(&gene_response_ce->function_table, ZEND_STRL("sseevent"));
		if (fn) {
			ZVAL_STR(&args[0], event);
			ZVAL_COPY(&args[1], data);
			zend_call_known_function(fn, NULL, gene_response_ce, &retval, 2, args, NULL);
			zval_ptr_dtor(&args[1]);
			zval_ptr_dtor(&retval);
		}
	}
	if (Z_TYPE(ctx->http_sse_cb) != IS_UNDEF) {
		ZVAL_STR(&args[0], event);
		ZVAL_COPY(&args[1], data);
		if (call_user_function(NULL, NULL, &ctx->http_sse_cb, &retval, 2, args) != SUCCESS) {
			if (EG(exception)) {
				/* leave pending */
			}
		}
		zval_ptr_dtor(&args[1]);
		zval_ptr_dtor(&retval);
	}
}

static void gene_http_sse_dispatch(gene_request_context *ctx, zend_string *event, smart_str *data_buf) {
	zval payload, null_zv;
	zend_string *data_s, *ev;

	if (!ctx || ctx->http_sse_done || !data_buf) {
		return;
	}
	smart_str_0(data_buf);
	if (!data_buf->s || ZSTR_LEN(data_buf->s) == 0) {
		return;
	}
	data_s = data_buf->s;
	if (ZSTR_LEN(data_s) == 6 && memcmp(ZSTR_VAL(data_s), "[DONE]", 6) == 0) {
		ZVAL_NULL(&null_zv);
		ev = zend_string_init("done", sizeof("done") - 1, 0);
		gene_http_sse_emit(ctx, ev, &null_zv);
		zend_string_release(ev);
		ctx->http_sse_done = 1;
		return;
	}
	if (gene_json_decode_throw(data_s, &payload) == SUCCESS) {
		if (Z_TYPE(payload) == IS_ARRAY || Z_TYPE(payload) == IS_OBJECT) {
			ev = event && ZSTR_LEN(event) > 0 ? zend_string_copy(event) : zend_string_init("message", sizeof("message") - 1, 0);
			gene_http_sse_emit(ctx, ev, &payload);
			zend_string_release(ev);
			zval_ptr_dtor(&payload);
			return;
		}
		zval_ptr_dtor(&payload);
	} else if (EG(exception)) {
		zend_clear_exception();
	}
	ev = event && ZSTR_LEN(event) > 0 ? zend_string_copy(event) : zend_string_init("message", sizeof("message") - 1, 0);
	ZVAL_STR(&payload, data_s);
	gene_http_sse_emit(ctx, ev, &payload);
	zend_string_release(ev);
}

static void gene_http_sse_parse_frame(gene_request_context *ctx, const char *frame, size_t len) {
	const char *p = frame, *end = frame + len, *line_end;
	zend_string *event = NULL;
	smart_str data_buf = {0};

	while (p < end) {
		size_t llen;
		const char *val;
		line_end = memchr(p, '\n', (size_t)(end - p));
		if (!line_end) {
			line_end = end;
		}
		llen = (size_t)(line_end - p);
		if (llen && p[llen - 1] == '\r') {
			llen--;
		}
		if (llen == 0) {
			p = (line_end < end) ? line_end + 1 : end;
			continue;
		}
		if (llen >= 6 && strncasecmp(p, "event:", 6) == 0) {
			val = p + 6;
			while (*val == ' ') val++;
			if (event) {
				zend_string_release(event);
			}
			event = zend_string_init(val, (size_t)(p + llen - val), 0);
		} else if (llen >= 5 && strncasecmp(p, "data:", 5) == 0) {
			val = p + 5;
			while (*val == ' ') val++;
			if (data_buf.s && ZSTR_LEN(data_buf.s) > 0) {
				smart_str_appendc(&data_buf, '\n');
			}
			smart_str_appendl(&data_buf, val, (size_t)(p + llen - val));
		}
		p = (line_end < end) ? line_end + 1 : end;
	}
	gene_http_sse_dispatch(ctx, event, &data_buf);
	if (event) {
		zend_string_release(event);
	}
	smart_str_free(&data_buf);
}

static void gene_http_sse_feed(gene_request_context *ctx, const char *data, size_t len) {
	smart_str *leftover;
	const char *p, *end, *sep;
	size_t total;

	if (!ctx || !ctx->http_sse_leftover || ctx->http_sse_done || !data || len == 0) {
		return;
	}
	leftover = (smart_str *)ctx->http_sse_leftover;
	total = (leftover->s ? ZSTR_LEN(leftover->s) : 0) + len;
	if (total > GENE_HTTP_SSE_MAX_BUF) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http SSE buffer exceeded 1MB");
		return;
	}
	smart_str_appendl(leftover, data, len);
	smart_str_0(leftover);
	if (!leftover->s) {
		return;
	}
	p = ZSTR_VAL(leftover->s);
	end = p + ZSTR_LEN(leftover->s);
	while (!ctx->http_sse_done && p < end) {
		sep = NULL;
		{
			const char *q = p;
			while (q + 1 < end) {
				if (q[0] == '\n' && q[1] == '\n') {
					sep = q;
					break;
				}
				q++;
			}
		}
		if (!sep) {
			if (p != ZSTR_VAL(leftover->s)) {
				size_t rest_len = (size_t)(end - p);
				char *tmp = estrndup(p, rest_len);
				smart_str_free(leftover);
				smart_str_appendl(leftover, tmp, rest_len);
				efree(tmp);
			}
			return;
		}
		gene_http_sse_parse_frame(ctx, p, (size_t)(sep - p));
		p = sep + 2;
	}
	if (p < end && !ctx->http_sse_done) {
		size_t rest_len = (size_t)(end - p);
		char *tmp = estrndup(p, rest_len);
		smart_str_free(leftover);
		smart_str_appendl(leftover, tmp, rest_len);
		efree(tmp);
	} else {
		smart_str_free(leftover);
	}
}

static void gene_http_feed_body_chunk(gene_request_context *ctx, zend_string *chunk) {
	if (!ctx || !chunk || ZSTR_LEN(chunk) == 0) {
		return;
	}
	if (gene_http_has_sse(ctx)) {
		gene_http_sse_feed(ctx, ZSTR_VAL(chunk), ZSTR_LEN(chunk));
		return;
	}
	if (Z_TYPE(ctx->http_stream_cb) != IS_UNDEF) {
		gene_http_invoke_stream(chunk);
	}
}

static void gene_http_restore_state(gene_request_context *ctx, zval *saved_stream, zval *saved_sse,
		int had_stream, int had_sse) {
	if (!ctx) {
		return;
	}
	if (had_stream) {
		zval_ptr_dtor(&ctx->http_stream_cb);
		ZVAL_COPY_VALUE(&ctx->http_stream_cb, saved_stream);
	}
	if (had_sse) {
		zval_ptr_dtor(&ctx->http_sse_cb);
		ZVAL_COPY_VALUE(&ctx->http_sse_cb, saved_sse);
	}
	ctx->http_sse_leftover = NULL;
	ctx->http_sse_forward = 0;
	ctx->http_sse_done = 0;
	ctx->http_discard_body = 0;
	ctx->http_busy = 0;
}

/* {{{ gene_php_call */
static int gene_http_php_call(const char *name, size_t name_len, uint32_t argc, zval *params, zval *retval) {
	zend_function *fn;
	ZVAL_UNDEF(retval);
	fn = zend_hash_str_find_ptr(CG(function_table), name, name_len);
	if (UNEXPECTED(!fn)) {
		return FAILURE;
	}
	zend_call_known_function(fn, NULL, NULL, retval, argc, params, NULL);
	return EG(exception) ? FAILURE : SUCCESS;
}
/* }}} */

static zend_long gene_http_const(const char *name, size_t len) {
	zval *c = zend_get_constant_str(name, len);
	if (c && Z_TYPE_P(c) == IS_LONG) {
		return Z_LVAL_P(c);
	}
	return 0;
}

static int gene_http_curl_setopt(zval *ch, const char *cname, size_t clen, zval *value) {
	zval opt, params[3], ret;
	zend_long code = gene_http_const(cname, clen);
	if (code == 0) {
		return FAILURE;
	}
	ZVAL_LONG(&opt, code);
	ZVAL_COPY_VALUE(&params[0], ch);
	params[1] = opt;
	ZVAL_COPY_VALUE(&params[2], value);
	if (gene_http_php_call("curl_setopt", sizeof("curl_setopt") - 1, 3, params, &ret) != SUCCESS) {
		return FAILURE;
	}
	zval_ptr_dtor(&ret);
	return SUCCESS;
}

static void gene_http_invoke_stream(zend_string *chunk) {
	gene_request_context *ctx = gene_request_ctx();
	zval retval, arg;
	if (!ctx || Z_TYPE(ctx->http_stream_cb) == IS_UNDEF) {
		return;
	}
	ZVAL_STR(&arg, chunk); /* borrow */
	if (call_user_function(NULL, NULL, &ctx->http_stream_cb, &retval, 1, &arg) != SUCCESS) {
		if (EG(exception)) {
			/* leave pending; curl write still returns length */
		}
	}
	zval_ptr_dtor(&retval);
}

/* Swoole Client has no write-function; after execute, feed stream in 8KB slices.
 * Full body is still returned in the result array (API contract). */
static void gene_http_invoke_stream_body(gene_request_context *ctx, zend_string *body) {
	size_t off = 0;
	const size_t chunk = 8192;
	if (!body || ZSTR_LEN(body) == 0) {
		return;
	}
	if (ZSTR_LEN(body) <= chunk) {
		gene_http_feed_body_chunk(ctx, body);
		return;
	}
	while (off < ZSTR_LEN(body)) {
		size_t n = ZSTR_LEN(body) - off;
		zend_string *part;
		if (n > chunk) {
			n = chunk;
		}
		part = zend_string_init(ZSTR_VAL(body) + off, n, 0);
		gene_http_feed_body_chunk(ctx, part);
		zend_string_release(part);
		off += n;
	}
}

/* Internal CURLOPT_WRITEFUNCTION adapter: (CurlHandle, string) -> int */
PHP_METHOD(gene_http, _writeFn) {
	zval *ch, *data;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &ch, &data) == FAILURE) {
		RETURN_LONG(0);
	}
	if (Z_TYPE_P(data) != IS_STRING) {
		RETURN_LONG(0);
	}
	ctx = gene_request_ctx();
	if (ctx && ctx->http_body_buf && !ctx->http_discard_body) {
		smart_str_appendl((smart_str *)ctx->http_body_buf, Z_STRVAL_P(data), Z_STRLEN_P(data));
	}
	if (ctx) {
		gene_http_feed_body_chunk(ctx, Z_STR_P(data));
	}
	RETURN_LONG((zend_long)Z_STRLEN_P(data));
}

PHP_METHOD(gene_http, _headerFn) {
	zval *ch, *data;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zz", &ch, &data) == FAILURE) {
		RETURN_LONG(0);
	}
	if (Z_TYPE_P(data) != IS_STRING) {
		RETURN_LONG(0);
	}
	ctx = gene_request_ctx();
	if (ctx && ctx->http_header_buf) {
		smart_str_appendl((smart_str *)ctx->http_header_buf, Z_STRVAL_P(data), Z_STRLEN_P(data));
	}
	RETURN_LONG((zend_long)Z_STRLEN_P(data));
}

static void gene_http_parse_headers(const char *raw, size_t len, zval *out) {
	const char *p = raw, *end = raw + len, *line_end;
	array_init(out);
	while (p < end) {
		line_end = memchr(p, '\n', (size_t)(end - p));
		if (!line_end) {
			line_end = end;
		}
		{
			size_t llen = (size_t)(line_end - p);
			const char *colon;
			if (llen && p[llen - 1] == '\r') {
				llen--;
			}
			if (llen == 0) {
				p = line_end + 1;
				continue;
			}
			colon = memchr(p, ':', llen);
			if (colon && colon != p) {
				size_t klen = (size_t)(colon - p);
				const char *v = colon + 1;
				size_t vlen = llen - klen - 1;
				while (vlen && (*v == ' ' || *v == '\t')) {
					v++;
					vlen--;
				}
				add_assoc_stringl_ex(out, p, klen, (char *)v, vlen);
			}
		}
		p = (line_end < end) ? line_end + 1 : end;
	}
}

static void gene_http_sleep_us(zend_long usec) {
	zval in, out;
	if (usec <= 0) {
		return;
	}
	ZVAL_LONG(&in, usec);
	gene_http_php_call("usleep", sizeof("usleep") - 1, 1, &in, &out);
	zval_ptr_dtor(&out);
}

static int gene_http_method_retryable(const char *method) {
	return (strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0);
}

static zval *gene_http_opt(HashTable *ht, const char *k, size_t klen) {
	return zend_hash_str_find(ht, k, klen);
}

static int gene_http_option_known(zend_string *key) {
	static const char *known[] = {
		"method", "url", "headers", "query", "json", "body", "form", "files",
		"timeout", "connect_timeout", "ssl_verify", "retry", "stream", "sse",
		"sse_forward", "discard_body", "keep_alive"
	};
	size_t i;
	for (i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
		size_t len = strlen(known[i]);
		if (ZSTR_LEN(key) == len && memcmp(ZSTR_VAL(key), known[i], len) == 0) {
			return 1;
		}
	}
	return 0;
}

static int gene_http_check_options(zval *opts) {
	zend_string *key;
	zend_ulong idx;
	ZEND_HASH_FOREACH_KEY(Z_ARRVAL_P(opts), idx, key) {
		if (!key || !gene_http_option_known(key)) {
			if (key) {
				php_error_docref(NULL, E_NOTICE, "Gene\\Http::request unknown option: %s", ZSTR_VAL(key));
			} else {
				php_error_docref(NULL, E_NOTICE, "Gene\\Http::request unknown option: %lu", (unsigned long)idx);
			}
			if (EG(exception)) {
				return FAILURE;
			}
		}
	} ZEND_HASH_FOREACH_END();
	return SUCCESS;
}

static int gene_http_validate_query_value(zval *value) {
	zval *v = value;
	ZVAL_DEREF(v);
	if (Z_TYPE_P(v) == IS_OBJECT || Z_TYPE_P(v) == IS_RESOURCE) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request query/form does not accept object or resource values");
		return FAILURE;
	}
	if (Z_TYPE_P(v) == IS_ARRAY) {
		zval *item;
		if (GC_IS_RECURSIVE(Z_ARRVAL_P(v))) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Http::request query/form does not accept recursive arrays");
			return FAILURE;
		}
		GC_TRY_PROTECT_RECURSION(Z_ARRVAL_P(v));
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(v), item) {
			if (gene_http_validate_query_value(item) != SUCCESS) {
				GC_TRY_UNPROTECT_RECURSION(Z_ARRVAL_P(v));
				return FAILURE;
			}
		} ZEND_HASH_FOREACH_END();
		GC_TRY_UNPROTECT_RECURSION(Z_ARRVAL_P(v));
	}
	return SUCCESS;
}

static int gene_http_build_query(zval *data, zend_string **out) {
	zval params[4], ret;
	zend_long rfc3986 = gene_http_const(ZEND_STRL("PHP_QUERY_RFC3986"));
	*out = NULL;
	if (!data || Z_TYPE_P(data) != IS_ARRAY) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request query/form must be an array");
		return FAILURE;
	}
	if (gene_http_validate_query_value(data) != SUCCESS) {
		return FAILURE;
	}
	ZVAL_COPY(&params[0], data);
	ZVAL_EMPTY_STRING(&params[1]);
	ZVAL_STRING(&params[2], "&");
	ZVAL_LONG(&params[3], rfc3986 ? rfc3986 : 2);
	if (gene_http_php_call("http_build_query", sizeof("http_build_query") - 1, 4, params, &ret) != SUCCESS
		|| Z_TYPE(ret) != IS_STRING) {
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&params[2]);
		zval_ptr_dtor(&ret);
		if (!EG(exception)) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Http::request failed to encode query/form");
		}
		return FAILURE;
	}
	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	*out = Z_STR(ret);
	return SUCCESS;
}

static zend_string *gene_http_url_with_query(zend_string *url, zend_string *query) {
	const char *fragment = memchr(ZSTR_VAL(url), '#', ZSTR_LEN(url));
	size_t base_len = fragment ? (size_t)(fragment - ZSTR_VAL(url)) : ZSTR_LEN(url);
	size_t fragment_len = ZSTR_LEN(url) - base_len;
	const char *question = memchr(ZSTR_VAL(url), '?', base_len);
	size_t separator_len = question ? ((size_t)(question - ZSTR_VAL(url)) + 1 < base_len ? 1 : 0) : 1;
	zend_string *result;
	char *p;
	if (!query || ZSTR_LEN(query) == 0) {
		return zend_string_copy(url);
	}
	result = zend_string_alloc(base_len + separator_len + ZSTR_LEN(query) + fragment_len, 0);
	p = ZSTR_VAL(result);
	memcpy(p, ZSTR_VAL(url), base_len);
	p += base_len;
	if (separator_len) {
		*p++ = question ? '&' : '?';
	}
	memcpy(p, ZSTR_VAL(query), ZSTR_LEN(query));
	p += ZSTR_LEN(query);
	if (fragment_len) {
		memcpy(p, fragment, fragment_len);
	}
	ZSTR_VAL(result)[ZSTR_LEN(result)] = '\0';
	return result;
}

static int gene_http_hex_value(unsigned char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

static zend_string *gene_http_percent_decode(const char *src, size_t len) {
	zend_string *out = zend_string_alloc(len, 0);
	size_t i, n = 0;
	for (i = 0; i < len; i++) {
		if (src[i] == '%' && i + 2 < len) {
			int hi = gene_http_hex_value((unsigned char)src[i + 1]);
			int lo = gene_http_hex_value((unsigned char)src[i + 2]);
			if (hi >= 0 && lo >= 0) {
				ZSTR_VAL(out)[n++] = (char)((hi << 4) | lo);
				i += 2;
				continue;
			}
		}
		ZSTR_VAL(out)[n++] = src[i];
	}
	ZSTR_VAL(out)[n] = '\0';
	ZSTR_LEN(out) = n;
	return out;
}

static int gene_http_build_multipart_form(zval *form, zval *out) {
	zend_string *encoded;
	const char *p, *end;
	array_init(out);
	if (gene_http_build_query(form, &encoded) != SUCCESS) {
		zval_ptr_dtor(out);
		ZVAL_UNDEF(out);
		return FAILURE;
	}
	p = ZSTR_VAL(encoded);
	end = p + ZSTR_LEN(encoded);
	while (p < end) {
		const char *amp = memchr(p, '&', (size_t)(end - p));
		const char *pair_end = amp ? amp : end;
		const char *eq = memchr(p, '=', (size_t)(pair_end - p));
		zend_string *key = gene_http_percent_decode(p, eq ? (size_t)(eq - p) : (size_t)(pair_end - p));
		zend_string *value = gene_http_percent_decode(eq ? eq + 1 : pair_end,
			eq ? (size_t)(pair_end - eq - 1) : 0);
		zval zv;
		ZVAL_STR(&zv, value);
		zend_hash_update(Z_ARRVAL_P(out), key, &zv);
		zend_string_release(key);
		p = amp ? amp + 1 : end;
	}
	zend_string_release(encoded);
	return SUCCESS;
}

static int gene_http_headers_have_content_type(zval *headers) {
	zend_string *key;
	if (!headers || Z_TYPE_P(headers) != IS_ARRAY) {
		return 0;
	}
	ZEND_HASH_FOREACH_STR_KEY(Z_ARRVAL_P(headers), key) {
		if (key && ZSTR_LEN(key) == sizeof("Content-Type") - 1
			&& strncasecmp(ZSTR_VAL(key), "Content-Type", sizeof("Content-Type") - 1) == 0) {
			return 1;
		}
	} ZEND_HASH_FOREACH_END();
	return 0;
}

static void gene_http_fill_result(zend_long status, zval *headers, zend_string *body, zval *return_value) {
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL("status"), status);
	if (headers && Z_TYPE_P(headers) == IS_ARRAY) {
		Z_TRY_ADDREF_P(headers);
		add_assoc_zval_ex(return_value, ZEND_STRL("headers"), headers);
	} else {
		zval empty;
		array_init(&empty);
		add_assoc_zval_ex(return_value, ZEND_STRL("headers"), &empty);
	}
	if (body) {
		add_assoc_str_ex(return_value, ZEND_STRL("body"), zend_string_copy(body));
	} else {
		add_assoc_string_ex(return_value, ZEND_STRL("body"), "");
	}
}

/* Build curl header list from assoc array. Returns IS_ARRAY of "K: V" strings. */
static void gene_http_build_curl_headers(zval *headers_in, zval *headers_out, const char *content_type) {
	array_init(headers_out);
	if (headers_in && Z_TYPE_P(headers_in) == IS_ARRAY) {
		zend_string *k;
		zval *v;
		zend_ulong idx;
		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(headers_in), idx, k, v) {
			zend_string *vs;
			char *line;
			size_t line_len;
			(void)idx;
			if (!k) {
				continue;
			}
			vs = zval_get_string(v);
			line_len = ZSTR_LEN(k) + 2 + ZSTR_LEN(vs);
			line = emalloc(line_len + 1);
			memcpy(line, ZSTR_VAL(k), ZSTR_LEN(k));
			memcpy(line + ZSTR_LEN(k), ": ", 2);
			memcpy(line + ZSTR_LEN(k) + 2, ZSTR_VAL(vs), ZSTR_LEN(vs));
			line[line_len] = '\0';
			add_next_index_stringl(headers_out, line, line_len);
			efree(line);
			zend_string_release(vs);
		} ZEND_HASH_FOREACH_END();
	}
	if (content_type && !gene_http_headers_have_content_type(headers_in)) {
		zend_string *line = strpprintf(0, "Content-Type: %s", content_type);
		add_next_index_str(headers_out, line);
	}
}

static int gene_http_ensure_curl(zval *ch_out) {
	gene_request_context *ctx = gene_request_ctx();
	zval ret;
	if (ctx && (Z_TYPE(ctx->http_curl) == IS_OBJECT || Z_TYPE(ctx->http_curl) == IS_RESOURCE)) {
		ZVAL_COPY(ch_out, &ctx->http_curl);
		if (gene_http_php_call("curl_reset", sizeof("curl_reset") - 1, 1, ch_out, &ret) == SUCCESS) {
			zval_ptr_dtor(&ret);
		} else if (EG(exception)) {
			zend_clear_exception();
		}
		return SUCCESS;
	}
	if (gene_http_php_call("curl_init", sizeof("curl_init") - 1, 0, NULL, ch_out) != SUCCESS
		|| (Z_TYPE_P(ch_out) != IS_OBJECT && Z_TYPE_P(ch_out) != IS_RESOURCE)) {
		zval_ptr_dtor(ch_out);
		ZVAL_UNDEF(ch_out);
		zend_throw_exception_ex(NULL, 0, "Gene\\Http requires the curl extension in FPM/CLI mode");
		return FAILURE;
	}
	if (ctx) {
		ZVAL_COPY(&ctx->http_curl, ch_out);
	}
	return SUCCESS;
}

static int gene_http_curl_exec_once(zval *ch, zend_long *status, zval *headers_out, zend_string **body_out, char **err_out) {
	gene_request_context *ctx = gene_request_ctx();
	smart_str body_buf = {0};
	smart_str header_buf = {0};
	zval exec_ret, info, info_opt, err;
	zval write_cb, header_cb;
	zend_long header_code;
	int failed = 0;

	*status = 0;
	*body_out = NULL;
	*err_out = NULL;
	ZVAL_UNDEF(headers_out);

	if (ctx) {
		ctx->http_body_buf = &body_buf;
		ctx->http_header_buf = &header_buf;
	}

	ZVAL_STRING(&write_cb, GENE_G(use_namespace) ? "Gene\\Http::_writeFn" : "Gene_Http::_writeFn");
	ZVAL_STRING(&header_cb, GENE_G(use_namespace) ? "Gene\\Http::_headerFn" : "Gene_Http::_headerFn");
	gene_http_curl_setopt(ch, ZEND_STRL("CURLOPT_WRITEFUNCTION"), &write_cb);
	gene_http_curl_setopt(ch, ZEND_STRL("CURLOPT_HEADERFUNCTION"), &header_cb);
	zval_ptr_dtor(&write_cb);
	zval_ptr_dtor(&header_cb);

	if (gene_http_php_call("curl_exec", sizeof("curl_exec") - 1, 1, ch, &exec_ret) != SUCCESS) {
		failed = 1;
	}
	zval_ptr_dtor(&exec_ret);

	if (ctx) {
		ctx->http_body_buf = NULL;
		ctx->http_header_buf = NULL;
	}

	header_code = gene_http_const(ZEND_STRL("CURLINFO_RESPONSE_CODE"));
	if (header_code == 0) {
		header_code = gene_http_const(ZEND_STRL("CURLINFO_HTTP_CODE"));
	}
	ZVAL_LONG(&info_opt, header_code);
	{
		zval params[2];
		ZVAL_COPY_VALUE(&params[0], ch);
		params[1] = info_opt;
		if (gene_http_php_call("curl_getinfo", sizeof("curl_getinfo") - 1, 2, params, &info) == SUCCESS
			&& Z_TYPE(info) == IS_LONG) {
			*status = Z_LVAL(info);
		}
		zval_ptr_dtor(&info);
	}

	if (failed || *status == 0) {
		if (gene_http_php_call("curl_error", sizeof("curl_error") - 1, 1, ch, &err) == SUCCESS
			&& Z_TYPE(err) == IS_STRING && Z_STRLEN(err) > 0) {
			*err_out = estrndup(Z_STRVAL(err), Z_STRLEN(err));
		}
		zval_ptr_dtor(&err);
	}

	smart_str_0(&header_buf);
	if (header_buf.s) {
		gene_http_parse_headers(ZSTR_VAL(header_buf.s), ZSTR_LEN(header_buf.s), headers_out);
	} else {
		array_init(headers_out);
	}
	smart_str_free(&header_buf);

	smart_str_0(&body_buf);
	if (body_buf.s) {
		*body_out = body_buf.s; /* steal */
		body_buf.s = NULL;
	} else {
		*body_out = ZSTR_EMPTY_ALLOC();
	}
	smart_str_free(&body_buf);
	return failed ? FAILURE : SUCCESS;
}

static int gene_http_file_parts(zval *entry, zend_string *key,
	const char **path, size_t *path_len, const char **name, size_t *name_len,
	const char **type, size_t *type_len)
{
	*path = NULL; *path_len = 0;
	*name = key ? ZSTR_VAL(key) : "file";
	*name_len = key ? ZSTR_LEN(key) : 4;
	*type = "application/octet-stream";
	*type_len = sizeof("application/octet-stream") - 1;
	if (Z_TYPE_P(entry) == IS_STRING && Z_STRLEN_P(entry) > 0) {
		*path = Z_STRVAL_P(entry);
		*path_len = Z_STRLEN_P(entry);
		return SUCCESS;
	}
	if (Z_TYPE_P(entry) == IS_ARRAY) {
		zval *tmp = zend_hash_str_find(Z_ARRVAL_P(entry), ZEND_STRL("tmp_name"));
		if (!tmp) {
			tmp = zend_hash_str_find(Z_ARRVAL_P(entry), ZEND_STRL("path"));
		}
		if (tmp && Z_TYPE_P(tmp) == IS_STRING && Z_STRLEN_P(tmp) > 0) {
			*path = Z_STRVAL_P(tmp);
			*path_len = Z_STRLEN_P(tmp);
		}
		tmp = zend_hash_str_find(Z_ARRVAL_P(entry), ZEND_STRL("name"));
		if (tmp && Z_TYPE_P(tmp) == IS_STRING && Z_STRLEN_P(tmp) > 0) {
			*name = Z_STRVAL_P(tmp);
			*name_len = Z_STRLEN_P(tmp);
		}
		tmp = zend_hash_str_find(Z_ARRVAL_P(entry), ZEND_STRL("type"));
		if (tmp && Z_TYPE_P(tmp) == IS_STRING && Z_STRLEN_P(tmp) > 0) {
			*type = Z_STRVAL_P(tmp);
			*type_len = Z_STRLEN_P(tmp);
		}
		return *path ? SUCCESS : FAILURE;
	}
	return FAILURE;
}

static int gene_http_curl_file_create(const char *path, size_t path_len, const char *type, size_t type_len,
	const char *name, size_t name_len, zval *out)
{
	zval params[3];
	ZVAL_STRINGL(&params[0], path, path_len);
	ZVAL_STRINGL(&params[1], type, type_len);
	ZVAL_STRINGL(&params[2], name, name_len);
	if (gene_http_php_call("curl_file_create", sizeof("curl_file_create") - 1, 3, params, out) != SUCCESS) {
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&params[2]);
		return FAILURE;
	}
	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&params[2]);
	return SUCCESS;
}

static int gene_http_build_multipart(zval *files, zval *form, zval *out) {
	zend_string *k;
	zval *v;
	zend_ulong idx;
	array_init(out);
	if (form && Z_TYPE_P(form) == IS_ARRAY) {
		zend_hash_copy(Z_ARRVAL_P(out), Z_ARRVAL_P(form), (copy_ctor_func_t) zval_add_ref);
	}
	if (!files || Z_TYPE_P(files) != IS_ARRAY) {
		return SUCCESS;
	}
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(files), idx, k, v) {
		const char *path, *name, *type;
		size_t path_len, name_len, type_len;
		zval cf;
		char idx_key[32];
		(void)idx;
		if (gene_http_file_parts(v, k, &path, &path_len, &name, &name_len, &type, &type_len) != SUCCESS) {
			zval_ptr_dtor(out);
			ZVAL_UNDEF(out);
			zend_throw_exception_ex(NULL, 0, "Gene\\Http::request files entry is invalid");
			return FAILURE;
		}
		if (gene_http_curl_file_create(path, path_len, type, type_len, name, name_len, &cf) != SUCCESS) {
			zval_ptr_dtor(out);
			ZVAL_UNDEF(out);
			zend_throw_exception_ex(NULL, 0, "Gene\\Http::request failed to create CURLFile");
			return FAILURE;
		}
		if (k) {
			zend_hash_update(Z_ARRVAL_P(out), k, &cf);
		} else {
			snprintf(idx_key, sizeof(idx_key), "file%lu", (unsigned long)idx);
			add_assoc_zval(out, idx_key, &cf);
		}
	} ZEND_HASH_FOREACH_END();
	return SUCCESS;
}

static int gene_http_swoole_once(const char *method, zend_string *url, zval *headers_in,
		zend_string *body, zval *files, zval *form, double timeout, double connect_timeout, zend_bool ssl_verify,
		zend_bool keep_alive, zend_long *status, zval *headers_out, zend_string **body_out, char **err_out) {
	zval parsed, cli, ctor_params, set_opts, set_params;
	zval *scheme, *host, *port, *path, *query;
	zend_bool ssl = 0;
	zend_long port_l = 80;
	zend_string *path_qs;
	zend_class_entry *ce;
	gene_request_context *hctx;
	char peer_key[320];
	size_t peer_key_len;

	*status = 0;
	*body_out = NULL;
	*err_out = NULL;
	ZVAL_UNDEF(headers_out);

	ce = gene_lookup_class_str("swoole\\coroutine\\http\\client", sizeof("swoole\\coroutine\\http\\client") - 1);
	if (!ce) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http Swoole backend requires Swoole\\Coroutine\\Http\\Client");
		return FAILURE;
	}

	{
		zval urlz;
		ZVAL_STR(&urlz, url);
		if (gene_http_php_call("parse_url", sizeof("parse_url") - 1, 1, &urlz, &parsed) != SUCCESS
			|| Z_TYPE(parsed) != IS_ARRAY) {
			zval_ptr_dtor(&parsed);
			zend_throw_exception_ex(NULL, 0, "Gene\\Http: invalid url");
			return FAILURE;
		}
	}
	scheme = zend_hash_str_find(Z_ARRVAL(parsed), ZEND_STRL("scheme"));
	host = zend_hash_str_find(Z_ARRVAL(parsed), ZEND_STRL("host"));
	port = zend_hash_str_find(Z_ARRVAL(parsed), ZEND_STRL("port"));
	path = zend_hash_str_find(Z_ARRVAL(parsed), ZEND_STRL("path"));
	query = zend_hash_str_find(Z_ARRVAL(parsed), ZEND_STRL("query"));
	if (!host || Z_TYPE_P(host) != IS_STRING) {
		zval_ptr_dtor(&parsed);
		zend_throw_exception_ex(NULL, 0, "Gene\\Http: url host missing");
		return FAILURE;
	}
	ssl = (scheme && Z_TYPE_P(scheme) == IS_STRING && Z_STRLEN_P(scheme) == 5
		&& strncasecmp(Z_STRVAL_P(scheme), "https", 5) == 0);
	port_l = port && Z_TYPE_P(port) == IS_LONG ? Z_LVAL_P(port) : (ssl ? 443 : 80);

	{
		const char *p = (path && Z_TYPE_P(path) == IS_STRING && Z_STRLEN_P(path)) ? Z_STRVAL_P(path) : "/";
		size_t plen = (path && Z_TYPE_P(path) == IS_STRING && Z_STRLEN_P(path)) ? Z_STRLEN_P(path) : 1;
		if (query && Z_TYPE_P(query) == IS_STRING && Z_STRLEN_P(query)) {
			path_qs = zend_string_alloc(plen + 1 + Z_STRLEN_P(query), 0);
			memcpy(ZSTR_VAL(path_qs), p, plen);
			ZSTR_VAL(path_qs)[plen] = '?';
			memcpy(ZSTR_VAL(path_qs) + plen + 1, Z_STRVAL_P(query), Z_STRLEN_P(query));
			ZSTR_VAL(path_qs)[plen + 1 + Z_STRLEN_P(query)] = '\0';
		} else {
			path_qs = zend_string_init(p, plen, 0);
		}
	}

	peer_key_len = (size_t)snprintf(peer_key, sizeof(peer_key), "%s:%ld:%d",
		Z_STRVAL_P(host), port_l, ssl ? 1 : 0);
	if (peer_key_len >= sizeof(peer_key)) {
		peer_key_len = sizeof(peer_key) - 1;
	}
	hctx = gene_request_ctx();
	ZVAL_UNDEF(&cli);
	if (files && Z_TYPE_P(files) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(files)) > 0) {
		keep_alive = 0;
	}
	if (keep_alive && hctx && Z_TYPE(hctx->http_curl) == IS_ARRAY) {
		zval *slot = zend_hash_str_find(Z_ARRVAL(hctx->http_curl), peer_key, peer_key_len);
		if (slot && Z_TYPE_P(slot) == IS_OBJECT) {
			ZVAL_COPY(&cli, slot);
		}
	}
	if (Z_TYPE(cli) != IS_OBJECT) {
		array_init(&ctor_params);
		Z_TRY_ADDREF_P(host);
		add_next_index_zval(&ctor_params, host);
		add_next_index_long(&ctor_params, port_l);
		add_next_index_bool(&ctor_params, ssl);
		if (!gene_factory("Swoole\\Coroutine\\Http\\Client", sizeof("Swoole\\Coroutine\\Http\\Client") - 1, &ctor_params, &cli)
			|| Z_TYPE(cli) != IS_OBJECT) {
			zval_ptr_dtor(&ctor_params);
			zval_ptr_dtor(&parsed);
			zend_string_release(path_qs);
			zend_throw_exception_ex(NULL, 0, "Gene\\Http: failed to create Swoole HTTP client");
			return FAILURE;
		}
		zval_ptr_dtor(&ctor_params);
		if (keep_alive && hctx) {
			if (Z_TYPE(hctx->http_curl) != IS_ARRAY) {
				if (Z_TYPE(hctx->http_curl) != IS_UNDEF) {
					zval_ptr_dtor(&hctx->http_curl);
				}
				array_init(&hctx->http_curl);
			}
			Z_TRY_ADDREF(cli);
			zend_hash_str_update(Z_ARRVAL(hctx->http_curl), peer_key, peer_key_len, &cli);
		}
	}

	array_init(&set_opts);
	add_assoc_double_ex(&set_opts, ZEND_STRL("timeout"), timeout);
	add_assoc_double_ex(&set_opts, ZEND_STRL("connect_timeout"), connect_timeout);
	add_assoc_bool_ex(&set_opts, ZEND_STRL("keep_alive"), keep_alive);
	add_assoc_bool_ex(&set_opts, ZEND_STRL("ssl_verify_peer"), ssl_verify);
	add_assoc_bool_ex(&set_opts, ZEND_STRL("ssl_verify_host"), ssl_verify);
	array_init(&set_params);
	add_next_index_zval(&set_params, &set_opts);
	{
		zval dummy;
		gene_factory_call(&cli, "set", sizeof("set") - 1, &set_params, &dummy);
		zval_ptr_dtor(&dummy);
	}
	zval_ptr_dtor(&set_params); /* also dtors set_opts */

	{
		zval mparams, mz, dummy;
		array_init(&mparams);
		ZVAL_STRING(&mz, method);
		add_next_index_zval(&mparams, &mz);
		gene_factory_call(&cli, "setmethod", sizeof("setmethod") - 1, &mparams, &dummy);
		zval_ptr_dtor(&dummy);
		zval_ptr_dtor(&mparams);
	}
	{
		zval hparams, dummy, hdrs;
		array_init(&hparams);
		if (headers_in && Z_TYPE_P(headers_in) == IS_ARRAY) {
			ZVAL_COPY(&hdrs, headers_in);
		} else {
			array_init(&hdrs);
		}
		add_next_index_zval(&hparams, &hdrs);
		gene_factory_call(&cli, "setheaders", sizeof("setheaders") - 1, &hparams, &dummy);
		zval_ptr_dtor(&dummy);
		zval_ptr_dtor(&hparams);
	}
	if (files && Z_TYPE_P(files) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(files)) > 0) {
		zend_string *k;
		zval *v;
		zend_ulong idx;
		ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(files), idx, k, v) {
			const char *path, *name, *type;
			size_t path_len, name_len, type_len;
			zval aparams, dummy;
			(void)idx;
			if (gene_http_file_parts(v, k, &path, &path_len, &name, &name_len, &type, &type_len) != SUCCESS) {
				zend_string_release(path_qs);
				zval_ptr_dtor(&cli);
				zval_ptr_dtor(&parsed);
				zend_throw_exception_ex(NULL, 0, "Gene\\Http::request files entry is invalid");
				return FAILURE;
			}
			array_init(&aparams);
			add_next_index_stringl(&aparams, path, path_len);
			add_next_index_stringl(&aparams, k ? ZSTR_VAL(k) : name, k ? ZSTR_LEN(k) : name_len);
			add_next_index_stringl(&aparams, type, type_len);
			add_next_index_stringl(&aparams, name, name_len);
			gene_factory_call(&cli, "addfile", sizeof("addfile") - 1, &aparams, &dummy);
			zval_ptr_dtor(&dummy);
			zval_ptr_dtor(&aparams);
		} ZEND_HASH_FOREACH_END();
		{
			zval dparams, dz, dummy;
			array_init(&dparams);
			if (form && Z_TYPE_P(form) == IS_ARRAY) {
				ZVAL_COPY(&dz, form);
			} else {
				array_init(&dz);
			}
			add_next_index_zval(&dparams, &dz);
			gene_factory_call(&cli, "setdata", sizeof("setdata") - 1, &dparams, &dummy);
			zval_ptr_dtor(&dummy);
			zval_ptr_dtor(&dparams);
		}
	} else {
		zval dparams, dz, dummy;
		array_init(&dparams);
		if (body && ZSTR_LEN(body) > 0) {
			ZVAL_STR_COPY(&dz, body);
		} else {
			ZVAL_EMPTY_STRING(&dz);
		}
		add_next_index_zval(&dparams, &dz);
		gene_factory_call(&cli, "setdata", sizeof("setdata") - 1, &dparams, &dummy);
		zval_ptr_dtor(&dummy);
		zval_ptr_dtor(&dparams);
	}
	{
		zval eparams, ez, dummy;
		array_init(&eparams);
		ZVAL_STR_COPY(&ez, path_qs);
		add_next_index_zval(&eparams, &ez);
		gene_factory_call(&cli, "execute", sizeof("execute") - 1, &eparams, &dummy);
		zval_ptr_dtor(&dummy);
		zval_ptr_dtor(&eparams);
	}
	zend_string_release(path_qs);

	{
		zval rv, *st, *hdrs, *bd, *errmsg;
		ZVAL_UNDEF(&rv);
		st = zend_read_property(Z_OBJCE(cli), gene_strip_obj(&cli), ZEND_STRL("statusCode"), 1, &rv);
		if (st && Z_TYPE_P(st) == IS_LONG) {
			*status = Z_LVAL_P(st);
		}
		if (!Z_ISUNDEF(rv)) {
			zval_ptr_dtor(&rv);
		}
		ZVAL_UNDEF(&rv);
		hdrs = zend_read_property(Z_OBJCE(cli), gene_strip_obj(&cli), ZEND_STRL("headers"), 1, &rv);
		if (hdrs && Z_TYPE_P(hdrs) == IS_ARRAY) {
			ZVAL_COPY(headers_out, hdrs);
		} else {
			array_init(headers_out);
		}
		if (!Z_ISUNDEF(rv)) {
			zval_ptr_dtor(&rv);
		}
		ZVAL_UNDEF(&rv);
		bd = zend_read_property(Z_OBJCE(cli), gene_strip_obj(&cli), ZEND_STRL("body"), 1, &rv);
		if (bd && Z_TYPE_P(bd) == IS_STRING) {
			*body_out = zend_string_copy(Z_STR_P(bd));
		} else {
			*body_out = ZSTR_EMPTY_ALLOC();
		}
		if (!Z_ISUNDEF(rv)) {
			zval_ptr_dtor(&rv);
		}
		ZVAL_UNDEF(&rv);
		errmsg = zend_read_property(Z_OBJCE(cli), gene_strip_obj(&cli), ZEND_STRL("errMsg"), 1, &rv);
		if (errmsg && Z_TYPE_P(errmsg) == IS_STRING && Z_STRLEN_P(errmsg) > 0) {
			*err_out = estrndup(Z_STRVAL_P(errmsg), Z_STRLEN_P(errmsg));
		}
		if (!Z_ISUNDEF(rv)) {
			zval_ptr_dtor(&rv);
		}
	}

	if (!keep_alive || *status <= 0) {
		zval dummy;
		gene_factory_call(&cli, "close", sizeof("close") - 1, NULL, &dummy);
		zval_ptr_dtor(&dummy);
		if (keep_alive && hctx && Z_TYPE(hctx->http_curl) == IS_ARRAY) {
			zend_hash_str_del(Z_ARRVAL(hctx->http_curl), peer_key, peer_key_len);
		}
	}
	zval_ptr_dtor(&cli);
	zval_ptr_dtor(&parsed);

	if (*status <= 0 && *err_out) {
		return FAILURE;
	}
	return SUCCESS;
}

/* {{{ proto static array Gene\Http::request(array $options) */
PHP_METHOD(gene_http, request) {
	zval *opts;
	zval *zurl, *zmethod, *zheaders, *zquery, *zjson, *zbody, *zform, *ztimeout, *zct, *zssl, *zretry, *zstream, *zsse, *zsse_forward, *zdiscard, *zka, *zfiles;
	zend_string *url, *owned_url = NULL, *body = NULL, *encoded = NULL;
	char method_buf[16];
	const char *method = "GET", *auto_content_type = NULL;
	zval curl_headers, multipart_form;
	zend_bool have_files = 0, have_multipart_form = 0, ssl_verify = 1, keep_alive = 0, own_body = 0;
	zend_bool had_stream = 0, had_sse = 0, sse_active = 0;
	double timeout = 60.0, connect_timeout = 3.0;
	zend_long retry = 0, attempt, status = 0;
	gene_request_context *ctx;
	zval saved_stream, saved_sse;
	smart_str sse_leftover = {0};
	zval headers_out;
	zend_string *resp_body = NULL;
	char *err = NULL;
	int use_swoole;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &opts) == FAILURE) {
		return;
	}
	if (gene_http_check_options(opts) != SUCCESS) {
		RETURN_THROWS();
	}
	ZVAL_UNDEF(&multipart_form);

	zurl = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("url"));
	if (!zurl || Z_TYPE_P(zurl) != IS_STRING || Z_STRLEN_P(zurl) == 0) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request requires a non-empty url");
		RETURN_THROWS();
	}
	url = Z_STR_P(zurl);
	zquery = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("query"));
	if (zquery) {
		if (gene_http_build_query(zquery, &encoded) != SUCCESS) {
			RETURN_THROWS();
		}
		if (ZSTR_LEN(encoded) > 0) {
			owned_url = gene_http_url_with_query(url, encoded);
			url = owned_url;
		}
		zend_string_release(encoded);
		encoded = NULL;
	}

	zmethod = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("method"));
	if (zmethod && Z_TYPE_P(zmethod) == IS_STRING && Z_STRLEN_P(zmethod) > 0 && Z_STRLEN_P(zmethod) < sizeof(method_buf)) {
		size_t i;
		memcpy(method_buf, Z_STRVAL_P(zmethod), Z_STRLEN_P(zmethod) + 1);
		for (i = 0; i < Z_STRLEN_P(zmethod); i++) {
			unsigned char c = (unsigned char)method_buf[i];
			if (c >= 'a' && c <= 'z') {
				method_buf[i] = (char)(c - 32);
			}
		}
		method = method_buf;
	}

	zjson = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("json"));
	zbody = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("body"));
	zform = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("form"));
	zfiles = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("files"));
	if (zfiles && Z_TYPE_P(zfiles) != IS_ARRAY) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request files must be an array");
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	have_files = zfiles && zend_hash_num_elements(Z_ARRVAL_P(zfiles)) > 0;
	if (zform && Z_TYPE_P(zform) != IS_ARRAY) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request form must be an array");
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	if (zjson && zbody) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request: json and body are mutually exclusive");
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	if (zjson && zform) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request: json and form are mutually exclusive");
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	if (zbody && zform) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request: body and form are mutually exclusive");
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	if (zjson && have_files) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request: json and files are mutually exclusive");
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	if (have_files && zbody && Z_TYPE_P(zbody) != IS_ARRAY) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request: files cannot be combined with a string body");
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	if (zjson) {
		zval json_encoded;
		if (gene_json_encode_throw(zjson, &json_encoded) != SUCCESS) {
			if (owned_url) zend_string_release(owned_url);
			RETURN_THROWS();
		}
		body = Z_STR(json_encoded);
		own_body = 1;
		auto_content_type = "application/json; charset=UTF-8";
	} else if (zform && !have_files) {
		if (gene_http_build_query(zform, &body) != SUCCESS) {
			if (owned_url) zend_string_release(owned_url);
			RETURN_THROWS();
		}
		own_body = 1;
		auto_content_type = "application/x-www-form-urlencoded";
	} else if (zbody) {
		if (have_files && Z_TYPE_P(zbody) == IS_ARRAY) {
			if (gene_http_build_multipart_form(zbody, &multipart_form) != SUCCESS) {
				if (owned_url) zend_string_release(owned_url);
				RETURN_THROWS();
			}
			have_multipart_form = 1;
		} else {
			body = zval_get_string(zbody);
			own_body = 1;
		}
	} else if (zform && have_files) {
		if (gene_http_build_multipart_form(zform, &multipart_form) != SUCCESS) {
			if (owned_url) zend_string_release(owned_url);
			RETURN_THROWS();
		}
		have_multipart_form = 1;
	}

	zheaders = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("headers"));
	ztimeout = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("timeout"));
	if (ztimeout) {
		timeout = zval_get_double(ztimeout);
	}
	zct = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("connect_timeout"));
	if (zct) {
		connect_timeout = zval_get_double(zct);
	}
	zssl = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("ssl_verify"));
	if (zssl) {
		ssl_verify = zend_is_true(zssl);
	}
	zretry = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("retry"));
	if (zretry) {
		retry = zval_get_long(zretry);
		if (retry < 0) retry = 0;
		if (retry > 3) retry = 3;
	}
	zka = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("keep_alive"));
	if (zka) {
		keep_alive = zend_is_true(zka);
	}
	zstream = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("stream"));
	zsse = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("sse"));
	zsse_forward = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("sse_forward"));
	zdiscard = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("discard_body"));
	if (zstream && zsse && Z_TYPE_P(zstream) != IS_NULL && Z_TYPE_P(zstream) != IS_UNDEF
		&& Z_TYPE_P(zsse) != IS_NULL && Z_TYPE_P(zsse) != IS_UNDEF) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request: stream and sse are mutually exclusive");
		if (have_multipart_form) zval_ptr_dtor(&multipart_form);
		if (own_body && body) zend_string_release(body);
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}

	ctx = gene_request_ctx();
	if (ctx && (ctx->http_busy || ctx->http_body_buf)) {
		zend_throw_exception_ex(NULL, 0, "Nested Gene\\Http::request is not supported");
		if (have_multipart_form) zval_ptr_dtor(&multipart_form);
		if (own_body && body) zend_string_release(body);
		if (owned_url) zend_string_release(owned_url);
		RETURN_THROWS();
	}
	if (ctx) {
		ctx->http_busy = 1;
	}
	ZVAL_UNDEF(&saved_stream);
	ZVAL_UNDEF(&saved_sse);
	if (zstream && Z_TYPE_P(zstream) != IS_NULL && Z_TYPE_P(zstream) != IS_UNDEF && ctx) {
		had_stream = 1;
		ZVAL_COPY_VALUE(&saved_stream, &ctx->http_stream_cb);
		ZVAL_UNDEF(&ctx->http_stream_cb);
		ZVAL_COPY(&ctx->http_stream_cb, zstream);
	}
	if (zsse && Z_TYPE_P(zsse) != IS_NULL && Z_TYPE_P(zsse) != IS_UNDEF && ctx) {
		had_sse = 1;
		ZVAL_COPY_VALUE(&saved_sse, &ctx->http_sse_cb);
		ZVAL_UNDEF(&ctx->http_sse_cb);
		ZVAL_COPY(&ctx->http_sse_cb, zsse);
		sse_active = 1;
	}
	if (zsse_forward && zend_is_true(zsse_forward) && ctx) {
		ctx->http_sse_forward = 1;
		sse_active = 1;
	}
	if (zdiscard && zend_is_true(zdiscard) && ctx) {
		ctx->http_discard_body = 1;
	}
	if (sse_active && ctx) {
		ctx->http_sse_leftover = &sse_leftover;
		ctx->http_sse_done = 0;
	}

	use_swoole = (GENE_G(runtime_type) >= 2);

	for (attempt = 0; attempt <= retry; attempt++) {
		int rc;
		int can_retry;
		ZVAL_UNDEF(&headers_out);
		status = 0;
		resp_body = NULL;
		err = NULL;

		if (use_swoole) {
			zval hdrs_assoc;
			array_init(&hdrs_assoc);
			if (zheaders && Z_TYPE_P(zheaders) == IS_ARRAY) {
				zend_hash_copy(Z_ARRVAL(hdrs_assoc), Z_ARRVAL_P(zheaders), (copy_ctor_func_t) zval_add_ref);
			}
			if (auto_content_type && !gene_http_headers_have_content_type(&hdrs_assoc)) {
				add_assoc_string_ex(&hdrs_assoc, ZEND_STRL("Content-Type"), (char *)auto_content_type);
			}
			rc = gene_http_swoole_once(method, url, &hdrs_assoc, body, have_files ? zfiles : NULL,
				have_multipart_form ? &multipart_form : NULL,
				timeout, connect_timeout,
				ssl_verify, keep_alive, &status, &headers_out, &resp_body, &err);
			zval_ptr_dtor(&hdrs_assoc);
			if (rc == SUCCESS && resp_body && ctx && ZSTR_LEN(resp_body) > 0
				&& (gene_http_has_sse(ctx) || Z_TYPE(ctx->http_stream_cb) != IS_UNDEF)) {
				gene_http_invoke_stream_body(ctx, resp_body);
			}
		} else {
			zval ch, zurlv, zto, zcto, zsslpeer, zsslhost, zfollow, zmaxr, zmethodv, zbodyv;
			if (gene_http_ensure_curl(&ch) != SUCCESS) {
				if (have_multipart_form) zval_ptr_dtor(&multipart_form);
				if (own_body && body) zend_string_release(body);
				if (owned_url) zend_string_release(owned_url);
				smart_str_free(&sse_leftover);
				gene_http_restore_state(ctx, &saved_stream, &saved_sse, had_stream, had_sse);
				RETURN_THROWS();
			}
			ZVAL_STR(&zurlv, url);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_URL"), &zurlv);
			ZVAL_STRING(&zmethodv, method);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_CUSTOMREQUEST"), &zmethodv);
			zval_ptr_dtor(&zmethodv);
			ZVAL_DOUBLE(&zto, timeout);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_TIMEOUT"), &zto);
			ZVAL_DOUBLE(&zcto, connect_timeout);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_CONNECTTIMEOUT"), &zcto);
			ZVAL_BOOL(&zsslpeer, ssl_verify);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_SSL_VERIFYPEER"), &zsslpeer);
			ZVAL_LONG(&zsslhost, ssl_verify ? 2 : 0);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_SSL_VERIFYHOST"), &zsslhost);
			ZVAL_TRUE(&zfollow);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_FOLLOWLOCATION"), &zfollow);
			ZVAL_LONG(&zmaxr, 5);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_MAXREDIRS"), &zmaxr);

			gene_http_build_curl_headers(zheaders, &curl_headers, auto_content_type);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_HTTPHEADER"), &curl_headers);
			zval_ptr_dtor(&curl_headers);

			if (have_files) {
				zval mp;
				if (gene_http_build_multipart(zfiles, have_multipart_form ? &multipart_form : NULL, &mp) != SUCCESS) {
					zval_ptr_dtor(&ch);
					if (have_multipart_form) zval_ptr_dtor(&multipart_form);
					if (own_body && body) zend_string_release(body);
					if (owned_url) zend_string_release(owned_url);
					smart_str_free(&sse_leftover);
					gene_http_restore_state(ctx, &saved_stream, &saved_sse, had_stream, had_sse);
					RETURN_THROWS();
				}
				gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_POSTFIELDS"), &mp);
				zval_ptr_dtor(&mp);
			} else if (body && ZSTR_LEN(body) > 0) {
				ZVAL_STR(&zbodyv, body);
				gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_POSTFIELDS"), &zbodyv);
			}
			rc = gene_http_curl_exec_once(&ch, &status, &headers_out, &resp_body, &err);
			zval_ptr_dtor(&ch);
		}

		can_retry = (attempt < retry) && gene_http_method_retryable(method)
			&& (status >= 500 || status <= 0);
		if (can_retry) {
			if (resp_body) {
				zend_string_release(resp_body);
				resp_body = NULL;
			}
			if (Z_TYPE(headers_out) != IS_UNDEF) {
				zval_ptr_dtor(&headers_out);
				ZVAL_UNDEF(&headers_out);
			}
			if (err) {
				efree(err);
				err = NULL;
			}
			/* exponential backoff: 100ms, 200ms, 400ms */
			gene_http_sleep_us(100000L << attempt);
			continue;
		}
		break;
	}

	if (had_stream && ctx) {
		zval_ptr_dtor(&ctx->http_stream_cb);
		ZVAL_COPY_VALUE(&ctx->http_stream_cb, &saved_stream);
	}
	if (had_sse && ctx) {
		zval_ptr_dtor(&ctx->http_sse_cb);
		ZVAL_COPY_VALUE(&ctx->http_sse_cb, &saved_sse);
	}
	if (ctx) {
		ctx->http_sse_leftover = NULL;
		ctx->http_sse_forward = 0;
	}
	smart_str_free(&sse_leftover);

	if (have_multipart_form) {
		zval_ptr_dtor(&multipart_form);
	}
	if (own_body && body) {
		zend_string_release(body);
	}
	if (owned_url) {
		zend_string_release(owned_url);
	}

	if (status <= 0 && err) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http request failed: %s", err);
		efree(err);
		if (resp_body) zend_string_release(resp_body);
		if (Z_TYPE(headers_out) != IS_UNDEF) zval_ptr_dtor(&headers_out);
		if (ctx) {
			ctx->http_sse_done = 0;
			ctx->http_discard_body = 0;
			ctx->http_busy = 0;
		}
		RETURN_THROWS();
	}
	if (err) {
		efree(err);
	}
	gene_http_fill_result(status, &headers_out, resp_body, return_value);
	if (Z_TYPE(headers_out) != IS_UNDEF) {
		zval_ptr_dtor(&headers_out);
	}
	if (resp_body) {
		zend_string_release(resp_body);
	}
	if (ctx) {
		ctx->http_sse_done = 0;
		ctx->http_discard_body = 0;
		ctx->http_busy = 0;
	}
}
/* }}} */

static void gene_http_fail_item(zval *out, const char *err) {
	zval empty_h;
	array_init(out);
	add_assoc_long(out, "status", 0);
	add_assoc_string(out, "error", err ? err : "request failed");
	array_init(&empty_h);
	add_assoc_zval(out, "headers", &empty_h);
	add_assoc_string(out, "body", "");
}

typedef struct _gene_http_mslot {
	zval ch;
	zval postfields;
	zval out;
	zend_string *req_body;
	zend_bool own_body;
	zend_bool skip;
	zend_bool added;
} gene_http_mslot;

static int gene_http_fn_exists(const char *name, size_t len) {
	return zend_hash_str_exists(CG(function_table), name, len);
}

static int gene_http_is_old_swoole_curl_handler(zval *ch) {
	zend_string *n;
	if (!ch || Z_TYPE_P(ch) != IS_OBJECT || !Z_OBJCE_P(ch) || !Z_OBJCE_P(ch)->name) {
		return 0;
	}
	n = Z_OBJCE_P(ch)->name;
	return zend_string_equals_literal(n, "Swoole\\Curl\\Handler")
		|| zend_string_equals_literal(n, "swoole\\curl\\handler");
}

static zend_long gene_http_swoole_hook_flags(void) {
	zend_class_entry *ce;
	zend_function *fn;
	zval ret;
	zend_long v;

	ce = gene_lookup_class_str(ZEND_STRL("Swoole\\Runtime"));
	if (!ce) {
		return -1;
	}
	fn = zend_hash_str_find_ptr(&ce->function_table, ZEND_STRL("gethookflags"));
	if (!fn) {
		return -1;
	}
	ZVAL_UNDEF(&ret);
	zend_call_known_function(fn, NULL, ce, &ret, 0, NULL, NULL);
	if (EG(exception)) {
		zend_clear_exception();
		zval_ptr_dtor(&ret);
		return -1;
	}
	v = (Z_TYPE(ret) == IS_LONG) ? Z_LVAL(ret) : -1;
	zval_ptr_dtor(&ret);
	return v;
}

static int gene_http_curl_multi_usable(void) {
	zval ch;

	if (!gene_http_fn_exists(ZEND_STRL("curl_init"))
		|| !gene_http_fn_exists(ZEND_STRL("curl_multi_init"))) {
		return 0;
	}
	if (gene_http_php_call("curl_init", sizeof("curl_init") - 1, 0, NULL, &ch) != SUCCESS) {
		zval_ptr_dtor(&ch);
		return 0;
	}
	if (gene_http_is_old_swoole_curl_handler(&ch)
		|| (Z_TYPE(ch) != IS_OBJECT && Z_TYPE(ch) != IS_RESOURCE)) {
		zval_ptr_dtor(&ch);
		return 0;
	}
	zval_ptr_dtor(&ch);
	if (GENE_G(runtime_type) >= 2) {
		zend_long flags = gene_http_swoole_hook_flags();
		zend_long native = gene_http_const(ZEND_STRL("SWOOLE_HOOK_NATIVE_CURL"));
		if (flags < 0 || native == 0 || (flags & native) == 0) {
			return 0;
		}
	}
	return 1;
}

static zend_long gene_http_curl_info_long(zval *ch, const char *cname, size_t clen) {
	zval opt, params[2], info;
	zend_long code = gene_http_const(cname, clen);
	zend_long v = 0;

	ZVAL_LONG(&opt, code);
	ZVAL_COPY_VALUE(&params[0], ch);
	params[1] = opt;
	if (gene_http_php_call("curl_getinfo", sizeof("curl_getinfo") - 1, 2, params, &info) == SUCCESS
		&& Z_TYPE(info) == IS_LONG) {
		v = Z_LVAL(info);
	}
	zval_ptr_dtor(&info);
	return v;
}

static int gene_http_curl_multi_exec(zval *mh, zend_long *still) {
	zval params[2], ret, tmp;

	ZVAL_COPY(&params[0], mh);
	ZVAL_LONG(&tmp, *still);
	ZVAL_NEW_REF(&params[1], &tmp);
	if (gene_http_php_call("curl_multi_exec", sizeof("curl_multi_exec") - 1, 2, params, &ret) != SUCCESS) {
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&ret);
		return FAILURE;
	}
	if (Z_ISREF(params[1]) && Z_TYPE_P(Z_REFVAL(params[1])) == IS_LONG) {
		*still = Z_LVAL_P(Z_REFVAL(params[1]));
	}
	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	zval_ptr_dtor(&ret);
	return SUCCESS;
}

static void gene_http_mslot_release_handles(gene_http_mslot *s) {
	if (Z_TYPE(s->ch) != IS_UNDEF) {
		zval_ptr_dtor(&s->ch);
		ZVAL_UNDEF(&s->ch);
	}
	if (Z_TYPE(s->postfields) != IS_UNDEF) {
		zval_ptr_dtor(&s->postfields);
		ZVAL_UNDEF(&s->postfields);
	}
	if (s->own_body && s->req_body) {
		zend_string_release(s->req_body);
		s->req_body = NULL;
		s->own_body = 0;
	}
}

static int gene_http_multi_prepare_easy(zval *opts, gene_http_mslot *s) {
	zval *zurl, *zmethod, *zheaders, *zjson, *zbody, *ztimeout, *zct, *zssl, *zfiles;
	zend_string *url;
	char method_buf[16];
	const char *method = "GET";
	zval curl_headers, ztrue, zurlv, zto, zcto, zsslpeer, zsslhost, zfollow, zmaxr, zmethodv, zbodyv;
	zend_bool have_json = 0, ssl_verify = 1;
	double timeout = 60.0, connect_timeout = 3.0;

	ZVAL_UNDEF(&s->ch);
	ZVAL_UNDEF(&s->postfields);
	ZVAL_UNDEF(&s->out);
	s->req_body = NULL;
	s->own_body = 0;
	s->skip = 0;
	s->added = 0;

	if (!opts || Z_TYPE_P(opts) != IS_ARRAY) {
		gene_http_fail_item(&s->out, "invalid request options");
		s->skip = 1;
		return SUCCESS;
	}
	if (gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("stream"))
		|| gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("sse"))) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::multi does not support stream or sse options");
		return FAILURE;
	}
	zurl = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("url"));
	if (!zurl || Z_TYPE_P(zurl) != IS_STRING || Z_STRLEN_P(zurl) == 0) {
		gene_http_fail_item(&s->out, "url required");
		s->skip = 1;
		return SUCCESS;
	}
	url = Z_STR_P(zurl);
	zmethod = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("method"));
	if (zmethod && Z_TYPE_P(zmethod) == IS_STRING && Z_STRLEN_P(zmethod) > 0 && Z_STRLEN_P(zmethod) < sizeof(method_buf)) {
		size_t i;
		memcpy(method_buf, Z_STRVAL_P(zmethod), Z_STRLEN_P(zmethod) + 1);
		for (i = 0; i < Z_STRLEN_P(zmethod); i++) {
			unsigned char c = (unsigned char)method_buf[i];
			if (c >= 'a' && c <= 'z') {
				method_buf[i] = (char)(c - 32);
			}
		}
		method = method_buf;
	}
	zjson = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("json"));
	zbody = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("body"));
	zfiles = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("files"));
	if (zjson) {
		zval encoded;
		if (gene_json_encode_throw(zjson, &encoded) != SUCCESS) {
			return FAILURE;
		}
		s->req_body = Z_STR(encoded);
		s->own_body = 1;
		have_json = 1;
	} else if (zbody && Z_TYPE_P(zbody) == IS_STRING) {
		s->req_body = zval_get_string(zbody);
		s->own_body = 1;
	}
	zheaders = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("headers"));
	ztimeout = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("timeout"));
	if (ztimeout) {
		timeout = zval_get_double(ztimeout);
	}
	zct = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("connect_timeout"));
	if (zct) {
		connect_timeout = zval_get_double(zct);
	}
	zssl = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("ssl_verify"));
	if (zssl) {
		ssl_verify = zend_is_true(zssl);
	}

	if (gene_http_php_call("curl_init", sizeof("curl_init") - 1, 0, NULL, &s->ch) != SUCCESS
		|| (Z_TYPE(s->ch) != IS_OBJECT && Z_TYPE(s->ch) != IS_RESOURCE)) {
		zval_ptr_dtor(&s->ch);
		ZVAL_UNDEF(&s->ch);
		if (s->own_body && s->req_body) {
			zend_string_release(s->req_body);
			s->req_body = NULL;
			s->own_body = 0;
		}
		gene_http_fail_item(&s->out, "curl_init failed");
		s->skip = 1;
		return SUCCESS;
	}

	ZVAL_STR(&zurlv, url);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_URL"), &zurlv);
	ZVAL_STRING(&zmethodv, method);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_CUSTOMREQUEST"), &zmethodv);
	zval_ptr_dtor(&zmethodv);
	ZVAL_DOUBLE(&zto, timeout);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_TIMEOUT"), &zto);
	ZVAL_DOUBLE(&zcto, connect_timeout);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_CONNECTTIMEOUT"), &zcto);
	ZVAL_BOOL(&zsslpeer, ssl_verify);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_SSL_VERIFYPEER"), &zsslpeer);
	ZVAL_LONG(&zsslhost, ssl_verify ? 2 : 0);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_SSL_VERIFYHOST"), &zsslhost);
	ZVAL_TRUE(&zfollow);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_FOLLOWLOCATION"), &zfollow);
	ZVAL_LONG(&zmaxr, 5);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_MAXREDIRS"), &zmaxr);
	ZVAL_TRUE(&ztrue);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_RETURNTRANSFER"), &ztrue);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_HEADER"), &ztrue);
	gene_http_build_curl_headers(zheaders, &curl_headers,
		have_json ? "application/json; charset=UTF-8" : NULL);
	gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_HTTPHEADER"), &curl_headers);
	zval_ptr_dtor(&curl_headers);

	if (zfiles && Z_TYPE_P(zfiles) == IS_ARRAY) {
		if (gene_http_build_multipart(zfiles,
			(zbody && Z_TYPE_P(zbody) == IS_ARRAY) ? zbody : NULL, &s->postfields) != SUCCESS) {
			gene_http_mslot_release_handles(s);
			return FAILURE;
		}
		gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_POSTFIELDS"), &s->postfields);
	} else if (s->req_body && ZSTR_LEN(s->req_body) > 0) {
		ZVAL_STR(&zbodyv, s->req_body);
		gene_http_curl_setopt(&s->ch, ZEND_STRL("CURLOPT_POSTFIELDS"), &zbodyv);
	}
	return SUCCESS;
}

static void gene_http_multi_harvest(gene_http_mslot *s) {
	zend_long status, hsz;
	zval raw, headers, err;
	zend_string *body;

	if (s->skip || Z_TYPE(s->out) != IS_UNDEF) {
		return;
	}
	status = gene_http_curl_info_long(&s->ch, ZEND_STRL("CURLINFO_RESPONSE_CODE"));
	if (status == 0) {
		status = gene_http_curl_info_long(&s->ch, ZEND_STRL("CURLINFO_HTTP_CODE"));
	}
	if (gene_http_php_call("curl_multi_getcontent", sizeof("curl_multi_getcontent") - 1, 1, &s->ch, &raw) != SUCCESS
		|| Z_TYPE(raw) != IS_STRING) {
		char *emsg = NULL;
		zval_ptr_dtor(&raw);
		if (gene_http_php_call("curl_error", sizeof("curl_error") - 1, 1, &s->ch, &err) == SUCCESS
			&& Z_TYPE(err) == IS_STRING && Z_STRLEN(err) > 0) {
			emsg = Z_STRVAL(err);
		}
		gene_http_fail_item(&s->out, emsg ? emsg : "request failed");
		zval_ptr_dtor(&err);
		return;
	}
	if (status <= 0) {
		char *emsg = NULL;
		if (gene_http_php_call("curl_error", sizeof("curl_error") - 1, 1, &s->ch, &err) == SUCCESS
			&& Z_TYPE(err) == IS_STRING && Z_STRLEN(err) > 0) {
			emsg = Z_STRVAL(err);
		}
		gene_http_fail_item(&s->out, emsg ? emsg : "request failed");
		zval_ptr_dtor(&err);
		zval_ptr_dtor(&raw);
		return;
	}
	hsz = gene_http_curl_info_long(&s->ch, ZEND_STRL("CURLINFO_HEADER_SIZE"));
	if (hsz < 0) {
		hsz = 0;
	}
	if ((size_t)hsz > Z_STRLEN(raw)) {
		hsz = (zend_long)Z_STRLEN(raw);
	}
	gene_http_parse_headers(Z_STRVAL(raw), (size_t)hsz, &headers);
	body = zend_string_init(Z_STRVAL(raw) + hsz, Z_STRLEN(raw) - (size_t)hsz, 0);
	gene_http_fill_result(status, &headers, body, &s->out);
	zend_string_release(body);
	zval_ptr_dtor(&headers);
	zval_ptr_dtor(&raw);
}

static void gene_http_multi_cleanup(zval *mh, gene_http_mslot *slots, uint32_t n) {
	uint32_t i;
	for (i = 0; i < n; i++) {
		if (slots[i].added && mh && Z_TYPE_P(mh) != IS_UNDEF) {
			zval params[2], ret;
			ZVAL_COPY(&params[0], mh);
			ZVAL_COPY(&params[1], &slots[i].ch);
			gene_http_php_call("curl_multi_remove_handle", sizeof("curl_multi_remove_handle") - 1, 2, params, &ret);
			zval_ptr_dtor(&params[0]);
			zval_ptr_dtor(&params[1]);
			zval_ptr_dtor(&ret);
			slots[i].added = 0;
			if (EG(exception)) {
				zend_clear_exception();
			}
		}
		gene_http_mslot_release_handles(&slots[i]);
		if (Z_TYPE(slots[i].out) != IS_UNDEF) {
			zval_ptr_dtor(&slots[i].out);
			ZVAL_UNDEF(&slots[i].out);
		}
	}
	if (mh && Z_TYPE_P(mh) != IS_UNDEF) {
		zval ret;
		gene_http_php_call("curl_multi_close", sizeof("curl_multi_close") - 1, 1, mh, &ret);
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(mh);
		ZVAL_UNDEF(mh);
		if (EG(exception)) {
			zend_clear_exception();
		}
	}
}

static int gene_http_multi_curl(zval *requests, zval *return_value, zend_long concurrency) {
	uint32_t n, i;
	gene_http_mslot *slots;
	zval mh, *item;
	zend_long still = 1;

	n = zend_hash_num_elements(Z_ARRVAL_P(requests));
	slots = ecalloc(n, sizeof(gene_http_mslot));
	i = 0;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(requests), item) {
		if (gene_http_multi_prepare_easy(item, &slots[i]) != SUCCESS) {
			gene_http_multi_cleanup(NULL, slots, i + 1);
			efree(slots);
			return FAILURE;
		}
		i++;
	} ZEND_HASH_FOREACH_END();

	if (gene_http_php_call("curl_multi_init", sizeof("curl_multi_init") - 1, 0, NULL, &mh) != SUCCESS
		|| (Z_TYPE(mh) != IS_OBJECT && Z_TYPE(mh) != IS_RESOURCE)) {
		gene_http_multi_cleanup(NULL, slots, n);
		zval_ptr_dtor(&mh);
		efree(slots);
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::multi curl_multi_init failed");
		return FAILURE;
	}
	{
		zend_long opt = gene_http_const(ZEND_STRL("CURLMOPT_MAX_TOTAL_CONNECTIONS"));
		if (opt != 0) {
			zval params[3], ret, zopt, zc;
			ZVAL_COPY_VALUE(&params[0], &mh);
			ZVAL_LONG(&zopt, opt);
			ZVAL_LONG(&zc, concurrency);
			params[1] = zopt;
			params[2] = zc;
			gene_http_php_call("curl_multi_setopt", sizeof("curl_multi_setopt") - 1, 3, params, &ret);
			zval_ptr_dtor(&ret);
			if (EG(exception)) {
				zend_clear_exception();
			}
		}
	}
	for (i = 0; i < n; i++) {
		if (slots[i].skip) {
			continue;
		}
		{
			zval params[2], ret;
			ZVAL_COPY(&params[0], &mh);
			ZVAL_COPY(&params[1], &slots[i].ch);
			if (gene_http_php_call("curl_multi_add_handle", sizeof("curl_multi_add_handle") - 1, 2, params, &ret) != SUCCESS) {
				zval_ptr_dtor(&params[0]);
				zval_ptr_dtor(&params[1]);
				zval_ptr_dtor(&ret);
				gene_http_multi_cleanup(&mh, slots, n);
				efree(slots);
				return FAILURE;
			}
			zval_ptr_dtor(&params[0]);
			zval_ptr_dtor(&params[1]);
			zval_ptr_dtor(&ret);
			slots[i].added = 1;
		}
	}

	while (still) {
		zval sel, tmo, sparams[2];
		if (gene_http_curl_multi_exec(&mh, &still) != SUCCESS) {
			gene_http_multi_cleanup(&mh, slots, n);
			efree(slots);
			return FAILURE;
		}
		if (!still) {
			break;
		}
		ZVAL_COPY(&sparams[0], &mh);
		ZVAL_DOUBLE(&tmo, 1.0);
		sparams[1] = tmo;
		if (gene_http_php_call("curl_multi_select", sizeof("curl_multi_select") - 1, 2, sparams, &sel) != SUCCESS) {
			zval_ptr_dtor(&sparams[0]);
			zval_ptr_dtor(&sel);
			gene_http_multi_cleanup(&mh, slots, n);
			efree(slots);
			return FAILURE;
		}
		if (Z_TYPE(sel) == IS_LONG && Z_LVAL(sel) < 0) {
			gene_http_sleep_us(10000);
		}
		zval_ptr_dtor(&sparams[0]);
		zval_ptr_dtor(&sel);
	}

	for (i = 0; i < n; i++) {
		if (!slots[i].skip) {
			gene_http_multi_harvest(&slots[i]);
		}
		if (slots[i].added) {
			zval params[2], ret;
			ZVAL_COPY(&params[0], &mh);
			ZVAL_COPY(&params[1], &slots[i].ch);
			gene_http_php_call("curl_multi_remove_handle", sizeof("curl_multi_remove_handle") - 1, 2, params, &ret);
			zval_ptr_dtor(&params[0]);
			zval_ptr_dtor(&params[1]);
			zval_ptr_dtor(&ret);
			slots[i].added = 0;
			if (EG(exception)) {
				zend_clear_exception();
			}
		}
		gene_http_mslot_release_handles(&slots[i]);
		if (Z_TYPE(slots[i].out) == IS_UNDEF) {
			gene_http_fail_item(&slots[i].out, "request failed");
		}
		add_next_index_zval(return_value, &slots[i].out);
		ZVAL_UNDEF(&slots[i].out);
	}
	{
		zval ret;
		gene_http_php_call("curl_multi_close", sizeof("curl_multi_close") - 1, 1, &mh, &ret);
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&mh);
	}
	efree(slots);
	return SUCCESS;
}

static int gene_http_multi_one_swoole(zval *item, zval *one) {
	zval hdrs_assoc, swoole_out;
	zend_string *url, *body = NULL;
	char method_buf[16];
	const char *method = "GET";
	zend_long status = 0;
	zend_string *resp_body = NULL;
	char *err = NULL;
	zend_bool own_body = 0, have_json = 0, ssl_verify = 1;
	double timeout = 60.0, connect_timeout = 3.0;
	zval *zurl, *zmethod, *zheaders, *zjson, *zbody, *ztimeout, *zct, *zssl, *zfiles;

	if (!item || Z_TYPE_P(item) != IS_ARRAY) {
		gene_http_fail_item(one, "invalid request options");
		return SUCCESS;
	}
	if (gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("stream"))
		|| gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("sse"))) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::multi does not support stream or sse options");
		return FAILURE;
	}
	zurl = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("url"));
	if (!zurl || Z_TYPE_P(zurl) != IS_STRING) {
		gene_http_fail_item(one, "url required");
		return SUCCESS;
	}
	url = Z_STR_P(zurl);
	zmethod = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("method"));
	if (zmethod && Z_TYPE_P(zmethod) == IS_STRING && Z_STRLEN_P(zmethod) > 0 && Z_STRLEN_P(zmethod) < sizeof(method_buf)) {
		size_t i;
		memcpy(method_buf, Z_STRVAL_P(zmethod), Z_STRLEN_P(zmethod) + 1);
		for (i = 0; i < Z_STRLEN_P(zmethod); i++) {
			unsigned char c = (unsigned char)method_buf[i];
			if (c >= 'a' && c <= 'z') method_buf[i] = (char)(c - 32);
		}
		method = method_buf;
	}
	zjson = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("json"));
	zbody = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("body"));
	zfiles = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("files"));
	if (zjson) {
		zval encoded;
		if (gene_json_encode_throw(zjson, &encoded) != SUCCESS) {
			return FAILURE;
		}
		body = Z_STR(encoded);
		own_body = 1;
		have_json = 1;
	} else if (zbody) {
		if (!(zfiles && Z_TYPE_P(zfiles) == IS_ARRAY && Z_TYPE_P(zbody) == IS_ARRAY)) {
			body = zval_get_string(zbody);
			own_body = 1;
		}
	}
	zheaders = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("headers"));
	ztimeout = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("timeout"));
	if (ztimeout) timeout = zval_get_double(ztimeout);
	zct = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("connect_timeout"));
	if (zct) connect_timeout = zval_get_double(zct);
	zssl = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("ssl_verify"));
	if (zssl) ssl_verify = zend_is_true(zssl);
	array_init(&hdrs_assoc);
	if (zheaders && Z_TYPE_P(zheaders) == IS_ARRAY) {
		zend_hash_copy(Z_ARRVAL(hdrs_assoc), Z_ARRVAL_P(zheaders), (copy_ctor_func_t) zval_add_ref);
	}
	if (have_json && !gene_http_headers_have_content_type(&hdrs_assoc)) {
		add_assoc_string_ex(&hdrs_assoc, ZEND_STRL("Content-Type"), "application/json; charset=UTF-8");
	}
	ZVAL_UNDEF(&swoole_out);
	if (gene_http_swoole_once(method, url, &hdrs_assoc, body, zfiles,
		(zfiles && zbody && Z_TYPE_P(zbody) == IS_ARRAY) ? zbody : NULL,
		timeout, connect_timeout, ssl_verify, 0, &status, &swoole_out, &resp_body, &err) != SUCCESS
		|| status <= 0) {
		gene_http_fail_item(one, err ? err : "request failed");
		if (Z_TYPE(swoole_out) != IS_UNDEF) zval_ptr_dtor(&swoole_out);
	} else {
		gene_http_fill_result(status, &swoole_out, resp_body, one);
		if (Z_TYPE(swoole_out) != IS_UNDEF) zval_ptr_dtor(&swoole_out);
	}
	if (err) efree(err);
	if (resp_body) zend_string_release(resp_body);
	if (own_body && body) zend_string_release(body);
	zval_ptr_dtor(&hdrs_assoc);
	return SUCCESS;
}

/* {{{ proto static array Gene\Http::multi(array $requests [, array $options = []]) */
PHP_METHOD(gene_http, multi) {
	zval *requests, *options = NULL;
	zend_long concurrency = 8;
	gene_request_context *ctx;
	uint32_t n;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|a!", &requests, &options) == FAILURE) {
		return;
	}
	if (options && Z_TYPE_P(options) == IS_ARRAY) {
		zval *zc = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("concurrency"));
		if (zc) {
			concurrency = zval_get_long(zc);
			if (concurrency < 1) concurrency = 1;
			if (concurrency > 16) concurrency = 16;
		}
	}
	n = zend_hash_num_elements(Z_ARRVAL_P(requests));
	if (n > GENE_HTTP_MULTI_MAX) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::multi accepts at most %d requests", GENE_HTTP_MULTI_MAX);
		RETURN_THROWS();
	}
	ctx = gene_request_ctx();
	if (ctx && ctx->http_busy) {
		zend_throw_exception_ex(NULL, 0, "Nested Gene\\Http::multi is not supported");
		RETURN_THROWS();
	}
	if (ctx) ctx->http_busy = 1;

	array_init(return_value);
	if (n == 0) {
		if (ctx) ctx->http_busy = 0;
		return;
	}

	if (gene_http_curl_multi_usable()) {
		if (gene_http_multi_curl(requests, return_value, concurrency) != SUCCESS) {
			zval_ptr_dtor(return_value);
			ZVAL_UNDEF(return_value);
			if (ctx) ctx->http_busy = 0;
			RETURN_THROWS();
		}
	} else if (GENE_G(runtime_type) >= 2) {
		zval *item;
		php_error_docref(NULL, E_NOTICE,
			"Gene\\Http::multi runs sequentially on Swoole without Native CURL hook; "
			"compile Swoole with --enable-swoole-curl and enableCoroutine(SWOOLE_HOOK_ALL) for parallel curl_multi");
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(requests), item) {
			zval one;
			ZVAL_UNDEF(&one);
			if (gene_http_multi_one_swoole(item, &one) != SUCCESS) {
				if (Z_TYPE(one) != IS_UNDEF) {
					zval_ptr_dtor(&one);
				}
				zval_ptr_dtor(return_value);
				ZVAL_UNDEF(return_value);
				if (ctx) ctx->http_busy = 0;
				RETURN_THROWS();
			}
			add_next_index_zval(return_value, &one);
		} ZEND_HASH_FOREACH_END();
	} else {
		zval_ptr_dtor(return_value);
		ZVAL_UNDEF(return_value);
		if (ctx) ctx->http_busy = 0;
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Http::multi requires the curl extension (curl_multi_init)");
		RETURN_THROWS();
	}
	if (ctx) ctx->http_busy = 0;
}
/* }}} */

#if 0
	ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL_P(requests), idx, item) {
		zval one;
		(void)idx;
		(void)concurrency;
		if (Z_TYPE_P(item) != IS_ARRAY) {
			array_init(&one);
			add_assoc_long(&one, "status", 0);
			add_assoc_string(&one, "error", "invalid request options");
			add_assoc_string(&one, "body", "");
			{
				zval empty_h;
				array_init(&empty_h);
				add_assoc_zval(&one, "headers", &empty_h);
			}
			add_next_index_zval(return_value, &one);
			continue;
		}
		if (GENE_G(runtime_type) >= 2) {
			zval hdrs_assoc, swoole_out;
			zend_string *url, *body = NULL;
			char method_buf[16];
			const char *method = "GET";
			zend_long status = 0;
			zend_string *resp_body = NULL;
			char *err = NULL;
			zend_bool own_body = 0, have_json = 0, ssl_verify = 1;
			double timeout = 60.0, connect_timeout = 3.0;
			zval *zurl, *zmethod, *zheaders, *zjson, *zbody, *ztimeout, *zct, *zssl, *zfiles;

			if (gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("stream"))
				|| gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("sse"))) {
				zend_throw_exception_ex(NULL, 0, "Gene\\Http::multi does not support stream or sse options");
				if (ctx) ctx->http_busy = 0;
				RETURN_THROWS();
			}
			zurl = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("url"));
			if (!zurl || Z_TYPE_P(zurl) != IS_STRING) {
				array_init(&one);
				add_assoc_long(&one, "status", 0);
				add_assoc_string(&one, "error", "url required");
				add_assoc_string(&one, "body", "");
				{
					zval empty_h;
					array_init(&empty_h);
					add_assoc_zval(&one, "headers", &empty_h);
				}
				add_next_index_zval(return_value, &one);
				continue;
			}
			url = Z_STR_P(zurl);
			zmethod = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("method"));
			if (zmethod && Z_TYPE_P(zmethod) == IS_STRING && Z_STRLEN_P(zmethod) > 0 && Z_STRLEN_P(zmethod) < sizeof(method_buf)) {
				size_t i;
				memcpy(method_buf, Z_STRVAL_P(zmethod), Z_STRLEN_P(zmethod) + 1);
				for (i = 0; i < Z_STRLEN_P(zmethod); i++) {
					unsigned char c = (unsigned char)method_buf[i];
					if (c >= 'a' && c <= 'z') method_buf[i] = (char)(c - 32);
				}
				method = method_buf;
			}
			zjson = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("json"));
			zbody = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("body"));
			zfiles = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("files"));
			if (zjson) {
				zval encoded;
				if (gene_json_encode_throw(zjson, &encoded) != SUCCESS) {
					if (ctx) ctx->http_busy = 0;
					RETURN_THROWS();
				}
				body = Z_STR(encoded);
				own_body = 1;
				have_json = 1;
			} else if (zbody) {
				body = zval_get_string(zbody);
				own_body = 1;
			}
			zheaders = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("headers"));
			ztimeout = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("timeout"));
			if (ztimeout) timeout = zval_get_double(ztimeout);
			zct = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("connect_timeout"));
			if (zct) connect_timeout = zval_get_double(zct);
			zssl = gene_http_opt(Z_ARRVAL_P(item), ZEND_STRL("ssl_verify"));
			if (zssl) ssl_verify = zend_is_true(zssl);
			array_init(&hdrs_assoc);
			if (zheaders && Z_TYPE_P(zheaders) == IS_ARRAY) {
				zend_hash_copy(Z_ARRVAL(hdrs_assoc), Z_ARRVAL_P(zheaders), (copy_ctor_func_t) zval_add_ref);
			}
			if (have_json) {
				add_assoc_string_ex(&hdrs_assoc, ZEND_STRL("Content-Type"), "application/json; charset=UTF-8");
			}
			ZVAL_UNDEF(&swoole_out);
			if (gene_http_swoole_once(method, url, &hdrs_assoc, body, zfiles,
				(zfiles && zbody && Z_TYPE_P(zbody) == IS_ARRAY) ? zbody : NULL,
				timeout, connect_timeout, ssl_verify, 0, &status, &swoole_out, &resp_body, &err) != SUCCESS
				|| status <= 0) {
				array_init(&one);
				add_assoc_long(&one, "status", 0);
				add_assoc_string(&one, "error", err ? err : "request failed");
				add_assoc_string(&one, "body", "");
				{
					zval empty_h;
					array_init(&empty_h);
					add_assoc_zval(&one, "headers", &empty_h);
				}
				if (Z_TYPE(swoole_out) != IS_UNDEF) zval_ptr_dtor(&swoole_out);
			} else {
				gene_http_fill_result(status, &swoole_out, resp_body, &one);
				if (Z_TYPE(swoole_out) != IS_UNDEF) zval_ptr_dtor(&swoole_out);
			}
			add_next_index_zval(return_value, &one);
			if (err) efree(err);
			if (resp_body) zend_string_release(resp_body);
			if (own_body && body) zend_string_release(body);
			zval_ptr_dtor(&hdrs_assoc);
		} else {
			if (gene_http_multi_one_fpm(item, &one) != SUCCESS) {
				if (EG(exception)) {
					if (ctx) ctx->http_busy = 0;
					RETURN_THROWS();
				}
				array_init(&one);
				add_assoc_long(&one, "status", 0);
				add_assoc_string(&one, "error", "request failed");
				add_assoc_string(&one, "body", "");
				{
					zval empty_h;
					array_init(&empty_h);
					add_assoc_zval(&one, "headers", &empty_h);
				}
			}
			add_next_index_zval(return_value, &one);
		}
	} ZEND_HASH_FOREACH_END();
	if (ctx) ctx->http_busy = 0;
}
/* }}} */
#endif

const zend_function_entry gene_http_methods[] = {
	PHP_ME(gene_http, request, gene_http_request_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_http, multi, gene_http_multi_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_http, _writeFn, gene_http_write_fn_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_http, _headerFn, gene_http_write_fn_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(http) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Http", "Gene\\Http", gene_http_methods);
	gene_http_ce = zend_register_internal_class(&ce);
	gene_http_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
