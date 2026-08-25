#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include <string.h>

#include "../gene.h"
#include "../common/common.h"
#include "../http/request.h"
#include "../http/json.h"
#include "../http/http.h"
#include "../http/invoke.h"
#include "../http/rest.h"

zend_class_entry *gene_rest_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_rest_construct_arginfo, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, config, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_rest_use_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_rest_local_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, class)
	ZEND_ARG_INFO(0, action)
	ZEND_ARG_ARRAY_INFO(0, params, 0)
	ZEND_ARG_ARRAY_INFO(0, files, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_rest_http_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_rest_call_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, class)
	ZEND_ARG_INFO(0, action)
	ZEND_ARG_ARRAY_INFO(0, params, 0)
	ZEND_ARG_ARRAY_INFO(0, options, 0)
ZEND_END_ARG_INFO()

static zval *gene_rest_prop(zval *obj, const char *name, size_t len) {
	return zend_read_property(gene_rest_ce, gene_strip_obj(obj), name, len, 1, NULL);
}

static zval *gene_rest_config(zval *obj) {
	zval *cfg = gene_rest_prop(obj, ZEND_STRL("config"));
	if (cfg && Z_TYPE_P(cfg) == IS_ARRAY) {
		return cfg;
	}
	return NULL;
}

static zval *gene_rest_service_name(zval *obj) {
	zval *s = gene_rest_prop(obj, ZEND_STRL("service"));
	if (s && Z_TYPE_P(s) == IS_STRING && Z_STRLEN_P(s) > 0) {
		return s;
	}
	return NULL;
}

static zval *gene_rest_service_cfg(zval *obj) {
	zval *cfg, *services, *svc, *name;
	cfg = gene_rest_config(obj);
	name = gene_rest_service_name(obj);
	if (!cfg || !name) {
		return NULL;
	}
	services = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("services"));
	if (!services || Z_TYPE_P(services) != IS_ARRAY) {
		return NULL;
	}
	svc = zend_hash_find(Z_ARRVAL_P(services), Z_STR_P(name));
	if (svc && Z_TYPE_P(svc) == IS_ARRAY) {
		return svc;
	}
	return NULL;
}

static double gene_rest_opt_double(zval *root, zval *svc, zval *opts, const char *k, size_t klen, double def) {
	zval *v;
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		v = zend_hash_str_find(Z_ARRVAL_P(opts), k, klen);
		if (v) return zval_get_double(v);
	}
	if (svc) {
		v = zend_hash_str_find(Z_ARRVAL_P(svc), k, klen);
		if (v) return zval_get_double(v);
	}
	if (root) {
		v = zend_hash_str_find(Z_ARRVAL_P(root), k, klen);
		if (v) return zval_get_double(v);
	}
	return def;
}

static zend_bool gene_rest_opt_bool(zval *root, zval *svc, zval *opts, const char *k, size_t klen, zend_bool def) {
	zval *v;
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		v = zend_hash_str_find(Z_ARRVAL_P(opts), k, klen);
		if (v) return zend_is_true(v);
	}
	if (svc) {
		v = zend_hash_str_find(Z_ARRVAL_P(svc), k, klen);
		if (v) return zend_is_true(v);
	}
	if (root) {
		v = zend_hash_str_find(Z_ARRVAL_P(root), k, klen);
		if (v) return zend_is_true(v);
	}
	return def;
}

static int gene_rest_header_exists(HashTable *headers, const char *name) {
	zend_string *k;
	zval *v;
	ZEND_HASH_FOREACH_STR_KEY_VAL(headers, k, v) {
		(void)v;
		if (k && strcasecmp(ZSTR_VAL(k), name) == 0) {
			return 1;
		}
	} ZEND_HASH_FOREACH_END();
	return 0;
}

