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
#include "tool/language.h"
#include "cache/memory.h"
#include "di/di.h"
#include "mvc/controller.h"
#include "http/request.h"
#include "http/webscan.h"
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
STD_PHP_INI_ENTRY("gene.runtime_type", "1", PHP_INI_SYSTEM, OnUpdateLong, runtime_type, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.use_namespace", "1", PHP_INI_SYSTEM, OnUpdateBool, use_namespace, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.view_compile", "0", PHP_INI_SYSTEM, OnUpdateBool, view_compile, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_BOOLEAN("gene.use_library", "0", PHP_INI_SYSTEM, OnUpdateBool, use_library, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
STD_PHP_INI_ENTRY("gene.library_root", "", PHP_INI_SYSTEM, OnUpdateString, library_root, zend_gene_globals, gene_globals) // @suppress("Symbol is not resolved")
PHP_INI_END();
/* }}} */

/* {{{ gene_get_coroutine_id */
zend_long gene_get_coroutine_id(void) {
	zval retval, fname;
	zend_long cid = -1;
	ZVAL_STRING(&fname, "Swoole\\Coroutine::getCid");
	if (call_user_function(NULL, NULL, &fname, &retval, 0, NULL) == SUCCESS) {
		if (Z_TYPE(retval) == IS_LONG) {
			cid = Z_LVAL(retval);
		}
		zval_ptr_dtor(&retval);
	}
	zval_ptr_dtor(&fname);
	return cid;
}
/* }}} */

/* {{{ gene_request_context_init */
void gene_request_context_init(gene_request_context *ctx) {
	if (!ctx) return;
	memset(ctx, 0, sizeof(gene_request_context));
	ctx->path_params = (zval*) emalloc(sizeof(zval));
	array_init(ctx->path_params);
	ZVAL_UNDEF(&ctx->request_attr);
	ZVAL_UNDEF(&ctx->di_regs);
	ZVAL_UNDEF(&ctx->response);
	ZVAL_UNDEF(&ctx->view_vars);
	ZVAL_UNDEF(&ctx->db_mysql_history);
	ZVAL_UNDEF(&ctx->db_pgsql_history);
	ZVAL_UNDEF(&ctx->db_sqlite_history);
	ZVAL_UNDEF(&ctx->db_mssql_history);
	ctx->view_scope_no = 0;
}
/* }}} */

/* {{{ gene_request_context_reset */
void gene_request_context_reset(gene_request_context *ctx) {
	if (!ctx) return;
	if (ctx->method) { efree(ctx->method); ctx->method = NULL; }
	if (ctx->path) { efree(ctx->path); ctx->path = NULL; }
	if (ctx->router_path) { efree(ctx->router_path); ctx->router_path = NULL; }
	if (ctx->module) { efree(ctx->module); ctx->module = NULL; }
	if (ctx->controller) { efree(ctx->controller); ctx->controller = NULL; }
	if (ctx->action) { efree(ctx->action); ctx->action = NULL; }
	if (ctx->child_views) { efree(ctx->child_views); ctx->child_views = NULL; }
	if (ctx->lang) { efree(ctx->lang); ctx->lang = NULL; }
	if (ctx->path_params) {
		zval_ptr_dtor(ctx->path_params);
		efree(ctx->path_params);
		ctx->path_params = NULL;
	}
	if (Z_TYPE(ctx->request_attr) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->request_attr);
		ZVAL_UNDEF(&ctx->request_attr);
	}
	if (Z_TYPE(ctx->di_regs) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->di_regs);
		ZVAL_UNDEF(&ctx->di_regs);
	}
	if (Z_TYPE(ctx->response) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->response);
		ZVAL_UNDEF(&ctx->response);
	}
	if (Z_TYPE(ctx->view_vars) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->view_vars);
		ZVAL_UNDEF(&ctx->view_vars);
	}
	if (Z_TYPE(ctx->db_mysql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_mysql_history);
		ZVAL_UNDEF(&ctx->db_mysql_history);
	}
	if (Z_TYPE(ctx->db_pgsql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_pgsql_history);
		ZVAL_UNDEF(&ctx->db_pgsql_history);
	}
	if (Z_TYPE(ctx->db_sqlite_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_sqlite_history);
		ZVAL_UNDEF(&ctx->db_sqlite_history);
	}
	if (Z_TYPE(ctx->db_mssql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_mssql_history);
		ZVAL_UNDEF(&ctx->db_mssql_history);
	}
	ctx->path_params = (zval*) emalloc(sizeof(zval));
	array_init(ctx->path_params);
	ctx->view_scope_no = 0;
}
/* }}} */

