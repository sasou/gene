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

ZEND_BEGIN_ARG_INFO_EX(gene_session_sets_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_cookie_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()
/* }}} */

void gene_cookie(zval *self) /*{{{*/
{
	zval *name = NULL, *cookie_id = NULL, *lifetime = NULL, *path = NULL, *domain = NULL, *secure = NULL, *httponly = NULL;
	name = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_NAME), 1, NULL);
	cookie_id = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_ID), 1, NULL);
	lifetime = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), 1, NULL);
	path = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_PATH), 1, NULL);
	domain = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN), 1, NULL);
	secure = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_SECURE), 1, NULL);
	httponly = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HTTPONLY), 1, NULL);

	zval times,curtime;
	int jg;
	gene_time(&curtime);
	if (Z_TYPE_P(lifetime) == IS_LONG) {
		jg = Z_LVAL(curtime) + Z_LVAL_P(lifetime);
	} else {
		jg = Z_LVAL(curtime) + 7200;
	}
	ZVAL_LONG(&times, jg);
	zval_ptr_dtor(&curtime);
	zval params[] = { *name,*cookie_id,times,*path,*domain,*secure,*httponly };
    zval function_name,ret;
    ZVAL_STRING(&function_name, "setcookie");
    call_user_function(NULL, NULL, &function_name, &ret, 7, params);
    zval_ptr_dtor(&function_name);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&times);
}/*}}}*/

void gene_set_cookie(zval *self, zval *name, zval *value, zval *time) /*{{{*/
{
	zval *path = NULL, *domain = NULL, *secure = NULL, *httponly = NULL;
	path = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_PATH), 1, NULL);
	domain = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN), 1, NULL);
	secure = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_SECURE), 1, NULL);
	httponly = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HTTPONLY), 1, NULL);

	zval params[] = { *name,*value,*time,*path,*domain,*secure,*httponly };
    zval function_name,ret;
    ZVAL_STRING(&function_name, "setcookie");
    call_user_function(NULL, NULL, &function_name, &ret, 7, params);
    zval_ptr_dtor(&function_name);
	zval_ptr_dtor(&ret);
}/*}}}*/


/** {{{ void gene_init_ssid(zval *obj)
 */
void gene_init_ssid(zval *obj) {
	zval *cookie_id = NULL;
	zval *name = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_NAME), 1, NULL);
	zval *prefix = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_PREFIX), 1, NULL);
	smart_str session_id = {0};
	smart_str_appendl(&session_id, Z_STRVAL_P(prefix), Z_STRLEN_P(prefix));
	cookie_id = getVal(TRACK_VARS_COOKIE, Z_STRVAL_P(name), Z_STRLEN_P(name));
	if (cookie_id && Z_TYPE_P(cookie_id) == IS_STRING) {
		smart_str_appendl(&session_id, Z_STRVAL_P(cookie_id), Z_STRLEN_P(cookie_id));
		zval ret;
		gene_strip_tags(cookie_id, &ret);
		zend_update_property_string(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_ID), Z_STRVAL(ret));
		zval_ptr_dtor(&ret);
	} else {
		zval time,uniqid,md5;
		gene_microtime(&time);
		gene_uniqid(&time, &uniqid);
		gene_md5(&uniqid, &md5);
		zend_update_property_string(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_ID), Z_STRVAL(md5));
		smart_str_appendl(&session_id, Z_STRVAL(md5), Z_STRLEN(md5));
		zval_ptr_dtor(&time);
		zval_ptr_dtor(&uniqid);
		zval_ptr_dtor(&md5);
	}
    smart_str_0(&session_id);
	zend_update_property_string(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), ZSTR_VAL(session_id.s));
	smart_str_free(&session_id);
}
/* }}} */

/** {{{ void gene_update_ssid(zval *obj)
 */
void gene_update_ssid(zval *obj) {
	zval *name = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_NAME), 1, NULL);
	zval *prefix = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_PREFIX), 1, NULL);
	smart_str session_id = {0};
	smart_str_appendl(&session_id, Z_STRVAL_P(prefix), Z_STRLEN_P(prefix));
	zval time,uniqid,md5;
	gene_microtime(&time);
	gene_uniqid(&time, &uniqid);
	gene_md5(&uniqid, &md5);
	zend_update_property_string(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_ID), Z_STRVAL(md5));
	smart_str_appendl(&session_id, Z_STRVAL(md5), Z_STRLEN(md5));
	zval_ptr_dtor(&time);
	zval_ptr_dtor(&uniqid);
	zval_ptr_dtor(&md5);
    smart_str_0(&session_id);
	zend_update_property_string(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), ZSTR_VAL(session_id.s));
	smart_str_free(&session_id);
}
/* }}} */

