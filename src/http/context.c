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

#include "../gene.h"
#include "../http/context.h"

zend_class_entry *gene_context_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_context_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_context_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_context_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, default_value)
ZEND_END_ARG_INFO()

/* {{{ gene_context_bag
 * Lazy-init the per-request KV bag. Recycled in-place by free_fields(). */
zval *gene_context_bag(void) {
	gene_request_context *ctx = gene_request_ctx();
	if (UNEXPECTED(!ctx)) {
		return NULL;
	}
	if (UNEXPECTED(Z_TYPE(ctx->user_bag) != IS_ARRAY)) {
		array_init(&ctx->user_bag);
	}
	return &ctx->user_bag;
}
/* }}} */

/* {{{ proto static void Gene\Context::set(string $key, mixed $value) */
PHP_METHOD(gene_context, set) {
	zend_string *key;
	zval *value;
	zval *bag;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &key, &value) == FAILURE) {
		return;
	}
	bag = gene_context_bag();
	if (UNEXPECTED(!bag)) {
		RETURN_FALSE;
	}
	Z_TRY_ADDREF_P(value);
	zend_symtable_update(Z_ARRVAL_P(bag), key, value);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto static mixed Gene\Context::get(string $key [, mixed $default = null]) */
PHP_METHOD(gene_context, get) {
	zend_string *key;
	zval *def = NULL;
	zval *bag, *found;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|z", &key, &def) == FAILURE) {
		return;
	}
	ctx = gene_request_ctx();
	if (!ctx || Z_TYPE(ctx->user_bag) != IS_ARRAY) {
		if (def) {
			RETURN_ZVAL(def, 1, 0);
		}
		RETURN_NULL();
	}
	bag = &ctx->user_bag;
	found = zend_symtable_find(Z_ARRVAL_P(bag), key);
	if (found) {
		RETURN_ZVAL(found, 1, 0);
	}
	if (def) {
		RETURN_ZVAL(def, 1, 0);
	}
	RETURN_NULL();
}
/* }}} */

PHP_METHOD(gene_context, has) {
	zend_string *key;
	gene_request_context *ctx;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &key) == FAILURE) {
		return;
	}
	ctx = gene_request_ctx();
	RETURN_BOOL(ctx && Z_TYPE(ctx->user_bag) == IS_ARRAY
		&& zend_symtable_exists(Z_ARRVAL(ctx->user_bag), key));
}

/* {{{ proto static array Gene\Context::all() */
PHP_METHOD(gene_context, all) {
	gene_request_context *ctx = gene_request_ctx();
	if (!ctx || Z_TYPE(ctx->user_bag) != IS_ARRAY) {
		array_init(return_value);
		return;
	}
	RETURN_ZVAL(&ctx->user_bag, 1, 0);
}
/* }}} */

const zend_function_entry gene_context_methods[] = {
	PHP_ME(gene_context, set, gene_context_set_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_context, get, gene_context_get_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_context, has, gene_context_get_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_context, all, gene_context_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(context) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Context", "Gene\\Context", gene_context_methods);
	gene_context_ce = zend_register_internal_class(&ce);
	gene_context_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
