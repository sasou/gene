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
#include "Zend/zend_exceptions.h"
#include "Zend/zend_alloc.h"
#include "Zend/zend_interfaces.h"
#include "ext/pcre/php_pcre.h"

#include "php_gene.h"
#include "gene_router.h"
#include "gene_cache.h"
#include "gene_common.h"
#include "gene_application.h"
#include "gene_view.h"

zend_class_entry *gene_router_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_router_call_arginfo, 0, 0, 2) ZEND_ARG_INFO(0, name)
ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */

/** {{{ int init()
 */
void init() {
	zval sval, *params = NULL;
	// param reset
	params = zend_hash_str_find(&EG(symbol_table), "params", 6);
	if (params == NULL) {
		array_init(&sval);
		zend_hash_str_update(&EG(symbol_table), "params", 6, &sval);
	}
	// uri reset
	if (GENE_G(router_path)) {
		efree(GENE_G(router_path));
		GENE_G(router_path) = NULL;
	}
}
/* }}} */


/** {{{ int setMca(zend_string *key TSRMLS_DC)
 */
int setMca(zend_string *key, char *val TSRMLS_DC) {
	zval sval, *params = NULL;
	char *tmp = NULL;
	if (key->len == 1) {
		switch (key->val[0]) {
		case 'm':
			ZVAL_STRING(&sval, val);
			zend_hash_str_update(&EG(symbol_table), "m", 1, &sval);
			break;
		case 'c':
			ZVAL_STRING(&sval, val);
			tmp = (char *) &sval.value.str->val;
			firstToUpper(tmp);
			zend_hash_str_update(&EG(symbol_table), "c", 1, &sval);
			break;
		case 'a':
			ZVAL_STRING(&sval, val);
			zend_hash_str_update(&EG(symbol_table), "a", 1, &sval);
			break;
		}
	} else {
		params = zend_hash_str_find(&EG(symbol_table), "params", 6);
		if (params) {
			ZVAL_STRING(&sval, val);
			zend_symtable_update(params->value.arr, key, &sval);
		}
	}
	return 1;
}
/* }}} */

/** {{{ void gene_router_set_uri(zval **leaf TSRMLS_DC)
 */
void gene_router_set_uri(zval **leaf TSRMLS_DC) {
	zval *key = NULL;
	key = zend_hash_str_find((*leaf)->value.arr, "key", 3);
	if (key) {
		if (Z_STRLEN_P(key)) {
			if (GENE_G(router_path)) {
				efree(GENE_G(router_path));
			}
			GENE_G(router_path) = estrndup(Z_STRVAL_P(key), Z_STRLEN_P(key));
		} else {
			GENE_G(router_path) = str_init("");
		}
	}
}
/* }}} */

/** {{{ static void get_path_router(char *keyString, int keyString_len TSRMLS_DC)
 */
zval *get_path_router(zval *val, char *paths TSRMLS_DC) {
	zval *ret = NULL, *tmp = NULL, *leaf = NULL;
	char *seg = NULL, *ptr = NULL, *path = NULL;
	zend_string *key = NULL;
	zend_long idx;
	if (strlen(paths) == 0) {
		leaf = zend_symtable_str_find(Z_ARRVAL_P(val), "leaf", 4);
		return leaf;
	} else {
		spprintf(&path, 0, "%s", paths);
		seg = php_strtok_r(path, "/", &ptr);
		if (ptr && strlen(seg) > 0) {
			ret = zend_symtable_str_find(Z_ARRVAL_P(val), seg, strlen(seg));
			if (ret) {
				leaf = get_path_router(ret, ptr TSRMLS_CC);
			} else {
				ret = zend_symtable_str_find(Z_ARRVAL_P(val), "chird", 5);
				if (ret) {
					ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(ret), idx, key, tmp) {
						if (key) {
							if (tmp != NULL) {
								leaf = get_path_router(tmp, ptr TSRMLS_CC);
								if (leaf) {
									setMca(key, seg TSRMLS_CC);
									break;
								}
							}

						} else {
							if (tmp != NULL) {
								leaf = get_path_router(tmp, ptr TSRMLS_CC);
								if (leaf) {
									setMca(key, seg TSRMLS_CC);
									break;
								}
							}
						}
					}ZEND_HASH_FOREACH_END();
				}
			}
		} else {
			ret = zend_symtable_str_find(Z_ARRVAL_P(val), seg, strlen(seg));
			if (ret) {
				leaf = zend_symtable_str_find(Z_ARRVAL_P(ret), "leaf", 4);
			} else {
				ret = zend_symtable_str_find(Z_ARRVAL_P(val), "chird", 5);
				if (ret) {
					ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(ret), idx, key, tmp) {
						if (key) {
							if (tmp != NULL) {
								leaf = zend_symtable_str_find(Z_ARRVAL_P(tmp), "leaf", 4);
								if (leaf) {
									setMca(key, seg TSRMLS_CC);
									break;
								}
							}
						} else {
							if (tmp != NULL) {
								leaf = zend_symtable_str_find(Z_ARRVAL_P(tmp), "leaf", 4);
								if (leaf) {
									setMca(key, seg TSRMLS_CC);
									break;
								}
							}
						}
					} ZEND_HASH_FOREACH_END();

				}
			}
		}
	}
	efree(path);
	path = NULL;
	return leaf;
}
/* }}} */

