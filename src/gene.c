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
 | Author: Sasou  <admin@php-gene.com> web:www.php-gene.com             |
 +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "ext/standard/info.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "php_gene.h"
#include "gene_application.h"
#include "gene_load.h"
#include "gene_config.h"
#include "gene_router.h"
#include "gene_execute.h"
#include "gene_memory.h"
#include "gene_di.h"
#include "gene_controller.h"
#include "gene_request.h"
#include "gene_response.h"
#include "gene_session.h"
#include "gene_view.h"
#include "gene_exception.h"
#include "gene_db.h"
#include "gene_common.h"
#include "gene_model.h"
#include "gene_service.h"
#include "gene_factory.h"
#include "gene_benchmark.h"

ZEND_DECLARE_MODULE_GLOBALS(gene);


/** {{{ PHP_INI
 */
PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN("gene.run_environment", "1", PHP_INI_SYSTEM, OnUpdateBool, run_environment, zend_gene_globals, gene_globals)
STD_PHP_INI_BOOLEAN("gene.use_namespace", "1", PHP_INI_SYSTEM, OnUpdateBool, use_namespace, zend_gene_globals, gene_globals)
STD_PHP_INI_BOOLEAN("gene.view_compile", "0", PHP_INI_SYSTEM, OnUpdateBool, view_compile, zend_gene_globals, gene_globals)
STD_PHP_INI_BOOLEAN("gene.use_library", "0", PHP_INI_SYSTEM, OnUpdateBool, use_library, zend_gene_globals, gene_globals)
STD_PHP_INI_ENTRY("gene.library_root", "", PHP_INI_SYSTEM, OnUpdateString, library_root, zend_gene_globals, gene_globals)
PHP_INI_END();
/* }}} */

/* {{{ php_gene_init_globals
 */
static void php_gene_init_globals() {
	GENE_G(directory) = NULL;
	GENE_G(app_root) = NULL;
	GENE_G(app_view) = NULL;
	GENE_G(app_ext) = NULL;
	GENE_G(method) = NULL;
	GENE_G(path) = NULL;
	GENE_G(router_path) = NULL;
	GENE_G(module) = NULL;
	GENE_G(controller) = NULL;
	GENE_G(action) = NULL;
	GENE_G(app_key) = NULL;
	GENE_G(auto_load_fun) = NULL;
	GENE_G(child_views) = NULL;
	GENE_G(cache) = NULL;
	GENE_G(cache_easy) = NULL;
	gene_memory_init(TSRMLS_CC);
}
/* }}} */

/* {{{ php_gene_close_globals
 */
static void php_gene_close_globals() {
	if (GENE_G(directory)) {
		efree(GENE_G(directory));
		GENE_G(directory) = NULL;
	}
	if (GENE_G(app_root)) {
		efree(GENE_G(app_root));
		GENE_G(app_root) = NULL;
	}
	if (GENE_G(app_view)) {
		efree(GENE_G(app_view));
		GENE_G(app_view) = NULL;
	}
	if (GENE_G(app_ext)) {
		efree(GENE_G(app_ext));
		GENE_G(app_ext) = NULL;
	}
	if (GENE_G(method)) {
		efree(GENE_G(method));
		GENE_G(method) = NULL;
	}
	if (GENE_G(path)) {
		efree(GENE_G(path));
		GENE_G(path) = NULL;
	}
	if (GENE_G(router_path)) {
		efree(GENE_G(router_path));
		GENE_G(router_path) = NULL;
	}
	if (GENE_G(module)) {
		efree(GENE_G(module));
		GENE_G(module) = NULL;
	}
	if (GENE_G(controller)) {
		efree(GENE_G(controller));
		GENE_G(controller) = NULL;
	}
	if (GENE_G(action)) {
		efree(GENE_G(module));
		GENE_G(module) = NULL;
	}
	if (GENE_G(auto_load_fun)) {
		efree(GENE_G(auto_load_fun));
		GENE_G(auto_load_fun) = NULL;
	}
	if (GENE_G(child_views)) {
		efree(GENE_G(child_views));
		GENE_G(child_views) = NULL;
	}
	if (GENE_G(app_key)) {
		efree(GENE_G(app_key));
		GENE_G(app_key) = NULL;
	}
}
/* }}} */

/** {{{ PHP_GINIT_FUNCTION
 */
PHP_GINIT_FUNCTION(gene) {
	gene_globals->gene_error = 1;
	gene_globals->gene_exception = 0;
	gene_globals->run_environment = 1;
	gene_globals->use_namespace = 1;
	gene_globals->view_compile = 0;
	gene_globals->use_library = 0;
}
/* }}} */

