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
  | Author: Sasou  <admin@caophp.com>                                    |
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
#include "zend_smart_str.h" /* for smart_str */

#include "../gene.h"
#include "../cache/cache.h"
#include "../common/common.h"
#include "../di/di.h"
#include "../factory/factory.h"

zend_class_entry * gene_cache_ce;


void gene_cache_get(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "get");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_cache_set(zval *object, zval *key, zval *value, zval *ttl, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "set");
	zval params[] = { *key,*value,*ttl };
    call_user_function(NULL, object, &function_name, retval, 3, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_cache_incr(zval *object, zval *key, zval *val, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "incr");
	zval params[] = { *key, *val };
    call_user_function(NULL, object, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_cache_del(zval *object, zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "delete");
	zval params[] = { *key };
    call_user_function(NULL, object, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_apcu_store(zval *key, zval *value, zval *ttl, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "apcu_store");
    zval params[] = { *key, *value, *ttl };
    call_user_function(NULL, NULL, &function_name, retval, 3, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_apcu_fetch(zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "apcu_fetch");
    zval params[] = { *key };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_apcu_del(zval *key, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "apcu_delete");
	zval params[] = { *key };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_cache_key(zval *sign, int type, zval *retval) /*{{{*/
{
    zval args,serialize,md5;
    int size = ZEND_CALL_NUM_ARGS(EG(current_execute_data));
    array_init_size(&args, size);
    zend_copy_parameters_array(size, &args);
    gene_serialize(&args, &serialize);
    zval_ptr_dtor(&args);
    gene_md5(&serialize, &md5);
    zval_ptr_dtor(&serialize);
    smart_str key = {0};
    smart_str_appendl(&key, Z_STRVAL_P(sign), Z_STRLEN_P(sign));
    if (type) {
    	smart_str_appendl(&key, GENE_CACHE_TMP, sizeof(GENE_CACHE_TMP) -1);
    }
    if (Z_TYPE(md5) == IS_STRING) {
    	smart_str_appendl(&key, Z_STRVAL(md5), Z_STRLEN(md5));
    }
    zval_ptr_dtor(&md5);
    smart_str_0(&key);
    ZVAL_STRING(retval, key.s->val);
    smart_str_free(&key);
}/*}}}*/

void gene_cache_call(zval *object, zval *args, zval *retval) /*{{{*/
{
	zval *class = NULL, *method = NULL, *element = NULL;
	if (Z_TYPE_P(object) != IS_ARRAY || Z_TYPE_P(args) != IS_ARRAY) {
		return;
	}

	zval params[10];
	int num = 0;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(args), element)
	{
		params[num] = *element;
		num++;
	}ZEND_HASH_FOREACH_END();

	class = zend_hash_index_find(Z_ARRVAL_P(object), 0);
	method = zend_hash_index_find(Z_ARRVAL_P(object), 1);
	if (Z_TYPE_P(class) == IS_STRING ) {
		zval tmp_class;
		class = gene_class_instance(&tmp_class, class, NULL);
		call_user_function(NULL, class, method, retval, num, params);
	} else {
	    call_user_function(NULL, class, method, retval, num, params);
	}
}/*}}}*/

void makeKey(zval *versionSign, zend_string *id, zval *element, zval *retval) {
	smart_str key_s = {0},tmp_s = {0};
	zval tmp_z,md5;
	if (id) {
		smart_str_appendl(&tmp_s,  ZSTR_VAL(id), ZSTR_LEN(id));
	}
	if (Z_TYPE_P(element) != IS_NULL) {
		smart_str_appendc(&tmp_s,  '.');
		zval tmp;
		ZVAL_COPY(&tmp, element);
		convert_to_string(&tmp);
		smart_str_appendl(&tmp_s,  Z_STRVAL(tmp), Z_STRLEN(tmp));
		zval_ptr_dtor(&tmp);
	}
    smart_str_0(&tmp_s);
    ZVAL_STRING(&tmp_z, tmp_s.s->val);
    smart_str_free(&tmp_s);
    gene_md5(&tmp_z, &md5);
    zval_ptr_dtor(&tmp_z);

    smart_str_appendl(&key_s,  Z_STRVAL_P(versionSign), Z_STRLEN_P(versionSign));
    smart_str_appendl(&key_s, Z_STRVAL(md5), Z_STRLEN(md5));
    zval_ptr_dtor(&md5);
    smart_str_0(&key_s);
    ZVAL_STRING(retval, key_s.s->val);
    smart_str_free(&key_s);
}

void gene_cache_get_version_arr(zval *versionSign, zval *versionField, zval *retval, zval *top) /*{{{*/
{
	zval *element = NULL;
	zval tmp_arr[10];
	zend_string *id = NULL;
	zend_ulong i = 0;
	array_init(retval);
	if (top) {
		add_next_index_zval(retval, top);
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(versionField), id, element)
	{
		makeKey(versionSign, id, element, &tmp_arr[i]);
		add_next_index_zval(retval, &tmp_arr[i]);
		i++;
	}ZEND_HASH_FOREACH_END();
}/*}}}*/

void gene_cache_update_version(zval *versionSign, zval *versionField, zval *object) /*{{{*/
{
	zval *value = NULL;
	zend_string *key = NULL;
	zval tmp_arr[10];
	zend_ulong i = 0;
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(versionField), key, value)
	{
		makeKey(versionSign, key, value, &tmp_arr[i]);
		zval ret, value;
		ZVAL_STRING(&value, "1");
		gene_cache_incr(object, &tmp_arr[i], &value, &ret);
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&value);
		zval_ptr_dtor(&tmp_arr[i]);
		i++;
	}ZEND_HASH_FOREACH_END();
}/*}}}*/


void hook_cache_set(zval *object, zval *key, zval *value, zval *ttl) {
	zval tmp_ttl, set_ret;
	ZVAL_LONG(&tmp_ttl, 0);
	if (ttl == NULL) {
		ttl = &tmp_ttl;
	}
	gene_cache_set(object, key, value, ttl, &set_ret);
	zval_ptr_dtor(&set_ret);
	zval_ptr_dtor(&tmp_ttl);
}

void curVersion(zval *versionField, zval *cache, zval *retval) {
	array_init(retval);
	zval *element;
	zend_ulong i = 0;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(versionField), element)
	{
		if (i > 0) {
			zval *val = zend_hash_find(Z_ARRVAL_P(cache), Z_STR_P(element));
			if (val) {
				Z_TRY_ADDREF_P(val);
				add_assoc_zval_ex(retval,Z_STRVAL_P(element), Z_STRLEN_P(element), val);
			}
		}
		i++;
	}ZEND_HASH_FOREACH_END();
}


