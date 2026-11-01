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
#include <time.h>

#include "../gene.h"
#include "../http/json.h"
#include "../http/policy.h"
#include "../tool/crypto.h"

zend_class_entry *gene_crypto_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_b64_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_hmac_token_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, payload)
	ZEND_ARG_INFO(0, secret)
	ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_hmac_verify_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, token)
	ZEND_ARG_INFO(0, secret)
	ZEND_ARG_INFO(0, leeway)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_hmac_sign_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, secret)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_hmac_check_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, sig)
	ZEND_ARG_INFO(0, secret)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_ts_skew_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, unix)
	ZEND_ARG_INFO(0, maxSkew)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_random_id_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, prefix)
	ZEND_ARG_INFO(0, bytes)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_crypt_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

/* {{{ gene_php_call_n */
static int gene_php_call_n(const char *name, size_t name_len, uint32_t argc, zval *params, zval *retval) {
	zend_function *fn;
	ZVAL_UNDEF(retval);
	fn = zend_hash_str_find_ptr(CG(function_table), name, name_len);
	if (UNEXPECTED(!fn)) {
		zend_throw_exception_ex(NULL, 0, "function %s() is not available", name);
		return FAILURE;
	}
	zend_call_known_function(fn, NULL, NULL, retval, argc, params, NULL);
	return EG(exception) ? FAILURE : SUCCESS;
}
/* }}} */

/* {{{ gene_b64url_from_b64 — in-place +/ → -_ and strip padding */
static void gene_b64url_from_b64(zend_string *s) {
	char *p = ZSTR_VAL(s);
	size_t i, n = ZSTR_LEN(s);
	for (i = 0; i < n; i++) {
		if (p[i] == '+') {
			p[i] = '-';
		} else if (p[i] == '/') {
			p[i] = '_';
		}
	}
	while (n > 0 && p[n - 1] == '=') {
		n--;
	}
	ZSTR_LEN(s) = n;
	p[n] = '\0';
}
/* }}} */

static zend_string *gene_base64_encode_raw(const char *data, size_t len) {
	zval in, out;
	ZVAL_STRINGL(&in, data, len);
	if (gene_php_call_n("base64_encode", sizeof("base64_encode") - 1, 1, &in, &out) != SUCCESS) {
		zval_ptr_dtor(&in);
		return NULL;
	}
	zval_ptr_dtor(&in);
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		zend_throw_exception_ex(NULL, 0, "base64_encode failed");
		return NULL;
	}
	return Z_STR(out);
}

static zend_string *gene_base64_decode_raw(zend_string *b64) {
	zval in, out;
	ZVAL_STR(&in, b64); /* borrow */
	if (gene_php_call_n("base64_decode", sizeof("base64_decode") - 1, 1, &in, &out) != SUCCESS) {
		return NULL;
	}
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		zend_throw_exception_ex(NULL, 0, "base64_decode failed");
		return NULL;
	}
	return Z_STR(out);
}

/* {{{ proto static string Gene\Crypto::base64UrlEncode(string $data) */
PHP_METHOD(gene_crypto, base64UrlEncode) {
	zend_string *data, *b64;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &data) == FAILURE) {
		return;
	}
	b64 = gene_base64_encode_raw(ZSTR_VAL(data), ZSTR_LEN(data));
	if (!b64) {
		RETURN_THROWS();
	}
	gene_b64url_from_b64(b64);
	RETURN_STR(b64);
}
/* }}} */

/* {{{ proto static string Gene\Crypto::base64UrlDecode(string $data) */
PHP_METHOD(gene_crypto, base64UrlDecode) {
	zend_string *data, *tmp, *raw;
	char *p;
	size_t i, n, pad;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &data) == FAILURE) {
		return;
	}
	n = ZSTR_LEN(data);
	pad = (4 - (n % 4)) % 4;
	tmp = zend_string_alloc(n + pad, 0);
	p = ZSTR_VAL(tmp);
	memcpy(p, ZSTR_VAL(data), n);
	for (i = 0; i < n; i++) {
		if (p[i] == '-') {
			p[i] = '+';
		} else if (p[i] == '_') {
			p[i] = '/';
		}
	}
	for (i = 0; i < pad; i++) {
		p[n + i] = '=';
	}
	p[n + pad] = '\0';
	ZSTR_LEN(tmp) = n + pad;
	raw = gene_base64_decode_raw(tmp);
	zend_string_release(tmp);
	if (!raw) {
		RETURN_THROWS();
	}
	RETURN_STR(raw);
}
/* }}} */

static zend_string *gene_hmac_sha256_raw(zend_string *data, zend_string *secret) {
	zval params[4], out;
	ZVAL_STRING(&params[0], "sha256");
	ZVAL_STR(&params[1], data);
	ZVAL_STR(&params[2], secret);
	ZVAL_TRUE(&params[3]);
	if (gene_php_call_n("hash_hmac", sizeof("hash_hmac") - 1, 4, params, &out) != SUCCESS) {
		zval_ptr_dtor(&params[0]);
		return NULL;
	}
	zval_ptr_dtor(&params[0]);
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		zend_throw_exception_ex(NULL, 0, "hash_hmac failed");
		return NULL;
	}
	return Z_STR(out);
}

static zend_string *gene_random_bytes(zend_long n) {
	zval in, out;
	ZVAL_LONG(&in, n);
	if (gene_php_call_n("random_bytes", sizeof("random_bytes") - 1, 1, &in, &out) != SUCCESS) {
		return NULL;
	}
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		zend_throw_exception_ex(NULL, 0, "random_bytes failed");
		return NULL;
	}
	return Z_STR(out);
}

static int gene_hash_equals(zend_string *a, zend_string *b) {
	zval params[2], out;
	int ok;
	ZVAL_STR(&params[0], a);
	ZVAL_STR(&params[1], b);
	if (gene_php_call_n("hash_equals", sizeof("hash_equals") - 1, 2, params, &out) != SUCCESS) {
		return 0;
	}
	ok = zend_is_true(&out);
	zval_ptr_dtor(&out);
	return ok;
}

