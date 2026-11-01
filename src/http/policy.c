/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01] Security policy engine (see policy.h). */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include "zend_smart_str.h"
#include "ext/standard/php_string.h"
#include "ext/standard/url.h"
#include <time.h>

#include "../gene.h"
#include "../http/request.h"
#include "../http/response.h"
#include "../http/ip.h"
#include "../http/policy.h"
#include "../app/application.h"
#include "../di/di.h"
#include "../cache/memory.h"
#include "../cache/redis.h"
#include "../exception/exception.h"

extern zend_class_entry *gene_application_ce;

zend_class_entry *gene_csrf_ce;

/* Method/path actually being dispatched (run() args win over server bag). */
typedef struct {
	const char *method;
	size_t mlen;
	const char *path;
	size_t plen;
} gene_policy_req;

/* Request headers through the HEADER bag (lowercase dash names): Swoole
 * fills it from $request->header, FPM materializes it from HTTP_* — reading
 * HTTP_* from the server bag directly silently disabled the Origin/Fetch-
 * Metadata checks under Swoole. */
static zval *policy_header(const char *name, size_t len) {
	zval *v = getVal(GENE_REQUEST_ATTR_HEADER, (char *)name, len);
	return (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) ? v : NULL;
}

/* {{{ small php-function call helper (crypto.c-style) */
static int policy_call(const char *name, size_t name_len, uint32_t argc, zval *params, zval *retval) {
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

static zend_string *policy_hash_hmac(const char *algo, size_t algo_len, zend_string *data, zend_string *secret) {
	zval params[4], out;
	ZVAL_STRINGL(&params[0], algo, algo_len);
	ZVAL_STR(&params[1], data);
	ZVAL_STR(&params[2], secret);
	ZVAL_TRUE(&params[3]);
	if (policy_call(ZEND_STRL("hash_hmac"), 4, params, &out) != SUCCESS) {
		zval_ptr_dtor(&params[0]);
		return NULL;
	}
	zval_ptr_dtor(&params[0]);
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		return NULL;
	}
	return Z_STR(out);
}

zend_string *policy_b64url(const char *data, size_t len) {
	zval in, out;
	zend_string *s;
	char *p;
	size_t i, n;
	ZVAL_STRINGL(&in, data, len);
	if (policy_call(ZEND_STRL("base64_encode"), 1, &in, &out) != SUCCESS) {
		zval_ptr_dtor(&in);
		return NULL;
	}
	zval_ptr_dtor(&in);
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		return NULL;
	}
	s = Z_STR(out);
	p = ZSTR_VAL(s); n = ZSTR_LEN(s);
	for (i = 0; i < n; i++) {
		if (p[i] == '+') p[i] = '-';
		else if (p[i] == '/') p[i] = '_';
	}
	while (n > 0 && p[n - 1] == '=') n--;
	ZSTR_LEN(s) = n;
	p[n] = '\0';
	return s;
}

zend_string *policy_random_bytes(zend_long n) {
	zval in, out;
	ZVAL_LONG(&in, n);
	if (policy_call(ZEND_STRL("random_bytes"), 1, &in, &out) != SUCCESS) {
		return NULL;
	}
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		return NULL;
	}
	return Z_STR(out);
}

static int policy_hash_equals(zend_string *a, zend_string *b) {
	zval params[2], out;
	int ok;
	ZVAL_STR(&params[0], a);
	ZVAL_STR(&params[1], b);
	if (policy_call(ZEND_STRL("hash_equals"), 2, params, &out) != SUCCESS) {
		return 0;
	}
	ok = zend_is_true(&out);
	zval_ptr_dtor(&out);
	return ok;
}
/* }}} */

/* {{{ gene_jsonp_valid — ^[A-Za-z_$][A-Za-z0-9_$]*(\.[A-Za-z_$][A-Za-z0-9_$]*)*$ max 128 */
int gene_jsonp_valid(const char *s, size_t n) {
	zend_bool need_head = 1;
	size_t i;
	if (!s || n == 0 || n > 128) {
		return 0;
	}
	for (i = 0; i < n; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c == '.') {
			if (need_head) return 0;
			need_head = 1;
			continue;
		}
		{
			zend_bool head = (c == '_' || c == '$' || ((c | 32) >= 'a' && (c | 32) <= 'z'));
			if (need_head) {
				if (!head) return 0;
				need_head = 0;
			} else if (!head && !(c >= '0' && c <= '9')) {
				return 0;
			}
		}
	}
	return !need_head;
}
/* }}} */

/* {{{ gene_path_glob */
int gene_path_glob(const char *pat, size_t plen, const char *s, size_t slen) {
	size_t i = 0, j = 0;
	size_t star_p = (size_t)-1, star_s = 0;
	while (j < slen) {
		if (i < plen && (pat[i] == '?' || tolower((unsigned char)pat[i]) == tolower((unsigned char)s[j]))) {
			i++; j++;
		} else if (i < plen && pat[i] == '*') {
			star_p = i++;
			star_s = j;
		} else if (star_p != (size_t)-1) {
			i = star_p + 1;
			j = ++star_s;
		} else {
			return 0;
		}
	}
	while (i < plen && pat[i] == '*') i++;
	return i == plen;
}
/* }}} */