int checkVersion(zval *oldVersion, zval *newVersion, zval *mode) {
	if (Z_TYPE_P(oldVersion) != IS_ARRAY || Z_TYPE_P(newVersion) != IS_ARRAY) {
		return 0;
	}
	if (mode && Z_TYPE_P(mode) == IS_TRUE) {
		if (zend_hash_num_elements(Z_ARRVAL_P(newVersion)) != zend_hash_num_elements(Z_ARRVAL_P(oldVersion))) {
			return 0;
		}
		if (zend_hash_num_elements(Z_ARRVAL_P(newVersion)) == 0) {
			return 0;
		}
	}
	zval *element;
	zend_string *id;
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(newVersion), id, element)
	{
		zval *val = zend_hash_find(Z_ARRVAL_P(oldVersion), id);
		if (val) {
			if (Z_TYPE_P(element) != IS_LONG) {
				convert_to_long(element);
			}
			if (Z_TYPE_P(val) != IS_LONG) {
				convert_to_long(val);
			}
			if (Z_LVAL_P(val) != Z_LVAL_P(element)) {
				return 0;
			}
		} else {
			return 0;
		}
	}ZEND_HASH_FOREACH_END();
	return 1;
}


/*
 * {{{ gene_cache
 */
PHP_METHOD(gene_cache, __construct)
{
	zval *config = NULL, *self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"z", &config) == FAILURE)
    {
        return;
    }

    if (Z_TYPE_P(config) == IS_ARRAY) {
    	zend_update_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), config);
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_cache::cached($key)
 */
PHP_METHOD(gene_cache, cached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	hook = gene_di_get(Z_STR_P(hookName));

	zval key, ret;
	gene_cache_key(sign, 1, &key);
	gene_cache_get(hook, &key, &ret);
	if (Z_TYPE(ret) == IS_FALSE) {
		zval data;
		gene_cache_call(obj, args, &data);
		hook = gene_di_get(Z_STR_P(hookName));
		hook_cache_set(hook, &key, &data, ttl);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&ret);
		RETURN_ZVAL(&data, 1, 1);
	}
	zval_ptr_dtor(&key);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_cache::localCached($key)
 */
