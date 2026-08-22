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

#include "../gene.h"
#include "../common/common.h"
#include "../factory/factory.h"
#include "../http/json.h"
#include "../http/http.h"

zend_class_entry *gene_http_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_http_request_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_http_write_fn_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, ch)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

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
			/* swallow stream-callback exceptions so curl write continues? rethrow after */
		}
	}
	zval_ptr_dtor(&retval);
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
	if (ctx && ctx->http_body_buf) {
		smart_str_appendl((smart_str *)ctx->http_body_buf, Z_STRVAL_P(data), Z_STRLEN_P(data));
	}
	if (ctx && Z_TYPE(ctx->http_stream_cb) != IS_UNDEF) {
		gene_http_invoke_stream(Z_STR_P(data));
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
static void gene_http_build_curl_headers(zval *headers_in, zval *headers_out, int json_body) {
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
	if (json_body) {
		add_next_index_string(headers_out, "Content-Type: application/json; charset=UTF-8");
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

static int gene_http_swoole_once(const char *method, zend_string *url, zval *headers_in,
		zend_string *body, double timeout, double connect_timeout, zend_bool ssl_verify,
		zend_bool keep_alive, zend_long *status, zval *headers_out, zend_string **body_out, char **err_out) {
	zval parsed, cli, ctor_params, set_opts, set_params;
	zval *scheme, *host, *port, *path, *query;
	zend_bool ssl = 0;
	zend_long port_l = 80;
	zend_string *path_qs;
	zend_class_entry *ce;

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

	array_init(&ctor_params);
	Z_TRY_ADDREF_P(host);
	add_next_index_zval(&ctor_params, host);
	add_next_index_long(&ctor_params, port_l);
	add_next_index_bool(&ctor_params, ssl);
	ZVAL_UNDEF(&cli);
	if (!gene_factory("Swoole\\Coroutine\\Http\\Client", sizeof("Swoole\\Coroutine\\Http\\Client") - 1, &ctor_params, &cli)
		|| Z_TYPE(cli) != IS_OBJECT) {
		zval_ptr_dtor(&ctor_params);
		zval_ptr_dtor(&parsed);
		zend_string_release(path_qs);
		zend_throw_exception_ex(NULL, 0, "Gene\\Http: failed to create Swoole HTTP client");
		return FAILURE;
	}
	zval_ptr_dtor(&ctor_params);

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
	if (headers_in && Z_TYPE_P(headers_in) == IS_ARRAY) {
		zval hparams, dummy;
		array_init(&hparams);
		Z_TRY_ADDREF_P(headers_in);
		add_next_index_zval(&hparams, headers_in);
		gene_factory_call(&cli, "setheaders", sizeof("setheaders") - 1, &hparams, &dummy);
		zval_ptr_dtor(&dummy);
		zval_ptr_dtor(&hparams);
	}
	if (body && ZSTR_LEN(body) > 0) {
		zval dparams, dz, dummy;
		array_init(&dparams);
		ZVAL_STR_COPY(&dz, body);
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

	if (!keep_alive) {
		zval dummy;
		gene_factory_call(&cli, "close", sizeof("close") - 1, NULL, &dummy);
		zval_ptr_dtor(&dummy);
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
	zval *zurl, *zmethod, *zheaders, *zjson, *zbody, *ztimeout, *zct, *zssl, *zretry, *zstream, *zka;
	zend_string *url, *body = NULL;
	char method_buf[16];
	const char *method = "GET";
	zval curl_headers;
	zend_bool have_json = 0, ssl_verify = 1, keep_alive = 0, own_body = 0;
	double timeout = 60.0, connect_timeout = 3.0;
	zend_long retry = 0, attempt, status = 0;
	gene_request_context *ctx;
	zval saved_stream;
	zval headers_out;
	zend_string *resp_body = NULL;
	char *err = NULL;
	int use_swoole;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &opts) == FAILURE) {
		return;
	}

	zurl = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("url"));
	if (!zurl || Z_TYPE_P(zurl) != IS_STRING || Z_STRLEN_P(zurl) == 0) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request requires a non-empty url");
		RETURN_THROWS();
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
	if (zjson && zbody) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request: json and body are mutually exclusive");
		RETURN_THROWS();
	}
	if (zjson) {
		zval encoded;
		if (gene_json_encode_throw(zjson, &encoded) != SUCCESS) {
			RETURN_THROWS();
		}
		body = Z_STR(encoded);
		own_body = 1;
		have_json = 1;
	} else if (zbody) {
		body = zval_get_string(zbody);
		own_body = 1;
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

	ctx = gene_request_ctx();
	ZVAL_UNDEF(&saved_stream);
	zstream = gene_http_opt(Z_ARRVAL_P(opts), ZEND_STRL("stream"));
	if (zstream && Z_TYPE_P(zstream) != IS_NULL && Z_TYPE_P(zstream) != IS_UNDEF && ctx) {
		ZVAL_COPY_VALUE(&saved_stream, &ctx->http_stream_cb);
		ZVAL_UNDEF(&ctx->http_stream_cb);
		ZVAL_COPY(&ctx->http_stream_cb, zstream);
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
			ZVAL_UNDEF(&hdrs_assoc);
			if (zheaders && Z_TYPE_P(zheaders) == IS_ARRAY) {
				ZVAL_COPY(&hdrs_assoc, zheaders);
			} else {
				array_init(&hdrs_assoc);
			}
			if (have_json) {
				add_assoc_string_ex(&hdrs_assoc, ZEND_STRL("Content-Type"), "application/json; charset=UTF-8");
			}
			rc = gene_http_swoole_once(method, url, &hdrs_assoc, body, timeout, connect_timeout,
				ssl_verify, keep_alive, &status, &headers_out, &resp_body, &err);
			zval_ptr_dtor(&hdrs_assoc);
			if (rc == SUCCESS && resp_body && ctx && Z_TYPE(ctx->http_stream_cb) != IS_UNDEF && ZSTR_LEN(resp_body) > 0) {
				gene_http_invoke_stream(resp_body);
			}
		} else {
			zval ch, zurlv, zto, zcto, zsslpeer, zsslhost, zfollow, zmaxr, zmethodv, zbodyv;
			if (gene_http_ensure_curl(&ch) != SUCCESS) {
				if (own_body && body) zend_string_release(body);
				if (ctx && zstream) {
					zval_ptr_dtor(&ctx->http_stream_cb);
					ZVAL_COPY_VALUE(&ctx->http_stream_cb, &saved_stream);
				}
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

			gene_http_build_curl_headers(zheaders, &curl_headers, have_json);
			gene_http_curl_setopt(&ch, ZEND_STRL("CURLOPT_HTTPHEADER"), &curl_headers);
			zval_ptr_dtor(&curl_headers);

			if (body && ZSTR_LEN(body) > 0) {
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

	if (ctx && zstream) {
		zval_ptr_dtor(&ctx->http_stream_cb);
		ZVAL_COPY_VALUE(&ctx->http_stream_cb, &saved_stream);
	}

	if (own_body && body) {
		zend_string_release(body);
	}

	if (status <= 0 && err) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Http request failed: %s", err);
		efree(err);
		if (resp_body) zend_string_release(resp_body);
		if (Z_TYPE(headers_out) != IS_UNDEF) zval_ptr_dtor(&headers_out);
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
}
/* }}} */

const zend_function_entry gene_http_methods[] = {
	PHP_ME(gene_http, request, gene_http_request_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