static void gene_rest_merge_headers(zval *out, zval *root, zval *svc, zval *opts) {
	zval *src;
	array_init(out);
	if (root) {
		src = zend_hash_str_find(Z_ARRVAL_P(root), ZEND_STRL("headers"));
		if (src && Z_TYPE_P(src) == IS_ARRAY) {
			zend_hash_copy(Z_ARRVAL_P(out), Z_ARRVAL_P(src), (copy_ctor_func_t) zval_add_ref);
		}
	}
	if (svc) {
		src = zend_hash_str_find(Z_ARRVAL_P(svc), ZEND_STRL("headers"));
		if (src && Z_TYPE_P(src) == IS_ARRAY) {
			zend_hash_copy(Z_ARRVAL_P(out), Z_ARRVAL_P(src), (copy_ctor_func_t) zval_add_ref);
		}
	}
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		src = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("headers"));
		if (src && Z_TYPE_P(src) == IS_ARRAY) {
			zend_hash_copy(Z_ARRVAL_P(out), Z_ARRVAL_P(src), (copy_ctor_func_t) zval_add_ref);
		}
	}
}

static void gene_rest_pass_request_id(zval *headers, zend_bool pass) {
	zval *rid = NULL;
	gene_request_context *ctx;
	if (!pass || !headers || Z_TYPE_P(headers) != IS_ARRAY) {
		return;
	}
	if (gene_rest_header_exists(Z_ARRVAL_P(headers), "X-Request-Id")
		|| gene_rest_header_exists(Z_ARRVAL_P(headers), "x-request-id")) {
		return;
	}
	ctx = gene_request_ctx();
	if (ctx && Z_TYPE(ctx->user_bag) == IS_ARRAY) {
		rid = zend_hash_str_find(Z_ARRVAL(ctx->user_bag), ZEND_STRL("request_id"));
	}
	if (!rid || (Z_TYPE_P(rid) != IS_STRING && Z_TYPE_P(rid) != IS_LONG)) {
		rid = getVal(7, ZEND_STRL("X-Request-Id"));
	}
	if (!rid || (Z_TYPE_P(rid) != IS_STRING && Z_TYPE_P(rid) != IS_LONG)) {
		rid = getVal(TRACK_VARS_SERVER, ZEND_STRL("HTTP_X_REQUEST_ID"));
	}
	if (rid && (Z_TYPE_P(rid) == IS_STRING || Z_TYPE_P(rid) == IS_LONG)) {
		Z_TRY_ADDREF_P(rid);
		zend_hash_str_update(Z_ARRVAL_P(headers), ZEND_STRL("X-Request-Id"), rid);
	}
}

static int gene_rest_join_url(zval *svc, const char *path, size_t path_len, zend_string **out) {
	zval *base;
	const char *b;
	size_t blen, n;
	if (!path || path_len == 0 || path[0] != '/') {
		zend_throw_exception_ex(NULL, 0, "Gene\\Rest::http path must start with /");
		return FAILURE;
	}
	if (!svc) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Rest::use() is required before http()");
		return FAILURE;
	}
	base = zend_hash_str_find(Z_ARRVAL_P(svc), ZEND_STRL("base_url"));
	if (!base || Z_TYPE_P(base) != IS_STRING || Z_STRLEN_P(base) == 0) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Rest service is missing base_url");
		return FAILURE;
	}
	b = Z_STRVAL_P(base);
	blen = Z_STRLEN_P(base);
	while (blen > 0 && b[blen - 1] == '/') {
		blen--;
	}
	n = blen + path_len;
	*out = zend_string_alloc(n, 0);
	memcpy(ZSTR_VAL(*out), b, blen);
	memcpy(ZSTR_VAL(*out) + blen, path, path_len);
	ZSTR_VAL(*out)[n] = '\0';
	return SUCCESS;
}