/* {{{ proto static string Gene\Crypto::hmacToken(array $payload, string $secret [, int $ttl = 0]) */
PHP_METHOD(gene_crypto, hmacToken) {
	zval *payload;
	zend_string *secret, *json_s, *body_b64, *sig_raw, *sig_b64;
	zend_long ttl = 0;
	zval payload_copy, encoded;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "aS|l", &payload, &secret, &ttl) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(secret) == 0) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::hmacToken secret must not be empty");
		RETURN_THROWS();
	}
	ZVAL_DUP(&payload_copy, payload);
	if (ttl > 0) {
		add_assoc_long_ex(&payload_copy, ZEND_STRL("exp"), (zend_long)time(NULL) + ttl);
	}
	if (gene_json_encode_throw(&payload_copy, &encoded) != SUCCESS) {
		zval_ptr_dtor(&payload_copy);
		RETURN_THROWS();
	}
	zval_ptr_dtor(&payload_copy);
	json_s = Z_STR(encoded);
	body_b64 = gene_base64_encode_raw(ZSTR_VAL(json_s), ZSTR_LEN(json_s));
	zval_ptr_dtor(&encoded);
	if (!body_b64) {
		RETURN_THROWS();
	}
	gene_b64url_from_b64(body_b64);
	sig_raw = gene_hmac_sha256_raw(body_b64, secret);
	if (!sig_raw) {
		zend_string_release(body_b64);
		RETURN_THROWS();
	}
	sig_b64 = gene_base64_encode_raw(ZSTR_VAL(sig_raw), ZSTR_LEN(sig_raw));
	zend_string_release(sig_raw);
	if (!sig_b64) {
		zend_string_release(body_b64);
		RETURN_THROWS();
	}
	gene_b64url_from_b64(sig_b64);
	{
		smart_str buf = {0};
		smart_str_append(&buf, body_b64);
		smart_str_appendc(&buf, '.');
		smart_str_append(&buf, sig_b64);
		smart_str_0(&buf);
		RETVAL_STR(buf.s);
	}
	zend_string_release(body_b64);
	zend_string_release(sig_b64);
}
/* }}} */

/* {{{ proto static array Gene\Crypto::hmacVerify(string $token, string $secret [, int $leeway = 0]) */
PHP_METHOD(gene_crypto, hmacVerify) {
	zend_string *token, *secret, *body, *sig_part, *sig_raw, *expect, *json_s, *tmp, *raw_sig_b64;
	zend_long leeway = 0;
	const char *dot, *end;
	size_t body_len, sig_len;
	zval decoded;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|l", &token, &secret, &leeway) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(secret) == 0 || ZSTR_LEN(token) == 0) {
		zend_throw_exception_ex(NULL, 0, "invalid token");
		RETURN_THROWS();
	}
	end = ZSTR_VAL(token) + ZSTR_LEN(token);
	dot = end;
	while (dot > ZSTR_VAL(token) && dot[-1] != '.') {
		dot--;
	}
	if (dot == ZSTR_VAL(token) || dot[-1] != '.') {
		zend_throw_exception_ex(NULL, 0, "invalid token");
		RETURN_THROWS();
	}
	dot--; /* points at '.' */
	body_len = (size_t)(dot - ZSTR_VAL(token));
	sig_len = (size_t)(end - (dot + 1));
	if (body_len == 0 || sig_len == 0) {
		zend_throw_exception_ex(NULL, 0, "invalid token");
		RETURN_THROWS();
	}
	body = zend_string_init(ZSTR_VAL(token), body_len, 0);
	sig_part = zend_string_init(dot + 1, sig_len, 0);
	expect = gene_hmac_sha256_raw(body, secret);
	if (!expect) {
		zend_string_release(body);
		zend_string_release(sig_part);
		RETURN_THROWS();
	}
	/* decode provided sig from base64url */
	{
		char *p;
		size_t i, n = ZSTR_LEN(sig_part), pad = (4 - (n % 4)) % 4;
		tmp = zend_string_alloc(n + pad, 0);
		p = ZSTR_VAL(tmp);
		memcpy(p, ZSTR_VAL(sig_part), n);
		for (i = 0; i < n; i++) {
			if (p[i] == '-') p[i] = '+';
			else if (p[i] == '_') p[i] = '/';
		}
		for (i = 0; i < pad; i++) p[n + i] = '=';
		p[n + pad] = '\0';
		ZSTR_LEN(tmp) = n + pad;
		raw_sig_b64 = tmp;
	}
	sig_raw = gene_base64_decode_raw(raw_sig_b64);
	zend_string_release(raw_sig_b64);
	zend_string_release(sig_part);
	if (!sig_raw) {
		zend_string_release(body);
		zend_string_release(expect);
		RETURN_THROWS();
	}
	if (!gene_hash_equals(expect, sig_raw)) {
		zend_string_release(body);
		zend_string_release(expect);
		zend_string_release(sig_raw);
		zend_throw_exception_ex(NULL, 0, "invalid token");
		RETURN_THROWS();
	}
	zend_string_release(expect);
	zend_string_release(sig_raw);
	/* decode payload */
	{
		char *p;
		size_t i, n = ZSTR_LEN(body), pad = (4 - (n % 4)) % 4;
		tmp = zend_string_alloc(n + pad, 0);
		p = ZSTR_VAL(tmp);
		memcpy(p, ZSTR_VAL(body), n);
		for (i = 0; i < n; i++) {
			if (p[i] == '-') p[i] = '+';
			else if (p[i] == '_') p[i] = '/';
		}
		for (i = 0; i < pad; i++) p[n + i] = '=';
		p[n + pad] = '\0';
		ZSTR_LEN(tmp) = n + pad;
	}
	zend_string_release(body);
	json_s = gene_base64_decode_raw(tmp);
	zend_string_release(tmp);
	if (!json_s) {
		RETURN_THROWS();
	}
	if (gene_json_decode_throw(json_s, &decoded) != SUCCESS) {
		zend_string_release(json_s);
		RETURN_THROWS();
	}
	zend_string_release(json_s);
	if (Z_TYPE(decoded) != IS_ARRAY) {
		zval_ptr_dtor(&decoded);
		zend_throw_exception_ex(NULL, 0, "invalid token");
		RETURN_THROWS();
	}
	{
		zval *exp = zend_hash_str_find(Z_ARRVAL(decoded), ZEND_STRL("exp"));
		zval *nbf = zend_hash_str_find(Z_ARRVAL(decoded), ZEND_STRL("nbf"));
		zend_long now = (zend_long)time(NULL);
		if (nbf && (Z_TYPE_P(nbf) == IS_LONG || Z_TYPE_P(nbf) == IS_DOUBLE)) {
			zend_long nbf_ts = (Z_TYPE_P(nbf) == IS_LONG) ? Z_LVAL_P(nbf) : (zend_long)Z_DVAL_P(nbf);
			if (nbf_ts > 0 && now + leeway < nbf_ts) {
				zval_ptr_dtor(&decoded);
				zend_throw_exception_ex(NULL, 0, "token not yet valid");
				RETURN_THROWS();
			}
		}
		if (exp && (Z_TYPE_P(exp) == IS_LONG || Z_TYPE_P(exp) == IS_DOUBLE)) {
			zend_long exp_ts = (Z_TYPE_P(exp) == IS_LONG) ? Z_LVAL_P(exp) : (zend_long)Z_DVAL_P(exp);
			if (exp_ts > 0 && now > exp_ts + leeway) {
				zval_ptr_dtor(&decoded);
				zend_throw_exception_ex(NULL, 0, "token expired");
				RETURN_THROWS();
			}
		}
	}
	RETURN_ZVAL(&decoded, 0, 1);
}
/* }}} */

