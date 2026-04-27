/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include "zend_string.h"
#include "zend_globals.h"
#include "zend_virtual_cwd.h"
#include "zend_smart_str.h"
#include "main/php_output.h"
#include "zend_execute.h"
#include "Zend/zend_interfaces.h" /* for zend_class_serialize_deny */

#include "../gene.h"
#include "../factory/load.h"
#include "../config/configs.h"
#include "../common/common.h"

zend_class_entry * gene_load_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_load_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_load_arg_import, 0, 0, 1)
	ZEND_ARG_INFO(0, file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_load_arg_autoload, 0, 0, 1)
	ZEND_ARG_INFO(0, className)
ZEND_END_ARG_INFO()


int exec_by_symbol_table(zval *obj, zend_op_array *op_array, zend_array *symbol_table, zval *result) /* {{{ */ {
	zend_execute_data *call;
	uint32_t call_info;

	op_array->scope = Z_OBJCE_P(obj);

	zend_function *func = (zend_function *)op_array;

	call_info = ZEND_CALL_HAS_THIS | ZEND_CALL_NESTED_CODE | ZEND_CALL_HAS_SYMBOL_TABLE;

	call = zend_vm_stack_push_call_frame(call_info, func, 0, Z_OBJ_P(obj));

	call->symbol_table = symbol_table;

	zend_init_execute_data(call, op_array, result);

	ZEND_ADD_CALL_FLAG(call, ZEND_CALL_TOP);
	zend_execute_ex(call);
	zend_vm_stack_free_call_frame(call);
	return 1;
}
/* }}} */

/** {{{ int gene_load_import(char *path)
 */
int gene_load_import(char *path , zval *obj, zend_array *symbol_table) {
	zend_file_handle file_handle;
	zend_op_array *op_array;
	zend_stat_t sb;

	if (UNEXPECTED(VCWD_STAT(path, &sb) == -1)) {
		return 0;
	}

	/* setup file-handle */
	zend_stream_init_filename(&file_handle, path);


	op_array = zend_compile_file(&file_handle, ZEND_INCLUDE);

	if (op_array) {
		if (!file_handle.opened_path) {
			file_handle.opened_path = zend_string_init(path, strlen(path), 0);
		}

		zend_hash_add_empty_element(&EG(included_files),
				file_handle.opened_path);
	}
	zend_destroy_file_handle(&file_handle);

	if (op_array) {
		zval result;
		ZVAL_UNDEF(&result);

		/* [GENE_FIX:2026-04-27] Wrap execution in zend_try so a bailout
		 * (longjmp from zend_error/zend_throw etc. inside the included
		 * file) does not leak op_array. Without this, the destroy_op_array
		 * + efree below were skipped on bailout and the op_array (often
		 * 10s of KB) remained allocated for the rest of the request. */
		zend_try {
			if (obj && symbol_table) {
				exec_by_symbol_table(obj, op_array, symbol_table, &result);
			} else {
				zend_execute(op_array, &result);
			}
		} zend_end_try();

		destroy_op_array(op_array);
		efree(op_array);
		if (!Z_ISUNDEF(result)) {
			zval_ptr_dtor(&result);
		}
		return 1;
	}
	return 0;
}
/* }}} */

