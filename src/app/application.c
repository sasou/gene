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

#include "../php_gene.h"
#include "../app/application.h"
#include "../factory/load.h"
#include "../cache/memory.h"
#include "../config/config.h"
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

ZEND_BEGIN_ARG_INFO_EX(gene_application_config, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_application_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

/*
 * {{{ void load_file(char *key, int key_len,char *php_script, int validity TSRMLS_DC)
 */
void load_file(char *key, int key_len, char *php_script, int validity TSRMLS_DC) {

	int import = 0, cur, times = 0;
	filenode *val;
	if (key_len) {
		val = file_cache_get_easy(key, key_len TSRMLS_CC);
		if (val) {
			cur = time(NULL);
			times = cur - val->stime;
			if (times > val->validity) {
				val->stime = cur;
				val->validity = validity;
				cur = gene_file_modified(php_script, 0 TSRMLS_CC);
				if (cur != val->ftime) {
					import = 1;
					val->ftime = cur;
				}
			}
		} else {
			import = 1;
			file_cache_set_val(key, key_len,
					gene_file_modified(php_script, 0 TSRMLS_CC),
					validity TSRMLS_CC);
		}
		if (import) {
			if(!gene_load_import(php_script TSRMLS_CC)) {
				php_error_docref(NULL, E_WARNING, "Unable to load config file %s", php_script);
			}
		}
	}
}
/* }}} */

/** {{{ gene_file_modified
 */
int gene_file_modified(char *file, long ctime TSRMLS_DC) {
	zval n_ctime;
	php_stat(file, strlen(file), 6 /* FS_MTIME */, &n_ctime TSRMLS_CC);
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
	if (!GENE_G(method) && !GENE_G(path) && !GENE_G(directory)) {
		server = request_query(TRACK_VARS_SERVER, NULL, 0 TSRMLS_CC);
		if (server) {
			if ((temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("DOCUMENT_ROOT"))) != NULL) {
				GENE_G(directory) = estrndup(Z_STRVAL_P(temp), Z_STRLEN_P(temp));
			}
			if ((temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("REQUEST_METHOD"))) != NULL) {
				GENE_G(method) = estrndup(Z_STRVAL_P(temp), Z_STRLEN_P(temp));
				strtolower(GENE_G(method));
			}
			if ((temp = zend_hash_str_find(HASH_OF(server), ZEND_STRL("REQUEST_URI"))) != NULL) {
				GENE_G(path) = ecalloc(Z_STRLEN_P(temp)+1, sizeof(char));
				leftByChar(GENE_G(path), Z_STRVAL_P(temp), '?');
			}
		}
		server = NULL;
		temp = NULL;
	}
}
/* }}} */

/*
 *  {{{ zval *gene_application_instance(zval *this_ptr,zval *safe TSRMLS_DC)
 */
zval *gene_application_instance(zval *this_ptr, zval *safe TSRMLS_DC) {
	zval *instance = zend_read_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), 1);

	if (Z_TYPE_P(instance) == IS_OBJECT) {
		return instance;
	}
	if (this_ptr) {
		instance = this_ptr;
	} else {
		object_init_ex(instance, gene_application_ce);
		gene_ini_router(TSRMLS_C);
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
	zend_update_static_property(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), instance);
	return instance;
}
/* }}} */


/*
 *  {{{ public gene_application::getInstance(void)
 */
