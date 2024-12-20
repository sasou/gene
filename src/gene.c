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

#include "gene.h"
#include "app/application.h"
#include "factory/load.h"
#include "config/configs.h"
#include "router/router.h"
#include "tool/execute.h"
#include "cache/memory.h"
#include "di/di.h"
#include "mvc/controller.h"
#include "http/request.h"
#include "http/response.h"
#include "http/validate.h"
#include "session/session.h"
#include "mvc/view.h"
#include "exception/exception.h"
#include "db/pdo.h"
#include "db/mysql.h"
#include "db/mssql.h"
#include "db/pgsql.h"
#include "db/sqlite.h"
#include "common/common.h"
#include "mvc/model.h"
#include "service/service.h"
#include "factory/factory.h"
#include "tool/benchmark.h"
#include "cache/memcached.h"
#include "cache/redis.h"
#include "cache/cache.h"

ZEND_DECLARE_MODULE_GLOBALS(gene);


/** {{{ PHP_INI
 */
PHP_INI_BEGIN()
STD_PHP_INI_ENTRY("gene.run_environment", "1", PHP_INI_SYSTEM, OnUpdateLong, run_environment, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.use_namespace", "1", PHP_INI_SYSTEM, OnUpdateBool, use_namespace, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.view_compile", "0", PHP_INI_SYSTEM, OnUpdateBool, view_compile, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.use_library", "0", PHP_INI_SYSTEM, OnUpdateBool, use_library, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.library_root", "", PHP_INI_SYSTEM, OnUpdateString, library_root, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
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
	GENE_G(lang) = NULL;
	GENE_G(path_params) = NULL;
	GENE_G(cache) = NULL;
	GENE_G(cache_easy) = NULL;
	gene_memory_init();
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
	if (GENE_G(lang)) {
		efree(GENE_G(lang));
		GENE_G(lang) = NULL;
	}
	if (GENE_G(app_key)) {
		efree(GENE_G(app_key));
		GENE_G(app_key) = NULL;
	}
	if (GENE_G(path_params)) {
		efree(GENE_G(path_params));
		GENE_G(path_params) = NULL;
	}

}
/* }}} */

/** {{{ PHP_GINIT_FUNCTION
 */
PHP_GINIT_FUNCTION(gene) {
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
	GENE_STARTUP(memory);
	GENE_STARTUP(controller);
	GENE_STARTUP(request);
	GENE_STARTUP(response);
	GENE_STARTUP(validate);
	GENE_STARTUP(session);
	GENE_STARTUP(view);
	GENE_STARTUP(benchmark);
	GENE_STARTUP(db_mysql);
	GENE_STARTUP(db_mssql);
	GENE_STARTUP(db_pgsql);
	GENE_STARTUP(db_sqlite);
	GENE_STARTUP(model);
	GENE_STARTUP(service);
	GENE_STARTUP(factory);
	GENE_STARTUP(redis);
	GENE_STARTUP(memcached);
	GENE_STARTUP(cache);
	GENE_STARTUP(exception);

	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */


/** {{{ ARG_INFO
 *  */
ZEND_BEGIN_ARG_INFO_EX(gene_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()
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
	return SUCCESS; // @suppress("Symbol is not resolved")
}

/* }}} */

/*
 * {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(gene) {
	if (!GENE_G(path_params)) {
		GENE_G(path_params) =  (zval*) pemalloc(sizeof(zval), 0);
		array_init(GENE_G(path_params));
	}
	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(gene) {
	php_gene_close_globals();
	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(gene) {
	php_info_print_table_start();
	php_info_print_table_header(2, "gene support", "enabled");
	php_info_print_table_row(2, "gene author:", " sasou <zaipd@qq.com>");
	php_info_print_table_row(2, "gene site:", " http://www.1xm.net");
	php_info_print_table_row(2, "gene version:", PHP_GENE_VERSION);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
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
	PHP_FE(gene_version, gene_void_arginfo)
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
	{ NULL, NULL, NULL }
};
#endif
/* }}} */

/** {{{ gene_module_entry
 */
zend_module_entry gene_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
	STANDARD_MODULE_HEADER_EX, NULL, gene_deps, // @suppress("Symbol is not resolved")
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