/** {{{ static void get_router_info(char *keyString, int keyString_len TSRMLS_DC)
 */
int get_router_info(zval **leaf, zval **cacheHook TSRMLS_DC) {
	zval *hname, *before, *after, *m, *h;
	int size = 0, is = 1;
	char *run = NULL, *hookname = NULL, *seg = NULL, *ptr = NULL, *filename = "";
	gene_router_set_uri(leaf TSRMLS_CC);
	if (GENE_G(router_path)) {
		filename = GENE_G(router_path);
	}

	hname = zend_hash_str_find((*leaf)->value.arr, "hook", 4);
	if (hname) {
		spprintf(&hookname, 0, "hook:%s", Z_STRVAL_P(hname));
	}
	if (hookname) {
		seg = php_strtok_r(hookname, "@", &ptr);
	}
	size = 1;
	run = (char *) ecalloc(size, sizeof(char));
	run[size - 1] = 0;
	if (ptr && strlen(ptr) > 0) {
		if ((strcmp(ptr, "clearBefore") == 0) || (strcmp(ptr, "clearAll") == 0)) {
			is = 0;
		}
	}
	if (is == 1) {
		before = zend_hash_str_find((*cacheHook)->value.arr, "hook:before", 11);
		if (before) {
			size = size + Z_STRLEN_P(before);
			run = erealloc(run, size);
			strcat(run, Z_STRVAL_P(before));
			run[size - 1] = 0;
		}
	}
	is = 1;
	if (seg && strlen(seg) > 0) {
		h = zend_hash_str_find((*cacheHook)->value.arr, seg, strlen(seg));
		if (h) {
			size = size + Z_STRLEN_P(h);
			run = erealloc(run, size);
			strcat(run, Z_STRVAL_P(h));
			run[size - 1] = 0;
		}
	}
	m = zend_hash_str_find((*leaf)->value.arr, "run", 3);
	if (m) {
		size = size + Z_STRLEN_P(m);
		run = erealloc(run, size);
		strcat(run, Z_STRVAL_P(m));
		run[size - 1] = 0;
	}
	if (ptr && strlen(ptr) > 0) {
		if ((strcmp(ptr, "clearAfter") == 0)
				|| (strcmp(ptr, "clearAll") == 0)) {
			is = 0;
		}
	}
	if (is == 1) {
		after = zend_hash_str_find((*cacheHook)->value.arr, "hook:after", 10);
		if (after) {
			size = size + Z_STRLEN_P(after);
			run = erealloc(run, size);
			strcat(run, Z_STRVAL_P(after));
			run[size - 1] = 0;
		}
	}

	zend_try{
		zend_eval_stringl(run, strlen(run), NULL, filename TSRMLS_CC);
	} zend_catch {
		efree(run);
		run = NULL;
		if (hookname) {
			efree(hookname);
			hookname = NULL;
	} zend_bailout();
			}zend_end_try();
	efree(run);
	run = NULL;
	if (hookname) {
		efree(hookname);
		hookname = NULL;
	}
	return 1;
}
/* }}} */

/** {{{ static void get_router_info(char *keyString, int keyString_len TSRMLS_DC)
 */