/* {{{ deny helper: json error + status, mirrors Hook::respond semantics */
static void gene_policy_deny(const char *msg, zend_long code) {
	smart_str buf = {0};
	gene_response_set_status(code);
	gene_response_set_header_ex("Content-Type", sizeof("Content-Type") - 1,
		"application/json; charset=UTF-8", sizeof("application/json; charset=UTF-8") - 1);
	smart_str_appends(&buf, "{\"code\":4000,\"msg\":\"");
	/* JSON string escape: quotes, backslash and control bytes (UTF-8 bytes
	 * >= 0x80 pass through verbatim) */
	{
		const unsigned char *p = (const unsigned char *)msg;
		for (; *p; p++) {
			unsigned char c = *p;
			if (c == '"' || c == '\\') {
				smart_str_appendc(&buf, '\\');
				smart_str_appendc(&buf, (char)c);
			} else if (c < 0x20) {
				char esc[8];
				snprintf(esc, sizeof(esc), "\\u%04x", c);
				smart_str_appends(&buf, esc);
			} else {
				smart_str_appendc(&buf, (char)c);
			}
		}
	}
	smart_str_appends(&buf, "\"}");
	smart_str_0(&buf);
	if (buf.s) {
		gene_response_end(buf.s);
	} else {
		gene_response_end(NULL);
	}
	smart_str_free(&buf);
}
/* }}} */

/* {{{ config accessors (Application static props, requestIdConfig pattern) */
static zval *gene_policy_cfg(const char *name, size_t len) {
	zval *cfg = zend_read_static_property(gene_application_ce, name, len, 1);
	if (cfg && Z_TYPE_P(cfg) == IS_ARRAY) {
		return cfg;
	}
	return NULL;
}

static zend_string *gene_policy_cfg_str(zval *cfg, const char *k, size_t klen) {
	zval *v = zend_hash_str_find(Z_ARRVAL_P(cfg), k, klen);
	if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
		return Z_STR_P(v);
	}
	return NULL;
}

static zend_bool gene_policy_cfg_bool(zval *cfg, const char *k, size_t klen, zend_bool def) {
	zval *v = zend_hash_str_find(Z_ARRVAL_P(cfg), k, klen);
	return v ? zend_is_true(v) : def;
}

static zend_long gene_policy_cfg_long(zval *cfg, const char *k, size_t klen, zend_long def) {
	zval *v = zend_hash_str_find(Z_ARRVAL_P(cfg), k, klen);
	return (v && Z_TYPE_P(v) == IS_LONG) ? Z_LVAL_P(v) : def;
}

static int gene_policy_excepted(zval *cfg, const char *path, size_t plen) {
	zval *except = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("except"));
	if (except && Z_TYPE_P(except) == IS_ARRAY) {
		zval *item;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(except), item) {
			if (Z_TYPE_P(item) == IS_STRING
				&& gene_path_glob(Z_STRVAL_P(item), Z_STRLEN_P(item), path, plen)) {
				return 1;
			}
		} ZEND_HASH_FOREACH_END();
	}
	return 0;
}
/* }}} */

/* {{{ CSRF token via session component */
static zval *gene_csrf_session_obj(zval *cfg) {
	zend_string *name = gene_policy_cfg_str(cfg, ZEND_STRL("session"));
	zval *obj;
	if (!name) {
		name = zend_string_init(ZEND_STRL("session"), 0);
		obj = gene_di_get(name);
		zend_string_release(name);
	} else {
		obj = gene_di_get(name);
	}
	return (obj && Z_TYPE_P(obj) == IS_OBJECT) ? obj : NULL;
}

static zend_string *gene_csrf_session_token_ex(zval *cfg, zend_bool create);

zend_string *gene_csrf_session_token(zval *cfg) {
	return gene_csrf_session_token_ex(cfg, 1);
}

/* create=0: read-only peek — verification must never mint+save a token
 * (a forged request would otherwise write session state on every probe). */
static zend_string *gene_csrf_session_token_ex(zval *cfg, zend_bool create) {
	zval *sess = gene_csrf_session_obj(cfg);
	zval ret, name;
	zend_string *tok = NULL;
	zend_function *fn;
	ZVAL_UNDEF(&ret);
	if (!sess) {
		return NULL;
	}
	fn = zend_hash_str_find_ptr(&Z_OBJCE_P(sess)->function_table, ZEND_STRL("get"));
	if (!fn) {
		return NULL;
	}
	ZVAL_STRING(&name, "_csrf");
	{
		zval params[] = { name };
		zend_call_known_function(fn, Z_OBJ_P(sess), Z_OBJCE_P(sess), &ret, 1, params, NULL);
	}
	zval_ptr_dtor(&name);
	if (Z_TYPE(ret) == IS_STRING && Z_STRLEN(ret) > 0) {
		tok = zend_string_copy(Z_STR(ret));
	}
	zval_ptr_dtor(&ret);
	if (!tok && create) {
		tok = policy_random_bytes(32);
		if (!tok) {
			return NULL;
		}
		{
			zend_string *b64 = policy_b64url(ZSTR_VAL(tok), ZSTR_LEN(tok));
			zend_string_release(tok);
			tok = b64 ? b64 : NULL;
		}
		if (!tok) {
			return NULL;
		}
		{
			zval params[2], sret;
			zend_function *set_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(sess)->function_table, ZEND_STRL("set"));
			if (!set_fn) {
				zend_string_release(tok);
				return NULL;
			}
			ZVAL_STRING(&params[0], "_csrf");
			ZVAL_STR(&params[1], tok);
			ZVAL_UNDEF(&sret);
			zend_call_known_function(set_fn, Z_OBJ_P(sess), Z_OBJCE_P(sess), &sret, 2, params, NULL);
			zval_ptr_dtor(&params[0]);
			if (!Z_ISUNDEF(sret)) zval_ptr_dtor(&sret);
			/* session save() so the token persists for the next request */
			{
				zend_function *save_fn = zend_hash_str_find_ptr(&Z_OBJCE_P(sess)->function_table, ZEND_STRL("save"));
				if (save_fn) {
					zval sret2;
					ZVAL_UNDEF(&sret2);
					zend_call_known_function(save_fn, Z_OBJ_P(sess), Z_OBJCE_P(sess), &sret2, 0, NULL, NULL);
					if (!Z_ISUNDEF(sret2)) zval_ptr_dtor(&sret2);
				}
			}
		}
	}
	return tok;
}