/* {{{ proto static string Gene\Crypto::hmacSign(string $data, string $secret) */
PHP_METHOD(gene_crypto, hmacSign) {
	zend_string *data, *secret, *sig_raw, *sig_b64;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &data, &secret) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(secret) == 0) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::hmacSign secret must not be empty");
		RETURN_THROWS();
	}
	sig_raw = gene_hmac_sha256_raw(data, secret);
	if (!sig_raw) {
		RETURN_THROWS();
	}
	sig_b64 = gene_base64_encode_raw(ZSTR_VAL(sig_raw), ZSTR_LEN(sig_raw));
	zend_string_release(sig_raw);
	if (!sig_b64) {
		RETURN_THROWS();
	}
	gene_b64url_from_b64(sig_b64);
	RETURN_STR(sig_b64);
}
/* }}} */

/* {{{ proto static bool Gene\Crypto::hmacCheck(string $data, string $sig, string $secret) */
PHP_METHOD(gene_crypto, hmacCheck) {
	zend_string *data, *sig_in, *secret, *sig_raw, *expect, *tmp, *raw_sig_b64;
	size_t i, n, pad;
	char *p;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SSS", &data, &sig_in, &secret) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(secret) == 0) {
		RETURN_FALSE;
	}
	expect = gene_hmac_sha256_raw(data, secret);
	if (!expect) {
		RETURN_FALSE;
	}
	n = ZSTR_LEN(sig_in);
	pad = (4 - (n % 4)) % 4;
	tmp = zend_string_alloc(n + pad, 0);
	p = ZSTR_VAL(tmp);
	memcpy(p, ZSTR_VAL(sig_in), n);
	for (i = 0; i < n; i++) {
		if (p[i] == '-') p[i] = '+';
		else if (p[i] == '_') p[i] = '/';
	}
	for (i = 0; i < pad; i++) p[n + i] = '=';
	p[n + pad] = '\0';
	ZSTR_LEN(tmp) = n + pad;
	raw_sig_b64 = tmp;
	sig_raw = gene_base64_decode_raw(raw_sig_b64);
	zend_string_release(raw_sig_b64);
	if (!sig_raw) {
		zend_string_release(expect);
		RETURN_FALSE;
	}
	RETVAL_BOOL(gene_hash_equals(expect, sig_raw));
	zend_string_release(expect);
	zend_string_release(sig_raw);
}
/* }}} */

/* {{{ proto static bool Gene\Crypto::tsSkew(int $unix [, int $maxSkew = 1800]) */
PHP_METHOD(gene_crypto, tsSkew) {
	zend_long unix_ts, max_skew = 1800;
	zend_long diff;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l", &unix_ts, &max_skew) == FAILURE) {
		return;
	}
	if (max_skew < 0) {
		max_skew = 0;
	}
	diff = (zend_long)time(NULL) - unix_ts;
	if (diff < 0) {
		diff = -diff;
	}
	RETURN_BOOL(diff <= max_skew);
}
/* }}} */

/* {{{ proto static string Gene\Crypto::randomId([string $prefix = '' [, int $bytes = 16]]) */
PHP_METHOD(gene_crypto, randomId) {
	zend_string *prefix = NULL, *raw, *hex;
	zend_long bytes = 16;
	zval in, out;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|Sl", &prefix, &bytes) == FAILURE) {
		return;
	}
	if (bytes < 1 || bytes > 64) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::randomId bytes must be 1..64");
		RETURN_THROWS();
	}
	raw = gene_random_bytes(bytes);
	if (!raw) {
		RETURN_THROWS();
	}
	ZVAL_STR(&in, raw);
	if (gene_php_call_n("bin2hex", sizeof("bin2hex") - 1, 1, &in, &out) != SUCCESS) {
		zend_string_release(raw);
		RETURN_THROWS();
	}
	zend_string_release(raw);
	if (Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&out);
		zend_throw_exception_ex(NULL, 0, "bin2hex failed");
		RETURN_THROWS();
	}
	hex = Z_STR(out);
	if (prefix && ZSTR_LEN(prefix) > 0) {
		zend_string *joined = zend_string_alloc(ZSTR_LEN(prefix) + ZSTR_LEN(hex), 0);
		memcpy(ZSTR_VAL(joined), ZSTR_VAL(prefix), ZSTR_LEN(prefix));
		memcpy(ZSTR_VAL(joined) + ZSTR_LEN(prefix), ZSTR_VAL(hex), ZSTR_LEN(hex));
		ZSTR_VAL(joined)[ZSTR_LEN(prefix) + ZSTR_LEN(hex)] = '\0';
		zend_string_release(hex);
		RETURN_STR(joined);
	}
	RETURN_STR(hex);
}
/* }}} */

#ifndef OPENSSL_RAW_DATA
#define OPENSSL_RAW_DATA 1
#endif

/* {{{ proto static string Gene\Crypto::encrypt(string $plain, string $key) */
PHP_METHOD(gene_crypto, encrypt) {
	zend_string *plain, *key, *iv, *ct, *packed, *out_b64;
	zval params[6], tag_val, retval;
	zend_function *fn;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &plain, &key) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(key) != 32) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::encrypt key must be 32 bytes (AES-256-GCM); inject from config/env, do not derive from DB passwords");
		RETURN_THROWS();
	}
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("openssl_encrypt"));
	if (UNEXPECTED(!fn)) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto requires the openssl extension");
		RETURN_THROWS();
	}
	iv = gene_random_bytes(12);
	if (!iv) {
		RETURN_THROWS();
	}
	ZVAL_STR(&params[0], plain);
	ZVAL_STRING(&params[1], "aes-256-gcm");
	ZVAL_STR(&params[2], key);
	ZVAL_LONG(&params[3], OPENSSL_RAW_DATA);
	ZVAL_STR(&params[4], iv);
	ZVAL_EMPTY_STRING(&tag_val);
	ZVAL_NEW_REF(&params[5], &tag_val);
	ZVAL_UNDEF(&retval);
	zend_call_known_function(fn, NULL, NULL, &retval, 6, params, NULL);
	zval_ptr_dtor(&params[1]);
	if (EG(exception) || Z_TYPE(retval) != IS_STRING) {
		zval_ptr_dtor(&params[5]);
		zval_ptr_dtor(&retval);
		zend_string_release(iv);
		if (!EG(exception)) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::encrypt failed");
		}
		RETURN_THROWS();
	}
	ct = Z_STR(retval);
	{
		zval *tag = Z_REFVAL(params[5]);
		size_t tag_len = (Z_TYPE_P(tag) == IS_STRING) ? Z_STRLEN_P(tag) : 0;
		if (tag_len == 0) {
			zval_ptr_dtor(&params[5]);
			zend_string_release(ct);
			zend_string_release(iv);
			zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::encrypt failed (empty GCM tag)");
			RETURN_THROWS();
		}
		packed = zend_string_alloc(ZSTR_LEN(iv) + tag_len + ZSTR_LEN(ct), 0);
		memcpy(ZSTR_VAL(packed), ZSTR_VAL(iv), ZSTR_LEN(iv));
		memcpy(ZSTR_VAL(packed) + ZSTR_LEN(iv), Z_STRVAL_P(tag), tag_len);
		memcpy(ZSTR_VAL(packed) + ZSTR_LEN(iv) + tag_len, ZSTR_VAL(ct), ZSTR_LEN(ct));
		ZSTR_VAL(packed)[ZSTR_LEN(iv) + tag_len + ZSTR_LEN(ct)] = '\0';
	}
	zval_ptr_dtor(&params[5]);
	zend_string_release(ct);
	zend_string_release(iv);
	out_b64 = gene_base64_encode_raw(ZSTR_VAL(packed), ZSTR_LEN(packed));
	zend_string_release(packed);
	if (!out_b64) {
		RETURN_THROWS();
	}
	gene_b64url_from_b64(out_b64);
	RETURN_STR(out_b64);
}
/* }}} */