int get_router_error_run_by_router(zval *cacheHook, char *errorName TSRMLS_DC) {
	zval *error = NULL;
	int router_e_len, size = 0;
	char *run = NULL, *router_e;
	if (cacheHook) {
		router_e_len = spprintf(&router_e, 0, "error:%s", errorName);
		error = zend_hash_str_find(cacheHook->value.arr, router_e, router_e_len);
		if (error) {
			size = Z_STRLEN_P(error) + 1;
			run = (char *) ecalloc(size, sizeof(char));
			strcat(run, Z_STRVAL_P(error));
			run[size - 1] = 0;
			zend_try{
				zend_eval_stringl(run, strlen(run), NULL, errorName TSRMLS_CC);
			}zend_catch {
				efree(router_e);
				efree(run);
				run = NULL;
				zend_bailout();
			}zend_end_try();
			efree(router_e);
			efree(run);
			run = NULL;
			return 1;
		}
		efree(router_e);
		return 0;
	}
	return 0;
}
/* }}} */

/** {{{ static void get_router_info(char *keyString, int keyString_len TSRMLS_DC)
 */
int get_router_error_run(char *errorName, zval *safe TSRMLS_DC) {
	zval *cacheHook = NULL, *error = NULL;
	int router_e_len, size = 0;
	char *run = NULL, *router_e;
	if (safe != NULL && Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_EVENT);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
	}
	cacheHook = gene_cache_get_quick(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	if (cacheHook) {
		router_e_len = spprintf(&router_e, 0, "error:%s", errorName);
		error = zend_hash_str_find(cacheHook->value.arr, router_e, router_e_len + 1);
		if (error) {
			size = Z_STRLEN_P(error) + 1;
			run = (char *) ecalloc(size, sizeof(char));
			strcat(run, Z_STRVAL_P(error));
			run[size - 1] = 0;
		} else {
			efree(router_e);
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Gene Unknown Error:%s", errorName);
			return 0;
		}
		efree(router_e);
	} else {
		return 0;
	}
	zend_try{
		zend_eval_stringl(run, strlen(run), NULL, "" TSRMLS_CC);
	}zend_catch{
		efree(run);
		run = NULL;
		zend_bailout();
	}zend_end_try();
	efree(run);
	run = NULL;
	return 1;
}
/* }}} */

/** {{{ static void get_function_content(char *keyString, int keyString_len TSRMLS_DC)
 */
char * get_function_content(zval *content TSRMLS_DC) {
	zval objEx, ret, fileName, arg, arg1;
	int startline, endline, size;
	char *result = NULL, *tmp = NULL;
	zend_string *c_key = zend_string_init(ZEND_STRL("ReflectionFunction"), 0);
	zend_class_entry *reflection_ptr = zend_lookup_class(c_key);

	if (reflection_ptr == NULL) {
		php_error_docref(NULL, E_WARNING, "Unable to start ReflectionFunction");
		return NULL;
	}
	zend_string_free(c_key);
	//get file info
	object_init_ex(&objEx, reflection_ptr);
	zend_call_method_with_1_params(&objEx, NULL, NULL, "__construct", NULL,
			content);
	zend_call_method_with_0_params(&objEx, NULL, NULL, "getFileName",
			&fileName);
	zend_call_method_with_0_params(&objEx, NULL, NULL, "getStartLine", &ret);
	startline = Z_LVAL(ret) - 1;
	zval_ptr_dtor(&ret);
	zend_call_method_with_0_params(&objEx, NULL, NULL, "getEndLine", &ret);
	endline = Z_LVAL(ret);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&objEx);

	//get codestartline
	c_key = zend_string_init(ZEND_STRL("SplFileObject"), 0);
	zend_class_entry *spl_file_ptr = zend_lookup_class(c_key);
	if (spl_file_ptr == NULL) {
		php_error_docref(NULL, E_WARNING, "Unable to start SplFileObject");
		return NULL;
	}
	zend_string_free(c_key);
	object_init_ex(&objEx, spl_file_ptr);

	zend_call_method_with_1_params(&objEx, NULL, NULL, "__construct", NULL,
			&fileName);
	zval_ptr_dtor(&fileName);

	ZVAL_LONG(&fileName, startline);
	zend_call_method_with_1_params(&objEx, NULL, NULL, "seek", NULL, &fileName);
	zval_ptr_dtor(&fileName);

	while (startline < endline) {
		zend_call_method_with_0_params(&objEx, NULL, NULL, "current", &ret);
		spprintf(&tmp, 0, "%s", Z_STRVAL(ret));
		zval_ptr_dtor(&ret);

		if (result) {
			size = strlen(result) + strlen(tmp) + 1;
			result = erealloc(result, size);
			strcat(result, tmp);
			result[size - 1] = 0;
		} else {
			size = strlen(tmp) + 1;
			result = ecalloc(size, sizeof(char));
			strcpy(result, tmp);
			result[size - 1] = 0;
		}
		efree(tmp);
		++startline;

		zend_call_method_with_0_params(&objEx, NULL, NULL, "next", NULL);
	}
	zval_ptr_dtor(&objEx);

	ZVAL_STRING(&fileName, "function");
	ZVAL_STRING(&arg1, result);
	zend_call_method_with_2_params(NULL, NULL, NULL, "strpos", &ret, &arg1,
			&fileName);
	startline = Z_LVAL(ret);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&fileName);

	ZVAL_STRING(&arg, "strrpos");
	ZVAL_STRING(&fileName, "}");
	zend_call_method_with_2_params(NULL, NULL, NULL, "strrpos", &ret, &arg1,
			&fileName);
	endline = Z_LVAL(ret);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&fileName);
	zval_ptr_dtor(&arg);
	zval_ptr_dtor(&arg1);

	tmp = ecalloc(endline - startline + 2, sizeof(char));
	mid(tmp, result, endline - startline + 1, startline);
	if (result != NULL) {
		efree(result);
		result = NULL;
	}
	remove_extra_space(tmp);
	return tmp;
}
/* }}} */

