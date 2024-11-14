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
#include "zend_smart_str.h" /* for smart_str */

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


void cookie(zval *self, zval *ret) /*{{{*/
{
	zval *name = NULL, *cookie_id = NULL, *lifetime = NULL, *path = NULL, *domain = NULL, *secure = NULL, *httponly = NULL;
	name = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_NAME), 1, NULL);
	cookie_id = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_ID), 1, NULL);
	lifetime = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), 1, NULL);
	path = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_PATH), 1, NULL);
	domain = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN), 1, NULL);
	secure = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_SECURE), 1, NULL);
	httponly = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HTTPONLY), 1, NULL);

	if (Z_TYPE_P(lifetime) == IS_LONG) {
		zval time;
		gene_time(&time);
		Z_LVAL_P(lifetime) += Z_LVAL(time);
		zval_ptr_dtor(&time);
	}
	zval params[] = { *name,*cookie_id,*lifetime,*path,*domain,*secure,*httponly };
    zval function_name;
    ZVAL_STRING(&function_name, "setcookie");
    call_user_function(NULL, NULL, &function_name, ret, 7, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void data_load(zval *obj) { /*{{{*/
	zval *session_id = NULL, *driver = NULL, *hook = NULL;

	session_id = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), 1, NULL);
	driver = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DRIVER), 1, NULL);
	if (session_id) {
		zval class_name;
		gene_class_name(&class_name);
		hook = gene_di_get_class(Z_STR(class_name), Z_STR_P(driver));
		zval_ptr_dtor(&class_name);
		if (hook) {
			zval params[] = { *session_id };
			zval function_name,ret;
			ZVAL_STRING(&function_name, "get");
			call_user_function(NULL, hook, &function_name, &ret, 1, params);
			zval_ptr_dtor(&function_name);
			if (Z_TYPE(ret) == IS_ARRAY) {
				zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), &ret);
			} else {
				zval karr;
				array_init(&karr);
				zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), &karr);
				zval_ptr_dtor(&karr);
			}
			zval_ptr_dtor(&ret);
		} else {
			php_error_docref(NULL, E_WARNING, "Configure or inject the session storage plug-in");
		}
	}
}/*}}}*/

void data_save(zval *obj) { /*{{{*/
	zval *session_id = NULL, *driver = NULL, *hook = NULL, *data = NULL, *stime = NULL;

	session_id = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), 1, NULL);
	driver = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DRIVER), 1, NULL);
	if (session_id) {
		data = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
		if (data && Z_TYPE_P(data) == IS_ARRAY) {
			stime = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), 1, NULL);
			zend_hash_str_update(Z_ARRVAL_P(data), ZEND_STRL("stime"), stime);
		}

		zval class_name;
		gene_class_name(&class_name);
		hook = gene_di_get_class(Z_STR(class_name), Z_STR_P(driver));
		zval_ptr_dtor(&class_name);
		if (hook) {
			zval params[] = { *session_id,*data, };
			zval function_name,ret;
			ZVAL_STRING(&function_name, "set");
			call_user_function(NULL, hook, &function_name, &ret, 2, params);
			zval_ptr_dtor(&function_name);
			zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), data);
			zval_ptr_dtor(&ret);
			zval sc;
			cookie(obj, &sc);
			zval_ptr_dtor(&sc);
		} else {
			php_error_docref(NULL, E_WARNING, "Configure or inject the session storage plug-in");
		}
	}

}/*}}}*/

/** {{{ zval * gene_session_get_by_path(char *path)
 */
zval * gene_session_get_by_path(zval *obj, char *path) {
	char *ptr = NULL, *seg = NULL;
	zval *tmp = NULL;
	zval *sess = NULL, *data = NULL;

	sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	if (sess && Z_TYPE_P(sess) == IS_NULL) {
		data_load(obj);
		sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
		php_printf("  aaa  ");
	}

	if (sess && Z_TYPE_P(sess) == IS_ARRAY) {
		data = zend_hash_str_find(Z_ARRVAL_P(sess), ZEND_STRL("data"));
		php_printf("  bbb  ");
		if (data) {
					php_printf("  ccc  ");
			tmp = Z_REFVAL_P(data);
			if (path != NULL) {
				php_printf("  ddd  ");
				seg = php_strtok_r(path, ".", &ptr);
				while (seg) {
					if (Z_TYPE_P(tmp) != IS_ARRAY) {
						php_printf("  fff  ");
						return NULL;
					}
					tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
					if (tmp == NULL) {
						return NULL;
					}
					seg = php_strtok_r(NULL, ".", &ptr);
				}
			}
			return tmp;
		}
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
		php_printf(" save  ");
		Z_TRY_ADDREF_P(zvalue);
		copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, zvalue);
	}
	return copyval;
}
/* }}} */

/** {{{ void gene_session_set_by_path(char *path, zval *zvalue)
 */