zend_bool gene_csrf_check_token(zval *cfg, zend_string *given) {
	zend_string *tok;
	zend_bool ok = 0;
	if (!given || ZSTR_LEN(given) == 0) {
		return 0;
	}
	tok = gene_csrf_session_token_ex(cfg, 0);
	if (tok) {
		ok = policy_hash_equals(tok, given);
		zend_string_release(tok);
	}
	return ok;
}
/* }}} */

/* {{{ csrf policy evaluation */
static zend_bool gene_csrf_method_in(zval *cfg, const char *method, size_t mlen) {
	zval *methods = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("methods"));
	if (methods && Z_TYPE_P(methods) == IS_ARRAY) {
		zval *m;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(methods), m) {
			if (Z_TYPE_P(m) == IS_STRING && Z_STRLEN_P(m) == mlen
				&& strncasecmp(Z_STRVAL_P(m), method, mlen) == 0) {
				return 1;
			}
		} ZEND_HASH_FOREACH_END();
		return 0;
	}
	/* default: all mutating verbs */
	return (mlen == 4 && strncasecmp(method, "POST", 4) == 0)
		|| (mlen == 3 && strncasecmp(method, "PUT", 3) == 0)
		|| (mlen == 5 && strncasecmp(method, "PATCH", 5) == 0)
		|| (mlen == 6 && strncasecmp(method, "DELETE", 6) == 0);
}

/* Origin/Referer host (+effective port) must match Host. Ports normalized to
 * scheme defaults (https:443/http:80); a Host without a port takes the current
 * request scheme's default so an explicit non-default Origin port cannot slip. */
static zend_bool gene_csrf_same_origin(zval *origin_zv, zval *host_zv) {
	const char *o, *h;
	size_t olen, hlen;
	php_url *u;
	const char *ohost;
	size_t ohost_len;
	zend_long oport;
	const char *hcol = NULL;
	zend_bool ok;

	if (!origin_zv || Z_TYPE_P(origin_zv) != IS_STRING || Z_STRLEN_P(origin_zv) == 0
		|| !host_zv || Z_TYPE_P(host_zv) != IS_STRING || Z_STRLEN_P(host_zv) == 0) {
		return 0;
	}
	o = Z_STRVAL_P(origin_zv); olen = Z_STRLEN_P(origin_zv);
	h = Z_STRVAL_P(host_zv); hlen = Z_STRLEN_P(host_zv);
	if (olen > 1024 || hlen > 256) {
		return 0;
	}
	u = php_url_parse_ex(o, olen);
	if (!u || !u->host || !u->scheme) {
		if (u) php_url_free(u);
		return 0;
	}
	ohost = ZSTR_VAL(u->host);
	ohost_len = ZSTR_LEN(u->host);
	oport = u->port ? (zend_long)u->port
		: (zend_string_equals_literal_ci(u->scheme, "https") ? 443 : 80);
	/* Host may be "example.com:8080" or "[::1]:8080" */
	if (h[0] == '[') {
		const char *rb = memchr(h, ']', hlen);
		if (rb) {
			if (rb + 1 < h + hlen && rb[1] == ':') {
				hcol = rb + 1;
			}
			hlen = (size_t)(rb + 1 - h); /* keep brackets for compare */
		}
	} else {
		hcol = memchr(h, ':', hlen);
		if (hcol) {
			hlen = (size_t)(hcol - h);
		}
	}
	ok = (ohost_len == hlen) && (strncasecmp(ohost, h, hlen) == 0);
	if (ok) {
		/* Host without an explicit port takes the request-scheme default —
		 * otherwise "Host: example.com" would wrongly match
		 * "Origin: https://example.com:8443". */
		zend_long hport = hcol
			? ZEND_STRTOL(hcol + 1, NULL, 10)
			: (gene_request_is_secure() ? 443 : 80);
		ok = (hport == oport);
	}
	php_url_free(u);
	return ok;
}

static zend_bool gene_csrf_origin_allowed(zval *cfg, zval *origin) {
	zval *list = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("origins"));
	if (list && Z_TYPE_P(list) == IS_ARRAY && origin && Z_TYPE_P(origin) == IS_STRING) {
		zval *item;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(list), item) {
			if (Z_TYPE_P(item) == IS_STRING
				&& Z_STRLEN_P(item) == Z_STRLEN_P(origin)
				&& strcasecmp(Z_STRVAL_P(item), Z_STRVAL_P(origin)) == 0) {
				return 1;
			}
		} ZEND_HASH_FOREACH_END();
	}
	return 0;
}