/** {{{ char * get_function_content_quik(zval **content TSRMLS_DC)
 */
char *get_function_content_quik(zval **content TSRMLS_DC) {

	return NULL;
}
/* }}} */

/** {{{ char get_router_content(char *content TSRMLS_DC)
 */
char *get_router_content_F(char *src, char *method, char *path TSRMLS_DC) {
	char *dist;
	if (strcmp(method, "hook") == 0) {
		if (strcmp(path, "before") == 0) {
			spprintf(&dist, 0, GENE_ROUTER_CONTENT_FB, src);
		} else if (strcmp(path, "after") == 0) {
			spprintf(&dist, 0, GENE_ROUTER_CONTENT_FA, src);
		} else {
			spprintf(&dist, 0, GENE_ROUTER_CONTENT_FH, src);
			;
		}
	} else {
		spprintf(&dist, 0, GENE_ROUTER_CONTENT_FM, src);
	}
	return dist;
}

/** {{{ char get_router_content(char *content TSRMLS_DC)
 */
char* get_router_content(zval **content, char *method, char *path TSRMLS_DC) {
	char *contents, *seg, *ptr, *tmp;
	spprintf(&contents, 0, "%s", Z_STRVAL_P(*content));
	seg = php_strtok_r(contents, "@", &ptr);
	replaceAll(seg, ':', '$');
	replaceAll(ptr, ':', '$');
	if (seg && ptr && strlen(ptr) > 0) {
		if (strcmp(method, "hook") == 0) {
			if (strcmp(path, "before") == 0) {
				spprintf(&tmp, 0, GENE_ROUTER_CONTENT_B, seg, ptr);
			} else if (strcmp(path, "after") == 0) {
				spprintf(&tmp, 0, GENE_ROUTER_CONTENT_A, seg, ptr);
			} else {
				spprintf(&tmp, 0, GENE_ROUTER_CONTENT_H, seg, ptr);
			}
		} else {
			spprintf(&tmp, 0, GENE_ROUTER_CONTENT_M, seg, ptr);
		}
		efree(contents);
		return tmp;
	}
	return NULL;
}

/*
 *  {{{ void get_router_content_run(char *methodin,char *pathin,zval *safe TSRMLS_DC)
 */
