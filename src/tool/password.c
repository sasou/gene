/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01 S6] Unified password API.
 * One place for: modern password_hash/verify, legacy md5(salt.sha1(salt.pwd))
 * verification (the 6-file copy-pasted app format), format auto-detection
 * (verifyAny), needs-rehash flags for transparent upgrade, and a dummy-verify
 * path for unknown-user timing resistance.
 * All hashing itself delegates to PHP builtins via the crypto.c-style call
 * helper — the framework value is the unified contract, not new crypto. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include "zend_smart_str.h"

#include "../gene.h"
#include "../tool/password.h"

zend_class_entry *gene_password_ce;

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(gene_password_hash_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_ARRAY_INFO(0, opts, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_password_verify_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_ARRAY_INFO(0, opts, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_password_legacy_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, salt)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_password_any_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_ARRAY_INFO(0, opts, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_password_rehash_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_ARRAY_INFO(0, opts, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_password_dummy_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ php-builtin call helper (crypto.c convention) */
static int password_call(const char *name, size_t name_len, uint32_t argc, zval *params, zval *retval) {
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

/* opts array → password_hash(algo, opts). 'algo' accepts the constant name
 * string ("bcrypt","argon2i","argon2id","default") or its int value;
 * everything else in opts is forwarded verbatim as the options array. */
static int gene_password_hash_call(zend_string *pwd, zval *opts, zval *retval) {
	zval params[3];
	const char *algo = NULL;
	zval *av = NULL;

	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		av = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("algo"));
	}
	if (av) {
		if (Z_TYPE_P(av) == IS_STRING) {
			if (zend_string_equals_literal_ci(Z_STR_P(av), "default")
				|| zend_string_equals_literal_ci(Z_STR_P(av), "bcrypt")) {
				algo = "2y";
			} else if (zend_string_equals_literal_ci(Z_STR_P(av), "argon2i")) {
				algo = "argon2i";
			} else if (zend_string_equals_literal_ci(Z_STR_P(av), "argon2id")) {
				algo = "argon2id";
			} else {
				zend_argument_value_error(2, "algo must be default/bcrypt/argon2i/argon2id");
				return FAILURE;
			}
		} else if (Z_TYPE_P(av) == IS_LONG) {
			/* int constant passthrough: 1="2y" 2="argon2i" 3="argon2id" */
			static const char *map[] = {NULL, "2y", "argon2i", "argon2id"};
			if (Z_LVAL_P(av) < 1 || Z_LVAL_P(av) > 3) {
				zend_argument_value_error(2, "algo int must be 1..3");
				return FAILURE;
			}
			algo = map[Z_LVAL_P(av)];
		}
	}
	ZVAL_STR(&params[0], pwd);
	if (algo) {
		ZVAL_STRING(&params[1], algo);
	} else {
		ZVAL_NULL(&params[1]); /* NULL → PASSWORD_DEFAULT */
	}
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		/* strip our 'algo' key; forward the rest (cost/memory_cost/threads...) */
		zval fwd;
		array_init(&fwd);
		{
			zend_string *k;
			zval *v;
			ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(opts), k, v) {
				if (k && zend_string_equals_literal(k, "algo")) {
					continue;
				}
				if (k) {
					Z_TRY_ADDREF_P(v);
					zend_hash_update(Z_ARRVAL(fwd), k, v);
				}
			} ZEND_HASH_FOREACH_END();
		}
		int rc;
		ZVAL_ARR(&params[2], Z_ARR(fwd));
		rc = password_call(ZEND_STRL("password_hash"), 3, params, retval);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&params[2]);
		return rc;
	}
	{
		int rc;
		ZVAL_NULL(&params[2]);
		rc = password_call(ZEND_STRL("password_hash"), 2, params, retval);
		zval_ptr_dtor(&params[1]);
		return rc;
	}
}

/* {{{ shared helpers */
static int gene_password_equals(zend_string *a, zend_string *b) {
	zval p[2], r;
	int ok = 0;
	ZVAL_STR(&p[0], a);
	ZVAL_STR(&p[1], b);
	if (password_call(ZEND_STRL("hash_equals"), 2, p, &r) == SUCCESS) {
		ok = zend_is_true(&r);
	}
	zval_ptr_dtor(&r);
	return ok;
}

