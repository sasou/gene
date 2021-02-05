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
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "../app/application.h"
#include "../common/common.h"
#include "../config/configs.h"
#include "../cache/memory.h"

zend_class_entry * gene_config_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_config_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, safe)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_config_set, 0, 0, 3)
	ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, ttl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_config_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_config_del, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_config_clear, 0, 0, 0)
ZEND_END_ARG_INFO()

/*
 * {{{ gene_config
 */
PHP_METHOD(gene_config, __construct) {
	zval *self = getThis(), *safe = NULL;
	int len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		RETURN_NULL();
	}
	if (safe) {
		zend_update_property_string(gene_config_ce, gene_strip_obj(self), GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), Z_STRVAL_P(safe));
	} else {
		if (GENE_G(app_key)) {
			zend_update_property_string(gene_config_ce, gene_strip_obj(self), GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), GENE_G(app_key));
		} else {
			gene_ini_router();
			zend_update_property_string(gene_config_ce, gene_strip_obj(self), GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), GENE_G(directory));
		}
	}
}
/* }}} */

/*
 * {{{ public gene_config::set()
 */
PHP_METHOD(gene_config, set) {
	char *keyString, *router_e, *path;
	int keyString_len, validity = 0;
	size_t router_e_len;
	zval *self = getThis(), *zvalue, *safe;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "sz|l", &keyString,
			&keyString_len, &zvalue, &validity) == FAILURE) {
		return;
	}
	safe = zend_read_property(gene_config_ce, gene_strip_obj(self), GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_CONFIG_CACHE);
	}
	if (zvalue) {
		spprintf(&path, 0, "%s", keyString);
		replaceAll(path, '.', '/');
		gene_memory_set_by_router(router_e, router_e_len, path, zvalue, validity);
		efree(path);
	}
	efree(router_e);
	RETURN_BOOL(1);
}
/* }}} */

/*
 * {{{ public gene_config::get()
 */
PHP_METHOD(gene_config, get) {
	zval *self = getThis(), *safe, *cache = NULL;
	size_t router_e_len;
	char *router_e, *path;
	zend_string *keyString;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &keyString)
			== FAILURE) {
		return;
	}
	safe = zend_read_property(gene_config_ce, gene_strip_obj(self), GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_CONFIG_CACHE);
	}
	spprintf(&path, 0, "%s", ZSTR_VAL(keyString));
	replaceAll(path, '.', '/');
	cache = gene_memory_get_by_config(router_e, router_e_len, path);
	efree(router_e);
	efree(path);
	if (cache) {
		gene_memory_zval_local(return_value, cache);
		return;
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_config::del()
 */
PHP_METHOD(gene_config, del) {
	zval *self = getThis(), *safe;
	size_t router_e_len;
	char *router_e;
	safe = zend_read_property(gene_config_ce, gene_strip_obj(self), GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_CONFIG_CACHE);
	}
	if (gene_memory_del(router_e, router_e_len)) {
		efree(router_e);
		RETURN_TRUE;
	}
	efree(router_e);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public gene_config::clear()
 */
PHP_METHOD(gene_config, clear) {
	zval *self = getThis(), *safe;
	size_t router_e_len;
	char *router_e;
	safe = zend_read_property(gene_config_ce, gene_strip_obj(self), GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_CONFIG_CACHE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_CONFIG_CACHE);
	}
	if (gene_memory_del(router_e, router_e_len)) {
		efree(router_e);
		RETURN_TRUE;
	}
	efree(router_e);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_config_methods
 */
zend_function_entry gene_config_methods[] = {
	PHP_ME(gene_config, __construct, gene_config_construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_config, set, gene_config_set, ZEND_ACC_PUBLIC)
	PHP_ME(gene_config, get, gene_config_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_config, del, gene_config_del, ZEND_ACC_PUBLIC)
	PHP_ME(gene_config, clear, gene_config_clear, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(config) {
	zend_class_entry gene_config;
	GENE_INIT_CLASS_ENTRY(gene_config, "Gene_Config", "Gene\\Config", gene_config_methods);
	gene_config_ce = zend_register_internal_class(&gene_config);

	//debug
	zend_declare_property_string(gene_config_ce, GENE_CONFIG_SAFE, strlen(GENE_CONFIG_SAFE), "", ZEND_ACC_PUBLIC);

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