void get_router_content_run(char *methodin, char *pathin, zval *safe TSRMLS_DC) {
	char *method = NULL, *path = NULL, *run = NULL, *hook = NULL, *router_e;
	size_t router_e_len;
	zval *temp, *lead;
	zval *cache = NULL, *cacheHook = NULL;

	if (methodin == NULL && pathin == NULL) {
		if (GENE_G(method)) {
			spprintf(&method, 0, "%s", GENE_G(method));
		}
		if (GENE_G(path)) {
			spprintf(&path, 0, "%s", GENE_G(path));
		}
	} else {
		spprintf(&method, 0, "%s", methodin);
		strtolower(method);
		spprintf(&path, 0, "%s", pathin);
	}

	if (method == NULL || path == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Gene Unknown Method And Url: NULL");
		return;
	}

	if (safe != NULL && Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_TREE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	}
	cache = gene_cache_get_quick(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	if (cache) {
		temp = zend_symtable_str_find(cache->value.arr, method, strlen(method));
		if (temp == NULL) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
					"Gene Unknown Method Cache:%s", method);
			efree(method);
			efree(path);
			// zval_ptr_dtor(&cache);
			cache = NULL;
			return;
		}
		if (safe != NULL && Z_STRLEN_P(safe)) {
			router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
			GENE_ROUTER_ROUTER_EVENT);
		} else {
			router_e_len = spprintf(&router_e, 0, "%s",
			GENE_ROUTER_ROUTER_EVENT);
		}
		cacheHook = gene_cache_get_quick(router_e, router_e_len TSRMLS_CC);
		efree(router_e);
		trim(path, '/');
		replaceAll(path, '.', '/');

		lead = get_path_router(temp, path TSRMLS_CC);

		if (lead) {
			get_router_info(&lead, &cacheHook TSRMLS_CC);
			lead = NULL;
		} else {
			if (!get_router_error_run_by_router(cacheHook, "404" TSRMLS_CC)) {
				if (GENE_G(path)) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING,
							"Gene Unknown Url:%s", GENE_G(path));
				} else {
					php_error_docref(NULL TSRMLS_CC, E_WARNING,
							"Gene Unknown Url:%s", path);
				}
			}
		}
		cache = NULL;
		//zval_ptr_dtor(&cache);
		if (cacheHook) {
			//zval_ptr_dtor(&cacheHook);
			cacheHook = NULL;
		}
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
				"Gene Unknown Router Cache");
	}
	efree(method);
	efree(path);
	temp = NULL;
	return;
}

char *
str_add1(const char *s, int length) {
	char *p;
#ifdef ZEND_SIGNALS
	TSRMLS_FETCH();
#endif

	HANDLE_BLOCK_INTERRUPTIONS();

	p = (char *) ecalloc(length + 1, sizeof(char));
	if (UNEXPECTED(p == NULL)) {
		HANDLE_UNBLOCK_INTERRUPTIONS();
		return p;
	}
	if (length) {
		memcpy(p, s, length);
	}
	p[length] = 0;
	HANDLE_UNBLOCK_INTERRUPTIONS();
	return p;
}

/*
 * {{{ gene_router_methods
 */
PHP_METHOD(gene_router, run) {
	zend_string *methodin = NULL, *pathin = NULL;
	zval *self = getThis(), *safe = NULL, safein;
	char *min = NULL, *pin = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|SS", &methodin, &pathin) == FAILURE) {
		RETURN_NULL();
	}

	safe = zend_read_property(gene_router_ce, self, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), 1, NULL);
	if (safe && Z_STRLEN_P(safe)) {
		ZVAL_STRING(&safein, Z_STRVAL_P(safe));
	} else {
		ZVAL_STRING(&safein, "");
	}
	if (methodin && ZSTR_LEN(methodin)) {
		min = ZSTR_VAL(methodin);
	}
	if (pathin && ZSTR_LEN(pathin)) {
		pin = ZSTR_VAL(pathin);
	}
	get_router_content_run(min, pin, &safein TSRMLS_CC);
	zval_ptr_dtor(&safein);
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_router_methods
 */
PHP_METHOD(gene_router, runError) {
	zend_string *methodin = NULL;
	zval safe;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &methodin) == FAILURE) {
		RETURN_NULL();
	}
	if (GENE_G(app_key)) {
		ZVAL_STRING(&safe, GENE_G(app_key));
	} else {
		ZVAL_STRING(&safe, GENE_G(directory));
	}
	get_router_error_run(ZSTR_VAL(methodin), &safe TSRMLS_CC);
	zval_ptr_dtor(&safe);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_router_methods
 */
PHP_METHOD(gene_router, __construct) {
	zval *safe = NULL;
	int len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|z", &safe)
			== FAILURE) {
		RETURN_NULL();
	}
	gene_ini_router(TSRMLS_C);
	if (safe) {
		zend_update_property_string(gene_router_ce, getThis(), GENE_ROUTER_SAFE,
				strlen(GENE_ROUTER_SAFE), Z_STRVAL_P(safe) TSRMLS_CC);
	} else {
		if (GENE_G(app_key)) {
			zend_update_property_string(gene_router_ce, getThis(),
			GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE),
					GENE_G(app_key) TSRMLS_CC);
		} else {
			zend_update_property_string(gene_router_ce, getThis(),
			GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE),
					GENE_G(directory) TSRMLS_CC);
		}
	}
}
/* }}} */

/*
 * {{{ public gene_router::__call($codeString)
 */