/* {{{ proto static string Gene\Crypto::decrypt(string $cipher, string $key) */
PHP_METHOD(gene_crypto, decrypt) {
	zend_string *cipher, *key, *tmp, *packed, *iv, *tag, *ct;
	zval params[6], retval;
	zend_function *fn;
	size_t i, n, pad;
	char *p;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &cipher, &key) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(key) != 32) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::decrypt key must be 32 bytes (AES-256-GCM)");
		RETURN_THROWS();
	}
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("openssl_decrypt"));
	if (UNEXPECTED(!fn)) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto requires the openssl extension");
		RETURN_THROWS();
	}
	n = ZSTR_LEN(cipher);
	pad = (4 - (n % 4)) % 4;
	tmp = zend_string_alloc(n + pad, 0);
	p = ZSTR_VAL(tmp);
	memcpy(p, ZSTR_VAL(cipher), n);
	for (i = 0; i < n; i++) {
		if (p[i] == '-') p[i] = '+';
		else if (p[i] == '_') p[i] = '/';
	}
	for (i = 0; i < pad; i++) p[n + i] = '=';
	p[n + pad] = '\0';
	ZSTR_LEN(tmp) = n + pad;
	packed = gene_base64_decode_raw(tmp);
	zend_string_release(tmp);
	if (!packed) {
		RETURN_THROWS();
	}
	/* iv(12) + tag(16) + ciphertext */
	if (ZSTR_LEN(packed) < 12 + 16) {
		zend_string_release(packed);
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::decrypt: ciphertext too short");
		RETURN_THROWS();
	}
	iv = zend_string_init(ZSTR_VAL(packed), 12, 0);
	tag = zend_string_init(ZSTR_VAL(packed) + 12, 16, 0);
	ct = zend_string_init(ZSTR_VAL(packed) + 28, ZSTR_LEN(packed) - 28, 0);
	zend_string_release(packed);

	ZVAL_STR(&params[0], ct);
	ZVAL_STRING(&params[1], "aes-256-gcm");
	ZVAL_STR(&params[2], key);
	ZVAL_LONG(&params[3], OPENSSL_RAW_DATA);
	ZVAL_STR(&params[4], iv);
	ZVAL_STR(&params[5], tag);
	ZVAL_UNDEF(&retval);
	zend_call_known_function(fn, NULL, NULL, &retval, 6, params, NULL);
	zval_ptr_dtor(&params[1]);
	zend_string_release(iv);
	zend_string_release(tag);
	zend_string_release(ct);
	if (EG(exception) || Z_TYPE(retval) != IS_STRING) {
		zval_ptr_dtor(&retval);
		if (!EG(exception)) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::decrypt failed");
		}
		RETURN_THROWS();
	}
	RETURN_STR(Z_STR(retval));
}
/* }}} */

/* ===================================================================
 * [GENE_SEC:2026-11-01 S9] Keyring + seal/unseal + derive + signRequest
 * [GENE_SEC:2026-11-01 S10] TOTP + secure random verification codes
 * =================================================================== */

#define GENE_CRYPTO_KEYRING        "keyring"
#define GENE_CRYPTO_KEYRING_ACTIVE "keyringActive"

static zval *gene_crypto_keyring_arr(void) {
	zval *v = zend_read_static_property(gene_crypto_ce, ZEND_STRL(GENE_CRYPTO_KEYRING), 1);
	return (v && Z_TYPE_P(v) == IS_ARRAY) ? v : NULL;
}

static zend_string *gene_crypto_active_kid(void) {
	zval *v = zend_read_static_property(gene_crypto_ce, ZEND_STRL(GENE_CRYPTO_KEYRING_ACTIVE), 1);
	return (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) ? Z_STR_P(v) : NULL;
}

/* Fetch the 32-byte key for kid (NULL kid => active). */
static zend_string *gene_crypto_ring_key(zend_string *kid) {
	zval *ring = gene_crypto_keyring_arr();
	zval *k;
	if (!kid) {
		kid = gene_crypto_active_kid();
	}
	if (!ring || !kid) {
		return NULL;
	}
	k = zend_hash_find(Z_ARRVAL_P(ring), kid);
	if (k && Z_TYPE_P(k) == IS_STRING && Z_STRLEN_P(k) == 32) {
		return Z_STR_P(k);
	}
	return NULL;
}

/* {{{ proto static bool Gene\Crypto::keyring(array $keys [, string $active])
 * Registers {'kid' => 32-byte key, ...}; $active defaults to the first kid.
 * Keys live on a class static prop (per-request under FPM, worker-lifetime
 * under Swoole bootstrap) and are NEVER exposed back to userland. */