void gene_data_load(zval *obj) { /*{{{*/
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

void gene_data_save(zval *obj, zval *data) { /*{{{*/
	zval *session_id = NULL, *driver = NULL, *hook = NULL;

	session_id = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), 1, NULL);
	driver = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DRIVER), 1, NULL);
	if (session_id) {
		if (data == NULL) {
			data = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
		}
		if (data && Z_TYPE_P(data) == IS_ARRAY) {
			zval time;
			gene_time(&time);
			Z_TRY_ADDREF_P(&time);
			zend_hash_str_update(Z_ARRVAL_P(data), ZEND_STRL("stime"), &time);
			zval_ptr_dtor(&time);
		}

		zval class_name;
		gene_class_name(&class_name);
		hook = gene_di_get_class(Z_STR(class_name), Z_STR_P(driver));
		zval_ptr_dtor(&class_name);
		if (hook) {
			zval params[] = { *session_id,*data };
			zval function_name,ret;
			ZVAL_STRING(&function_name, "set");
			call_user_function(NULL, hook, &function_name, &ret, 2, params);
			zval_ptr_dtor(&function_name);
			zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), data);
			zval_ptr_dtor(&ret);
			gene_cookie(obj);
		} else {
			php_error_docref(NULL, E_WARNING, "Configure or inject the session storage plug-in");
		}
	}

}/*}}}*/

/** {{{ void gene_data_clear(zval *obj)
 */
void gene_data_clear(zval *obj) {
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
			ZVAL_STRING(&function_name, "delete");
			call_user_function(NULL, hook, &function_name, &ret, 1, params);
			zval_ptr_dtor(&function_name);
			zval_ptr_dtor(&ret);
			zend_update_property_null(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA));
			gene_update_ssid(obj);
			gene_cookie(obj);
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
	zval *sess = NULL, *stime = NULL, *uptime = NULL;

	sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	if (sess && Z_TYPE_P(sess) == IS_NULL) {
		gene_data_load(obj);
		sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	}

	if (sess && Z_TYPE_P(sess) == IS_ARRAY) {
		tmp = sess;
		if (path != NULL) {
			seg = php_strtok_r(path, ".", &ptr);
			while (seg) {
				if (ptr && strlen(ptr) > 0) {
					if (Z_TYPE_P(tmp) != IS_ARRAY) {
						return NULL;
					}
				}
				tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
				if (tmp == NULL) {
					return NULL;
				}
				seg = php_strtok_r(NULL, ".", &ptr);
			}
		}
		stime = zend_symtable_str_find(Z_ARRVAL_P(sess), ZEND_STRL("stime"));
		if (stime && Z_TYPE_P(stime) == IS_LONG) {
			uptime = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_UPTIME), 1, NULL);
			zval curtime;
			gene_time(&curtime);
			zend_long jg = Z_LVAL_P(&curtime) - Z_LVAL_P(stime);
			zval_ptr_dtor(&curtime);
			if (jg > Z_LVAL_P(uptime)) {
				gene_data_save(obj, NULL);
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

	if (zvalue == NULL) {
		copyval = zend_symtable_str_find(Z_ARRVAL_P(val), keyString, keyString_len);
		if (copyval == NULL) {
			array_init(&tmp);
			Z_TRY_ADDREF_P(&tmp);
			copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
			zval_ptr_dtor(&tmp);
		} else {
			if (Z_TYPE_P(copyval) != IS_ARRAY) {
				array_init(&tmp);
				Z_TRY_ADDREF_P(&tmp);
				copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
				zval_ptr_dtor(&tmp);
			}
		}
	} else {
		Z_TRY_ADDREF_P(zvalue);
		copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, zvalue);
	}
	return copyval;
}
/* }}} */

/** {{{ void gene_session_set_by_path(char *path, zval *zvalue)
 */
void gene_session_set_by_path(zval *obj, char *path, zval *zvalue) {
	char *ptr = NULL, *seg = NULL, *parent = NULL;
	zval *sess = NULL,*tmp = NULL, ret;
	sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	if (sess && Z_TYPE_P(sess) == IS_NULL) {
		gene_data_load(obj);
		sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	}

	if (Z_TYPE_P(sess) == IS_ARRAY) {
		tmp = sess;
		seg = php_strtok_r(path, ".", &ptr);
		while (seg) {
			if (ptr && strlen(ptr) > 0) {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), NULL);
			} else {
				tmp = gene_session_set_val(tmp, seg, strlen(seg), zvalue);
			}
			seg = php_strtok_r(NULL, ".", &ptr);
		}
	}

	gene_data_save(obj, NULL);
	return;
}
/* }}} */