static int gene_rest_http_exec(zval *this_ptr, const char *method, size_t method_len,
	const char *path, size_t path_len, zval *options, zval *retval)
{
	zval *cfg, *svc, *opts;
	zval headers, http_opts, http_ret;
	zend_string *url = NULL;
	zend_function *fn;
	zval params[1];
	zend_bool decode = 0;

	cfg = gene_rest_config(this_ptr);
	svc = gene_rest_service_cfg(this_ptr);
	opts = options;
	if (gene_rest_join_url(svc, path, path_len, &url) != SUCCESS) {
		return FAILURE;
	}

	gene_rest_merge_headers(&headers, cfg, svc, opts);
	gene_rest_pass_request_id(&headers, gene_rest_opt_bool(cfg, svc, opts, ZEND_STRL("pass_request_id"), 1));

	array_init(&http_opts);
	add_assoc_str_ex(&http_opts, ZEND_STRL("url"), url);
	add_assoc_stringl_ex(&http_opts, ZEND_STRL("method"), (char *)method, method_len);
	add_assoc_zval_ex(&http_opts, ZEND_STRL("headers"), &headers);
	add_assoc_double_ex(&http_opts, ZEND_STRL("timeout"), gene_rest_opt_double(cfg, svc, opts, ZEND_STRL("timeout"), 5.0));
	add_assoc_double_ex(&http_opts, ZEND_STRL("connect_timeout"), gene_rest_opt_double(cfg, svc, opts, ZEND_STRL("connect_timeout"), 2.0));
	add_assoc_bool_ex(&http_opts, ZEND_STRL("ssl_verify"), gene_rest_opt_bool(cfg, svc, opts, ZEND_STRL("ssl_verify"), 1));
	add_assoc_bool_ex(&http_opts, ZEND_STRL("keep_alive"), gene_rest_opt_bool(cfg, svc, opts, ZEND_STRL("keep_alive"), 1));

	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		zval *json = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("json"));
		zval *body = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("body"));
		zval *files = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("files"));
		zval *retry = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("retry"));
		zval *stream = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("stream"));
		zval *dec = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("decode"));
		if (json) {
			Z_TRY_ADDREF_P(json);
			add_assoc_zval_ex(&http_opts, ZEND_STRL("json"), json);
		}
		if (body) {
			Z_TRY_ADDREF_P(body);
			add_assoc_zval_ex(&http_opts, ZEND_STRL("body"), body);
		}
		if (files) {
			Z_TRY_ADDREF_P(files);
			add_assoc_zval_ex(&http_opts, ZEND_STRL("files"), files);
		}
		if (retry) {
			add_assoc_long_ex(&http_opts, ZEND_STRL("retry"), zval_get_long(retry));
		}
		if (stream) {
			Z_TRY_ADDREF_P(stream);
			add_assoc_zval_ex(&http_opts, ZEND_STRL("stream"), stream);
		}
		if (dec) {
			decode = zend_is_true(dec);
		}
	}

	fn = zend_hash_str_find_ptr(&gene_http_ce->function_table, ZEND_STRL("request"));
	if (!fn) {
		zval_ptr_dtor(&http_opts);
		zend_throw_exception_ex(NULL, 0, "Gene\\Http::request is unavailable");
		return FAILURE;
	}
	ZVAL_COPY_VALUE(&params[0], &http_opts);
	ZVAL_UNDEF(&http_ret);
	zend_call_known_function(fn, NULL, gene_http_ce, &http_ret, 1, params, NULL);
	zval_ptr_dtor(&http_opts);
	if (EG(exception)) {
		zval_ptr_dtor(&http_ret);
		return FAILURE;
	}
	if (decode) {
		zval *body = NULL;
		if (Z_TYPE(http_ret) == IS_ARRAY) {
			body = zend_hash_str_find(Z_ARRVAL(http_ret), ZEND_STRL("body"));
		}
		if (!body || Z_TYPE_P(body) != IS_STRING) {
			zval_ptr_dtor(&http_ret);
			zend_throw_exception_ex(NULL, 0, "Gene\\Rest decode failed: empty body");
			return FAILURE;
		}
		{
			zval decoded;
			if (gene_json_decode_throw(Z_STR_P(body), &decoded) != SUCCESS) {
				zval_ptr_dtor(&http_ret);
				return FAILURE;
			}
			zend_hash_str_update(Z_ARRVAL(http_ret), ZEND_STRL("body"), &decoded);
		}
	}
	ZVAL_COPY_VALUE(retval, &http_ret);
	return SUCCESS;
}

