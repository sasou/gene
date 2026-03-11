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
#include "standard/php_filestat.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../app/application.h"
#include "../factory/load.h"
#include "../cache/memory.h"
#include "../config/configs.h"
#include "../router/router.h"
#include "../http/request.h"
#include "../common/common.h"
#include "../mvc/view.h"
#include "../di/di.h"
#include "../exception/exception.h"

zend_class_entry * gene_application_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_application_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, safe)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_instance, 0, 0, 0)
	ZEND_ARG_INFO(0, safe)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_load, 0, 0, 1)
	ZEND_ARG_INFO(0, file)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, validity)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_autoload, 0, 0, 1)
	ZEND_ARG_INFO(0, app_root)
	ZEND_ARG_INFO(0, auto_function)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_mode, 0, 0, 0)
	ZEND_ARG_INFO(0, error_type)
	ZEND_ARG_INFO(0, exception_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_view, 0, 0, 0)
	ZEND_ARG_INFO(0, view)
	ZEND_ARG_INFO(0, tpl_ext)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_error, 0, 0, 3)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, error_type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_exception, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_run, 0, 0, 0)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, uri)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_method, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_path, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_uri, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_lang, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_module, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_controller, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_action, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_environment, 0, 0, 0)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_environment, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_environment_name, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_runtime_type, 0, 0, 0)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_runtime_type, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get_runtime_type_name, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_config, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set_response, 0, 0, 1)
    ZEND_ARG_INFO(0, response)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

/*
 * {{{ void load_file(char *key, size_t key_len,char *php_script, int validity)
 */
void load_file(char *key, size_t key_len, char *php_script, int validity) {

	int import = 0;
	zend_long cur, times = 0;
	filenode *val;
	if (key_len) {
		val = file_cache_get_easy(key, key_len);
		if (val) {
			cur = time(NULL);
			times = cur - val->stime;
			if (times > val->validity) {
				val->stime = cur;
				val->validity = validity;
				cur = gene_file_modified(php_script, 0);
				if (cur != val->ftime) {
					import = 1;
					val->ftime = cur;
				}
			}
		} else {
			import = 1;
			file_cache_set_val(key, key_len,
					gene_file_modified(php_script, 0), validity);
		}
		if (import) {
			if(!gene_load_import(php_script, NULL, NULL)) {
				php_error_docref(NULL, E_WARNING, "Unable to load config file %s", php_script);
			}
		}
	}
}
/* }}} */

/** {{{ gene_file_modified
 */
zend_long gene_file_modified(char *file, zend_long ctime) {
	zval n_ctime;
	#if PHP_VERSION_ID >= 80101
		zend_string *filename;
		filename = zend_string_init(file, strlen(file), 0);
		php_stat(filename, 6 /* FS_MTIME */, &n_ctime);
		zend_string_release(filename);
	#else
	    php_stat(file, strlen(file), 6 /* FS_MTIME */, &n_ctime);
	#endif
	if (ctime != Z_LVAL(n_ctime)) {
		return Z_LVAL(n_ctime);
	}
	return 0;
}
/* }}} */

/** {{{ void gene_ini_router()
 */
void gene_ini_router() {
	zval *server = NULL, *temp = NULL;
	if (!GENE_REQ(method) && !GENE_REQ(path) && !GENE_G(directory)) {
		server = request_query(TRACK_VARS_SERVER, NULL, 0);
		if (server) {
			if ((temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("DOCUMENT_ROOT"))) != NULL) {
				GENE_G(directory) = estrndup(Z_STRVAL_P(temp), Z_STRLEN_P(temp));
			}
			if ((temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("REQUEST_METHOD"))) != NULL) {
				GENE_REQ(method) = estrndup(Z_STRVAL_P(temp), Z_STRLEN_P(temp));
				strtolower(GENE_REQ(method));
			}
			if ((temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("REQUEST_URI"))) != NULL) {
				GENE_REQ(path) = ecalloc(Z_STRLEN_P(temp)+1, sizeof(char));
				leftByChar(GENE_REQ(path), Z_STRVAL_P(temp), '?');
			}
		}
		server = NULL;
		temp = NULL;
	}
}
/* }}} */

/*
 *  {{{ zval *gene_application_instance(zval *this_ptr,zval *safe)
 */