PHP_METHOD(gene_crypto, keyring) {
	zval *keys;
	zend_string *active = NULL;
	zend_string *kid, *first = NULL;
	zval *k;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|S!", &keys, &active) == FAILURE) {
		return;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(keys), kid, k) {
		if (!kid || Z_TYPE_P(k) != IS_STRING || Z_STRLEN_P(k) != 32) {
			zend_argument_value_error(1, "values must be 32-byte AES-256 keys indexed by kid");
			RETURN_THROWS();
		}
		if (!first) {
			first = kid;
		}
	} ZEND_HASH_FOREACH_END();
	if (!first) {
		zend_argument_value_error(1, "must contain at least one key");
		RETURN_THROWS();
	}
	if (!active) {
		active = first;
	}
	if (!zend_hash_exists(Z_ARRVAL_P(keys), active)) {
		zend_argument_value_error(2, "must be a kid present in the keyring");
		RETURN_THROWS();
	}
	zend_update_static_property(gene_crypto_ce, ZEND_STRL(GENE_CRYPTO_KEYRING), keys);
	{
		zval av;
		ZVAL_STR(&av, active);
		zend_update_static_property(gene_crypto_ce, ZEND_STRL(GENE_CRYPTO_KEYRING_ACTIVE), &av);
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto static string Gene\Crypto::derive(string $ctx [, int $len = 32])
 * HKDF-expand style subkey: HMAC-SHA256(active_key, "gene:derive\0" + ctx + 0x01)
 * truncated to $len (1..32). Each distinct ctx yields an independent key. */
PHP_METHOD(gene_crypto, derive) {
	zend_string *ctx, *key, *mac;
	zend_long len = 32;
	smart_str msg = {0};
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &ctx, &len) == FAILURE) {
		return;
	}
	if (len < 1 || len > 32) {
		zend_argument_value_error(2, "must be between 1 and 32");
		RETURN_THROWS();
	}
	key = gene_crypto_ring_key(NULL);
	if (!key) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::derive requires Crypto::keyring()");
		RETURN_THROWS();
	}
	smart_str_appends(&msg, "gene:derive");
	smart_str_appendc(&msg, '\0');
	smart_str_append(&msg, ctx);
	smart_str_appendc(&msg, '\x01');
	smart_str_0(&msg);
	mac = gene_hmac_sha256_raw(msg.s, key);
	smart_str_free(&msg);
	if (!mac) {
		RETURN_THROWS();
	}
	if ((size_t)len < ZSTR_LEN(mac)) {
		ZSTR_LEN(mac) = (size_t)len;
		ZSTR_VAL(mac)[len] = '\0';
	}
	RETURN_STR(mac);
}
/* }}} */

/* AES-256-GCM with AAD "gene1.<kid>.<ctx>"; wire form "g1.<kid>.<b64url>" */
zend_string *gene_crypto_seal_str(zend_string *plain, zend_string *ctx) {
	zend_string *kid, *key, *iv, *ct, *packed, *out_b64, *aad, *joined;
	zval params[8], tag_val, retval;
	zend_function *fn;
	size_t tag_len;

	kid = gene_crypto_active_kid();
	key = gene_crypto_ring_key(kid);
	if (!kid || !key) {
		return NULL;
	}
	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("openssl_encrypt"));
	if (UNEXPECTED(!fn)) {
		return NULL;
	}
	iv = gene_random_bytes(12);
	if (!iv) {
		return NULL;
	}
	aad = zend_string_alloc(6 + ZSTR_LEN(kid) + 1 + (ctx ? ZSTR_LEN(ctx) : 0), 0);
	memcpy(ZSTR_VAL(aad), "gene1.", 6);
	memcpy(ZSTR_VAL(aad) + 6, ZSTR_VAL(kid), ZSTR_LEN(kid));
	ZSTR_VAL(aad)[6 + ZSTR_LEN(kid)] = '.';
	if (ctx && ZSTR_LEN(ctx)) {
		memcpy(ZSTR_VAL(aad) + 7 + ZSTR_LEN(kid), ZSTR_VAL(ctx), ZSTR_LEN(ctx));
	}
	ZSTR_VAL(aad)[ZSTR_LEN(aad)] = '\0';

	ZVAL_STR(&params[0], plain);
	ZVAL_STRING(&params[1], "aes-256-gcm");
	ZVAL_STR(&params[2], key);
	ZVAL_LONG(&params[3], OPENSSL_RAW_DATA);
	ZVAL_STR(&params[4], iv);
	ZVAL_EMPTY_STRING(&tag_val);
	ZVAL_NEW_REF(&params[5], &tag_val);
	ZVAL_STR(&params[6], aad);
	ZVAL_LONG(&params[7], 16);
	ZVAL_UNDEF(&retval);
	zend_call_known_function(fn, NULL, NULL, &retval, 8, params, NULL);
	zval_ptr_dtor(&params[1]);
	zend_string_release(aad);
	if (EG(exception) || Z_TYPE(retval) != IS_STRING) {
		zval_ptr_dtor(&params[5]);
		zval_ptr_dtor(&retval);
		zend_string_release(iv);
		return NULL;
	}
	ct = Z_STR(retval);
	{
		zval *tag = Z_REFVAL(params[5]);
		tag_len = (Z_TYPE_P(tag) == IS_STRING) ? Z_STRLEN_P(tag) : 0;
		if (tag_len == 0) {
			zval_ptr_dtor(&params[5]);
			zend_string_release(ct);
			zend_string_release(iv);
			return NULL;
		}
		packed = zend_string_alloc(ZSTR_LEN(iv) + tag_len + ZSTR_LEN(ct), 0);
		memcpy(ZSTR_VAL(packed), ZSTR_VAL(iv), ZSTR_LEN(iv));
		memcpy(ZSTR_VAL(packed) + ZSTR_LEN(iv), Z_STRVAL_P(tag), tag_len);
		memcpy(ZSTR_VAL(packed) + ZSTR_LEN(iv) + tag_len, ZSTR_VAL(ct), ZSTR_LEN(ct));
		ZSTR_VAL(packed)[ZSTR_LEN(iv) + tag_len + ZSTR_LEN(ct)] = '\0';
	}
	zval_ptr_dtor(&params[5]);
	zend_string_release(ct);
	zend_string_release(iv);
	out_b64 = gene_base64_encode_raw(ZSTR_VAL(packed), ZSTR_LEN(packed));
	zend_string_release(packed);
	if (!out_b64) {
		return NULL;
	}
	gene_b64url_from_b64(out_b64);
	joined = zend_string_alloc(3 + ZSTR_LEN(kid) + 1 + ZSTR_LEN(out_b64), 0);
	memcpy(ZSTR_VAL(joined), "g1.", 3);
	memcpy(ZSTR_VAL(joined) + 3, ZSTR_VAL(kid), ZSTR_LEN(kid));
	ZSTR_VAL(joined)[3 + ZSTR_LEN(kid)] = '.';
	memcpy(ZSTR_VAL(joined) + 4 + ZSTR_LEN(kid), ZSTR_VAL(out_b64), ZSTR_LEN(out_b64));
	ZSTR_VAL(joined)[ZSTR_LEN(joined)] = '\0';
	zend_string_release(out_b64);
	return joined;
}