static int gene_policy_csrf_run(zval *cfg, const gene_policy_req *req) {
	const char *method = req->method;
	size_t mlen = req->mlen;
	const char *path = req->path;
	size_t plen = req->plen;
	zval *fetch, *origin = NULL, *host;
	zend_string *fail_msg;
	zend_long fail_code;

	if (!gene_csrf_method_in(cfg, method, mlen)) {
		return 0;
	}
	if (gene_policy_excepted(cfg, path, plen)) {
		return 0;
	}
	/* precedence: on_fail_msg/on_fail_code keys, then on_fail{msg,code}, 403 */
	fail_msg = gene_policy_cfg_str(cfg, ZEND_STRL("on_fail_msg"));
	fail_code = gene_policy_cfg_long(cfg, ZEND_STRL("on_fail_code"), 0);
	{
		zval *on_fail = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("on_fail"));
		if (on_fail && Z_TYPE_P(on_fail) == IS_ARRAY) {
			zval *m = zend_hash_str_find(Z_ARRVAL_P(on_fail), ZEND_STRL("msg"));
			zval *c = zend_hash_str_find(Z_ARRVAL_P(on_fail), ZEND_STRL("code"));
			if (!fail_msg && m && Z_TYPE_P(m) == IS_STRING) fail_msg = Z_STR_P(m);
			if (!fail_code && c && Z_TYPE_P(c) == IS_LONG) fail_code = Z_LVAL_P(c);
		}
	}
	if (!fail_code) {
		fail_code = 403;
	}

	/* 1. Fetch metadata: cross-site is always denied for mutating verbs;
	 *    same-site needs allow_same_site. */
	if (gene_policy_cfg_bool(cfg, ZEND_STRL("fetch_site"), 1)) {
		fetch = policy_header(ZEND_STRL("sec-fetch-site"));
		if (fetch) {
			if (zend_string_equals_literal_ci(Z_STR_P(fetch), "cross-site")) {
				gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "非法请求来源", fail_code);
				return 1;
			}
			if (zend_string_equals_literal_ci(Z_STR_P(fetch), "same-site")
				&& !gene_policy_cfg_bool(cfg, ZEND_STRL("allow_same_site"), 0)) {
				gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "非法请求来源", fail_code);
				return 1;
			}
		}
	}

	/* 2. Origin/Referer same-origin (any method carrying Origin). */
	host = policy_header(ZEND_STRL("host"));
	origin = policy_header(ZEND_STRL("origin"));
	if (!origin) {
		origin = policy_header(ZEND_STRL("referer"));
	}
	if (gene_policy_cfg_bool(cfg, ZEND_STRL("origin"), 1)) {
		if (origin) {
			if (!gene_csrf_same_origin(origin, host) && !gene_csrf_origin_allowed(cfg, origin)) {
				gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "非法请求来源", fail_code);
				return 1;
			}
		} else if (gene_policy_cfg_bool(cfg, ZEND_STRL("require_origin"), 0)) {
			gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "非法请求来源", fail_code);
			return 1;
		}
	}

	/* 3. Synchronizer token (high-sensitivity backends). */
	if (gene_policy_cfg_bool(cfg, ZEND_STRL("token"), 0)) {
		/* header or POST field only — a token in the query string leaks via
		 * Referer/access logs */
		zval *given = policy_header(ZEND_STRL("x-csrf-token"));
		if (!given) {
			given = getVal(TRACK_VARS_POST, ZEND_STRL("_token"));
		}
		if (!gene_csrf_check_token(cfg, (given && Z_TYPE_P(given) == IS_STRING) ? Z_STR_P(given) : NULL)) {
			gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "非法请求来源", fail_code);
			return 1;
		}
	}
	return 0;
}
/* }}} */

/* {{{ rest guard */
zend_string *gene_policy_sign_request(zend_string *secret, const char *method, size_t mlen,
	const char *path, size_t plen, zend_long ts, zend_string *nonce, zend_string *body) {
	smart_str s = {0};
	zend_string *sig_raw, *sig;
	zval params[2], out;

	ZVAL_STRING(&params[0], "sha256");
	ZVAL_STR(&params[1], body);
	if (policy_call(ZEND_STRL("hash"), 2, params, &out) != SUCCESS) {
		zval_ptr_dtor(&params[0]);
		return NULL;
	}
	zval_ptr_dtor(&params[0]);
	if (Z_TYPE(out) != IS_STRING || Z_STRLEN(out) != 64) {
		zval_ptr_dtor(&out);
		return NULL;
	}

	/* method is canonicalized to upper case: the dispatcher lower-cases
	 * ctx->method while clients send "POST" — both sides must agree */
	{
		size_t i;
		for (i = 0; i < mlen; i++) {
			unsigned char c = (unsigned char)method[i];
			smart_str_appendc(&s, (char)((c >= 'a' && c <= 'z') ? (c - 32) : c));
		}
	}
	smart_str_appendc(&s, '\n');
	smart_str_appendl(&s, path, plen);
	smart_str_appendc(&s, '\n');
	smart_str_append_long(&s, ts);
	smart_str_appendc(&s, '\n');
	smart_str_append(&s, nonce);
	smart_str_appendc(&s, '\n');
	smart_str_append(&s, Z_STR(out)); /* body sha256 hex */
	smart_str_0(&s);
	zval_ptr_dtor(&out);
	sig_raw = policy_hash_hmac(ZEND_STRL("sha256"), s.s, secret);
	smart_str_free(&s);
	if (!sig_raw) {
		return NULL;
	}
	sig = policy_b64url(ZSTR_VAL(sig_raw), ZSTR_LEN(sig_raw));
	zend_string_release(sig_raw);
	return sig;
}

/* {{{ gene_store_once — atomic "first writer wins" marker with a TTL.
 * 1 = first use, 0 = already seen (replay), -1 = store error/unsupported.
 *   Gene\Memory / Gene\Cache\Redis → lock(key, ttl)   (SET NX EX / wrlock)
 *   other objects                  → add(key, 1, ttl) (memcached shape)
 *                                  → set(key, 1, ['nx', 'ex' => ttl]) (phpredis)
 * The former set(key, 1, ['nx' => true, ...]) form was not NX at all:
 * phpredis only honours 'nx' as a list value, Gene\Cache\Redis::set reads the
 * array as a long ttl and Gene\Memory::set throws on it (swallowed → fresh) —
 * replay protection silently never engaged. */
static int gene_store_call(zval *store, const char *m, size_t mlen, uint32_t argc, zval *params, zval *ret) {
	zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(store)->function_table, m, mlen);
	ZVAL_UNDEF(ret);
	if (fn) {
		zend_call_known_function(fn, Z_OBJ_P(store), Z_OBJCE_P(store), ret, argc, params, NULL);
	} else if ((fn = Z_OBJCE_P(store)->__call) != NULL
			&& (fn->common.fn_flags & ZEND_ACC_PUBLIC)) {
		/* __call-proxied drivers (Gene\Cache\* wrappers) expose add/set the
		 * same way a userland call sees them — without this the NX marker
		 * never reached stores that only proxy their methods. */
		zval cparams[2], args;
		uint32_t i;
		array_init(&args);
		for (i = 0; i < argc; i++) {
			Z_TRY_ADDREF(params[i]);
			add_next_index_zval(&args, &params[i]);
		}
		ZVAL_STRINGL(&cparams[0], m, mlen);
		ZVAL_COPY_VALUE(&cparams[1], &args);
		zend_call_known_function(fn, Z_OBJ_P(store), Z_OBJCE_P(store), ret, 2, cparams, NULL);
		zval_ptr_dtor(&cparams[0]);
		zval_ptr_dtor(&cparams[1]);
	}
	if (EG(exception)) {
		zend_clear_exception();
		zval_ptr_dtor(ret);
		ZVAL_UNDEF(ret);
		return FAILURE;
	}
	return SUCCESS;
}