static zend_string *gene_password_digest(const char *algo, zend_string *data) {
	zval p[2], r;
	ZVAL_STRING(&p[0], algo);
	ZVAL_STR(&p[1], data);
	if (password_call(ZEND_STRL("hash"), 2, p, &r) != SUCCESS || Z_TYPE(r) != IS_STRING) {
		zval_ptr_dtor(&p[0]);
		zval_ptr_dtor(&r);
		return NULL;
	}
	zval_ptr_dtor(&p[0]);
	return Z_STR(r);
}

/* md5(salt . sha1(salt . pwd)) compared in constant time. Binary-safe (no
 * printf-style concatenation that would truncate at NUL). */
static int gene_password_legacy_ok(zend_string *pwd, zend_string *hash, zend_string *salt) {
	zend_string *in, *inner, *outer, *digest;
	int ok;
	in = salt ? zend_string_concat2(ZSTR_VAL(salt), ZSTR_LEN(salt), ZSTR_VAL(pwd), ZSTR_LEN(pwd))
		: zend_string_copy(pwd);
	inner = gene_password_digest("sha1", in);
	zend_string_release(in);
	if (!inner) {
		return 0;
	}
	outer = salt ? zend_string_concat2(ZSTR_VAL(salt), ZSTR_LEN(salt), ZSTR_VAL(inner), ZSTR_LEN(inner))
		: zend_string_copy(inner);
	zend_string_release(inner);
	digest = gene_password_digest("md5", outer);
	zend_string_release(outer);
	if (!digest) {
		return 0;
	}
	ok = gene_password_equals(digest, hash);
	zend_string_release(digest);
	return ok;
}

/* password_needs_rehash($hash, $algo ?? PASSWORD_DEFAULT, $opts) */
static int gene_password_needs_rehash(zend_string *hash, zval *opts) {
	zval p[3], r;
	int rh = 0;
	uint32_t argc = 2;
	zval *av = (opts && Z_TYPE_P(opts) == IS_ARRAY) ? zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("algo")) : NULL;
	ZVAL_STR(&p[0], hash);
	ZVAL_NULL(&p[1]);
	if (av && Z_TYPE_P(av) == IS_STRING && Z_STRLEN_P(av) > 0) {
		if (zend_string_equals_literal_ci(Z_STR_P(av), "bcrypt") || zend_string_equals_literal_ci(Z_STR_P(av), "default")) {
			ZVAL_STRING(&p[1], "2y");
		} else {
			ZVAL_STR_COPY(&p[1], Z_STR_P(av));
		}
	} else if (av && Z_TYPE_P(av) == IS_LONG && Z_LVAL_P(av) >= 1 && Z_LVAL_P(av) <= 3) {
		static const char *map[] = {NULL, "2y", "argon2i", "argon2id"};
		ZVAL_STRING(&p[1], map[Z_LVAL_P(av)]);
	}
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		zend_string *k;
		zval *v;
		array_init(&p[2]);
		ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(opts), k, v) {
			if (k && !zend_string_equals_literal(k, "algo") && !zend_string_equals_literal(k, "salt")
					&& !zend_string_equals_literal(k, "legacy")) {
				Z_TRY_ADDREF_P(v);
				zend_hash_update(Z_ARRVAL(p[2]), k, v);
			}
		} ZEND_HASH_FOREACH_END();
		argc = 3;
	}
	if (password_call(ZEND_STRL("password_needs_rehash"), argc, p, &r) == SUCCESS) {
		rh = zend_is_true(&r);
	} else if (EG(exception)) {
		zend_clear_exception();
	}
	zval_ptr_dtor(&r);
	zval_ptr_dtor(&p[1]);
	if (argc == 3) {
		zval_ptr_dtor(&p[2]);
	}
	return rh;
}

static int gene_password_modern_ok(zend_string *pwd, zend_string *hash) {
	zval p[2], r;
	int ok = 0;
	ZVAL_STR(&p[0], pwd);
	ZVAL_STR(&p[1], hash);
	if (password_call(ZEND_STRL("password_verify"), 2, p, &r) == SUCCESS) {
		ok = zend_is_true(&r);
	}
	zval_ptr_dtor(&r);
	return ok;
}

/* On successful verify + needs_rehash, attach a fresh hash so the caller
 * can persist the upgrade in one line (plan §8.1 contract). */
