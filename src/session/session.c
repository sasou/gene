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
#include "ext/session/php_session.h"

#include "../gene.h"
#include "../session/session.h"
#include "../common/common.h"
#include "../di/di.h"
#include "../http/request.h"

zend_class_entry * gene_session_ce;

#if defined(HAVE_SPL) && PHP_VERSION_ID < 70200
extern PHPAPI zend_class_entry *spl_ce_Countable;
#endif

/* {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_session_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_has_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_del_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */


void gene_strip_tags(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "strip_tags");
	zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_session_start() /*{{{*/
{
    zval function_name,ret;
    ZVAL_STRING(&function_name, "session_start");
    call_user_function(NULL, NULL, &function_name, &ret, 0, NULL);
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&ret);
}/*}}}*/

zend_long gene_session_status() /*{{{*/
{
    zval function_name,ret;
    zend_long type = 0;
    ZVAL_STRING(&function_name, "session_status");
    call_user_function(NULL, NULL, &function_name, &ret, 0, NULL);
    zval_ptr_dtor(&function_name);
    if (Z_TYPE(ret) == IS_LONG) {
    	type = Z_LVAL(ret);
    }
    zval_ptr_dtor(&ret);
    return type;
}/*}}}*/

void gene_session_commit() /*{{{*/
{
    zval function_name,ret;
    ZVAL_STRING(&function_name, "session_commit");
    call_user_function(NULL, NULL, &function_name, &ret, 0, NULL);
    zval_ptr_dtor(&function_name);
    zval_ptr_dtor(&ret);
}/*}}}*/


/** {{{ zval * gene_session_get_by_path(char *path)
 */
zval * gene_session_get_by_path(char *path) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp = NULL;
	zval *sess = NULL;

	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		tmp = Z_REFVAL_P(sess);
		if (path != NULL) {
			seg = php_strtok_r(path, "/", &ptr);
			while (seg) {
				if (Z_TYPE_P(tmp) != IS_ARRAY) {
					return NULL;
				}
				tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
				if (tmp == NULL) {
					return NULL;
				}
				seg = php_strtok_r(NULL, "/", &ptr);
			}
		}
		return tmp;
	}
	return NULL;
}
/* }}} */

/** {{{ static zval * gene_session_set_val(char *keyString, size_t keyString_len)
 */
static zval * gene_session_set_val(zval *val, char *keyString, size_t keyString_len, zval *zvalue) {
	zval tmp, *copyval;
	if (val == NULL) {
		return NULL;
	}

	#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(Z_ARRVAL_P(val)) = 1;
	#else
		GC_SET_REFCOUNT(Z_ARRVAL_P(val), 1);
	#endif

	if (zvalue == NULL) {
		copyval = zend_symtable_str_find(Z_ARRVAL_P(val), keyString, keyString_len);
		if (copyval == NULL) {
			array_init(&tmp);
			copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
			zval_ptr_dtor(&tmp);
		} else {
			if (Z_TYPE_P(copyval) != IS_ARRAY) {
				array_init(&tmp);
				copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
				zval_ptr_dtor(&tmp);
			}
		}
	} else {
		copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, zvalue);
	}
	return copyval;
}
/* }}} */

/** {{{ void gene_session_set_by_path(char *path, zval *zvalue)
 */
void gene_session_set_by_path(char *path, zval *zvalue) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp;
	zval *sess = NULL, ret;
	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess == NULL) {
		array_init(&ret);
		zend_symtable_str_update(GENE_G(cache), ZEND_STRL("_SESSION"), &ret);
		tmp = &ret;
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), zvalue);
				Z_TRY_ADDREF_P(zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
		zval_ptr_dtor(&ret);
	} else {
		tmp = Z_REFVAL_P(sess);
		seg = php_strtok_r(path, "/", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), zvalue);
				Z_TRY_ADDREF_P(zvalue);
			}
			seg = php_strtok_r(NULL, "/", &ptr);
		}
	}
	return;
}
/* }}} */

/** {{{ zend_bool gene_session_del_by_path(char *path)
 */
zend_bool gene_session_del_by_path(char *path) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp = NULL;
	zval *sess = NULL;

	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		tmp = Z_REFVAL_P(sess);
		if (path != NULL) {
			seg = php_strtok_r(path, "/", &ptr);
			while (seg) {
				if (Z_TYPE_P(tmp) != IS_ARRAY) {
					return 0;
				}
				if (ptr && strlen(ptr) > 0) {
					tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
				} else {
					return zend_symtable_str_del(Z_ARRVAL_P(tmp), seg, strlen(seg)) == 0 ? 1 : 0;
				}
				seg = php_strtok_r(NULL, "/", &ptr);
			}
		}
		return 0;
	}
	return 0;
}
/* }}} */

/*
 * {{{ gene_session
 */