int gene_store_once(zval *store, zend_string *key, zend_long ttl) {
	zval params[3], ret;
	int r = -1;
	if (!store || Z_TYPE_P(store) != IS_OBJECT) {
		return -1;
	}
	if (ttl < 1) {
		ttl = 1;
	}
	ZVAL_STR(&params[0], key);
	if (instanceof_function(Z_OBJCE_P(store), gene_memory_ce)
			|| instanceof_function(Z_OBJCE_P(store), gene_redis_ce)) {
		ZVAL_LONG(&params[1], ttl);
		if (gene_store_call(store, ZEND_STRL("lock"), 2, params, &ret) == SUCCESS) {
			/* Redis::lock returns NULL when the connection is down */
			r = (Z_TYPE(ret) == IS_NULL) ? -1 : (zend_is_true(&ret) ? 1 : 0);
		}
		zval_ptr_dtor(&ret);
		return r;
	}
	ZVAL_LONG(&params[1], 1);
	ZVAL_LONG(&params[2], ttl);
	if (gene_store_call(store, ZEND_STRL("add"), 3, params, &ret) == SUCCESS) {
		r = zend_is_true(&ret) ? 1 : 0;
		zval_ptr_dtor(&ret);
		return r;
	}
	array_init(&params[2]);
	add_next_index_string(&params[2], "nx");
	add_assoc_long_ex(&params[2], ZEND_STRL("ex"), ttl);
	if (gene_store_call(store, ZEND_STRL("set"), 3, params, &ret) == SUCCESS) {
		r = zend_is_true(&ret) ? 1 : 0;
	}
	zval_ptr_dtor(&params[2]);
	zval_ptr_dtor(&ret);
	return r;
}
/* }}} */

/* nonce single-use. A configured-but-missing component is a deployment
 * error → reject (fail closed, visible at once). A store outage → accept
 * with a diag (time skew still bounds replay; internal APIs stay up). */
zend_bool gene_policy_nonce_check(zend_string *component, zend_string *key, zend_long ttl) {
	zval *store = gene_di_get(component);
	int r;
	if (!store || Z_TYPE_P(store) != IS_OBJECT) {
		gene_log_diag(E_WARNING, "Gene restGuard: nonce store '%s' is not a registered component", ZSTR_VAL(component));
		return 0;
	}
	r = gene_store_once(store, key, ttl);
	if (r < 0) {
		gene_log_diag(E_WARNING, "Gene restGuard: nonce store '%s' unavailable, replay check skipped", ZSTR_VAL(component));
		return 1;
	}
	return r == 1;
}

/* Signed request target: path plus raw query string ("?a=1" covered by the
 * signature so query parameters cannot be swapped). FPM QUERY_STRING and
 * Swoole query_string resolve through the server bag's lowercase fallback. */
static zend_string *gene_policy_target(const char *path, size_t plen) {
	zval *qs = getVal(TRACK_VARS_SERVER, ZEND_STRL("QUERY_STRING"));
	if (qs && Z_TYPE_P(qs) == IS_STRING && Z_STRLEN_P(qs) > 0) {
		return zend_string_concat3(path, plen, "?", 1, Z_STRVAL_P(qs), Z_STRLEN_P(qs));
	}
	return zend_string_init(path, plen, 0);
}