/* Returns NULL on any tamper/format/key failure (never throws). */
zend_string *gene_crypto_unseal_str(zend_string *cipher, zend_string *ctx) {
	const char *s, *dot2, *end;
	zend_string *kid, *key, *aad, *tmp, *packed, *iv, *tag, *ct;
	zval params[8], retval;
	zend_function *fn;
	size_t n, pad, i;
	char *p;

	if (!cipher || ZSTR_LEN(cipher) < 8 || memcmp(ZSTR_VAL(cipher), "g1.", 3) != 0) {
		return NULL;
	}
	s = ZSTR_VAL(cipher) + 3;
	end = ZSTR_VAL(cipher) + ZSTR_LEN(cipher);
	dot2 = memchr(s, '.', (size_t)(end - s));
	if (!dot2 || dot2 == s || (size_t)(dot2 - s) > 64 || dot2 + 1 >= end) {
		return NULL;
	}
	kid = zend_string_init(s, (size_t)(dot2 - s), 0);
	key = gene_crypto_ring_key(kid);
	if (!key) {
		zend_string_release(kid);
		return NULL;
	}
	n = (size_t)(end - dot2 - 1);
	pad = (4 - (n % 4)) % 4;
	tmp = zend_string_alloc(n + pad, 0);
	p = ZSTR_VAL(tmp);
	memcpy(p, dot2 + 1, n);
	for (i = 0; i < n; i++) {
		if (p[i] == '-') p[i] = '+';
		else if (p[i] == '_') p[i] = '/';
	}
	for (i = 0; i < pad; i++) p[n + i] = '=';
	p[n + pad] = '\0';
	ZSTR_LEN(tmp) = n + pad;
	packed = gene_base64_decode_raw(tmp);
	zend_string_release(tmp);
	if (!packed || ZSTR_LEN(packed) < 12 + 16) {
		if (packed) zend_string_release(packed);
		zend_string_release(kid);
		return NULL;
	}
	aad = zend_string_alloc(6 + ZSTR_LEN(kid) + 1 + (ctx ? ZSTR_LEN(ctx) : 0), 0);
	memcpy(ZSTR_VAL(aad), "gene1.", 6);
	memcpy(ZSTR_VAL(aad) + 6, ZSTR_VAL(kid), ZSTR_LEN(kid));
	ZSTR_VAL(aad)[6 + ZSTR_LEN(kid)] = '.';
	if (ctx && ZSTR_LEN(ctx)) {
		memcpy(ZSTR_VAL(aad) + 7 + ZSTR_LEN(kid), ZSTR_VAL(ctx), ZSTR_LEN(ctx));
	}
	ZSTR_VAL(aad)[ZSTR_LEN(aad)] = '\0';
	zend_string_release(kid);

	fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("openssl_decrypt"));
	if (UNEXPECTED(!fn)) {
		zend_string_release(aad);
		zend_string_release(packed);
		return NULL;
	}
	iv = zend_string_init(ZSTR_VAL(packed), 12, 0);
	tag = zend_string_init(ZSTR_VAL(packed) + 12, 16, 0);
	ct = zend_string_init(ZSTR_VAL(packed) + 28, ZSTR_LEN(packed) - 28, 0);
	zend_string_release(packed);
	ZVAL_STR(&params[0], ct);
	ZVAL_STRING(&params[1], "aes-256-gcm");
	ZVAL_STR(&params[2], key);
	ZVAL_LONG(&params[3], OPENSSL_RAW_DATA);
	ZVAL_STR(&params[4], iv);
	ZVAL_STR(&params[5], tag);
	ZVAL_STR(&params[6], aad);
	ZVAL_LONG(&params[7], 16);
	ZVAL_UNDEF(&retval);
	zend_call_known_function(fn, NULL, NULL, &retval, 8, params, NULL);
	zval_ptr_dtor(&params[1]);
	zend_string_release(aad);
	zend_string_release(iv);
	zend_string_release(tag);
	zend_string_release(ct);
	if (EG(exception)) {
		zend_clear_exception();
		zval_ptr_dtor(&retval);
		return NULL;
	}
	if (Z_TYPE(retval) != IS_STRING) {
		zval_ptr_dtor(&retval);
		return NULL;
	}
	return Z_STR(retval);
}

/* {{{ proto static string Gene\Crypto::seal(string $plain [, string $ctx]) */
PHP_METHOD(gene_crypto, seal) {
	zend_string *plain, *ctx = NULL, *out;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S!", &plain, &ctx) == FAILURE) {
		return;
	}
	out = gene_crypto_seal_str(plain, ctx);
	if (!out) {
		if (!EG(exception)) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::seal requires Crypto::keyring() and openssl");
		}
		RETURN_THROWS();
	}
	RETURN_STR(out);
}
/* }}} */

/* {{{ proto static ?string Gene\Crypto::unseal(string $cipher [, string $ctx]) */
PHP_METHOD(gene_crypto, unseal) {
	zend_string *cipher, *ctx = NULL, *out;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S!", &cipher, &ctx) == FAILURE) {
		return;
	}
	out = gene_crypto_unseal_str(cipher, ctx);
	if (!out) {
		RETURN_NULL();
	}
	RETURN_STR(out);
}
/* }}} */

/* {{{ proto static array Gene\Crypto::signRequest(string $method, string $path [, array $opts])
 * Produces ['ts'=>..,'nonce'=>..,'sign'=>..] for the @restGuard HMAC scheme.
 * opts: secret (32B, else keyring 'rest'/active), body, ts, nonce. */
PHP_METHOD(gene_crypto, signRequest) {
	zend_string *method, *path;
	zval *opts = NULL;
	zend_string *secret = NULL, *body, *nonce, *sign;
	zend_long ts;
	zval *v;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|a!", &method, &path, &opts) == FAILURE) {
		return;
	}
	ts = (zend_long)time(NULL);
	body = ZSTR_EMPTY_ALLOC();
	nonce = NULL;
	if (opts) {
		v = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("secret"));
		if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
			secret = Z_STR_P(v);
		}
		v = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("body"));
		if (v && Z_TYPE_P(v) == IS_STRING) {
			body = Z_STR_P(v);
		}
		v = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("ts"));
		if (v && Z_TYPE_P(v) == IS_LONG) {
			ts = Z_LVAL_P(v);
		}
		v = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("nonce"));
		if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0) {
			nonce = Z_STR_P(v);
		}
	}
	if (!secret) {
		zend_string *kid;
		zval *ring = gene_crypto_keyring_arr();
		if (ring) {
			kid = zend_string_init(ZEND_STRL("rest"), 0);
			secret = gene_crypto_ring_key(kid);
			zend_string_release(kid);
		}
		if (!secret) {
			secret = gene_crypto_ring_key(NULL);
		}
	}
	if (!secret) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Crypto::signRequest requires opts.secret or Crypto::keyring()");
		RETURN_THROWS();
	}
	if (!nonce) {
		nonce = gene_random_bytes(12);
		if (!nonce) {
			RETURN_THROWS();
		}
		{
			zend_string *b = gene_base64_encode_raw(ZSTR_VAL(nonce), ZSTR_LEN(nonce));
			zend_string_release(nonce);
			if (!b) {
				RETURN_THROWS();
			}
			gene_b64url_from_b64(b);
			nonce = b;
		}
	}
	sign = gene_policy_sign_request(secret, ZSTR_VAL(method), ZSTR_LEN(method),
		ZSTR_VAL(path), ZSTR_LEN(path), ts, nonce, body);
	if (!sign) {
		zend_string_release(nonce);
		RETURN_THROWS();
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL("ts"), ts);
	add_assoc_str_ex(return_value, ZEND_STRL("nonce"), nonce);
	add_assoc_str_ex(return_value, ZEND_STRL("sign"), sign);
}
/* }}} */