PHP_METHOD(gene_session, __construct) {
	zval *config = NULL, *self = getThis(), *val = NULL, *request = NULL, *cookie_id = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(),"z", &config) == FAILURE)
    {
        return;
    }

    if (config && Z_TYPE_P(config) == IS_ARRAY) {
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("driver"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_DRIVER), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("prefix"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_PREFIX), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("name"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_NAME), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("domain"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("path"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_PATH), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("secure"));
		if (val) {
			if (Z_TYPE_P(val) == IS_TRUE) {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_SECURE), TRUE);
			} else {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_SECURE), FALSE);
			}
		}
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("httponly"));
		if (val) {
			if (Z_TYPE_P(val) == IS_TRUE) {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HTTPONLY), TRUE);
			} else {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HTTPONLY), FALSE);
			}
		}
	}
	zval class_name;
	gene_class_name(&class_name);
	zend_string *name;
	name = zend_string_init(ZEND_STRL("request"), 0);
	request = gene_di_get_class(Z_STR(class_name), name);
	zval_ptr_dtor(&class_name);
	zend_string_free(name);
	if (request) {
		zval *name = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_NAME), 1, NULL);
		cookie_id = getVal(TRACK_VARS_COOKIE, Z_STRVAL_P(name), Z_STRLEN_P(name));
		if (cookie_id && Z_TYPE_P(cookie_id) == IS_STRING) {
			zval ret;
			gene_strip_tags(cookie_id, &ret);
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_PATH), Z_STRVAL(ret));
			zval_ptr_dtor(&ret);
		}
	}

    RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/** {{{ public static gene_session::get($name)
 */
PHP_METHOD(gene_session, get) {
	zend_string *name = NULL;
	zval *sess = NULL;
	char *path = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
		replaceAll(path, '.', '/');
	}

	sess = gene_session_get_by_path(path);
	if (path) {
		efree(path);
	}
	gene_session_commit();
	if (sess) {
		RETURN_ZVAL(sess, 1, 0);
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ public static gene_session::set($name, $value)
 */
PHP_METHOD(gene_session, set) {
	zval *value;
	zend_string *name;
	char *path = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
		replaceAll(path, '.', '/');
	}
	gene_session_set_by_path(path, value);
	if (path) {
		efree(path);
	}
	gene_session_commit();
	RETURN_TRUE;
}
/* }}} */

/** {{{ public static gene_session::del($name)
 */
PHP_METHOD(gene_session, del) {
	zend_string *name;
	char *path = NULL;
	zend_bool ret = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
		replaceAll(path, '.', '/');
	}
	ret = gene_session_del_by_path(path);
	if (path) {
		efree(path);
	}
	gene_session_commit();
	RETURN_BOOL(ret);
}
/* }}} */

/** {{{ public static gene_session::clear()
 */
PHP_METHOD(gene_session, clear) {
	zval *sess = NULL;
	if (gene_session_status() < 2) {
		gene_session_start();
	}
	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		zend_hash_clean(Z_ARRVAL_P(Z_REFVAL_P(sess)));
	}
	gene_session_commit();
	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_session::has($name)
 */
PHP_METHOD(gene_session, has) {
	zend_string *name;
	zend_bool ret = 0;
	zval *sess = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		return;
	}
	if (gene_session_status() != 2) {
		gene_session_start();
	}
	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		ret = zend_hash_exists(Z_ARRVAL_P(Z_REFVAL_P(sess)), name);
	}
	gene_session_commit();
	RETURN_BOOL(ret);

}
/* }}} */

/*
 * {{{ gene_session_methods
 */
zend_function_entry gene_session_methods[] = {
	PHP_ME(gene_session, __construct, gene_session_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_session, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_session, clear, gene_session_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_MALIAS(gene_session, __get, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __isset, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __set, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __unset, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(session) {
	zend_class_entry gene_session;
	GENE_INIT_CLASS_ENTRY(gene_session, "Gene_Session", "Gene\\Session", gene_session_methods);
	gene_session_ce = zend_register_internal_class(&gene_session);

    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_DRIVER), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_DATA), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_ID), ZEND_ACC_PUBLIC);
    zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_NAME), "SSID", ZEND_ACC_PUBLIC);
    zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_PREFIX), "", ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_ID), ZEND_ACC_PUBLIC);
    zend_declare_property_long(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), 86400, ZEND_ACC_PUBLIC);
	zend_declare_property_long(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_UPTIME), 1800, ZEND_ACC_PUBLIC);
	zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN), "", ZEND_ACC_PUBLIC);
	zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_PATH), "/", ZEND_ACC_PUBLIC);
	zend_declare_property_bool(gene_session_ce, ZEND_STRL(GENE_SESSION_SECURE), FALSE, ZEND_ACC_PUBLIC);
	zend_declare_property_bool(gene_session_ce, ZEND_STRL(GENE_SESSION_HTTPONLY), TRUE, ZEND_ACC_PUBLIC);

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