PHP_METHOD(gene_cache, localCached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));

	zval key, ret;
	gene_cache_key(sign, 1, &key);
	gene_apcu_fetch(&key, &ret);
	if (Z_TYPE(ret) == IS_FALSE) {
		zval data;
		gene_cache_call(obj, args, &data);
		gene_apcu_store(&key, &data, ttl, &ret);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&ret);
		RETURN_ZVAL(&data, 1, 1);
	}
	zval_ptr_dtor(&key);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_cache::unsetCached($key)
 */
PHP_METHOD(gene_cache, unsetCached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	hook = gene_di_get(Z_STR_P(hookName));

	zval key;
	gene_cache_key(sign, 1, &key);
	gene_cache_del(hook, &key, return_value);
	zval_ptr_dtor(&key);
}
/* }}} */

/*
 * {{{ public gene_cache::unsetLocalCached($key)
 */
PHP_METHOD(gene_cache, unsetLocalCached)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*obj = NULL, *args = NULL, *ttl = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &obj, &args, &ttl) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));

	zval key;
	gene_cache_key(sign, 1, &key);
	gene_apcu_del(&key, return_value);
	zval_ptr_dtor(&key);
}
/* }}} */

/*
 * {{{ public gene_cache::cachedVersion($key)
 */
PHP_METHOD(gene_cache, cachedVersion)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*versionSign = NULL, *obj = NULL, *args = NULL,*versionField = NULL, *ttl = NULL, *mode = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz|zz", &obj, &args, &versionField, &ttl, &mode) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	versionSign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));
	hook = gene_di_get(Z_STR_P(hookName));

	zval key, cache, cache_key;
	zval *data = NULL,*cacheData = NULL,*cacheVersion = NULL;
	gene_cache_key(sign, 0, &key);
	gene_cache_get_version_arr(versionSign, versionField, &cache_key, &key);
	gene_cache_get(hook, &cache_key, &cache);

	if (Z_TYPE(cache) == IS_ARRAY) {
		data = zend_hash_find(Z_ARRVAL(cache), Z_STR(key));
		if (data != NULL && Z_TYPE_P(data) == IS_ARRAY) {
			cacheData = zend_hash_str_find(Z_ARRVAL_P(data), ZEND_STRL("data"));
			cacheVersion = zend_hash_str_find(Z_ARRVAL_P(data), ZEND_STRL("version"));
			zval cur_version;
			curVersion(&cache_key, &cache, &cur_version);
			if (cacheVersion == NULL || checkVersion(cacheVersion, &cur_version, mode) == 0) {
				zval data_new,cur_data;
				gene_cache_call(obj, args, &cur_data);
				array_init(&data_new);
				Z_TRY_ADDREF(cur_data);
				add_assoc_zval_ex(&data_new, ZEND_STRL("data"), &cur_data);
				add_assoc_zval_ex(&data_new, ZEND_STRL("version"), &cur_version);
				hook = gene_di_get(Z_STR_P(hookName));
				hook_cache_set(hook, &key, &data_new, ttl);
				zval_ptr_dtor(&data_new);
				zval_ptr_dtor(&cache_key);
				zval_ptr_dtor(&cache);
				RETURN_ZVAL(&cur_data, 1, 1);
			}
			Z_TRY_ADDREF_P(cacheData);
			zval_ptr_dtor(&cache);
			zval_ptr_dtor(&cache_key);
			zval_ptr_dtor(&cur_version);
			RETURN_ZVAL(cacheData, 1, 1);
		} else {
			zval data_new,cur_data,cur_version;
			gene_cache_call(obj, args, &cur_data);
			curVersion(&cache_key, &cache, &cur_version);
			array_init(&data_new);
			Z_TRY_ADDREF(cur_data);
			add_assoc_zval_ex(&data_new, ZEND_STRL("data"), &cur_data);
			add_assoc_zval_ex(&data_new, ZEND_STRL("version"), &cur_version);
			hook = gene_di_get(Z_STR_P(hookName));
			hook_cache_set(hook, &key, &data_new, ttl);
			zval_ptr_dtor(&data_new);
			zval_ptr_dtor(&cache_key);
			zval_ptr_dtor(&cache);
			RETURN_ZVAL(&cur_data, 1, 1);
		}
	}
	zval_ptr_dtor(&cache_key);
	zval_ptr_dtor(&cache);
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_cache::localCachedVersion($key)
 */