static int gene_policy_rest_run(zval *cfg, const gene_policy_req *req) {
	const char *path = req->path;
	size_t plen = req->plen;
	zval *prefixes;
	int hit = 0;
	zend_bool inner = 0;
	zend_string *fail_msg = NULL;
	zend_long fail_code = 403;

	prefixes = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("prefixes"));
	if (prefixes && Z_TYPE_P(prefixes) == IS_ARRAY) {
		zval *p;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(prefixes), p) {
			if (Z_TYPE_P(p) == IS_STRING && Z_STRLEN_P(p) <= plen
				&& strncasecmp(Z_STRVAL_P(p), path, Z_STRLEN_P(p)) == 0) {
				hit = 1;
				break;
			}
		} ZEND_HASH_FOREACH_END();
	} else {
		hit = (plen >= 6 && strncasecmp(path, "/rest/", 6) == 0);
	}
	if (!hit) {
		return 0;
	}
	if (gene_policy_excepted(cfg, path, plen)) {
		return 0;
	}
	/* inner local invoke bypass (Invoke::local sets Di 'inner') */
	{
		gene_request_context *c = gene_request_ctx();
		if (Z_TYPE(c->di_regs) == IS_ARRAY) {
			zval *inn = zend_hash_str_find(Z_ARRVAL(c->di_regs), ZEND_STRL("inner"));
			if (inn) inner = zend_is_true(inn);
		}
	}
	if (inner) {
		return 2;
	}
	{
		zval *on_fail = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("on_fail"));
		if (on_fail && Z_TYPE_P(on_fail) == IS_ARRAY) {
			zval *m = zend_hash_str_find(Z_ARRVAL_P(on_fail), ZEND_STRL("msg"));
			zval *c = zend_hash_str_find(Z_ARRVAL_P(on_fail), ZEND_STRL("code"));
			if (m && Z_TYPE_P(m) == IS_STRING) fail_msg = Z_STR_P(m);
			if (c && Z_TYPE_P(c) == IS_LONG) fail_code = Z_LVAL_P(c);
		}
	}

	/* internal-only gate on the TCP peer (never XFF) */
	if (gene_policy_cfg_bool(cfg, ZEND_STRL("internal"), 1) && !gene_request_ip_is_internal()) {
		gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "访问未授权", fail_code);
		return 1;
	}

	/* shared-secret / hmac signature */
	{
		zend_string *secret = gene_policy_cfg_str(cfg, ZEND_STRL("secret"));
		zend_string *mode = gene_policy_cfg_str(cfg, ZEND_STRL("mode"));
		if (secret) {
			if (mode && zend_string_equals_literal_ci(mode, "shared")) {
				zval *given = getVal(GENE_REQUEST_ATTR_HEADER, ZEND_STRL("x-rest-secret"));
				if (!given || Z_TYPE_P(given) != IS_STRING
					|| !policy_hash_equals(secret, Z_STR_P(given))) {
					gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "访问未授权", fail_code);
					return 1;
				}
			} else {
				/* hmac mode: X-Gene-Ts / X-Gene-Nonce / X-Gene-Sign */
				zval *ts_v = getVal(GENE_REQUEST_ATTR_HEADER, ZEND_STRL("x-gene-ts"));
				zval *nonce_v = getVal(GENE_REQUEST_ATTR_HEADER, ZEND_STRL("x-gene-nonce"));
				zval *sign_v = getVal(GENE_REQUEST_ATTR_HEADER, ZEND_STRL("x-gene-sign"));
				zend_long skew = gene_policy_cfg_long(cfg, ZEND_STRL("skew"), 30);
				zend_long ts;
				if (!ts_v || Z_TYPE_P(ts_v) != IS_STRING
					|| !nonce_v || Z_TYPE_P(nonce_v) != IS_STRING || Z_STRLEN_P(nonce_v) > 128
					|| !sign_v || Z_TYPE_P(sign_v) != IS_STRING) {
					gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "访问未授权", fail_code);
					return 1;
				}
				{
					/* strict decimal seconds; bounded digits keeps the
					 * subtraction below free of signed overflow */
					const char *tp = Z_STRVAL_P(ts_v);
					size_t ti, tl = Z_STRLEN_P(ts_v);
					zend_bool ts_ok = tl > 0 && tl <= 12;
					for (ti = 0; ts_ok && ti < tl; ti++) {
						if (tp[ti] < '0' || tp[ti] > '9') ts_ok = 0;
					}
					ts = ts_ok ? ZEND_STRTOL(tp, NULL, 10) : 0;
					if (!ts_ok || Z_STRLEN_P(nonce_v) < 8) {
						gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "访问未授权", fail_code);
						return 1;
					}
				}
				{
					zend_long diff = (zend_long)time(NULL) - ts;
					if (diff < 0) diff = -diff;
					if (diff > skew) {
						gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "访问未授权", fail_code);
						return 1;
					}
				}
				{
					zend_string *body, *expect;
					{
						zval rv;
						ZVAL_UNDEF(&rv);
						{
							zend_function *raw_fn = zend_hash_str_find_ptr(&gene_request_ce->function_table, ZEND_STRL("rawcontent"));
							if (raw_fn) {
								zend_call_known_function(raw_fn, NULL, gene_request_ce, &rv, 0, NULL, NULL);
							}
						}
						body = (Z_TYPE(rv) == IS_STRING) ? zend_string_copy(Z_STR(rv)) : ZSTR_EMPTY_ALLOC();
						if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
					}
					{
						zend_string *target = gene_policy_target(path, plen);
						expect = gene_policy_sign_request(secret, req->method, req->mlen,
							ZSTR_VAL(target), ZSTR_LEN(target), ts, Z_STR_P(nonce_v), body);
						zend_string_release(target);
					}
					zend_string_release(body);
					if (!expect || !policy_hash_equals(expect, Z_STR_P(sign_v))) {
						if (expect) zend_string_release(expect);
						gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "访问未授权", fail_code);
						return 1;
					}
					zend_string_release(expect);
				}
				/* nonce replay check */
				{
					zend_string *comp = gene_policy_cfg_str(cfg, ZEND_STRL("nonce"));
					if (comp) {
						smart_str k = {0};
						smart_str_appends(&k, "gene:rest:nonce:");
						smart_str_append(&k, Z_STR_P(nonce_v));
						smart_str_0(&k);
						if (k.s && !gene_policy_nonce_check(comp, k.s, skew * 2)) {
							smart_str_free(&k);
							gene_policy_deny(fail_msg ? ZSTR_VAL(fail_msg) : "访问未授权", fail_code);
							return 1;
						}
						smart_str_free(&k);
					}
				}
			}
		}
	}
	return 2; /* guarded REST route admitted */
}
/* }}} */

/* {{{ readonly (S12): GET on dynamic {c,a} not whitelisted => 405 */
/* Controller lookup is case-insensitive: dispatch resolves :c to a PHP
 * class name (case-insensitive), so "/admin/USER/del" must hit the 'user'
 * entry — an exact-case hash probe let any case variant skip the guard.
 * No fixed buffers either (long names used to `continue` = fail-open). */
static zval *gene_policy_readonly_find(zval *ent, const char *c, size_t clen) {
	zend_string *k;
	zval *v, *star = NULL;
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(ent), k, v) {
		if (!k) {
			continue;
		}
		if (ZSTR_LEN(k) == clen && strncasecmp(ZSTR_VAL(k), c, clen) == 0) {
			return v;
		}
		if (ZSTR_LEN(k) == 1 && ZSTR_VAL(k)[0] == '*') {
			star = v;
		}
	} ZEND_HASH_FOREACH_END();
	return star;
}