static int gene_rest_resolve_class(zval *this_ptr, const char *class_name, size_t class_len,
	zend_class_entry **ce_out, char **alloc_out, const char **used_name, size_t *used_len)
{
	zend_class_entry *ce;
	zval *svc, *local;
	char *buf = NULL;
	size_t n;

	*alloc_out = NULL;
	*used_name = class_name;
	*used_len = class_len;
	*ce_out = NULL;

	ce = gene_lookup_class_str(class_name, class_len);
	if (ce) {
		*ce_out = ce;
		return SUCCESS;
	}
	svc = gene_rest_service_cfg(this_ptr);
	if (!svc) {
		return SUCCESS;
	}
	local = zend_hash_str_find(Z_ARRVAL_P(svc), ZEND_STRL("local"));
	if (!local || Z_TYPE_P(local) != IS_STRING || Z_STRLEN_P(local) == 0) {
		return SUCCESS;
	}
	n = Z_STRLEN_P(local) + class_len;
	buf = emalloc(n + 1);
	memcpy(buf, Z_STRVAL_P(local), Z_STRLEN_P(local));
	memcpy(buf + Z_STRLEN_P(local), class_name, class_len);
	buf[n] = '\0';
	ce = gene_lookup_class_str(buf, n);
	if (ce) {
		*ce_out = ce;
		*alloc_out = buf;
		*used_name = buf;
		*used_len = n;
		return SUCCESS;
	}
	efree(buf);
	return SUCCESS;
}

PHP_METHOD(gene_rest, __construct) {
	zval *cfg = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a!", &cfg) == FAILURE) {
		return;
	}
	if (cfg && Z_TYPE_P(cfg) == IS_ARRAY) {
		zend_update_property(gene_rest_ce, gene_strip_obj(getThis()), ZEND_STRL("config"), cfg);
	} else {
		zval empty;
		array_init(&empty);
		zend_update_property(gene_rest_ce, gene_strip_obj(getThis()), ZEND_STRL("config"), &empty);
		zval_ptr_dtor(&empty);
	}
}

PHP_METHOD(gene_rest, use) {
	char *name = NULL;
	size_t name_len = 0;
	zval *cfg, *svc, *services, namez;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name, &name_len) == FAILURE) {
		return;
	}
	cfg = gene_rest_config(getThis());
	if (!cfg) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Rest config is missing");
		RETURN_THROWS();
	}
	services = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("services"));
	if (!services || Z_TYPE_P(services) != IS_ARRAY
		|| !(svc = zend_hash_str_find(Z_ARRVAL_P(services), name, name_len))
		|| Z_TYPE_P(svc) != IS_ARRAY) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Rest unknown service '%s'", name);
		RETURN_THROWS();
	}
	object_init_ex(return_value, gene_rest_ce);
	zend_update_property(gene_rest_ce, gene_strip_obj(return_value), ZEND_STRL("config"), cfg);
	ZVAL_STRINGL(&namez, name, name_len);
	zend_update_property(gene_rest_ce, gene_strip_obj(return_value), ZEND_STRL("service"), &namez);
	zval_ptr_dtor(&namez);
}

PHP_METHOD(gene_rest, local) {
	char *class_name = NULL, *action = NULL;
	size_t class_len = 0, action_len = 0;
	zval *params = NULL, *files = NULL, empty_params;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss|a!a!", &class_name, &class_len, &action, &action_len, &params, &files) == FAILURE) {
		return;
	}
	if (!params) {
		array_init(&empty_params);
		params = &empty_params;
	}
	if (gene_invoke_local(class_name, class_len, action, action_len, params, files, return_value) != SUCCESS) {
		if (params == &empty_params) {
			zval_ptr_dtor(&empty_params);
		}
		RETURN_THROWS();
	}
	if (params == &empty_params) {
		zval_ptr_dtor(&empty_params);
	}
}

PHP_METHOD(gene_rest, http) {
	char *method = NULL, *path = NULL;
	size_t method_len = 0, path_len = 0;
	zval *options = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss|a!", &method, &method_len, &path, &path_len, &options) == FAILURE) {
		return;
	}
	if (gene_rest_http_exec(getThis(), method, method_len, path, path_len, options, return_value) != SUCCESS) {
		RETURN_THROWS();
	}
}