void gene_session_set_by_path(zval *obj, char *path, zval *zvalue) {
	char *ptr = NULL, *seg = NULL;
	zval *sess = NULL,*data = NULL,*tmp = NULL, localsess, ret;
	sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	php_printf("  000  ");
	if (sess && Z_TYPE_P(sess) == IS_NULL) {
		data_load(obj);
		sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
		php_printf("  111  ");
	}
	ZVAL_COPY(&localsess, sess);

	if (Z_TYPE(localsess) == IS_ARRAY) {
		php_printf("  222  ");
		data = zend_hash_str_find(Z_ARRVAL(localsess), ZEND_STRL("data"));
		if (data == NULL) {
			zval tmp_data;
			array_init(&tmp_data);
			zend_symtable_str_update(Z_ARRVAL(localsess), ZEND_STRL("data"), &tmp_data);
			zval_ptr_dtor(&tmp_data);
			data = zend_hash_str_find(Z_ARRVAL(localsess), ZEND_STRL("data"));
			php_printf("  333  ");
		}
	}

	if (data) {
		php_printf("  444  ");
		tmp = data;
		seg = php_strtok_r(path, ".", &ptr);
		while (seg) {
			php_printf("  444  ");
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, ".", &ptr);
		}
	}

	zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), &localsess);
	data_save(obj);
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
	zval *config = NULL, *self = getThis(), *val = NULL, *cookie_id = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(),"z", &config) == FAILURE) {
        RETURN_NULL();
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
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("ttl"));
		if (val && Z_TYPE_P(val) == IS_LONG) {
			zend_update_property_long(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), Z_LVAL_P(val));
		}
		val = zend_hash_str_find(config->value.arr, ZEND_STRL("uttl"));
		if (val && Z_TYPE_P(val) == IS_LONG) {
			zend_update_property_long(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_UPTIME), Z_LVAL_P(val));
		}
	}

	zval *name = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_NAME), 1, NULL);
	zval *prefix = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_PREFIX), 1, NULL);
	smart_str session_id = {0};
	smart_str_appendl(&session_id, Z_STRVAL_P(prefix), Z_STRLEN_P(prefix));
	cookie_id = getVal(TRACK_VARS_COOKIE, Z_STRVAL_P(name), Z_STRLEN_P(name));
	if (cookie_id && Z_TYPE_P(cookie_id) == IS_STRING) {
		smart_str_appendl(&session_id, Z_STRVAL_P(cookie_id), Z_STRLEN_P(cookie_id));
		zval ret;
		gene_strip_tags(cookie_id, &ret);
		zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_ID), Z_STRVAL(ret));
		zval_ptr_dtor(&ret);
	} else {
		zval time,uniqid,md5,ret;
		gene_microtime(&time);
		gene_uniqid(&time, &uniqid);
		gene_md5(&uniqid, &md5);
		zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_ID), Z_STRVAL(md5));
		cookie(self, &ret);
		smart_str_appendl(&session_id, Z_STRVAL(md5), Z_STRLEN(md5));
		zval_ptr_dtor(&time);
		zval_ptr_dtor(&uniqid);
		zval_ptr_dtor(&md5);
		zval_ptr_dtor(&ret);
	}
    smart_str_0(&session_id);
	zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_ID), ZSTR_VAL(session_id.s));
	smart_str_free(&session_id);
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::load()
 */
PHP_METHOD(gene_session, load) {
	zval *self = getThis();

	data_load(self);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::save()
 */
PHP_METHOD(gene_session, save) {
	zval *self = getThis();

	data_save(self);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::get($name)
 */
PHP_METHOD(gene_session, get) {
	zend_string *name = NULL;
	zval *self = getThis(),*sess = NULL;
	char *path = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		RETURN_NULL();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
	}

	sess = gene_session_get_by_path(self, path);
	if (path) {
		efree(path);
	}
	if (sess) {
		RETURN_ZVAL(sess, 1, 0);
	}

	RETURN_NULL();
}
/* }}} */

/** {{{ public static gene_session::set($name, $value)
 */
PHP_METHOD(gene_session, set) {
	zval *self = getThis(),*value = NULL;
	zend_string *name;
	char *path = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		RETURN_NULL();
	}

	spprintf(&path, 0, "%s", ZSTR_VAL(name));
	gene_session_set_by_path(self, path, value);
	efree(path);

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

	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
		replaceAll(path, '.', '/');
	}
	ret = gene_session_del_by_path(path);
	if (path) {
		efree(path);
	}

	RETURN_BOOL(ret);
}
/* }}} */

/** {{{ public static gene_session::clear()
 */
PHP_METHOD(gene_session, clear) {
	zval *sess = NULL;

	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		zend_hash_clean(Z_ARRVAL_P(Z_REFVAL_P(sess)));
	}

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

	sess = zend_hash_str_find(&EG(symbol_table), ZEND_STRL("_SESSION"));
	if (sess) {
		ret = zend_hash_exists(Z_ARRVAL_P(Z_REFVAL_P(sess)), name);
	}

	RETURN_BOOL(ret);

}
/* }}} */

/*
 * {{{ gene_session_methods
 */
zend_function_entry gene_session_methods[] = {
	PHP_ME(gene_session, __construct, gene_session_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_session, load, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, save, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, clear, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
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
    INIT_CLASS_ENTRY(gene_session,"Gene_Session",gene_session_methods);
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