void gene_load_file_by_class_name (char *className) {
	char *fileNmae = NULL, *filePath = NULL, *class_lowercase = NULL;
	zend_class_entry *ce 	= NULL;

	fileNmae = estrdup(className);

	if (GENE_G(use_namespace)) {
		replaceAll(fileNmae, '\\', '/');
	} else {
		replaceAll(fileNmae, '_', '/');
	}

	size_t file_path_len = 0;
	char file_path_buf[512];
	int file_path_heap = 0;
	if (GENE_G(app_root)) {
		file_path_len = strlen(GENE_G(app_root)) + strlen(fileNmae) + 6;
		if (file_path_len >= sizeof(file_path_buf)) {
			filePath = emalloc(file_path_len + 1);
			file_path_heap = 1;
		} else {
			filePath = file_path_buf;
		}
		snprintf(filePath, file_path_len + 1, "%s/%s.php", GENE_G(app_root), fileNmae);
	} else {
		file_path_len = strlen(fileNmae) + 5;
		if (file_path_len >= sizeof(file_path_buf)) {
			filePath = emalloc(file_path_len + 1);
			file_path_heap = 1;
		} else {
			filePath = file_path_buf;
		}
		snprintf(filePath, file_path_len + 1, "%s.php", fileNmae);
	}
	if (!gene_load_import(filePath, NULL, NULL)) {
		if (GENE_G(use_library)) {
			if (file_path_heap) {
				efree(filePath);
			}
			file_path_len = strlen(GENE_G(library_root)) + strlen(fileNmae) + 6;
			if (file_path_len >= sizeof(file_path_buf)) {
				filePath = emalloc(file_path_len + 1);
				file_path_heap = 1;
			} else {
				filePath = file_path_buf;
				file_path_heap = 0;
			}
			snprintf(filePath, file_path_len + 1, "%s/%s.php", GENE_G(library_root), fileNmae);
			gene_load_import(filePath, NULL, NULL);
		}
	}
	if (file_path_heap) {
		efree(filePath);
	}
	efree(fileNmae);
}

/*
 *  {{{ zval *gene_load_instance(zval *this_ptr)
 */
zval *gene_load_instance(zval *this_ptr) {
	zval *instance = zend_read_static_property(gene_load_ce,
	GENE_LOAD_PROPERTY_INSTANCE, strlen(GENE_LOAD_PROPERTY_INSTANCE), 1);

	if (Z_TYPE_P(instance) == IS_OBJECT) {
		return instance;
	}
	if (this_ptr) {
		instance = this_ptr;
	} else {
		zval new_instance;
		object_init_ex(&new_instance, gene_load_ce);
		zend_update_static_property(gene_load_ce, GENE_LOAD_PROPERTY_INSTANCE,
				strlen(GENE_LOAD_PROPERTY_INSTANCE), &new_instance);
		zval_ptr_dtor(&new_instance);
		return zend_read_static_property(gene_load_ce,
		GENE_LOAD_PROPERTY_INSTANCE, strlen(GENE_LOAD_PROPERTY_INSTANCE), 1);
	}
	zend_update_static_property(gene_load_ce, GENE_LOAD_PROPERTY_INSTANCE,
			strlen(GENE_LOAD_PROPERTY_INSTANCE), instance);
	return instance;
}
/* }}} */

/** {{{ int gene_loader_register(zval *loader,char *methodName)
 *
 * [GENE_PERF:2026-04-23] Per-FPM-request re-registration is required because
 * ext/spl clears SPL_G(autoload_functions) in RSHUTDOWN, so we cannot skip
 * the call. What we CAN skip is the cost of resolving "spl_autoload_register"
 * from CG(function_table) on every request and allocating a fresh zval for
 * the callback name. Both are now resolved once per process and cached:
 *   - spl_autoload_register zend_function* — cached via static local
 *   - The default callback name — persistent interned zend_string
 *
 * Net savings per FPM request: 1 function_table lookup + 1 ZVAL_STRING
 * heap alloc + fci/fcc setup overhead replaced by zend_call_known_function.
 */