PHP_METHOD(gene_router, __call) {
	zval *val = NULL, content, *prefix, *safe, *self = getThis(), pathvals;
	zval *pathVal, *contentval, *hook = NULL;
	long methodlen;
	int i, router_e_len;
	char *result = NULL, *tmp = NULL, *router_e, *key = NULL, *method, *path =
	NULL;
	const char *methods[9] = { "get", "post", "put", "patch", "delete", "trace",
			"connect", "options", "head" }, *event[2] = { "hook", "error" },
			*common[2] = { "group", "prefix" };

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "sz", &method,
			&methodlen, &val) == FAILURE) {
		RETURN_NULL()
		;
	} else {
		strtolower(method);
		if (IS_ARRAY == Z_TYPE_P(val)) {
			pathVal = zend_hash_index_find(val->value.arr, 0);
			if (pathVal) {
				convert_to_string(pathVal);
				//paths = estrndup((*path)->value.str.val,(*path)->value.str.len);
				for (i = 0; i < 9; i++) {
					if (strcmp(methods[i], method) == 0) {
						prefix = zend_read_property(gene_router_ce, self,
						GENE_ROUTER_PREFIX, strlen(GENE_ROUTER_PREFIX), 1,
						NULL);
						spprintf(&path, 0, "%s%s", Z_STRVAL_P(prefix),
								Z_STRVAL_P(pathVal));
						break;
					}
				}
				if (path == NULL) {
					spprintf(&path, 0, "%s", Z_STRVAL_P(pathVal));
				}
			}
			contentval = zend_hash_index_find(val->value.arr, 1);
			if (contentval) {
				if (IS_OBJECT == Z_TYPE_P(contentval)) {
					tmp = get_function_content(contentval TSRMLS_CC);
					result = get_router_content_F(tmp, method, path TSRMLS_CC);
					if (tmp != NULL) {
						efree(tmp);
						tmp = NULL;
					}
				} else {
					result = get_router_content(&contentval, method,
							path TSRMLS_CC);
				}
				if (result) {
					ZVAL_STRING(&content, result);
					efree(result);
				} else {
					ZVAL_STRING(&content, "");
				}
			}
			hook = zend_hash_index_find(val->value.arr, 2);
			if (hook) {
				convert_to_string(hook);
			}

			//call tree
			for (i = 0; i < 9; i++) {
				if (strcmp(methods[i], method) == 0) {
					trim(path, '/');
					if (path != NULL) {
						ZVAL_STRING(&pathvals, path);
					} else {
						ZVAL_STRING(&pathvals, "");
					}
					safe = zend_read_property(gene_router_ce, self,
					GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1,
					NULL);
					if (Z_STRLEN_P(safe)) {
						router_e_len = spprintf(&router_e, 0, "%s%s",
								Z_STRVAL_P(safe),
								GENE_ROUTER_ROUTER_TREE);
					} else {
						router_e_len = spprintf(&router_e, 0, "%s",
						GENE_ROUTER_ROUTER_TREE);
					}
					if (strlen(path) == 0) {
						spprintf(&key, 0, GENE_ROUTER_LEAF_KEY, method);
						gene_cache_set_by_router(router_e, router_e_len, key,
								&pathvals, 0 TSRMLS_CC);
						efree(key);
						spprintf(&key, 0, GENE_ROUTER_LEAF_RUN, method);
						gene_cache_set_by_router(router_e, router_e_len, key,
								&content, 0 TSRMLS_CC);
						efree(key);
						if (hook) {
							spprintf(&key, 0, GENE_ROUTER_LEAF_HOOK, method);
							gene_cache_set_by_router(router_e, router_e_len,
									key, hook, 0 TSRMLS_CC);
							efree(key);
						}
					} else {
						replaceAll(path, '.', '/');
						tmp = replace_string(path, ':', GENE_ROUTER_CHIRD);
						if (tmp == NULL) {
							spprintf(&key, 0, GENE_ROUTER_LEAF_KEY_L, method,
									path);
							gene_cache_set_by_router(router_e, router_e_len,
									key, &pathvals, 0 TSRMLS_CC);
							efree(key);
							spprintf(&key, 0, GENE_ROUTER_LEAF_RUN_L, method,
									path);
							gene_cache_set_by_router(router_e, router_e_len,
									key, &content, 0 TSRMLS_CC);
							efree(key);
							if (hook) {
								spprintf(&key, 0, GENE_ROUTER_LEAF_HOOK_L,
										method, path);
								gene_cache_set_by_router(router_e, router_e_len,
										key, hook, 0 TSRMLS_CC);
								efree(key);
							}
						} else {
							spprintf(&key, 0, GENE_ROUTER_LEAF_KEY_L, method,
									tmp);
							gene_cache_set_by_router(router_e, router_e_len,
									key, &pathvals, 0 TSRMLS_CC);
							efree(key);
							spprintf(&key, 0, GENE_ROUTER_LEAF_RUN_L, method,
									tmp);
							gene_cache_set_by_router(router_e, router_e_len,
									key, &content, 0 TSRMLS_CC);
							efree(key);
							if (hook) {
								spprintf(&key, 0, GENE_ROUTER_LEAF_HOOK_L,
										method, tmp);
								gene_cache_set_by_router(router_e, router_e_len,
										key, hook, 0 TSRMLS_CC);
								efree(key);
							}
							efree(tmp);
						}
					}
					efree(router_e);
					efree(path);
					RETURN_ZVAL(self, 1, 0);
				}
			}

			//call event
			for (i = 0; i < 2; i++) {
				if (strcmp(event[i], method) == 0) {
					safe = zend_read_property(gene_router_ce, self,
					GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1,
					NULL);
					if (Z_STRLEN_P(safe)) {
						router_e_len = spprintf(&router_e, 0, "%s%s",
								Z_STRVAL_P(safe),
								GENE_ROUTER_ROUTER_EVENT);
					} else {
						router_e_len = spprintf(&router_e, 0, "%s",
						GENE_ROUTER_ROUTER_EVENT);
					}
					spprintf(&key, 0, "%s:%s", method, path);
					gene_cache_set_by_router(router_e, router_e_len, key,
							&content, 0 TSRMLS_CC);
					efree(router_e);
					efree(key);
					efree(path);
					RETURN_ZVAL(self, 1, 0);
				}
			}
			//call common
			for (i = 0; i < 2; i++) {
				if (strcmp(common[i], method) == 0) {
					if (path == NULL) {
						zend_update_property_string(gene_router_ce, self,
						GENE_ROUTER_PREFIX, strlen(GENE_ROUTER_PREFIX),
								"" TSRMLS_CC);
					} else {
						trim(path, '/');
						zend_update_property_string(gene_router_ce, self,
						GENE_ROUTER_PREFIX, strlen(GENE_ROUTER_PREFIX),
								path TSRMLS_CC);
					}
					efree(path);
					RETURN_ZVAL(self, 1, 0);
				}
			}
			if (val) {
				zval_ptr_dtor(val);
			}
		}
		RETURN_ZVAL(self, 1, 0);
	}
}
/* }}} */