static void gene_password_attach_rehash(zval *rv, zend_string *pwd, zval *opts) {
	zval h;
	zval fwd;
	ZVAL_UNDEF(&fwd);
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		zend_string *k;
		zval *v;
		array_init(&fwd);
		ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(opts), k, v) {
			if (k && !zend_string_equals_literal(k, "salt") && !zend_string_equals_literal(k, "legacy")) {
				Z_TRY_ADDREF_P(v);
				zend_hash_update(Z_ARRVAL(fwd), k, v);
			}
		} ZEND_HASH_FOREACH_END();
	}
	if (gene_password_hash_call(pwd, Z_ISUNDEF(fwd) ? NULL : &fwd, &h) == SUCCESS && Z_TYPE(h) == IS_STRING) {
		add_assoc_str_ex(rv, ZEND_STRL("hash"), Z_STR(h));
	} else {
		zval_ptr_dtor(&h);
		if (EG(exception)) {
			zend_clear_exception();
		}
	}
	zval_ptr_dtor(&fwd);
}

static const char *gene_password_algo_name(zend_string *hash) {
	const char *h = ZSTR_VAL(hash);
	size_t n = ZSTR_LEN(hash);
	if (n >= 10 && memcmp(h, "$argon2id$", 10) == 0) return "argon2id";
	if (n >= 9 && memcmp(h, "$argon2i$", 9) == 0) return "argon2i";
	if (n >= 4 && h[0] == '$' && h[1] == '2' && (h[2] == 'y' || h[2] == 'b' || h[2] == 'a') && h[3] == '$') return "bcrypt";
	return "unknown";
}
/* }}} */

/* {{{ proto static string Gene\Password::hash(string $password [, array $opts])
 * opts: {'algo':'bcrypt|argon2i|argon2id|default'|int, ...PHP options}. */
PHP_METHOD(gene_password, hash) {
	zend_string *pwd;
	zval *opts = NULL, ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a!", &pwd, &opts) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(pwd) == 0) {
		zend_argument_value_error(1, "must not be empty");
		RETURN_THROWS();
	}
	if (gene_password_hash_call(pwd, opts, &ret) != SUCCESS) {
		zval_ptr_dtor(&ret);
		if (!EG(exception)) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Password::hash failed");
		}
		RETURN_THROWS();
	}
	RETURN_ZVAL(&ret, 0, 1);
}
/* }}} */

/* {{{ proto static array Gene\Password::verify(string $password, string $hash [, array $opts])
 * → {'ok':bool,'needs_rehash':bool[,'hash':string]} — modern formats; with
 * opts.salt/opts.legacy a legacy hash is accepted too (same as verifyAny).
 * 'hash' (fresh PASSWORD_DEFAULT hash) is present only when ok && needs_rehash. */