PHP_METHOD(gene_application, getInstance) {
	zval *safe = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z", &safe) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_ZVAL(gene_application_instance(NULL, safe TSRMLS_CC), 1, 0);
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __construct) {
	zval *safe = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z", &safe) == FAILURE) {
		RETURN_NULL();
	}
	RETURN_ZVAL(gene_application_instance(NULL, safe TSRMLS_CC), 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::load($key,$validity)
 */
PHP_METHOD(gene_application, load) {
	zval *self = getThis();
	zend_string *file = NULL, *path = NULL;
	char *cache_key;
	int validity = 10, cache_key_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|Sl", &file, &path, &validity) == FAILURE) {
		return;
	}
	if (path && ZSTR_LEN(path) > 0) {
		cache_key_len = spprintf(&cache_key, 0, "%s/%s", ZSTR_VAL(path), ZSTR_VAL(file));
	} else {
		cache_key_len = spprintf(&cache_key, 0, "%s/Config/%s", GENE_G(app_root), ZSTR_VAL(file));
	}
	load_file(cache_key, cache_key_len, cache_key, validity TSRMLS_CC);
	efree(cache_key);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::getMethod()
 */
PHP_METHOD(gene_application, getMethod) {
	if (GENE_G(method)) {
		RETURN_STRING(GENE_G(method));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getPath()
 */
PHP_METHOD(gene_application, getPath) {
	if (GENE_G(path)) {
		RETURN_STRING(GENE_G(path));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getModule()
 */
PHP_METHOD(gene_application, getModule) {
	if (GENE_G(module)) {
		RETURN_STRING(GENE_G(module));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getController()
 */
PHP_METHOD(gene_application, getController) {
	if (GENE_G(controller)) {
		RETURN_STRING(GENE_G(controller));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getAction()
 */
PHP_METHOD(gene_application, getAction) {
	if (GENE_G(action)) {
		RETURN_STRING(GENE_G(action));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getRouterUri()
 */
PHP_METHOD(gene_application, getRouterUri) {
	zval *ret = NULL;
	char *path = NULL,*tmp = NULL, *seg = NULL, *ptr = NULL, *seg_c = NULL, *ptr_c = NULL;
	if (!GENE_G(router_path)) {
		RETURN_NULL();
	}

	path = str_init(GENE_G(router_path));
	if (GENE_G(module) != NULL) {
		path = strreplace2(path, ":m", GENE_G(module));
	}
	if (GENE_G(controller) != NULL) {
		path = strreplace2(path, ":c", GENE_G(controller));
	}
	if (GENE_G(action) != NULL) {
		path = strreplace2(path, ":a", GENE_G(action));
	}
	strtolower(path);
	RETVAL_STRING(path);
}
/* }}} */

/*
 * {{{ public gene_application::getEnvironment()
 */
PHP_METHOD(gene_application, getEnvironment) {
	RETURN_BOOL(GENE_G(run_environment));
}
/* }}} */


/*
 * {{{ public gene_application::setEnvironment($type)
 */
PHP_METHOD(gene_application, setEnvironment) {
	zend_long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|l", &type) == FAILURE) {
		return;
	}
	GENE_G(run_environment) = type;
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public gene_application::config()
 */
PHP_METHOD(gene_application, config) {
	zval *cache = NULL;
	int router_e_len;
	char *router_e;
	zend_string *keyString;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &keyString) == FAILURE) {
		return;
	}

	if (GENE_G(app_key)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(app_key),
		GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(directory),
		GENE_CONFIG_CACHE);
	}
	cache = gene_memory_get_by_config(router_e, router_e_len, ZSTR_VAL(keyString) TSRMLS_CC);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|SS", &app_root, &fileName) == FAILURE) {
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
	long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "lzz", &type, &callback, &error_type) == FAILURE) {
		return;
	}
	if (type > 0) {
		gene_exception_error_register(callback, error_type TSRMLS_CC);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public gene_application::exception(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
 */
PHP_METHOD(gene_application, exception) {
	zval *callback = NULL, *error_type = NULL, *self = getThis();
	long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "lz", &type, &callback) == FAILURE) {
		return;
	}
	if (type > 0) {
		gene_exception_register(callback TSRMLS_CC);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::setMode($error_type, $exception_type)
 */
PHP_METHOD(gene_application, setMode) {
	zval *self = getThis();
	long error_type = 0, exception_type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|ll", &error_type, &exception_type) == FAILURE) {
		return;
	}
	if (error_type > 0) {
		gene_exception_error_register(NULL, NULL TSRMLS_CC);
	}
	if (exception_type > 0) {
		gene_exception_register(NULL TSRMLS_CC);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|SS", &view, &tpl) == FAILURE) {
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
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|SS", &methodin, &pathin) == FAILURE) {
		RETURN_NULL();
	}

	gene_loader_register(TSRMLS_CC);

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
	init();
	get_router_content_run(min, pin, &safe TSRMLS_CC);
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
	zval *value, *props = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	props = zend_read_property(gene_application_ce, getThis(), ZEND_STRL(GENE_APPLICATION_ATTR), 1, NULL);
	if (Z_TYPE_P(props) == IS_NULL) {
		zval prop;
		array_init(&prop);
		if (zend_hash_update(Z_ARRVAL(prop), name, value) != NULL) {
			Z_TRY_ADDREF_P(value);
		}
		zend_update_property(gene_application_ce, getThis(), ZEND_STRL(GENE_APPLICATION_ATTR), &prop);
		zval_ptr_dtor(&prop);
	} else {
		if (zend_hash_update(Z_ARRVAL_P(props), name, value) != NULL) {
			Z_TRY_ADDREF_P(value);
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __get)
{
	zval *pzval = NULL, *props = NULL;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		props = zend_read_property(gene_application_ce, getThis(), ZEND_STRL(GENE_APPLICATION_ATTR), 1, NULL);
		if (Z_TYPE_P(props) == IS_NULL) {
			zval prop;
			array_init(&prop);
			zend_update_property(gene_application_ce, getThis(), ZEND_STRL(GENE_APPLICATION_ATTR), &prop);
			zval_ptr_dtor(&prop);
			pzval = gene_di_get_easy(name);
			if (pzval) {
				RETURN_ZVAL(pzval, 1, 0);
			}
			RETURN_NULL();
		} else {
			pzval = zend_hash_find(Z_ARRVAL_P(props), name);
			if (pzval == NULL) {
				pzval = gene_di_get_easy(name);
				if (pzval) {
					RETURN_ZVAL(pzval, 1, 0);
				}
				RETURN_NULL();
			}
			RETURN_ZVAL(pzval, 1, 0);
		}
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_application_methods
 */
zend_function_entry gene_application_methods[] = {
	PHP_ME(gene_application, __construct, gene_application_construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_application, getInstance, gene_application_get_instance, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, load, gene_application_load, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, autoload, gene_application_autoload, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, setMode, gene_application_set_mode, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, setView, gene_application_set_view, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, error, gene_application_error, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, exception, gene_application_exception, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, run, gene_application_run, ZEND_ACC_PUBLIC)
	PHP_ME(gene_application, getMethod, gene_application_get_method, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getPath, gene_application_get_path, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getRouterUri, gene_application_get_uri, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getModule, gene_application_get_module, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getController, gene_application_get_controller, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getAction, gene_application_get_action, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, setEnvironment, gene_application_set_environment, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, getEnvironment, gene_application_get_environment, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_application, config, gene_application_config, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
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
	gene_application_ce = zend_register_internal_class( &gene_application TSRMLS_CC);
	gene_application_ce->ce_flags |= ZEND_ACC_FINAL;

	//static
	zend_declare_property_null(gene_application_ce, ZEND_STRL(GENE_APPLICATION_INSTANCE), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC TSRMLS_CC);
	zend_declare_property_null(gene_application_ce, ZEND_STRL(GENE_APPLICATION_ATTR), ZEND_ACC_PUBLIC);

	return SUCCESS;
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