zval *gene_application_instance(zval *this_ptr, zval *safe) {
	zval *instance = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), 1);

	if (Z_TYPE_P(instance) == IS_OBJECT) {
		return instance;
	}
	if (this_ptr) {
		instance = this_ptr;
		zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), instance);
	} else {
		zval new_instance;
		object_init_ex(&new_instance, gene_application_ce);
		zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), &new_instance);
		zval_ptr_dtor(&new_instance);
		instance = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), 1);

		gene_ini_router();
		if (safe && !GENE_G(app_key)) {
			GENE_G(app_key) = estrndup(Z_STRVAL_P(safe), Z_STRLEN_P(safe));
		}

		if (GENE_G(directory) != NULL) {
			if (GENE_G(app_root)) {
				efree(GENE_G(app_root));
			}
			spprintf(&GENE_G(app_root), 0, "%s/application", GENE_G(directory));
		}
	}
	return instance;
}
/* }}} */


/*
 *  {{{ public gene_application::getInstance(void)
 */
PHP_METHOD(gene_application, getInstance) {
	zval *safe = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_ZVAL(gene_application_instance(NULL, safe), 1, 0);
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __construct) {
	zval *safe = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_ZVAL(gene_application_instance(NULL, safe), 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::load($key,$validity)
 */
PHP_METHOD(gene_application, load) {
	zval *self = getThis();
	zend_string *file = NULL, *path = NULL;
	char *cache_key;
	int validity = 10;
	size_t cache_key_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|Sl", &file, &path, &validity) == FAILURE) {
		return;
	}
	if (path && ZSTR_LEN(path) > 0) {
		cache_key_len = spprintf(&cache_key, 0, "%s/%s", ZSTR_VAL(path), ZSTR_VAL(file));
	} else {
		cache_key_len = spprintf(&cache_key, 0, "%s/Config/%s", GENE_G(app_root), ZSTR_VAL(file));
	}
	load_file(cache_key, cache_key_len, cache_key, validity);
	efree(cache_key);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::getMethod()
 */
PHP_METHOD(gene_application, getMethod) {
	if (GENE_REQ(method)) {
		RETURN_STRING(GENE_REQ(method));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getPath()
 */
PHP_METHOD(gene_application, getPath) {
	if (GENE_REQ(path)) {
		RETURN_STRING(GENE_REQ(path));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getModule()
 */
PHP_METHOD(gene_application, getModule) {
	if (GENE_REQ(module)) {
		RETURN_STRING(GENE_REQ(module));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getController()
 */
PHP_METHOD(gene_application, getController) {
	if (GENE_REQ(controller)) {
		RETURN_STRING(GENE_REQ(controller));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getAction()
 */
PHP_METHOD(gene_application, getAction) {
	if (GENE_REQ(action)) {
		RETURN_STRING(GENE_REQ(action));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getLang()
 */
PHP_METHOD(gene_application, getLang) {
	if (GENE_REQ(lang)) {
		RETURN_STRING(GENE_REQ(lang));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getRouterUri()
 */
PHP_METHOD(gene_application, getRouterUri) {
	char *path = NULL, *new_path = NULL;
	if (!GENE_REQ(router_path)) {
		RETURN_NULL();
	}

	path = str_init(GENE_REQ(router_path));
	if (GENE_REQ(module) != NULL) {
		new_path = strreplace2(path, ":m", GENE_REQ(module));
		efree(path);
		path = new_path;
	}
	if (GENE_REQ(controller) != NULL) {
		new_path = strreplace2(path, ":c", GENE_REQ(controller));
		efree(path);
		path = new_path;
	}
	if (GENE_REQ(action) != NULL) {
		new_path = strreplace2(path, ":a", GENE_REQ(action));
		efree(path);
		path = new_path;
	}
	strtolower(path);
	RETVAL_STRING(path);
	efree(path);
}
/* }}} */

/*
 * {{{ public gene_application::getEnvironment()
 */
PHP_METHOD(gene_application, getEnvironment) {
	RETURN_LONG(GENE_G(run_environment));
}
/* }}} */

/*
 * {{{ public gene_application::getEnvironmentName()
 */
PHP_METHOD(gene_application, getEnvironmentName) {
	switch (GENE_G(run_environment)) {
		case 2:
			RETURN_STRING("prod");
		case 1:
			RETURN_STRING("test");
		case 0:
		default:
			RETURN_STRING("dev");
	}
}
/* }}} */

/*
 * {{{ public gene_application::getRuntimeType()
 */
PHP_METHOD(gene_application, getRuntimeType) {
	RETURN_LONG(GENE_G(runtime_type));
}
/* }}} */

/*
 * {{{ public gene_application::getRuntimeTypeName()
 */
PHP_METHOD(gene_application, getRuntimeTypeName) {
	switch (GENE_G(runtime_type)) {
		case 2:
			RETURN_STRING("swoole");
		case 3:
			RETURN_STRING("coroutine");
		case 1:
		default:
			RETURN_STRING("fpm");
	}
}
/* }}} */


/*
 * {{{ public gene_application::params($key)
 */
PHP_METHOD(gene_application, params) {
	char *name = NULL;
	zend_long name_len = 0;
	zval *params = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &name, &name_len) == FAILURE) {
		return;
	}

	params = GENE_REQ(path_params);
	if (name_len == 0) {
		RETURN_ZVAL(GENE_REQ(path_params), 1, 0);
	} else {
		zval *val = zend_symtable_str_find(Z_ARRVAL_P(params), name, name_len);
		if (val) {
			RETURN_ZVAL(val, 1, 0);
		}
		RETURN_NULL();
	}
}
/* }}} */

/*
 * {{{ public gene_application::setEnvironment($type)
 */
PHP_METHOD(gene_application, setEnvironment) {
	zval *self = getThis();
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &type) == FAILURE) {
		return;
	}
	GENE_G(run_environment) = type;
	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::setRuntimeType($type)
 */
PHP_METHOD(gene_application, setRuntimeType) {
	zval *self = getThis(), *type = NULL;
	zend_long runtime = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &type) == FAILURE) {
		return;
	}
	if (type == NULL || Z_TYPE_P(type) == IS_NULL) {
		RETURN_FALSE;
	}
	if (Z_TYPE_P(type) == IS_LONG) {
		runtime = Z_LVAL_P(type);
	} else if (Z_TYPE_P(type) == IS_STRING) {
		char *name = Z_STRVAL_P(type);
		size_t len = Z_STRLEN_P(type);
		if ((len == 3 && strncasecmp(name, "fpm", 3) == 0)
		|| (len == 7 && strncasecmp(name, "php-fpm", 7) == 0)) {
			runtime = 1;
		} else if ((len == 6 && strncasecmp(name, "swoole", 6) == 0)
		|| (len == 8 && strncasecmp(name, "resident", 8) == 0)
		|| (len == 6 && strncasecmp(name, "daemon", 6) == 0)) {
			runtime = 2;
		} else if ((len == 9 && strncasecmp(name, "coroutine", 9) == 0)
		|| (len == 2 && strncasecmp(name, "co", 2) == 0)) {
			runtime = 3;
		} else {
			php_error_docref(NULL, E_WARNING, "Unknown runtime type '%s', fallback to fpm.", name);
			runtime = 1;
		}
	} else {
		php_error_docref(NULL, E_WARNING, "setRuntimeType type must be int or string.");
		RETURN_FALSE;
	}

	if (runtime < 1) {
		runtime = 1;
	}
	GENE_G(runtime_type) = runtime;
	if (runtime >= 2) {
		gene_init_co_contexts();
	}
	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ void gene_clear_request_state()
 */
static void gene_clear_request_state() {
	if (GENE_REQ(method)) {
		efree(GENE_REQ(method));
		GENE_REQ(method) = NULL;
	}
	if (GENE_REQ(path)) {
		efree(GENE_REQ(path));
		GENE_REQ(path) = NULL;
	}
	if (GENE_REQ(router_path)) {
		efree(GENE_REQ(router_path));
		GENE_REQ(router_path) = NULL;
	}
	if (GENE_REQ(module)) {
		efree(GENE_REQ(module));
		GENE_REQ(module) = NULL;
	}
	if (GENE_REQ(controller)) {
		efree(GENE_REQ(controller));
		GENE_REQ(controller) = NULL;
	}
	if (GENE_REQ(action)) {
		efree(GENE_REQ(action));
		GENE_REQ(action) = NULL;
	}
	if (GENE_REQ(child_views)) {
		efree(GENE_REQ(child_views));
		GENE_REQ(child_views) = NULL;
	}
	if (GENE_REQ(lang)) {
		efree(GENE_REQ(lang));
		GENE_REQ(lang) = NULL;
	}
	if (GENE_REQ(path_params)) {
		zval_ptr_dtor(GENE_REQ(path_params));
		efree(GENE_REQ(path_params));
		GENE_REQ(path_params) = NULL;
	}
	GENE_REQ(path_params) = (zval*) pemalloc(sizeof(zval), 0);
	array_init(GENE_REQ(path_params));
	if (Z_TYPE(GENE_REQ(response)) != IS_UNDEF) {
		zval_ptr_dtor(&GENE_REQ(response));
		ZVAL_UNDEF(&GENE_REQ(response));
	}
}
/* }}} */

/*
 * {{{ public gene_application::clearState()
 */
PHP_METHOD(gene_application, clearState) {
	zval *self = getThis();
	gene_clear_request_state();

	zval null_val;
	ZVAL_NULL(&null_val);
	zend_update_static_property(gene_request_ce, GENE_REQUEST_PROPERTY_ATTR, strlen(GENE_REQUEST_PROPERTY_ATTR), &null_val);

	if (Z_TYPE(gene_request_ctx()->di_regs) != IS_UNDEF) {
		zval_ptr_dtor(&gene_request_ctx()->di_regs);
		ZVAL_UNDEF(&gene_request_ctx()->di_regs);
	}
	if (Z_TYPE(gene_request_ctx()->request_attr) != IS_UNDEF) {
		zval_ptr_dtor(&gene_request_ctx()->request_attr);
		ZVAL_UNDEF(&gene_request_ctx()->request_attr);
	}

	gene_view_reset_vars();

	if (self) {
		RETURN_ZVAL(self, 1, 0);
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::destroyContext()
 */
PHP_METHOD(gene_application, destroyContext) {
	if (GENE_G(runtime_type) >= 2 && GENE_G(co_contexts)) {
		zend_long cid = gene_get_coroutine_id();
		if (cid >= 0) {
			zend_hash_index_del(GENE_G(co_contexts), (zend_ulong)cid);
			if (GENE_G(current_cid) == cid) {
				GENE_G(current_ctx) = NULL;
				GENE_G(current_cid) = -1;
			}
		}
	}
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::setResponse($response)
 */
PHP_METHOD(gene_application, setResponse) {
	zval *resp;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &resp) == FAILURE) {
		return;
	}
	gene_request_context *ctx = gene_request_ctx();
	if (Z_TYPE(ctx->response) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->response);
	}
	ZVAL_COPY(&ctx->response, resp);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::config()
 */
PHP_METHOD(gene_application, config) {
	zval *cache = NULL;
	size_t router_e_len;
	char *router_e;
	zend_string *keyString;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString) == FAILURE) {
		return;
	}

	if (GENE_G(app_key)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(app_key),
		GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(directory),
		GENE_CONFIG_CACHE);
	}
	cache = gene_memory_get_by_config(router_e, router_e_len, ZSTR_VAL(keyString));
	efree(router_e);
	if (cache) {
		gene_memory_zval_local(return_value, cache);
		return;
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::autoload($key)
 */
PHP_METHOD(gene_application, autoload) {
	zend_string *fileName = NULL, *app_root = NULL;
	zval *self = getThis();
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &app_root, &fileName) == FAILURE) {
		return;
	}
	if (app_root && ZSTR_LEN(app_root) > 0) {
		if (GENE_G(app_root)) {
			efree(GENE_G(app_root));
		}
		GENE_G(app_root) = estrndup(ZSTR_VAL(app_root), ZSTR_LEN(app_root));
	}
	if (fileName && ZSTR_LEN(fileName) > 0) {
		if (GENE_G(auto_load_fun)) {
			efree(GENE_G(auto_load_fun));
		}
		GENE_G(auto_load_fun) = estrndup(ZSTR_VAL(fileName), ZSTR_LEN(fileName));
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::error($callback, $type)
 */
PHP_METHOD(gene_application, error) {
	zval *callback = NULL, *error_type = NULL, *self = getThis();
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|zz", &type, &callback, &error_type) == FAILURE) {
		return;
	}
	if (type > 0) {
		gene_exception_error_register(callback, error_type);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public gene_application::exception(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
 */
PHP_METHOD(gene_application, exception) {
	zval *callback = NULL, *error_type = NULL, *self = getThis();
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|z", &type, &callback) == FAILURE) {
		return;
	}
	if (type > 0) {
		gene_exception_register(callback);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::setMode($error_type, $exception_type)
 */
PHP_METHOD(gene_application, setMode) {
	zval *self = getThis();
	zend_long error_type = 0, exception_type = 0;
	zval *error_callback = NULL, *ex_callback = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|llzz", &error_type, &exception_type, &ex_callback, &error_callback) == FAILURE) {
		return;
	}
	if (error_type > 0) {
		gene_exception_error_register(error_callback, NULL);
	}
	if (exception_type > 0) {
		gene_exception_register(ex_callback);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::setView($view, $ext)
 */
PHP_METHOD(gene_application, setView) {
	zval *self = getThis();
	zend_string *view = NULL, *tpl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &view, &tpl) == FAILURE) {
		return;
	}
	if (view && ZSTR_LEN(view)) {
		GENE_G(app_view) = estrndup(ZSTR_VAL(view), ZSTR_LEN(view));
	} else {
		GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, strlen(GENE_VIEW_VIEW));
	}
	if (tpl && ZSTR_LEN(tpl)) {
		GENE_G(app_ext) = estrndup(ZSTR_VAL(tpl), ZSTR_LEN(tpl));
	} else {
		GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::run($method,$path)
 */
PHP_METHOD(gene_application, run) {
	zend_string *methodin = NULL, *pathin = NULL;
	zval *self = getThis(), safe;
	char *min = NULL, *pin = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &methodin, &pathin) == FAILURE) {
		RETURN_NULL();
	}

	gene_loader_register();

	if (GENE_G(app_key)) {
		ZVAL_STRING(&safe, GENE_G(app_key));
	} else {
		ZVAL_STRING(&safe, GENE_G(directory));
	}
	if (methodin && ZSTR_LEN(methodin)) {
		min = ZSTR_VAL(methodin);
	}
	if (pathin && ZSTR_LEN(pathin)) {
		pin = ZSTR_VAL(pathin);
	}

	get_router_content_run(min, pin, &safe);
	zval_ptr_dtor(&safe);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __set)
{
	zend_string *name;
	zval *value;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	zval class_name;
	gene_class_name(&class_name);
	if (gene_di_set_class(Z_STR(class_name), name, value)) {
		zval_ptr_dtor(&class_name);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&class_name);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __get)
{
	zval *pzval = NULL;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		zval class_name;
		gene_class_name(&class_name);
		pzval = gene_di_get_class(Z_STR(class_name), name);
		if (pzval) {
			zval_ptr_dtor(&class_name);
			RETURN_ZVAL(pzval, 1, 0);
		}
		zval_ptr_dtor(&class_name);
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_application_methods
 */
const zend_function_entry gene_application_methods[] = {
	PHP_ME(gene_application, __construct, gene_application_construct, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, getInstance, gene_application_get_instance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, load, gene_application_load, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, autoload, gene_application_autoload, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, setMode, gene_application_set_mode, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, setView, gene_application_set_view, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, error, gene_application_error, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, exception, gene_application_exception, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, run, gene_application_run, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, clearState, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, destroyContext, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, setResponse, gene_application_set_response, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getMethod, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getPath, gene_application_get_path, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getRouterUri, gene_application_get_uri, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getLang, gene_application_get_lang, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getModule, gene_application_get_module, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getController, gene_application_get_controller, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getAction, gene_application_get_action, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, setEnvironment, gene_application_set_environment, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getEnvironment, gene_application_get_environment, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getEnvironmentName, gene_application_get_environment_name, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, setRuntimeType, gene_application_set_runtime_type, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getRuntimeType, gene_application_get_runtime_type, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getRuntimeTypeName, gene_application_get_runtime_type_name, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, config, gene_application_config, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, params, gene_application_config, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, __get, gene_application_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, __set, gene_application_set, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(application) {
	zend_class_entry gene_application;
	GENE_INIT_CLASS_ENTRY(gene_application, "Gene_Application", "Gene\\Application", gene_application_methods);
	gene_application_ce = zend_register_internal_class( &gene_application);
	gene_application_ce->ce_flags |= ZEND_ACC_FINAL;

	//static
	zend_declare_property_null(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);

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