/* {{{ gene_request_context_destroy */
void gene_request_context_destroy(gene_request_context *ctx) {
	if (!ctx) return;
	if (ctx->method) { efree(ctx->method); ctx->method = NULL; }
	if (ctx->path) { efree(ctx->path); ctx->path = NULL; }
	if (ctx->router_path) { efree(ctx->router_path); ctx->router_path = NULL; }
	if (ctx->module) { efree(ctx->module); ctx->module = NULL; }
	if (ctx->controller) { efree(ctx->controller); ctx->controller = NULL; }
	if (ctx->action) { efree(ctx->action); ctx->action = NULL; }
	if (ctx->child_views) { efree(ctx->child_views); ctx->child_views = NULL; }
	if (ctx->lang) { efree(ctx->lang); ctx->lang = NULL; }
	if (ctx->path_params) {
		zval_ptr_dtor(ctx->path_params);
		efree(ctx->path_params);
		ctx->path_params = NULL;
	}
	if (Z_TYPE(ctx->request_attr) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->request_attr);
		ZVAL_UNDEF(&ctx->request_attr);
	}
	if (Z_TYPE(ctx->di_regs) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->di_regs);
		ZVAL_UNDEF(&ctx->di_regs);
	}
	if (Z_TYPE(ctx->response) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->response);
		ZVAL_UNDEF(&ctx->response);
	}
	if (Z_TYPE(ctx->view_vars) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->view_vars);
		ZVAL_UNDEF(&ctx->view_vars);
	}
	if (Z_TYPE(ctx->db_mysql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_mysql_history);
		ZVAL_UNDEF(&ctx->db_mysql_history);
	}
	if (Z_TYPE(ctx->db_pgsql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_pgsql_history);
		ZVAL_UNDEF(&ctx->db_pgsql_history);
	}
	if (Z_TYPE(ctx->db_sqlite_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_sqlite_history);
		ZVAL_UNDEF(&ctx->db_sqlite_history);
	}
	if (Z_TYPE(ctx->db_mssql_history) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->db_mssql_history);
		ZVAL_UNDEF(&ctx->db_mssql_history);
	}
	ctx->view_scope_no = 0;
}
/* }}} */

/* {{{ gene_co_context_dtor - destructor for co_contexts HashTable entries */
static void gene_co_context_dtor(zval *zv) {
	gene_request_context *ctx = (gene_request_context *)Z_PTR_P(zv);
	if (ctx) {
		gene_request_context_destroy(ctx);
		efree(ctx);
	}
}
/* }}} */

/* {{{ gene_init_co_contexts */
void gene_init_co_contexts(void) {
	if (!GENE_G(co_contexts)) {
		ALLOC_HASHTABLE(GENE_G(co_contexts));
		zend_hash_init(GENE_G(co_contexts), 8, NULL, gene_co_context_dtor, 0);
	}
}
/* }}} */