PHP_METHOD(gene_rest, call) {
	char *class_name = NULL, *action = NULL;
	size_t class_len = 0, action_len = 0;
	zval *params = NULL, *options = NULL, empty_params;
	zend_class_entry *ce = NULL;
	char *alloc = NULL;
	const char *used = NULL;
	size_t used_len = 0;
	char act_buf[128];
	char *act;
	zval *files, *pathz, *methodz;
	const char *method = "POST";
	size_t method_len = 4;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss|a!a!", &class_name, &class_len, &action, &action_len, &params, &options) == FAILURE) {
		return;
	}
	if (!params) {
		array_init(&empty_params);
		params = &empty_params;
	}

	gene_rest_resolve_class(getThis(), class_name, class_len, &ce, &alloc, &used, &used_len);
	act = act_buf;
	if (action_len < sizeof(act_buf)) {
		memcpy(act_buf, action, action_len);
		act_buf[action_len] = '\0';
	} else {
		act = estrndup(action, action_len);
	}
	gene_strtolower(act);

	if (ce && zend_hash_str_exists(&ce->function_table, act, action_len)) {
		files = (options && Z_TYPE_P(options) == IS_ARRAY)
			? zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("files")) : NULL;
		if (gene_invoke_local(used, used_len, action, action_len, params, files, return_value) != SUCCESS) {
			if (act != act_buf) efree(act);
			if (alloc) efree(alloc);
			if (params == &empty_params) zval_ptr_dtor(&empty_params);
			RETURN_THROWS();
		}
		if (act != act_buf) efree(act);
		if (alloc) efree(alloc);
		if (params == &empty_params) zval_ptr_dtor(&empty_params);
		return;
	}
	if (act != act_buf) {
		efree(act);
	}
	if (alloc) {
		efree(alloc);
	}

	if (!options || Z_TYPE_P(options) != IS_ARRAY
		|| !(pathz = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("path")))
		|| Z_TYPE_P(pathz) != IS_STRING) {
		if (params == &empty_params) zval_ptr_dtor(&empty_params);
		zend_throw_exception_ex(NULL, 0, "Gene\\Rest::call remote path requires options['path']");
		RETURN_THROWS();
	}
	methodz = zend_hash_str_find(Z_ARRVAL_P(options), ZEND_STRL("method"));
	if (methodz && Z_TYPE_P(methodz) == IS_STRING && Z_STRLEN_P(methodz) > 0) {
		method = Z_STRVAL_P(methodz);
		method_len = Z_STRLEN_P(methodz);
	}
	{
		zval http_opts;
		ZVAL_DUP(&http_opts, options);
		if (!zend_hash_str_exists(Z_ARRVAL(http_opts), ZEND_STRL("json"))
			&& !zend_hash_str_exists(Z_ARRVAL(http_opts), ZEND_STRL("body"))
			&& !zend_hash_str_exists(Z_ARRVAL(http_opts), ZEND_STRL("files"))) {
			Z_TRY_ADDREF_P(params);
			zend_hash_str_update(Z_ARRVAL(http_opts), ZEND_STRL("json"), params);
		}
		if (gene_rest_http_exec(getThis(), method, method_len, Z_STRVAL_P(pathz), Z_STRLEN_P(pathz), &http_opts, return_value) != SUCCESS) {
			zval_ptr_dtor(&http_opts);
			if (params == &empty_params) zval_ptr_dtor(&empty_params);
			RETURN_THROWS();
		}
		zval_ptr_dtor(&http_opts);
	}
	if (params == &empty_params) {
		zval_ptr_dtor(&empty_params);
	}
}

const zend_function_entry gene_rest_methods[] = {
	PHP_ME(gene_rest, __construct, gene_rest_construct_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_rest, use, gene_rest_use_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_rest, local, gene_rest_local_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_rest, http, gene_rest_http_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_rest, call, gene_rest_call_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(rest) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Rest", "Gene\\Rest", gene_rest_methods);
	gene_rest_ce = zend_register_internal_class(&ce);
	zend_declare_property_null(gene_rest_ce, ZEND_STRL("config"), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_rest_ce, ZEND_STRL("service"), ZEND_ACC_PROTECTED);
	return SUCCESS;
}
