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

/* {{{ proto static array Gene\Crypto::hmacVerify(string $token, string $secret) */
PHP_METHOD(gene_crypto, hmacVerify) {
	zend_string *token, *secret, *body, *sig_part, *sig_raw, *expect, *json_s, *tmp, *raw_sig_b64;
	const char *dot, *end;
	size_t body_len, sig_len;
	zval decoded;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &token, &secret) == FAILURE) {
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
		if (exp && (Z_TYPE_P(exp) == IS_LONG || Z_TYPE_P(exp) == IS_DOUBLE)) {
			zend_long exp_ts = (Z_TYPE_P(exp) == IS_LONG) ? Z_LVAL_P(exp) : (zend_long)Z_DVAL_P(exp);
			if (exp_ts > 0 && (zend_long)time(NULL) > exp_ts) {
				zval_ptr_dtor(&decoded);
				zend_throw_exception_ex(NULL, 0, "token expired");
				RETURN_THROWS();
			}
		}
	}
	RETURN_ZVAL(&decoded, 0, 1);
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

const zend_function_entry gene_crypto_methods[] = {
	PHP_ME(gene_crypto, base64UrlEncode, gene_crypto_b64_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, base64UrlDecode, gene_crypto_b64_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, hmacToken, gene_crypto_hmac_token_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, hmacVerify, gene_crypto_hmac_verify_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, randomId, gene_crypto_random_id_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, encrypt, gene_crypto_crypt_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_crypto, decrypt, gene_crypto_crypt_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(crypto) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Crypto", "Gene\\Crypto", gene_crypto_methods);
	gene_crypto_ce = zend_register_internal_class(&ce);
	gene_crypto_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
