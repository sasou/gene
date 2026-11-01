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
		ZVAL_ARR(&params[2], Z_ARR(fwd));
		return password_call(ZEND_STRL("password_hash"), 3, params, retval);
	}
	ZVAL_NULL(&params[2]);
	return password_call(ZEND_STRL("password_hash"), 2, params, retval);
}

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

/* {{{ proto static array Gene\Password::verify(string $password, string $hash)
 * → {'ok':bool,'needs_rehash':bool} — modern-format hashes only. */
PHP_METHOD(gene_password, verify) {
	zend_string *pwd, *hash;
	zval params[2], ok_r, rh_r;
	zend_bool ok = 0, rh = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &pwd, &hash) == FAILURE) {
		return;
	}
	ZVAL_STR(&params[0], pwd);
	ZVAL_STR(&params[1], hash);
	if (password_call(ZEND_STRL("password_verify"), 2, params, &ok_r) == SUCCESS) {
		ok = zend_is_true(&ok_r);
		zval_ptr_dtor(&ok_r);
	}
	if (ok) {
		zval p2[2];
		ZVAL_STR(&p2[0], hash);
		ZVAL_STRING(&p2[1], "2y"); /* PASSWORD_BCRYPT target */
		if (password_call(ZEND_STRL("password_needs_rehash"), 2, p2, &rh_r) == SUCCESS) {
			rh = zend_is_true(&rh_r);
			zval_ptr_dtor(&rh_r);
		}
		zval_ptr_dtor(&p2[1]);
	}
	array_init(return_value);
	add_assoc_bool_ex(return_value, ZEND_STRL("ok"), ok);
	add_assoc_bool_ex(return_value, ZEND_STRL("needs_rehash"), rh);
}
/* }}} */

/* {{{ proto static bool Gene\Password::verifyLegacy(string $password, string $hash [, string $salt])
 * The app legacy format: md5($salt . sha1($salt . $password)) === $hash.
 * salt defaults to '' for deployments that hashed without one. */
PHP_METHOD(gene_password, verifyLegacy) {
	zend_string *pwd, *hash, *salt = NULL;
	zval params[2], r1, r2;
	smart_str inner = {0}, outer = {0};
	zend_bool ok = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|S!", &pwd, &hash, &salt) == FAILURE) {
		return;
	}
	/* inner = sha1(salt . pwd) — hash(algo, data) arg order */
	{
		zend_string *in = salt
			? zend_strpprintf(0, "%s%s", ZSTR_VAL(salt), ZSTR_VAL(pwd))
			: zend_string_copy(pwd);
		ZVAL_STRING(&params[0], "sha1");
		ZVAL_STR(&params[1], in);
		if (password_call(ZEND_STRL("hash"), 2, params, &r1) != SUCCESS) {
			zval_ptr_dtor(&params[0]);
			zend_string_release(in);
			RETURN_THROWS();
		}
		zval_ptr_dtor(&params[0]);
		if (Z_TYPE(r1) != IS_STRING) {
			zval_ptr_dtor(&r1);
			zend_string_release(in);
			RETURN_FALSE;
		}
		smart_str_append(&inner, Z_STR(r1));
		smart_str_0(&inner);
		zval_ptr_dtor(&r1);
		/* outer = md5(salt . inner) */
		if (salt) smart_str_append(&outer, salt);
		smart_str_append(&outer, inner.s);
		smart_str_0(&outer);
		zend_string_release(in);
	}
	if (!outer.s) {
		smart_str_free(&inner);
		RETURN_FALSE;
	}
	ZVAL_STRING(&params[0], "md5");
	ZVAL_STR(&params[1], outer.s);
	if (password_call(ZEND_STRL("hash"), 2, params, &r2) != SUCCESS) {
		zval_ptr_dtor(&params[0]);
		smart_str_free(&inner);
		smart_str_free(&outer);
		RETURN_THROWS();
	}
	zval_ptr_dtor(&params[0]);
	if (Z_TYPE(r2) == IS_STRING) {
		zval eq_params[2], eq_r;
		ZVAL_STR(&eq_params[0], Z_STR(r2));
		ZVAL_STR(&eq_params[1], hash);
		if (password_call(ZEND_STRL("hash_equals"), 2, eq_params, &eq_r) == SUCCESS) {
			ok = zend_is_true(&eq_r);
			zval_ptr_dtor(&eq_r);
		}
	}
	zval_ptr_dtor(&r2);
	smart_str_free(&inner);
	smart_str_free(&outer);
	RETURN_BOOL(ok);
}
/* }}} */