/* {{{ gene_request_ctx */
gene_request_context *gene_request_ctx(void) {
	gene_request_context *ctx;
	zend_long cid;
	if (GENE_G(runtime_type) < 2) {
		return &GENE_G(default_ctx);
	}
	gene_init_co_contexts();
	cid = gene_get_coroutine_id();
	if (cid < 0) {
		if (!GENE_G(resident_ctx)) {
			GENE_G(resident_ctx) = ecalloc(1, sizeof(gene_request_context));
			gene_request_context_init(GENE_G(resident_ctx));
		}
		GENE_G(current_cid) = -2;
		GENE_G(current_ctx) = GENE_G(resident_ctx);
		return GENE_G(resident_ctx);
	}
	if (GENE_G(current_cid) == cid && GENE_G(current_ctx)) {
		return GENE_G(current_ctx);
	}
	ctx = zend_hash_index_find_ptr(GENE_G(co_contexts), (zend_ulong)cid);
	if (!ctx) {
		ctx = ecalloc(1, sizeof(gene_request_context));
		gene_request_context_init(ctx);
		zend_hash_index_update_ptr(GENE_G(co_contexts), (zend_ulong)cid, ctx);
	}
	GENE_G(current_cid) = cid;
	GENE_G(current_ctx) = ctx;
	return ctx;
}
/* }}} */

/* {{{ php_gene_init_globals
 */
static void php_gene_init_globals() {
	GENE_G(directory) = NULL;
	GENE_G(app_root) = NULL;
	GENE_G(app_view) = NULL;
	GENE_G(app_ext) = NULL;
	GENE_G(app_key) = NULL;
	GENE_G(auto_load_fun) = NULL;
	memset(&GENE_G(default_ctx), 0, sizeof(gene_request_context));
	GENE_G(resident_ctx) = NULL;
	GENE_G(co_contexts) = NULL;
	GENE_G(current_ctx) = NULL;
	GENE_G(current_cid) = -1;
	GENE_G(cache) = NULL;
	GENE_G(cache_easy) = NULL;
	gene_rwlock_init(&GENE_G(cache_lock));
	gene_memory_init();
}
/* }}} */

/* {{{ php_gene_close_request_globals
 */
static void php_gene_close_request_globals() {
	gene_request_context_destroy(&GENE_G(default_ctx));
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
	if (GENE_G(auto_load_fun)) {
		efree(GENE_G(auto_load_fun));
		GENE_G(auto_load_fun) = NULL;
	}
	if (GENE_G(app_key)) {
		efree(GENE_G(app_key));
		GENE_G(app_key) = NULL;
	}
	GENE_G(current_ctx) = NULL;
	GENE_G(current_cid) = -1;
	if (GENE_G(resident_ctx)) {
		gene_request_context *tmp = GENE_G(resident_ctx);
		GENE_G(resident_ctx) = NULL;
		gene_request_context_destroy(tmp);
		efree(tmp);
	}
	if (GENE_G(co_contexts)) {
		zend_hash_destroy(GENE_G(co_contexts));
		FREE_HASHTABLE(GENE_G(co_contexts));
		GENE_G(co_contexts) = NULL;
	}
}
/* }}} */

/* {{{ php_gene_close_globals
 */
static void php_gene_close_globals() {
	php_gene_close_request_globals();
}
/* }}} */

/** {{{ PHP_GINIT_FUNCTION
 */
PHP_GINIT_FUNCTION(gene) {
	gene_globals->run_environment = 1;
	gene_globals->runtime_type = 1;
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
	GENE_STARTUP(language);
	GENE_STARTUP(memory);
	GENE_STARTUP(controller);
	GENE_STARTUP(request);
	GENE_STARTUP(webscan);
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
	gene_rwlock_destroy(&GENE_G(cache_lock));
	return SUCCESS; // @suppress("Symbol is not resolved")
}

/* }}} */

/*
 * {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(gene) {
	if (!GENE_G(default_ctx).path_params) {
		gene_request_context_init(&GENE_G(default_ctx));
	} else {
		gene_request_context_reset(&GENE_G(default_ctx));
	}
	if (GENE_G(runtime_type) >= 2 && !GENE_G(co_contexts)) {
		ALLOC_HASHTABLE(GENE_G(co_contexts));
		zend_hash_init(GENE_G(co_contexts), 8, NULL, gene_co_context_dtor, 0);
	}
	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(gene) {
	php_gene_close_request_globals();
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
	php_info_print_table_row(2, "gene site:", " https://www.1xm.net");
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
const zend_function_entry gene_functions[] = {
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
const zend_module_dep gene_deps[] = {
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