/*
 * {{{ public gene_router::getEvent()
 */
PHP_METHOD(gene_router, getEvent) {
	zval *self = getThis(), *safe, *cache = NULL;
	int router_e_len;
	char *router_e;
	safe = zend_read_property(gene_router_ce, self, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_EVENT);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
	}
	cache = gene_cache_get(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	if (cache) {
		ZVAL_COPY_VALUE(return_value, cache);
		return;
	}
	RETURN_NULL()
	;
}
/* }}} */

/*
 * {{{ public gene_router::getTree()
 */
PHP_METHOD(gene_router, getTree) {
	zval *self = getThis(), *safe, *cache = NULL;
	int router_e_len;
	char *router_e;
	safe = zend_read_property(gene_router_ce, self, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_TREE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	}
	cache = gene_cache_get(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	if (cache) {
		ZVAL_COPY_VALUE(return_value, cache);
		return;
	}
	RETURN_NULL()
	;
}
/* }}} */

/*
 * {{{ public gene_router::delTree()
 */
PHP_METHOD(gene_router, delTree) {
	zval *self = getThis(), *safe;
	int router_e_len, ret;
	char *router_e;
	safe = zend_read_property(gene_router_ce, self, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_TREE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	}
	ret = gene_cache_del(router_e, router_e_len TSRMLS_CC);
	if (ret) {
		efree(router_e);
		RETURN_TRUE
		;
	}
	efree(router_e);
	RETURN_FALSE
	;
}
/* }}} */

/*
 * {{{ public gene_router::delTree()
 */
PHP_METHOD(gene_router, delEvent) {
	zval *self = getThis(), *safe;
	int router_e_len, ret;
	char *router_e;
	safe = zend_read_property(gene_router_ce, self, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_EVENT);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
	}
	ret = gene_cache_del(router_e, router_e_len TSRMLS_CC);
	if (ret) {
		efree(router_e);
		RETURN_TRUE
		;
	}
	efree(router_e);
	RETURN_FALSE
	;
}
/* }}} */