static void gene_password_verify_impl(INTERNAL_FUNCTION_PARAMETERS, zend_bool with_algo) {
	zend_string *pwd, *hash;
	zval *opts = NULL;
	const char *algo = "unknown";
	zend_bool ok = 0, rh = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|a!", &pwd, &hash, &opts) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(hash) > 0 && ZSTR_VAL(hash)[0] == '$') {
		algo = gene_password_algo_name(hash);
		ok = gene_password_modern_ok(pwd, hash);
		if (ok) {
			rh = gene_password_needs_rehash(hash, opts);
		}
	} else if (opts) {
		zval *lg = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("legacy"));
		zval *sv = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("salt"));
		zend_string *salt = (sv && Z_TYPE_P(sv) == IS_STRING) ? Z_STR_P(sv) : NULL;
		if (lg && zend_is_callable(lg, 0, NULL)) {
			/* custom legacy scheme: fn(string $plain, string $stored, ?string $salt): bool */
			zval args[3], r;
			ZVAL_STR(&args[0], pwd);
			ZVAL_STR(&args[1], hash);
			if (salt) {
				ZVAL_STR(&args[2], salt);
			} else {
				ZVAL_NULL(&args[2]);
			}
			ZVAL_UNDEF(&r);
			if (call_user_function(NULL, NULL, lg, &r, 3, args) == SUCCESS && !EG(exception)) {
				ok = zend_is_true(&r);
			}
			zval_ptr_dtor(&r);
			if (EG(exception)) {
				RETURN_THROWS();
			}
			algo = "legacy";
		} else if (ZSTR_LEN(hash) == 32) {
			size_t i;
			zend_bool hex = 1;
			for (i = 0; i < 32; i++) {
				char c = ZSTR_VAL(hash)[i];
				if (!((c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f'))) { hex = 0; break; }
			}
			if (hex) {
				algo = "legacy";
				ok = gene_password_legacy_ok(pwd, hash, salt);
			}
		}
		if (ok) {
			rh = 1; /* a verified legacy hash always upgrades */
		}
	}
	array_init(return_value);
	add_assoc_bool_ex(return_value, ZEND_STRL("ok"), ok);
	if (with_algo) {
		add_assoc_string_ex(return_value, ZEND_STRL("algo"), (char *)algo);
	}
	add_assoc_bool_ex(return_value, ZEND_STRL("needs_rehash"), rh);
	if (ok && rh) {
		gene_password_attach_rehash(return_value, pwd, opts);
	}
}

PHP_METHOD(gene_password, verify) {
	gene_password_verify_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto static bool Gene\Password::verifyLegacy(string $password, string $hash [, string $salt])
 * The app legacy format: md5($salt . sha1($salt . $password)) === $hash
 * (constant-time compare). salt defaults to '' for deployments without one. */
PHP_METHOD(gene_password, verifyLegacy) {
	zend_string *pwd, *hash, *salt = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|S!", &pwd, &hash, &salt) == FAILURE) {
		return;
	}
	RETURN_BOOL(gene_password_legacy_ok(pwd, hash, salt));
}
/* }}} */

/* {{{ proto static array Gene\Password::verifyAny(string $password, string $hash [, array $opts])
 * Auto-detect: '$'-prefixed → modern path; 32-hex → legacy md5salt (opts.salt);
 * opts.legacy callable overrides the legacy scheme. Returns
 * {'ok','algo':'bcrypt|argon2i|argon2id|legacy|unknown','needs_rehash'[,'hash']}. */
PHP_METHOD(gene_password, verifyAny) {
	gene_password_verify_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */

/* {{{ proto static bool Gene\Password::needsRehash(string $hash [, array $opts])
 * True for any non password_hash() format (legacy md5 etc.) as well. */
PHP_METHOD(gene_password, needsRehash) {
	zend_string *hash;
	zval *opts = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a!", &hash, &opts) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(hash) == 0 || ZSTR_VAL(hash)[0] != '$') {
		RETURN_TRUE;
	}
	RETURN_BOOL(gene_password_needs_rehash(hash, opts));
}
/* }}} */

/* {{{ proto static bool Gene\Password::dummy(string $password)
 * Always false, at the same cost as a real PASSWORD_DEFAULT verify. Call it
 * for unknown users so "user not found" isn't timing-detectable. The dummy
 * hash is generated once per process with the live default algo/cost — a
 * hard-coded literal drifts from the real cost (and a malformed literal
 * makes password_verify return instantly, defeating the purpose). */
PHP_METHOD(gene_password, dummy) {
	zend_string *pwd;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &pwd) == FAILURE) {
		return;
	}
	if (!GENE_G(password_dummy)) {
		zval h;
		zend_string *seed = zend_string_init(ZEND_STRL("gene-dummy-password"), 0);
		if (gene_password_hash_call(seed, NULL, &h) == SUCCESS && Z_TYPE(h) == IS_STRING) {
			GENE_G(password_dummy) = zend_string_init(Z_STRVAL(h), Z_STRLEN(h), 1);
		} else if (EG(exception)) {
			zend_clear_exception();
		}
		zval_ptr_dtor(&h);
		zend_string_release(seed);
	}
	if (GENE_G(password_dummy)) {
		zend_string *d = zend_string_init(ZSTR_VAL(GENE_G(password_dummy)), ZSTR_LEN(GENE_G(password_dummy)), 0);
		gene_password_modern_ok(pwd, d);
		zend_string_release(d);
	}
	RETURN_FALSE;
}
/* }}} */

const zend_function_entry gene_password_methods[] = {
	PHP_ME(gene_password, hash, gene_password_hash_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_password, verify, gene_password_verify_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_password, verifyLegacy, gene_password_legacy_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_password, verifyAny, gene_password_any_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_password, needsRehash, gene_password_rehash_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_password, dummy, gene_password_dummy_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(password) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Password", "Gene\\Password", gene_password_methods);
	gene_password_ce = zend_register_internal_class(&ce);
	gene_password_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
/* }}} */
