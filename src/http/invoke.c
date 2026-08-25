#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../common/common.h"
#include "../factory/factory.h"
#include "../http/request.h"
#include "../http/invoke.h"

zend_class_entry *gene_invoke_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_invoke_local_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, class)
	ZEND_ARG_INFO(0, action)
	ZEND_ARG_ARRAY_INFO(0, params, 0)
	ZEND_ARG_ARRAY_INFO(0, files, 0)
ZEND_END_ARG_INFO()

int gene_invoke_local(const char *class_name, size_t class_len,
	const char *action, size_t action_len, zval *params, zval *files, zval *retval)
{
	gene_request_context *ctx;
	zval classObject, empty_files, call_ret;
	char act_buf[128];
	char *act;
	size_t act_len;
	int snapped = 0;
	zend_bool failed = 0;

	ZVAL_UNDEF(retval);
	ZVAL_UNDEF(&call_ret);

	if (!class_name || class_len == 0 || !action || action_len == 0) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Invoke::local requires class and action");
		return FAILURE;
	}

	ctx = gene_request_ctx();
	if (ctx->invoke_depth >= GENE_INVOKE_DEPTH_MAX) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Invoke nesting exceeds %d", GENE_INVOKE_DEPTH_MAX);
		return FAILURE;
	}
	ctx->invoke_depth++;

	if (gene_request_snapshot_ctx(ctx, NULL) != SUCCESS) {
		ctx->invoke_depth--;
		return FAILURE;
	}
	snapped = 1;

	if (!files) {
		array_init(&empty_files);
		files = &empty_files;
	}
	gene_request_scope(params, params, files, params);
	if (files == &empty_files) {
		zval_ptr_dtor(&empty_files);
	}

	act_len = action_len;
	if (act_len < sizeof(act_buf)) {
		memcpy(act_buf, action, act_len);
		act_buf[act_len] = '\0';
		act = act_buf;
	} else {
		act = estrndup(action, act_len);
	}
	gene_strtolower(act);

	ZVAL_UNDEF(&classObject);
	if (!gene_factory((char *)class_name, class_len, NULL, &classObject)) {
		php_error_docref(NULL, E_WARNING, "Unable to init class '%s'.", class_name);
		ZVAL_NULL(&call_ret);
		failed = 1;
	} else if (EG(exception) || Z_TYPE(classObject) != IS_OBJECT
		|| !zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), act, act_len)) {
		if (!EG(exception)) {
			php_error_docref(NULL, E_WARNING, "Unable to call method '%s' in class '%s'.", act, class_name);
		}
		ZVAL_NULL(&call_ret);
		failed = 1;
	} else {
		gene_factory_call_1(&classObject, act, act_len, NULL, &call_ret);
	}
	if (!Z_ISUNDEF(classObject)) {
		zval_ptr_dtor(&classObject);
	}

	if (act != act_buf) {
		efree(act);
	}
	if (snapped) {
		gene_request_restore_ctx(ctx);
	}
	ctx->invoke_depth--;

	if (EG(exception)) {
		zval_ptr_dtor(&call_ret);
		return FAILURE;
	}
	if (failed && Z_TYPE(call_ret) == IS_UNDEF) {
		ZVAL_NULL(retval);
		return SUCCESS;
	}
	ZVAL_COPY_VALUE(retval, &call_ret);
	return SUCCESS;
}

PHP_METHOD(gene_invoke, local) {
	char *class_name = NULL, *action = NULL;
	size_t class_len = 0, action_len = 0;
	zval *params = NULL, *files = NULL;
	zval empty_params;

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

const zend_function_entry gene_invoke_methods[] = {
	PHP_ME(gene_invoke, local, gene_invoke_local_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(invoke) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Invoke", "Gene\\Invoke", gene_invoke_methods);
	gene_invoke_ce = zend_register_internal_class(&ce);
	gene_invoke_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