PHP_METHOD(gene_cache, localCachedVersion)
{
	zval *self = getThis(), *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL,*versionSign = NULL, *obj = NULL, *args = NULL,*versionField = NULL, *ttl = NULL, *mode = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz|zz", &obj, &args, &versionField, &ttl, &mode) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("sign"));
	versionSign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));

	zval key, cache, cache_key, cur_version;
	gene_cache_key(sign, 0, &key);
	gene_apcu_fetch(&key, &cache);
	gene_cache_get_version_arr(versionSign, versionField, &cache_key, NULL);
	hook = gene_di_get(Z_STR_P(hookName));
	gene_cache_get(hook, &cache_key, &cur_version);

	if (Z_TYPE(cache) == IS_ARRAY) {
		zval *cacheData = NULL,*cacheVersion = NULL;
		cacheData = zend_hash_str_find(Z_ARRVAL(cache), ZEND_STRL("data"));
		cacheVersion = zend_hash_str_find(Z_ARRVAL(cache), ZEND_STRL("version"));
		if (cacheVersion == NULL || checkVersion(cacheVersion, &cur_version, mode) == 0) {
			zval data_new,cur_data;
			gene_cache_call(obj, args, &cur_data);
			array_init(&data_new);
			Z_TRY_ADDREF(cur_data);
			add_assoc_zval_ex(&data_new, ZEND_STRL("data"), &cur_data);
			add_assoc_zval_ex(&data_new, ZEND_STRL("version"), &cur_version);
			zval ret;
			gene_apcu_store(&key, &data_new, ttl, &ret);
			zval_ptr_dtor(&ret);
			zval_ptr_dtor(&key);
			zval_ptr_dtor(&data_new);
			zval_ptr_dtor(&cache_key);
			zval_ptr_dtor(&cache);
			RETURN_ZVAL(&cur_data, 1, 1);
		}
		Z_TRY_ADDREF_P(cacheData);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&cache);
		zval_ptr_dtor(&cache_key);
		zval_ptr_dtor(&cur_version);
		RETURN_ZVAL(cacheData, 1, 1);
	} else {
		zval data_new,cur_data;
		gene_cache_call(obj, args, &cur_data);
		array_init(&data_new);
		Z_TRY_ADDREF(cur_data);
		add_assoc_zval_ex(&data_new, ZEND_STRL("data"), &cur_data);
		add_assoc_zval_ex(&data_new, ZEND_STRL("version"), &cur_version);
		zval ret;
		gene_apcu_store(&key, &data_new, ttl, &ret);
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&key);
		zval_ptr_dtor(&data_new);
		zval_ptr_dtor(&cache_key);
		zval_ptr_dtor(&cache);
		RETURN_ZVAL(&cur_data, 1, 1);
	}
}
/* }}} */

/*
 * {{{ public gene_cache::getVersion($key)
 */
PHP_METHOD(gene_cache, getVersion)
{
	zval *self = getThis(), *versionField = NULL, *config = NULL, *hook = NULL, *hookName = NULL,*versionSign = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &versionField) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	versionSign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));

	zval ret, new_arr;
	hook = gene_di_get(Z_STR_P(hookName));
	gene_cache_get_version_arr(versionSign, versionField, &new_arr, NULL);
	gene_cache_get(hook, &new_arr, &ret);
	zval_ptr_dtor(&new_arr);
	RETURN_ZVAL(&ret, 1, 1);
}
/* }}} */


/*
 * {{{ public gene_cache::updateVersion($key)
 */
PHP_METHOD(gene_cache, updateVersion)
{
	zval *self = getThis(), *versionField = NULL, *config = NULL, *hook = NULL, *hookName = NULL,*sign = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &versionField) == FAILURE) {
		return;
	}
	config =  zend_read_property(gene_cache_ce, self, ZEND_STRL(GENE_CACHE_CONFIG), 1, NULL);
	hookName = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hook"));
	sign = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("versionSign"));

	hook = gene_di_get(Z_STR_P(hookName));
	gene_cache_update_version(sign, versionField, hook);
	RETURN_TRUE;
}
/* }}} */


/*
 * {{{ gene_cache_methods
 */
zend_function_entry gene_cache_methods[] = {
		PHP_ME(gene_cache, cached, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, localCached, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, unsetCached, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, unsetLocalCached, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, cachedVersion, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, localCachedVersion, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, getVersion, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, updateVersion, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_cache, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(cache)
{
    zend_class_entry gene_cache;
    GENE_INIT_CLASS_ENTRY(gene_cache, "Gene_Cache_Cache", "Gene\\Cache\\Cache", gene_cache_methods);
    gene_cache_ce = zend_register_internal_class(&gene_cache TSRMLS_CC);

    zend_declare_property_null(gene_cache_ce, ZEND_STRL(GENE_CACHE_CONFIG), ZEND_ACC_PUBLIC TSRMLS_CC);
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