static int gene_policy_readonly_run(zval *cfg, const gene_policy_req *req) {
	const char *path = req->path;
	size_t plen = req->plen;
	zend_string *key;
	zval *ent;

	if (plen == 0 || !((req->mlen == 3 && strncasecmp(req->method, "GET", 3) == 0)
			|| (req->mlen == 4 && strncasecmp(req->method, "HEAD", 4) == 0))) {
		return 0;
	}
	/* config shape: { "/admin": { "user": ["lists","edit"], "*": ["run"] }, ... } */
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(cfg), key, ent) {
		size_t klen;
		const char *rest, *rest_end = path + plen, *seg, *p2, *a_end;
		size_t clen, alen;
		zval *al;
		if (!key || Z_TYPE_P(ent) != IS_ARRAY) {
			continue;
		}
		klen = ZSTR_LEN(key);
		while (klen > 0 && ZSTR_VAL(key)[klen - 1] == '/') {
			klen--; /* "/admin/" == "/admin" */
		}
		if (klen >= plen || strncasecmp(ZSTR_VAL(key), path, klen) != 0 || path[klen] != '/') {
			continue;
		}
		rest = path + klen + 1;
		seg = memchr(rest, '/', (size_t)(rest_end - rest));
		if (!seg) {
			continue; /* single segment: no {c}/{a} pair */
		}
		clen = (size_t)(seg - rest);
		p2 = seg + 1;
		for (a_end = p2; a_end < rest_end && *a_end != '/' && *a_end != '.'; a_end++);
		alen = (size_t)(a_end - p2);
		if (clen == 0 || alen == 0) {
			continue;
		}
		al = gene_policy_readonly_find(ent, rest, clen);
		if (al && Z_TYPE_P(al) == IS_ARRAY) {
			zval *a;
			int allowed = 0;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(al), a) {
				if (Z_TYPE_P(a) == IS_STRING && Z_STRLEN_P(a) == alen
					&& strncasecmp(Z_STRVAL_P(a), p2, alen) == 0) {
					allowed = 1;
					break;
				}
			} ZEND_HASH_FOREACH_END();
			if (!allowed) {
				gene_policy_deny("method not allowed", 405);
				return 1;
			}
		}
	} ZEND_HASH_FOREACH_END();
	return 0;
}
/* }}} */

/* {{{ security headers apply */
void gene_policy_headers_apply(void) {
	zval *cfg = gene_policy_cfg(ZEND_STRL(GENE_APPLICATION_HEADERS_CONFIG));
	zval *v;
	zend_bool secure;

	if (!cfg) {
		return;
	}
	secure = gene_request_is_secure();

	if (gene_policy_cfg_bool(cfg, ZEND_STRL("nosniff"), 1)) {
		gene_response_set_header_ex(ZEND_STRL("X-Content-Type-Options"), ZEND_STRL("nosniff"));
	}
	v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("frame"));
	if (!v || (Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) || Z_TYPE_P(v) == IS_TRUE) {
		const char *fv = "SAMEORIGIN";
		if (v && Z_TYPE_P(v) == IS_STRING) fv = Z_STRVAL_P(v);
		gene_response_set_header_ex(ZEND_STRL("X-Frame-Options"), fv, strlen(fv));
	}
	v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("referrer"));
	if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
		gene_response_set_header_ex(ZEND_STRL("Referrer-Policy"), Z_STRVAL_P(v), Z_STRLEN_P(v));
	} else if (!v) {
		gene_response_set_header_ex(ZEND_STRL("Referrer-Policy"),
			ZEND_STRL("strict-origin-when-cross-origin"));
	}
	/* HSTS: on by default for secure requests (plan §5.1); max-age only —
	 * includeSubDomains/preload are opt-in because they bind sibling hosts */
	v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("hsts"));
	if (secure) {
		if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
			gene_response_set_header_ex(ZEND_STRL("Strict-Transport-Security"), Z_STRVAL_P(v), Z_STRLEN_P(v));
		} else if (!v || Z_TYPE_P(v) == IS_TRUE) {
			gene_response_set_header_ex(ZEND_STRL("Strict-Transport-Security"), ZEND_STRL("max-age=31536000"));
		}
	}
	v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("permissions"));
	if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
		gene_response_set_header_ex(ZEND_STRL("Permissions-Policy"), Z_STRVAL_P(v), Z_STRLEN_P(v));
	}
	v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("coop"));
	if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
		gene_response_set_header_ex(ZEND_STRL("Cross-Origin-Opener-Policy"), Z_STRVAL_P(v), Z_STRLEN_P(v));
	}
	/* CSP: string verbatim, {nonce} replaced by per-request nonce. */
	v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("csp"));
	if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
		zend_string *csp = Z_STR_P(v);
		zend_string *out = zend_string_copy(csp);
		if (strstr(ZSTR_VAL(csp), "{nonce}")) {
			zend_string *nonce = gene_response_csp_nonce();
			if (nonce) {
				smart_str b = {0};
				const char *cur = ZSTR_VAL(csp), *ph;
				while ((ph = strstr(cur, "{nonce}")) != NULL) {
					smart_str_appendl(&b, cur, (size_t)(ph - cur));
					smart_str_append(&b, nonce);
					cur = ph + sizeof("{nonce}") - 1;
				}
				smart_str_appends(&b, cur);
				smart_str_0(&b);
				zend_string_release(out);
				out = b.s;
			}
		}
		if (out) {
			const char *hn = gene_policy_cfg_bool(cfg, ZEND_STRL("csp_report_only"), 0)
				? "Content-Security-Policy-Report-Only" : "Content-Security-Policy";
			gene_response_set_header_ex(hn, strlen(hn), ZSTR_VAL(out), ZSTR_LEN(out));
			zend_string_release(out);
		}
	}
	/* header removals (FPM SAPI only; Swoole responses have no preset headers) */
	v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("remove"));
	if (v && Z_TYPE_P(v) == IS_ARRAY && GENE_G(runtime_type) < 2) {
		zval *item;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(v), item) {
			if (Z_TYPE_P(item) == IS_STRING && Z_STRLEN_P(item) > 0) {
				sapi_header_line ctr = {0};
				char buf[256];
				size_t kl = Z_STRLEN_P(item);
				if (kl + 1 < sizeof(buf)) {
					memcpy(buf, Z_STRVAL_P(item), kl);
					buf[kl] = ':';
					ctr.line = buf;
					ctr.line_len = kl + 1;
					ctr.response_code = 0;
					sapi_header_op(SAPI_HEADER_DELETE, &ctr);
				}
			}
		} ZEND_HASH_FOREACH_END();
	}
}
/* }}} */