int gene_loader_register() {
	static zend_function *spl_register_fn = NULL;
	static zend_string *default_cb_ns = NULL;
	static zend_string *default_cb = NULL;
	zval autoload, ret;

	if (GENE_G(autoload_registered)) {
		return 1;
	}

	if (UNEXPECTED(!spl_register_fn)) {
		spl_register_fn = zend_hash_str_find_ptr(CG(function_table),
			ZEND_STRL(GENE_SPL_AUTOLOAD_REGISTER_NAME));
	}
	if (UNEXPECTED(!spl_register_fn)) {
		php_error_docref(NULL, E_WARNING,
			"spl_autoload_register() is not available — required by Gene");
		return 0;
	}

	if (GENE_G(auto_load_fun)) {
		/* Custom callback name from userland — can't safely intern, keep ZVAL_STRING. */
		ZVAL_STRING(&autoload, GENE_G(auto_load_fun));
	} else {
		if (GENE_G(use_namespace)) {
			if (UNEXPECTED(!default_cb_ns)) {
				default_cb_ns = zend_string_init_interned(
					GENE_AUTOLOAD_FUNC_NAME_NS,
					sizeof(GENE_AUTOLOAD_FUNC_NAME_NS) - 1, 1);
			}
			ZVAL_STR_COPY(&autoload, default_cb_ns);
		} else {
			if (UNEXPECTED(!default_cb)) {
				default_cb = zend_string_init_interned(
					GENE_AUTOLOAD_FUNC_NAME,
					sizeof(GENE_AUTOLOAD_FUNC_NAME) - 1, 1);
			}
			ZVAL_STR_COPY(&autoload, default_cb);
		}
	}
	ZVAL_UNDEF(&ret);

	zend_call_known_function(spl_register_fn, NULL, NULL, &ret, 1, &autoload, NULL);

	if (UNEXPECTED(EG(exception))) {
		zval_ptr_dtor(&autoload);
		zval_ptr_dtor(&ret);
		php_error_docref(NULL, E_WARNING,
			"Unable to register autoload function %s",
			GENE_G(auto_load_fun) ? GENE_G(auto_load_fun) : GENE_AUTOLOAD_FUNC_NAME);
		return 0;
	}

	zval_ptr_dtor(&autoload);
	zval_ptr_dtor(&ret);
	GENE_G(autoload_registered) = 1;
	return 1;
}
/* }}} */

/*
 * {{{ gene_load
 */
PHP_METHOD(gene_load, __construct) {

}
/* }}} */

/*
 * {{{ public gene_load::autoload($key)
 */
PHP_METHOD(gene_load, autoload) {
	zend_string *className;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &className) == FAILURE) {
		return;
	}
	gene_load_file_by_class_name(ZSTR_VAL(className));
}
/* }}} */

/*
 * {{{ public gene_load::load($key)
 */
PHP_METHOD(gene_load, import) {
	zval *self = getThis();
	zend_string *php_script;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &php_script) == FAILURE) {
		return;
	}
	if (php_script && ZSTR_LEN(php_script)) {
		if(!gene_load_import(ZSTR_VAL(php_script), NULL, NULL)) {
			php_error_docref(NULL, E_WARNING, "Unable to load file %s", ZSTR_VAL(php_script));
		}
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 *  {{{ public gene_reg::getInstance(void)
 */
PHP_METHOD(gene_load, getInstance) {
	zval *load = gene_load_instance(NULL);
	RETURN_ZVAL(load, 1, 0);
}
/* }}} */

/*
 * {{{ gene_load_methods
 */
const zend_function_entry gene_load_methods[] = {
	PHP_ME(gene_load, __construct, gene_load_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_load, getInstance, gene_load_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_load, import, gene_load_arg_import, ZEND_ACC_PUBLIC)
	PHP_ME(gene_load, autoload, gene_load_arg_autoload, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(load) {
	zend_class_entry gene_load;
	GENE_INIT_CLASS_ENTRY(gene_load, "Gene_Load", "Gene\\Load", gene_load_methods);
	gene_load_ce = zend_register_internal_class_ex(&gene_load, NULL);
	gene_load_ce->ce_flags |= ZEND_ACC_FINAL;
#if PHP_VERSION_ID >= 80200
	gene_load_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif
	//static
	zend_declare_property_null(gene_load_ce, GENE_LOAD_PROPERTY_INSTANCE, strlen(GENE_LOAD_PROPERTY_INSTANCE), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);

	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