/** {{{ zend_bool gene_session_del_by_path(zval *obj, char *path)
 */
zend_bool gene_session_del_by_path(zval *obj, char *path) {
	char *ptr = NULL, *seg = NULL;
	zval *sess = NULL,*tmp = NULL, ret;

	sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	if (sess && Z_TYPE_P(sess) == IS_NULL) {
		return 0;
	}

	if (Z_TYPE_P(sess) == IS_ARRAY) {
		tmp = sess;
		if (path != NULL) {
			seg = php_strtok_r(path, ".", &ptr);
			while (seg) {
				if (ptr && strlen(ptr) > 0) {
					tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg, strlen(seg));
				} else {
					if (zend_symtable_str_del(Z_ARRVAL_P(tmp), seg, strlen(seg)) == 0) {
						gene_data_save(obj, NULL);
						return 1;
					}
				}
				seg = php_strtok_r(NULL, "0", &ptr);
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
	zval *config = NULL, *self = getThis(), *val = NULL;
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

	gene_init_ssid(self);
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::load()
 */
PHP_METHOD(gene_session, load) {
	zval *self = getThis();

	gene_data_load(self);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::save()
 */
PHP_METHOD(gene_session, save) {
	zval *self = getThis();

	gene_data_save(self, NULL);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::get($name)
 */
PHP_METHOD(gene_session, get) {
	zend_string *name = NULL;
	zval *self = getThis(),*data = NULL;
	char *path = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		RETURN_NULL();
	}
	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
	}

	data = gene_session_get_by_path(self, path);
	if (path) {
		efree(path);
	}
	if (data) {
		RETURN_ZVAL(data, 1, 0);
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
	zval *self = getThis();
	zend_string *name;
	char *path = NULL;
	zend_bool ret = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}

	spprintf(&path, 0, "%s", ZSTR_VAL(name));
	ret = gene_session_del_by_path(self, path);
	efree(path);

	RETURN_BOOL(ret);
}
/* }}} */

/** {{{ public static gene_session::destroy()
 */
PHP_METHOD(gene_session, destroy) {
	zval *self = getThis();

	gene_data_clear(self);

	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_session::has($name)
 */
PHP_METHOD(gene_session, has) {
	zend_string *name;
	zval *self = getThis(), *data = NULL;
	char *path = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}

	if (name) {
		spprintf(&path, 0, "%s", ZSTR_VAL(name));
	}

	data = gene_session_get_by_path(self, path);
	if (path) {
		efree(path);
	}
	if (data) {
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ public gene_session::cookie($name, $value, $time)
 */
PHP_METHOD(gene_session, cookie) {
	zval *self = getThis(), *name = NULL, *value = NULL, *time = NULL;
	char *path = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zzz", &name, &value, &time) == FAILURE) {
		RETURN_FALSE;
	}

	gene_set_cookie(self, name, value, time);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::getSessionId()
 */
PHP_METHOD(gene_session, getSessionId) {
	zval *self = getThis(),*data = NULL;

	data = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_ID), 1, NULL);
	if (Z_TYPE_P(data) == IS_STRING) {
		RETURN_ZVAL(data, 1, 0);
	}

	RETURN_NULL();
}
/* }}} */

/** {{{ public static gene_session::setSessionId()
 */
PHP_METHOD(gene_session, setSessionId) {
	zval *self = getThis(),*cookie = NULL,*prefix = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &cookie) == FAILURE) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(cookie) == IS_STRING && Z_STRLEN_P(cookie) > 0) {
		zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_ID), Z_STRVAL_P(cookie));
		prefix = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_PREFIX), 1, NULL);
		smart_str session_id = {0};
		smart_str_appendl(&session_id, Z_STRVAL_P(prefix), Z_STRLEN_P(prefix));
		smart_str_appendl(&session_id, Z_STRVAL_P(cookie), Z_STRLEN_P(cookie));
		smart_str_0(&session_id);
		zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_ID), ZSTR_VAL(session_id.s));
		smart_str_free(&session_id);
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::setLifeTime()
 */
PHP_METHOD(gene_session, setLifeTime) {
	zval *self = getThis(),*time = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &time) == FAILURE) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(time) == IS_LONG) {
		zend_update_property_long(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), Z_LVAL_P(time));
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_session_methods
 */
zend_function_entry gene_session_methods[] = {
	PHP_ME(gene_session, __construct, gene_session_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_session, load, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, save, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, setSessionId, gene_session_sets_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, getSessionId, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, setLifeTime, gene_session_sets_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, destroy, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, cookie, gene_session_cookie_arginfo, ZEND_ACC_PUBLIC)
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