/*
 * {{{ public gene_router::clear()
 */
PHP_METHOD(gene_router, clear) {
	zval *self = getThis(), *safe;
	int router_e_len, ret;
	char *router_e;
	safe = zend_read_property(gene_router_ce, self, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_TREE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	}
	ret = gene_cache_del(router_e, router_e_len TSRMLS_CC);
	if (ret) {
		efree(router_e);
	}
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_EVENT);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
	}
	ret = gene_cache_del(router_e, router_e_len TSRMLS_CC);
	if (ret) {
		efree(router_e);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_router::getTime()
 */
PHP_METHOD(gene_router, getTime) {
	zval *self = getThis(), *safe;
	int router_e_len;
	long ctime = 0;
	char *router_e;
	safe = zend_read_property(gene_router_ce, self, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), 1, NULL);
	if (Z_STRLEN_P(safe)) {
		router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		GENE_ROUTER_ROUTER_TREE);
	} else {
		router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	}
	ctime = gene_cache_getTime(router_e, router_e_len TSRMLS_CC);
	efree(router_e);
	if (ctime > 0) {
		ctime = time(NULL) - ctime;
		RETURN_LONG(ctime);
	}
	RETURN_NULL()
	;
}
/* }}} */

/*
 * {{{ public gene_router::getRouter()
 */
PHP_METHOD(gene_router, getRouter) {
	zval *self = getThis();
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_router::readFile()
 */
PHP_METHOD(gene_router, readFile) {
	zend_string *fileName = NULL;
	char *rec = NULL;
	int fileNameLen = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S", &fileName)
			== FAILURE) {
		RETURN_NULL()
		;
	}
	rec = readfilecontent(ZSTR_VAL(fileName));
	if (rec != NULL) {
		RETURN_STRING(rec);
	}
	RETURN_NULL()
	;
}
/* }}} */

/*
 static void php_free_pcre_cache(void *data)
 {
 pcre_cache_entry *pce = (pcre_cache_entry *) data;
 if (!pce) return;
 pefree(pce->re, 1);
 if (pce->extra) pefree(pce->extra, 1);
 #if HAVE_SETLOCALE
 if ((void*)pce->tables) pefree((void*)pce->tables, 1);
 pefree(pce->locale, 1);
 #endif
 }
 */

/** {{{ public gene_router::display(string $file)
 */
PHP_METHOD(gene_router, display) {
	zend_string *file;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|l", &file)
			== FAILURE) {
		return;
	}
	if (ZSTR_LEN(file)) {
		gene_view_display(ZSTR_VAL(file) TSRMLS_CC);
	}
}
/* }}} */

/** {{{ public gene_router::display(string $file)
 */
PHP_METHOD(gene_router, displayExt) {
	zend_string *file, *parent_file = NULL;
	zend_bool isCompile = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|Sb", &file,
			&parent_file, &isCompile) == FAILURE) {
		return;
	}
	if (parent_file && ZSTR_LEN(parent_file)) {
		GENE_G(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display_ext(ZSTR_VAL(parent_file), isCompile TSRMLS_CC);
	} else {
		gene_view_display_ext(ZSTR_VAL(file), isCompile TSRMLS_CC);
	}
}
/* }}} */


/*
 * {{{ gene_router_methods
 */
zend_function_entry gene_router_methods[] = {
	PHP_ME(gene_router, getEvent, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, getTree, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, delTree, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, delEvent, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, clear, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, getTime, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, getRouter, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, display, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_router, displayExt, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_router, runError, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_router, run, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, readFile, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, __call, gene_router_call_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_router, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(router) {
	zend_class_entry gene_router;
	GENE_INIT_CLASS_ENTRY(gene_router, "Gene_Router", "Gene\\Router",
			gene_router_methods);
	gene_router_ce = zend_register_internal_class(&gene_router TSRMLS_CC);

	//prop
	zend_declare_property_string(gene_router_ce, GENE_ROUTER_SAFE,
			strlen(GENE_ROUTER_SAFE), "",
			ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_string(gene_router_ce, GENE_ROUTER_PREFIX,
			strlen(GENE_ROUTER_PREFIX), "",
			ZEND_ACC_PUBLIC TSRMLS_CC);
	//zend_declare_property_string(gene_router_ce, GENE_ROUTER_PREFIX, strlen(GENE_ROUTER_PREFIX), "", ZEND_ACC_PUBLIC TSRMLS_CC);
	//
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