/*
 * {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(gene) {
	php_gene_init_globals();
	REGISTER_INI_ENTRIES();

	/* startup components */
	GENE_STARTUP(application);
	GENE_STARTUP(load);
	GENE_STARTUP(di);
	GENE_STARTUP(config);
	GENE_STARTUP(router);
	GENE_STARTUP(execute);
	GENE_STARTUP(cache);
	GENE_STARTUP(controller);
	GENE_STARTUP(request);
	GENE_STARTUP(response);
	GENE_STARTUP(session);
	GENE_STARTUP(view);
	GENE_STARTUP(benchmark);
	GENE_STARTUP(db);
	GENE_STARTUP(model);
	GENE_STARTUP(service);
	GENE_STARTUP(factory);
	GENE_STARTUP(exception);

	return SUCCESS;
}
/* }}} */

/*
 * {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(gene) {
	UNREGISTER_INI_ENTRIES();
	php_gene_close_globals();

	if (GENE_G(cache)) {
		gene_hash_destroy(GENE_G(cache));
		GENE_G(cache) = NULL;
	}
	if (GENE_G(cache_easy)) {
		gene_hash_destroy(GENE_G(cache_easy));
		GENE_G(cache_easy) = NULL;
	}
	return SUCCESS;
}

/* }}} */

/*
 * {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(gene) {
	return SUCCESS;
}
/* }}} */

/*
 * {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(gene) {
	php_gene_close_globals();
	return SUCCESS;
}
/* }}} */

/*
 * {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(gene) {
	php_info_print_table_start();
	php_info_print_table_header(2, "gene support", "enabled");
	php_info_print_table_row(2, "Author:", " sasou <admin@php-gene.com>");
	php_info_print_table_row(2, "Web site:", " http://www.php-gene.com");
	php_info_print_table_row(2, "Version:", PHP_GENE_VERSION);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/*
 * {{{ gene_load()
 */
PHP_FUNCTION(gene_call) {
	char *class = NULL, *action = NULL;
	zend_long class_len = 0, action_len = 0;
	zval *params = NULL, classObject, ret;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "ssz", &class, &class_len, &action, &action_len, &params) == FAILURE) {
		return;
	}
	if (GENE_G(module) != NULL) {
		class = strreplace2(class, ":m", GENE_G(module));
	}
	if (GENE_G(controller) != NULL) {
		class = strreplace2(class, ":c", GENE_G(controller));
	}

	if (gene_factory(class, strlen(class), NULL, &classObject)) {
		if (GENE_G(action) != NULL) {
			action = strreplace2(action, ":a", GENE_G(action));
		}
		strtolower(action);
		if (Z_TYPE(classObject) == IS_OBJECT
				&& zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), action, strlen(action))) {
			gene_factory_call_action(&classObject, action, params, &ret);
			RETURN_ZVAL(&ret, 1, 0);
		} else {
			php_error_docref(NULL, E_WARNING, "Unable to call method '%s' in class '%s'." , action, class);
		}
		RETURN_NULL();

	} else {
		php_error_docref(NULL, E_WARNING, "Unable to init calss '%s'." , class);
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_version()
 */
PHP_FUNCTION(gene_version) {
	RETURN_STRING(PHP_GENE_VERSION);
}
/* }}} */

/* {{{ gene_functions[]
 *
 * Every user visible function must have an entry in gene_functions[].
 */
zend_function_entry gene_functions[] = {
	PHP_FE(gene_call,NULL)
	PHP_FE(gene_version,NULL)
	PHP_FE_END
};
/* }}} */

/*
 * {{{ gene_module_entry
 */
#ifdef COMPILE_DL_GENE
ZEND_GET_MODULE(gene)
#endif
/* }}} */

/** {{{ module depends
 */
#if ZEND_MODULE_API_NO >= 20050922
zend_module_dep gene_deps[] = {
	ZEND_MOD_REQUIRED("spl")
	ZEND_MOD_REQUIRED("pcre")
	ZEND_MOD_OPTIONAL("session")
	{ NULL, NULL, NULL }
};
#endif
/* }}} */

/** {{{ gene_module_entry
 */
zend_module_entry gene_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
	STANDARD_MODULE_HEADER_EX, NULL, gene_deps,
#else
	STANDARD_MODULE_HEADER,
#endif
	"gene", gene_functions,
	PHP_MINIT(gene),
	PHP_MSHUTDOWN(gene),
	PHP_RINIT(gene),
	PHP_RSHUTDOWN(gene),
	PHP_MINFO(gene),
	PHP_GENE_VERSION,
	PHP_MODULE_GLOBALS(gene),
	PHP_GINIT(gene),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
