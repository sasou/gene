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


#include "php_gene.h"
#include "gene_application.h"
#include "gene_load.h"
#include "gene_cache.h"
#include "gene_config.h"
#include "gene_router.h"
#include "gene_request.h"
#include "gene_common.h"
#include "gene_view.h"
#include "gene_exception.h"

zend_class_entry * gene_application_ce;

/*
 * {{{ void load_file(char *key, int key_len,char *php_script, int validity TSRMLS_DC)
 */
void load_file(char *key, int key_len,char *php_script, int validity TSRMLS_DC)
{

	int import = 0,cur,times = 0;
	filenode *val;
	if (key_len) {
		val =  file_cache_get_easy(key, key_len TSRMLS_CC);
		if (val) {
			cur = time(NULL);
			times = cur - val->stime;
			if (times > val->validity) {
				val->stime = cur;
				val->validity = validity;
				cur = gene_file_modified(php_script,0 TSRMLS_CC);
				if (cur != val->ftime) {
					import = 1;
					val->ftime = cur;
				}
			}
		} else {
			import = 1;
			file_cache_set_val(key, key_len,gene_file_modified(php_script,0 TSRMLS_CC),validity TSRMLS_CC);
		}
		if (import) {
			gene_load_import(php_script TSRMLS_CC);
		}
	}
}
/* }}} */

/** {{{ gene_file_modified
*/
int gene_file_modified(char *file, long ctime TSRMLS_DC)
{
	zval  n_ctime;
	php_stat(file, strlen(file), 6 /* FS_MTIME */ , &n_ctime TSRMLS_CC);
	if (ctime != Z_LVAL(n_ctime)) {
		return Z_LVAL(n_ctime);
	}
	return 0;
}
/* }}} */


/** {{{ void gene_ini_router()
*/
void gene_ini_router()
{
	zval *server,*temp;
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
	    		GENE_G(path) = ecalloc(Z_STRLEN_P(temp)+1,sizeof(char));
	    		leftByChar(GENE_G(path),Z_STRVAL_P(temp), '?');
	    	}
		}
	}
}
/* }}} */

/** {{{ void gene_router_set_uri(zval **leaf TSRMLS_DC)
*/
void gene_router_set_uri(zval **leaf TSRMLS_DC)
{
	zval *key;
	if ((key = zend_hash_str_find((*leaf)->value.arr, "key", 3)) != NULL){
		if (Z_STRLEN_P(key)) {
			if (GENE_G(router_path)){
			    efree(GENE_G(router_path));
			    GENE_G(router_path) = NULL;
			}
			GENE_G(router_path) =  estrndup(Z_STRVAL_P(key), Z_STRLEN_P(key));
		}
	}
}
/* }}} */

/*
 * {{{ gene_application
 */
PHP_METHOD(gene_application, __construct)
{
	zval *safe = NULL;
	int len = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|z", &safe) == FAILURE)
    {
        RETURN_NULL();
    }
    gene_ini_router(TSRMLS_C);
    if (safe && !GENE_G(app_key)) {
    	GENE_G(app_key) = estrndup(Z_STRVAL_P(safe), Z_STRLEN_P(safe));
    }

    if (GENE_G(directory) != NULL) {
    	spprintf(&GENE_G(app_root), 0, "%s/app/", GENE_G(directory));
    }

}
/* }}} */


/*
 * {{{ public gene_application::load($key,$validity)
 */
PHP_METHOD(gene_application, load)
{
	zval *self = getThis();
	zend_string *file = NULL,*path = NULL;
	char *cache_key;
	int validity = 10,cache_key_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|SSl", &file, &path, &validity) == FAILURE) {
		return;
	}
	if (path && ZSTR_LEN(path) > 0) {
		cache_key_len = spprintf(&cache_key, 0, "%s/%s", ZSTR_VAL(path), ZSTR_VAL(file));
	} else {
		cache_key_len = spprintf(&cache_key, 0, "%sConfig/%s", GENE_G(app_root), ZSTR_VAL(file));
	}
	load_file(cache_key, cache_key_len, cache_key, validity TSRMLS_CC);
	efree(cache_key);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::urlParams()
 */
PHP_METHOD(gene_application, urlParams)
{
	zval *cache = NULL;
	zend_string *keyString = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|S", &keyString) == FAILURE) {
		return;
	}
    cache = gene_cache_get_by_config(PHP_GENE_URL_PARAMS, strlen(PHP_GENE_URL_PARAMS), ZSTR_VAL(keyString) TSRMLS_CC);
    if (cache) {
    	RETURN_ZVAL(cache, 1, 1);
    }
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getMethod()
 */