/* {{{ S10: base32 (RFC 4648) helpers + TOTP + randomCode */
static const char gene_b32_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static zend_string *gene_b32_encode(const unsigned char *in, size_t n) {
	zend_string *out = zend_string_alloc(n * 8 / 5 + 8, 0);
	char *o = ZSTR_VAL(out);
	size_t i = 0, olen = 0;
	unsigned int buf = 0;
	int bits = 0;
	for (i = 0; i < n; i++) {
		buf = (buf << 8) | in[i];
		bits += 8;
		while (bits >= 5) {
			o[olen++] = gene_b32_alpha[(buf >> (bits - 5)) & 31];
			bits -= 5;
		}
	}
	if (bits > 0) {
		o[olen++] = gene_b32_alpha[(buf << (5 - bits)) & 31];
	}
	o[olen] = '\0';
	ZSTR_LEN(out) = olen;
	return out;
}

static int gene_b32_decode(zend_string *in, zend_string **out) {
	zend_string *r;
	unsigned char *o;
	size_t i, olen = 0;
	unsigned int buf = 0;
	int bits = 0;
	r = zend_string_alloc(ZSTR_LEN(in) * 5 / 8 + 4, 0);
	o = (unsigned char *)ZSTR_VAL(r);
	for (i = 0; i < ZSTR_LEN(in); i++) {
		unsigned char c = (unsigned char)ZSTR_VAL(in)[i];
		unsigned int v;
		if (c >= 'A' && c <= 'Z') v = c - 'A';
		else if (c >= 'a' && c <= 'z') v = c - 'a';
		else if (c >= '2' && c <= '7') v = c - '2' + 26;
		else if (c == '=' || c == ' ' || c == '-') continue;
		else {
			zend_string_release(r);
			return 0;
		}
		buf = (buf << 5) | v;
		bits += 5;
		if (bits >= 8) {
			o[olen++] = (unsigned char)(buf >> (bits - 8));
			bits -= 8;
		}
	}
	o[olen] = '\0';
	ZSTR_LEN(r) = olen;
	*out = r;
	return 1;
}

static zend_long gene_totp_code(zend_string *secret_raw, zend_long counter, zend_long digits) {
	unsigned char ctr[8];
	zval params[4], out;
	zend_string *mac;
	const unsigned char *m;
	size_t off;
	zend_long code, mod = 1;
	int i;
	for (i = 7; i >= 0; i--) {
		ctr[i] = (unsigned char)(counter & 0xff);
		counter >>= 8;
	}
	ZVAL_STRING(&params[0], "sha1");
	ZVAL_STRINGL(&params[1], (const char *)ctr, 8);
	ZVAL_STR(&params[2], secret_raw);
	ZVAL_TRUE(&params[3]);
	if (gene_php_call_n("hash_hmac", sizeof("hash_hmac") - 1, 4, params, &out) != SUCCESS) {
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
		return -1;
	}
	zval_ptr_dtor(&params[0]);
	zval_ptr_dtor(&params[1]);
	if (Z_TYPE(out) != IS_STRING || Z_STRLEN(out) != 20) {
		zval_ptr_dtor(&out);
		return -1;
	}
	mac = Z_STR(out);
	m = (const unsigned char *)ZSTR_VAL(mac);
	off = m[19] & 0x0f;
	code = ((zend_long)(m[off] & 0x7f) << 24) | ((zend_long)m[off + 1] << 16)
		| ((zend_long)m[off + 2] << 8) | (zend_long)m[off + 3];
	zend_string_release(mac);
	for (i = 0; i < digits; i++) {
		mod *= 10;
	}
	return code % mod;
}

/* {{{ proto static string Gene\Crypto::totpSecret([int $bytes = 20]) */
PHP_METHOD(gene_crypto, totpSecret) {
	zend_long bytes = 20;
	zend_string *raw;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &bytes) == FAILURE) {
		return;
	}
	if (bytes < 10 || bytes > 64) {
		zend_argument_value_error(1, "must be between 10 and 64");
		RETURN_THROWS();
	}
	raw = gene_random_bytes(bytes);
	if (!raw) {
		RETURN_THROWS();
	}
	{
		zend_string *b32 = gene_b32_encode((const unsigned char *)ZSTR_VAL(raw), ZSTR_LEN(raw));
		zend_string_release(raw);
		RETURN_STR(b32);
	}
}
/* }}} */

/* {{{ proto static string Gene\Crypto::totp(string $secret [, int $ts [, int $step [, int $digits]]]) */
PHP_METHOD(gene_crypto, totp) {
	zend_string *secret, *raw;
	zend_long ts = 0, step = 30, digits = 6, code;
	char buf[16];
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|lll", &secret, &ts, &step, &digits) == FAILURE) {
		return;
	}
	if (step < 1 || step > 300 || digits < 6 || digits > 8) {
		zend_argument_value_error(3, "invalid step/digits range");
		RETURN_THROWS();
	}
	if (!gene_b32_decode(secret, &raw)) {
		zend_argument_value_error(1, "must be a base32 TOTP secret");
		RETURN_THROWS();
	}
	if (ts <= 0) {
		ts = (zend_long)time(NULL);
	}
	code = gene_totp_code(raw, ts / step, digits);
	zend_string_release(raw);
	if (code < 0) {
		RETURN_THROWS();
	}
	{
		int l = snprintf(buf, sizeof(buf), "%0*lld", (int)digits, (long long)code);
		RETURN_STRINGL(buf, (size_t)l);
	}
}
/* }}} */

/* {{{ proto static bool Gene\Crypto::totpVerify(string $secret, string $code [, int $window [, int $step [, int $digits [, array $opts]]]])
 * opts['used'] => store object (Memory/Redis/memcached-shaped): marks each
 * accepted code single-use for step*(2*window+1)s so the same code in the
 * same window cannot be replayed. */