/* {{{ proto static array Gene\Password::verifyAny(string $password, string $hash [, array $opts])
 * Auto-detect: hash starting with '$' → modern path (verify + needs_rehash);
 * 32-hex → legacy path (opts.salt). Returns
 * {'ok':bool,'algo':'bcrypt|argon2i|argon2id|legacy|unknown','needs_rehash':bool}. */
PHP_METHOD(gene_password, verifyAny) {
	zend_string *pwd, *hash;
	zval *opts = NULL;
	const char *algo = "unknown";
	zend_bool ok = 0, rh = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS|a!", &pwd, &hash, &opts) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(hash) > 0 && ZSTR_VAL(hash)[0] == '$') {
		zval params[2], ok_r, rh_r;
		if (ZSTR_LEN(hash) >= 7 && memcmp(ZSTR_VAL(hash), "$argon2", 7) == 0) {
			algo = "argon2id";
		} else if (ZSTR_LEN(hash) >= 4 && memcmp(ZSTR_VAL(hash), "$2y$", 4) == 0) {
			algo = "bcrypt";
		}
		ZVAL_STR(&params[0], pwd);
		ZVAL_STR(&params[1], hash);
		if (password_call(ZEND_STRL("password_verify"), 2, params, &ok_r) == SUCCESS) {
			ok = zend_is_true(&ok_r);
			zval_ptr_dtor(&ok_r);
		}
		if (ok) {
			zval p2[2];
			ZVAL_STR(&p2[0], hash);
			ZVAL_STRING(&p2[1], "2y");
			if (password_call(ZEND_STRL("password_needs_rehash"), 2, p2, &rh_r) == SUCCESS) {
				rh = zend_is_true(&rh_r);
				zval_ptr_dtor(&rh_r);
			}
			zval_ptr_dtor(&p2[1]);
		}
	} else {
		/* legacy 32-hex md5 */
		zend_string *salt = NULL;
		zval *sv = NULL;
		if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
			sv = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("salt"));
			if (sv && Z_TYPE_P(sv) == IS_STRING) salt = Z_STR_P(sv);
		}
		if (ZSTR_LEN(hash) == 32) {
			zend_bool hex = 1;
			size_t i;
			for (i = 0; i < 32; i++) {
				char c = ZSTR_VAL(hash)[i];
				if (!((c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f'))) { hex = 0; break; }
			}
			if (hex) {
				algo = "legacy";
				{
					/* md5(salt.sha1(salt.pwd)) — hash(algo, data) arg order;
					 * params2[1] borrows in/outer.s (owned elsewhere, no dtor). */
					zval params2[2], r1, r2;
					smart_str outer = {0};
					zend_string *in = salt
						? zend_strpprintf(0, "%s%s", ZSTR_VAL(salt), ZSTR_VAL(pwd))
						: zend_string_copy(pwd);
					ZVAL_STRING(&params2[0], "sha1");
					ZVAL_STR(&params2[1], in);
					if (password_call(ZEND_STRL("hash"), 2, params2, &r1) == SUCCESS && Z_TYPE(r1) == IS_STRING) {
						if (salt) smart_str_append(&outer, salt);
						smart_str_append(&outer, Z_STR(r1));
						smart_str_0(&outer);
						zval_ptr_dtor(&r1);
						zval_ptr_dtor(&params2[0]);
						ZVAL_STRING(&params2[0], "md5");
						ZVAL_STR(&params2[1], outer.s);
						if (password_call(ZEND_STRL("hash"), 2, params2, &r2) == SUCCESS && Z_TYPE(r2) == IS_STRING) {
							zval eq_params[2], eq_r;
							ZVAL_STR(&eq_params[0], Z_STR(r2));
							ZVAL_STR(&eq_params[1], hash);
							if (password_call(ZEND_STRL("hash_equals"), 2, eq_params, &eq_r) == SUCCESS) {
								ok = zend_is_true(&eq_r);
								zval_ptr_dtor(&eq_r);
							}
						}
						zval_ptr_dtor(&r2);
						zval_ptr_dtor(&params2[0]);
					} else {
						zval_ptr_dtor(&r1);
						zval_ptr_dtor(&params2[0]);
					}
					zend_string_release(in);
					smart_str_free(&outer);
				}
				/* verified legacy always needs rehash on success */
				if (ok) rh = 1;
			}
		}
	}
	array_init(return_value);
	add_assoc_bool_ex(return_value, ZEND_STRL("ok"), ok);
	add_assoc_string_ex(return_value, ZEND_STRL("algo"), (char *)algo);
	add_assoc_bool_ex(return_value, ZEND_STRL("needs_rehash"), rh);
}
/* }}} */

/* {{{ proto static bool Gene\Password::needsRehash(string $hash [, array $opts]) */
PHP_METHOD(gene_password, needsRehash) {
	zend_string *hash;
	zval *opts = NULL, params[2], ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|a!", &hash, &opts) == FAILURE) {
		return;
	}
	ZVAL_STR(&params[0], hash);
	ZVAL_STRING(&params[1], "2y");
	if (opts && Z_TYPE_P(opts) == IS_ARRAY) {
		zval *av = zend_hash_str_find(Z_ARRVAL_P(opts), ZEND_STRL("algo"));
		if (av && Z_TYPE_P(av) == IS_STRING && Z_STRLEN_P(av) > 0) {
			zval_ptr_dtor(&params[1]);
			ZVAL_STR_COPY(&params[1], Z_STR_P(av));
		} else if (av && Z_TYPE_P(av) == IS_LONG && Z_LVAL_P(av) >= 1 && Z_LVAL_P(av) <= 3) {
			static const char *map[] = {NULL, "2y", "argon2i", "argon2id"};
			zval_ptr_dtor(&params[1]);
			ZVAL_STRING(&params[1], map[Z_LVAL_P(av)]);
		}
	}
	if (password_call(ZEND_STRL("password_needs_rehash"), 2, params, &ret) != SUCCESS) {
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&ret);
		RETURN_FALSE;
	}
	zval_ptr_dtor(&params[1]);
	RETVAL_BOOL(zend_is_true(&ret));
	zval_ptr_dtor(&ret);
}
/* }}} */

/* {{{ proto static bool Gene\Password::dummy(string $password)
 * Fixed bcrypt dummy verify — always false, ~same cost as a real check.
 * Call it for unknown users so "user not found" isn't timing-detectable. */
PHP_METHOD(gene_password, dummy) {
	zend_string *pwd;
	zval params[2], ret;
	static const char *DUMMY = "$2y$10$8JBXW1YpZqQ4xV1m6F9w5e8w6sK1uQ3yR7tN0bV2cX4dA5fG6hI7jK";
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &pwd) == FAILURE) {
		return;
	}
	ZVAL_STR(&params[0], pwd);
	ZVAL_STRING(&params[1], DUMMY);
	if (password_call(ZEND_STRL("password_verify"), 2, params, &ret) == SUCCESS) {
		zval_ptr_dtor(&ret);
	}
	zval_ptr_dtor(&params[1]);
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