int gene_policy_check(const char *method, size_t mlen, const char *path, size_t plen) {
	gene_policy_req req;
	zval *cfg;
	int rest;

	req.method = (method && mlen) ? method : "GET";
	req.mlen = (method && mlen) ? mlen : 3;
	req.path = (path && plen) ? path : "/";
	req.plen = (path && plen) ? plen : 1;

	/* restGuard first: an admitted server-to-server REST call (2) carries no
	 * browser Origin and is authenticated by the guard, so csrf skips it */
	cfg = gene_policy_cfg(ZEND_STRL(GENE_APPLICATION_REST_CONFIG));
	rest = cfg ? gene_policy_rest_run(cfg, &req) : 0;
	if (rest == 1) {
		return 1;
	}
	cfg = gene_policy_cfg(ZEND_STRL(GENE_APPLICATION_CSRF_CONFIG));
	if (cfg && rest != 2 && gene_policy_csrf_run(cfg, &req)) {
		return 1;
	}
	cfg = gene_policy_cfg(ZEND_STRL(GENE_APPLICATION_READONLY_CONFIG));
	if (cfg && gene_policy_readonly_run(cfg, &req)) {
		return 1;
	}
	return 0;
}
/* }}} */

/* {{{ Gene\Csrf class */
/* {{{ proto static string Gene\Csrf::token([array $cfg]) */
PHP_METHOD(gene_csrf, token) {
	zval *cfg = NULL;
	zend_string *tok;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a!", &cfg) == FAILURE) {
		return;
	}
	if (!cfg) {
		cfg = gene_policy_cfg(ZEND_STRL(GENE_APPLICATION_CSRF_CONFIG));
		if (!cfg) {
			zval empty;
			array_init(&empty);
			tok = gene_csrf_session_token(&empty);
			zval_ptr_dtor(&empty);
			if (!tok) {
				zend_throw_exception_ex(NULL, 0, "Gene\\Csrf: session component unavailable");
				RETURN_THROWS();
			}
			RETURN_STR(tok);
		}
	}
	tok = gene_csrf_session_token(cfg);
	if (!tok) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Csrf: session component unavailable");
		RETURN_THROWS();
	}
	RETURN_STR(tok);
}
/* }}} */

/* {{{ proto static bool Gene\Csrf::check([string $given [, array $cfg]]) */
PHP_METHOD(gene_csrf, check) {
	zend_string *given = NULL;
	zval *cfg = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S!a!", &given, &cfg) == FAILURE) {
		return;
	}
	if (!cfg) {
		cfg = gene_policy_cfg(ZEND_STRL(GENE_APPLICATION_CSRF_CONFIG));
	}
	if (!given) {
		zval *g = getVal(GENE_REQUEST_ATTR_HEADER, ZEND_STRL("x-csrf-token"));
		if (!g || Z_TYPE_P(g) != IS_STRING) {
			g = getVal(TRACK_VARS_POST, ZEND_STRL("_token"));
		}
		if (g && Z_TYPE_P(g) == IS_STRING) {
			given = Z_STR_P(g);
		}
	}
	if (!cfg) {
		zval empty;
		zend_bool ok;
		array_init(&empty);
		ok = gene_csrf_check_token(&empty, given);
		zval_ptr_dtor(&empty);
		RETURN_BOOL(ok);
	}
	RETURN_BOOL(gene_csrf_check_token(cfg, given));
}
/* }}} */

/* {{{ proto static string Gene\Csrf::field([array $cfg]) — <input> for views */
PHP_METHOD(gene_csrf, field) {
	zval *cfg = NULL;
	zend_string *tok;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a!", &cfg) == FAILURE) {
		return;
	}
	if (!cfg) {
		cfg = gene_policy_cfg(ZEND_STRL(GENE_APPLICATION_CSRF_CONFIG));
	}
	if (!cfg) {
		zval empty;
		array_init(&empty);
		tok = gene_csrf_session_token(&empty);
		zval_ptr_dtor(&empty);
	} else {
		tok = gene_csrf_session_token(cfg);
	}
	if (!tok) {
		RETURN_EMPTY_STRING();
	}
	{
		smart_str b = {0};
		smart_str_appends(&b, "<input type=\"hidden\" name=\"_token\" value=\"");
		smart_str_append(&b, tok);
		smart_str_appends(&b, "\">");
		smart_str_0(&b);
		zend_string_release(tok);
		if (b.s) {
			RETURN_STR(b.s);
		}
		RETURN_EMPTY_STRING();
	}
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(gene_csrf_cfg_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, cfg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_csrf_check_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, given)
	ZEND_ARG_INFO(0, cfg)
ZEND_END_ARG_INFO()

const zend_function_entry gene_csrf_methods[] = {
	PHP_ME(gene_csrf, token, gene_csrf_cfg_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_csrf, check, gene_csrf_check_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_csrf, field, gene_csrf_cfg_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{ NULL, NULL, NULL }
};

GENE_MINIT_FUNCTION(policy) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Csrf", "Gene\\Csrf", gene_csrf_methods);
	gene_csrf_ce = zend_register_internal_class(&ce);
	return SUCCESS;
}
/* }}} */