PHP_METHOD(gene_application, getMethod)
{
	if (GENE_G(method)) {
		RETURN_STRING(GENE_G(method));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getPath()
 */
PHP_METHOD(gene_application, getPath)
{
	if (GENE_G(path)) {
		RETURN_STRING(GENE_G(path));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getRouterUri()
 */
PHP_METHOD(gene_application, getRouterUri)
{
	if (GENE_G(router_path)) {
		RETURN_STRING(GENE_G(router_path));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_application::getEnvironment()
 */
PHP_METHOD(gene_application, getEnvironment)
{
	RETURN_BOOL(GENE_G(run_environment));
}
/* }}} */

/*
 * {{{ public gene_application::config()
 */
PHP_METHOD(gene_application, config)
{
	zval *cache = NULL;
	int router_e_len;
	char *router_e;
	zend_string *keyString;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &keyString) == FAILURE) {
		return;
	}

	if (GENE_G(app_key)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(app_key), GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s%s", GENE_G(directory), GENE_CONFIG_CACHE);
	}
    cache = gene_cache_get_by_config(router_e, router_e_len, ZSTR_VAL(keyString) TSRMLS_CC);
    efree(router_e);
    if (cache) {
    	gene_cache_zval_local(return_value, cache);
		return;
    }
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_load::autoload($key)
 */
PHP_METHOD(gene_application, autoload)
{
	zend_string *fileName = NULL,*app_root = NULL;
	zval *self = getThis();
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|SS", &app_root, &fileName) == FAILURE) {
		return;
	}
	if (app_root && ZSTR_LEN(app_root) >0 ) {
	    GENE_G(app_root) = estrndup(ZSTR_VAL(app_root), ZSTR_LEN(app_root));
	}
	if (fileName && ZSTR_LEN(fileName) >0 ) {
		GENE_G(auto_load_fun) =  estrndup(ZSTR_VAL(fileName), ZSTR_LEN(fileName));
	}
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_load::error($callback, $type)
 */
PHP_METHOD(gene_application, error)
{
	zval *callback = NULL, *error_type = NULL,*self = getThis();
	long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lzz", &type, &callback, &error_type) == FAILURE) {
		return;
	}
	if (type > 0) {
		GENE_G(gene_error) = 1;
	} else {
		GENE_G(gene_error) = 0;
	}
	gene_exception_error_register(callback, error_type TSRMLS_CC);
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public gene_exception::exception(string $callbacak[, int $error_types = E_ALL | E_STRICT ] )
*/
PHP_METHOD(gene_application, exception) {
	zval *callback = NULL, *error_type = NULL,*self = getThis();
	long type = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lz", &type, &callback) == FAILURE) {
		return;
	}
	if (type > 0) {
		GENE_G(gene_exception) = 1;
	} else {
		GENE_G(gene_exception) = 0;
	}
	gene_exception_register(callback, NULL TSRMLS_CC);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_load::error($callback, $type)
 */
PHP_METHOD(gene_application, setMode)
{
	zval *self=getThis();
	long error_type = 0,exception_type=0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ll", &error_type, &exception_type) == FAILURE) {
		return;
	}
	if (error_type > 0) {
		GENE_G(gene_error) = 1;
	} else {
		GENE_G(gene_error) = 0;
	}
	if (exception_type > 0) {
		GENE_G(gene_exception) = 1;
	} else {
		GENE_G(gene_exception) = 0;
	}
	gene_exception_error_register(NULL, NULL TSRMLS_CC);
	gene_exception_register(NULL, NULL TSRMLS_CC);
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_application::setView($view, $ext)
 */
PHP_METHOD(gene_application, setView)
{
	zval *self=getThis();
	zend_string *view = NULL,*tpl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|SS", &view, &tpl) == FAILURE) {
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
PHP_METHOD(gene_application, run)
{
	zend_string *methodin = NULL,*pathin = NULL;
	zval *self = getThis(),safe;
	char *min = NULL,*pin= NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|SS", &methodin, &pathin) == FAILURE)
    {
        RETURN_NULL();
    }

	gene_loader_register_function(NULL TSRMLS_CC);

    if (GENE_G(app_key)) {
    	ZVAL_STRING(&safe,GENE_G(app_key));
    } else {
    	ZVAL_STRING(&safe,GENE_G(directory));
    }
	if (methodin && ZSTR_LEN(methodin)) {
		min = ZSTR_VAL(methodin);
	}
	if (pathin && ZSTR_LEN(pathin)) {
		pin = ZSTR_VAL(pathin);
	}
	get_router_content_run(min, pin, &safe TSRMLS_CC);
	zval_ptr_dtor(&safe);
}
/* }}} */

/*
 * {{{ gene_application_methods
 */
zend_function_entry gene_application_methods[] = {
		PHP_ME(gene_application, load, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_application, autoload, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_application, setMode, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_application, setView, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_application, error, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_application, exception, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_application, run, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_application, urlParams, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_application, getMethod, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_application, getPath, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_application, getRouterUri, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_application, getEnvironment, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_application, config, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_application, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(application)
{
    zend_class_entry gene_application;
    GENE_INIT_CLASS_ENTRY(gene_application, "Gene_Application",  "Gene\\Application", gene_application_methods);
    gene_application_ce = zend_register_internal_class(&gene_application TSRMLS_CC);

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