PHP_METHOD(gene_crypto, totpVerify) {
	zend_string *secret, *code_s, *raw;
	zval *opts = NULL, *store = NULL;
	zend_long window = 1, step = 30, digits = 6;
	zend_long given, ts, w, counter;
	char *endp;
	zend_bool ok = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|llla!", &secret, &code_s, &window, &step, &digits, &opts) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(code_s) == 0 || ZSTR_LEN(code_s) > 8) {
		RETURN_FALSE;
	}
	if (step < 1 || step > 300 || digits < 6 || digits > 8 || window < 0 || window > 10) {
		RETURN_FALSE;
	}
	/* strict: exactly `digits` ASCII digits (strtol tolerates whitespace/sign) */
	if (ZSTR_LEN(code_s) != (size_t)digits) {
		RETURN_FALSE;
	}
	{
		size_t ci;
		for (ci = 0; ci < ZSTR_LEN(code_s); ci++) {
			if (ZSTR_VAL(code_s)[ci] < '0' || ZSTR_VAL(code_s)[ci] > '9') {
				RETURN_FALSE;
			}
		}
	}
	given = ZEND_STRTOL(ZSTR_VAL(code_s), &endp, 10);
	if (endp != ZSTR_VAL(code_s) + ZSTR_LEN(code_s)) {
		RETURN_FALSE;
	}
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		store = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("used"));
		if (store && Z_TYPE_P(store) != IS_OBJECT) {
			store = NULL;
		}
	}
	if (!gene_b32_decode(secret, &raw)) {
		RETURN_FALSE;
	}
	ts = (zend_long)time(NULL) / step;
	for (w = -window; w <= window; w++) {
		counter = ts + w;
		if (counter < 0) {
			continue;
		}
		if (gene_totp_code(raw, counter, digits) == given) {
			ok = 1;
			break;
		}
	}
	if (ok && store) {
		/* single-use: "gene:totp:<sha1(secret)>" marked for the full window —
		 * a second verify with the same code in the window fails */
		smart_str k = {0};
		smart_str_appends(&k, "gene:totp:");
		smart_str_append(&k, secret);
		smart_str_0(&k);
		if (k.s) {
			if (gene_store_once(store, k.s, step * (2 * window + 1)) != 1) {
				smart_str_free(&k);
				zend_string_release(raw);
				RETURN_FALSE; /* replayed or store refused the marker */
			}
			smart_str_free(&k);
		}
	}
	zend_string_release(raw);
	RETURN_BOOL(ok);
}
/* }}} */

/* {{{ proto static string Gene\Crypto::randomCode(int $len [, string $alphabet])
 * Uniform rejection sampling over $alphabet — CAPTCHA/2FA codes, replaces
 * application rand()/mt_rand() code paths. */
PHP_METHOD(gene_crypto, randomCode) {
	zend_long len = 6;
	zend_string *alpha = NULL, *raw;
	const char *chars;
	size_t clen, i;
	unsigned int limit;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|lS!", &len, &alpha) == FAILURE) {
		return;
	}
	if (len < 1 || len > 128) {
		zend_argument_value_error(1, "must be between 1 and 128");
		RETURN_THROWS();
	}
	if (!alpha || ZSTR_LEN(alpha) == 0) {
		chars = "0123456789";
		clen = 10;
	} else {
		if (ZSTR_LEN(alpha) < 2 || ZSTR_LEN(alpha) > 256) {
			zend_argument_value_error(2, "must be between 2 and 256 chars");
			RETURN_THROWS();
		}
		chars = ZSTR_VAL(alpha);
		clen = ZSTR_LEN(alpha);
	}
	/* largest multiple of clen <= 256 — bytes >= limit are rejected (unbiased) */
	limit = 256 - (256 % clen);
	raw = zend_string_alloc((size_t)len, 0);
	{
		size_t filled = 0;
		while (filled < (size_t)len) {
			zend_string *batch = gene_random_bytes((zend_long)((size_t)len - filled) * 2);
			size_t j;
			if (!batch) {
				zend_string_release(raw);
				RETURN_THROWS();
			}
			for (j = 0; j < ZSTR_LEN(batch) && filled < (size_t)len; j++) {
				unsigned char b = (unsigned char)ZSTR_VAL(batch)[j];
				if (b < limit) {
					ZSTR_VAL(raw)[filled++] = chars[b % clen];
				}
			}
			zend_string_release(batch);
		}
	}
	ZSTR_VAL(raw)[len] = '\0';
	RETURN_STR(raw);
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_keyring_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, keys, 0)
	ZEND_ARG_INFO(0, active)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_derive_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, ctx)
	ZEND_ARG_INFO(0, len)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_seal_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, plain)
	ZEND_ARG_INFO(0, ctx)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_sign_request_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, opts)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_totp_secret_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, bytes)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_totp_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, secret)
	ZEND_ARG_INFO(0, ts)
	ZEND_ARG_INFO(0, step)
	ZEND_ARG_INFO(0, digits)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_totp_verify_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, secret)
	ZEND_ARG_INFO(0, code)
	ZEND_ARG_INFO(0, window)
	ZEND_ARG_INFO(0, step)
	ZEND_ARG_INFO(0, digits)
	ZEND_ARG_INFO(0, opts)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_crypto_random_code_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, len)
	ZEND_ARG_INFO(0, alphabet)
ZEND_END_ARG_INFO()

const zend_function_entry gene_crypto_methods[] = {
	PHP_ME(gene_crypto, base64UrlEncode, gene_crypto_b64_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, base64UrlDecode, gene_crypto_b64_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, hmacToken, gene_crypto_hmac_token_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, hmacVerify, gene_crypto_hmac_verify_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, hmacSign, gene_crypto_hmac_sign_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, hmacCheck, gene_crypto_hmac_check_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, tsSkew, gene_crypto_ts_skew_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, randomId, gene_crypto_random_id_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, encrypt, gene_crypto_crypt_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, decrypt, gene_crypto_crypt_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	/* [GENE_SEC:2026-11-01] S9 keyring/seal/derive/signRequest + S10 totp/randomCode. */
	PHP_ME(gene_crypto, keyring, gene_crypto_keyring_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, derive, gene_crypto_derive_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, seal, gene_crypto_seal_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, unseal, gene_crypto_seal_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, signRequest, gene_crypto_sign_request_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, totpSecret, gene_crypto_totp_secret_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, totp, gene_crypto_totp_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, totpVerify, gene_crypto_totp_verify_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, randomCode, gene_crypto_random_code_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(crypto) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Crypto", "Gene\\Crypto", gene_crypto_methods);
	gene_crypto_ce = zend_register_internal_class(&ce);
	gene_crypto_ce->ce_flags |= ZEND_ACC_FINAL;
	/* [GENE_SEC:2026-11-01 S9] Keyring state (config scope, never per-request data). */
	zend_declare_property_null(gene_crypto_ce, ZEND_STRL(GENE_CRYPTO_KEYRING), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_null(gene_crypto_ce, ZEND_STRL(GENE_CRYPTO_KEYRING_ACTIVE), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	return SUCCESS;
}
