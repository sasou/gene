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
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
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
 
#include "../gene.h"
#include "../factory/factory.h"
#include "../router/router.h"
#include "../cache/memory.h"
#include "../common/common.h"
#include "../app/application.h"
#include "../http/request.h"
 #include "../mvc/view.h"
 #include "../mvc/hook.h"
 #include "zend_smart_str.h"
 
 zend_class_entry *gene_router_ce;
 
 /** {{{ ARG_INFO
  */
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_void_arginfo, 0, 0, 0)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_display, 0, 0, 1)
	 ZEND_ARG_INFO(0, file)
	 ZEND_ARG_INFO(0, parent_file)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_display_ext, 0, 0, 1)
	 ZEND_ARG_INFO(0, file)
	 ZEND_ARG_INFO(0, parent_file)
	 ZEND_ARG_INFO(0, isCompile)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_call_arginfo, 0, 0, 2)
	 ZEND_ARG_INFO(0, method)
	 ZEND_ARG_INFO(0, params)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_run, 0, 0, 2)
	 ZEND_ARG_INFO(0, method)
	 ZEND_ARG_INFO(0, uri)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_run_error, 0, 0, 1)
	 ZEND_ARG_INFO(0, method)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_read_file, 0, 0, 1)
 ZEND_ARG_INFO(0, file)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_dispatch, 0, 0, 3)
 ZEND_ARG_INFO(0, class)
 ZEND_ARG_INFO(0, action)
 ZEND_ARG_INFO(0, params)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_assign, 0, 0, 2)
	 ZEND_ARG_INFO(0, name)
	 ZEND_ARG_INFO(0, value)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_params, 0, 0, 1)
	 ZEND_ARG_INFO(0, name)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_prefix_arginfo, 0, 0, 1)
	 ZEND_ARG_INFO(0, name)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_group_arginfo, 0, 0, 1)
	 ZEND_ARG_INFO(0, name)
 ZEND_END_ARG_INFO()
 
 ZEND_BEGIN_ARG_INFO_EX(gene_router_lang_arginfo, 0, 0, 2)
	 ZEND_ARG_INFO(0, lang)
	 ZEND_ARG_INFO(0, lang_list)
 ZEND_END_ARG_INFO()
 /* }}} */
 
 /** {{{ int setMca(zend_string *key)
  * [GENE_PERF:2026-04-19] Inline strlen + emalloc + memcpy + uppercase-first into
  * a single pass. Called once per path segment (m/c/a) during router dispatch, so
  * shaving an emalloc+3 function-call overhead per segment compounds across all
  * matched routes (typically 3 calls per request: module, controller, action).
  * [GENE_PERF:2026-04-20] Populate cached length fields (module_len/controller_len/
  * action_len) so downstream strreplace_fast and dispatch-direct can skip strlen().
  */
 int setMca(zend_string *key, const char *val, size_t val_len) {
	 zval sval, *params = NULL;
	 gene_request_context *ctx = gene_request_ctx();
	 if (key->len == 1) {
		 char c0 = key->val[0];
		 if (c0 == 'm' || c0 == 'c' || c0 == 'a') {
			 char *buf = emalloc(val_len + 1);
			 if (val_len > 0) {
				 unsigned char first = (unsigned char)val[0];
				 /* uppercase first char only for module/controller; action stays verbatim */
				 if ((c0 == 'm' || c0 == 'c') && first >= 'a' && first <= 'z') {
					 buf[0] = (char)(first & ~0x20);
				 } else {
					 buf[0] = (char)first;
				 }
				 if (val_len > 1) memcpy(buf + 1, val + 1, val_len - 1);
			 }
			 buf[val_len] = '\0';
			 switch (c0) {
			 case 'm':
				 if (ctx->module) efree(ctx->module);
				 ctx->module = buf;
				 ctx->module_len = val_len;
				 break;
			 case 'c':
				 if (ctx->controller) efree(ctx->controller);
				 ctx->controller = buf;
				 ctx->controller_len = val_len;
				 break;
			 case 'a':
				 if (ctx->action) efree(ctx->action);
				 ctx->action = buf;
				 ctx->action_len = val_len;
				 break;
			 }
		 }
	 } else {
		 params = &ctx->path_params;
		 if (Z_TYPE_P(params) == IS_ARRAY) {
			 ZVAL_STRINGL(&sval, val, val_len);
			 zend_symtable_update(Z_ARRVAL_P(params), key, &sval);
		 }
	 }
	 return 1;
 }
 /* }}} */
 
 /** {{{ void gene_router_reset_path_params()
  * [GENE_MEM:2026-04-24] path_params is inlined in gene_request_context; the
  * outer zval is always present, only its array backing store is heap-backed.
  * Fast path is a pure HashTable clean — no emalloc branch. If a prior request
  * ballooned the table, drop+re-init to keep worker RSS bounded.
  */
 static zend_always_inline void gene_router_reset_path_params() {
	 gene_request_context *ctx = gene_request_ctx();
	 zval *params = &ctx->path_params;

	 if (EXPECTED(Z_TYPE_P(params) == IS_ARRAY)) {
		 if (UNEXPECTED(Z_ARRVAL_P(params)->nTableSize > 128)) {
			 zval_ptr_dtor(params);
			 array_init(params);
			 return;
		 }
		 zend_hash_clean(Z_ARRVAL_P(params));
		 return;
	 }
	 if (Z_TYPE_P(params) != IS_UNDEF) {
		 zval_ptr_dtor(params);
	 }
	 array_init(params);
 }
 /* }}} */
 
 /** {{{ void gene_router_set_uri(zval **leaf)
  * [GENE_PERF:2026-04-20] Populate ctx->router_path_len so getRouterUri() and
  * later consumers can skip a strlen() on the cached router path.
  */
 void gene_router_set_uri(zval **leaf) {
	 zval *key = NULL;
	 gene_request_context *ctx = gene_request_ctx();
	 key = zend_hash_str_find(Z_ARRVAL_P(*leaf), "key", 3);
	 if (key) {
		 if (ctx->router_path) {
			 efree(ctx->router_path);
		 }
		 if (Z_STRLEN_P(key)) {
			 ctx->router_path = estrndup(Z_STRVAL_P(key), Z_STRLEN_P(key));
			 ctx->router_path_len = Z_STRLEN_P(key);
		 } else {
			 ctx->router_path = estrndup("", 0);
			 ctx->router_path_len = 0;
		 }
	 }
 }
 /* }}} */
 
 /** {{{ static void get_path_router(char *keyString, int keyString_len)
  * [GENE_PERF:2026-04-19 #2] Cache gene_request_ctx() once — prior code invoked
  * GENE_REQ(lang) up to 4 times per request when a language prefix matched.
  * [GENE_PERF:2026-04-20] Replaced str_sub+strcmp pair (one emalloc+efree per
  * request when prefix matched) with a direct memcmp against path. Also capture
  * seg_len from the first tokenizer pass to set ctx->lang_len directly,
  * avoiding a downstream strlen() when lang is later read by getLang().
  */
 char *get_path_router_init(zval *conf, char *path) {
	 zval *prefix = NULL, *langs = NULL;
	 char *seg = NULL, *ptr = NULL, *result = NULL, *search = NULL, *work = NULL;
	 size_t seg_len = 0;
	 zend_long path_len = 0;
	 path_len = strlen(path);
	 if (path_len == 0) {
		 return path;
	 }
 
	 result = NULL;
	 prefix = zend_symtable_str_find(Z_ARRVAL_P(conf), "prefix", 6);
	 if (prefix) {
		 size_t prefix_len = Z_STRLEN_P(prefix);
		 if (prefix_len <= (size_t)path_len
				 && memcmp(path, Z_STRVAL_P(prefix), prefix_len) == 0) {
			 result = str_sub_len(path, prefix_len, (size_t)path_len - prefix_len);
			 trim(result, '/');
			 if (result[0] == '\0') {
				 return result;
			 }
		 }
	 }
	 langs = zend_symtable_str_find(Z_ARRVAL_P(conf), "langs", 5);
	 if (langs) {
		 work = str_init(result ? result : path);
		 seg = php_strtok_r(work, "/", &ptr);
		 if (seg && (seg_len = strlen(seg)) > 0) {
			 if (ptr == NULL) {
				 ptr = "";
			 }
			 {
				 char lang_buf[128];
				 char *lang_p = (seg_len + 3 <= sizeof(lang_buf)) ? lang_buf : emalloc(seg_len + 3);
				 lang_p[0] = ',';
				 memcpy(lang_p + 1, seg, seg_len);
				 lang_p[seg_len + 1] = ',';
				 lang_p[seg_len + 2] = '\0';
				 search = strstr(Z_STRVAL_P(langs), lang_p);
				 if (lang_p != lang_buf) efree(lang_p);
			 }
			 if (search != NULL) {
				 gene_request_context *ctx = gene_request_ctx();
				 if (ctx->lang) {
					 efree(ctx->lang);
					 ctx->lang = NULL;
				 }
				 ctx->lang = estrndup(seg, seg_len);
				 ctx->lang_len = seg_len;
				 if (result) {
					 efree(result);
				 }
				 result = estrdup(ptr);
				 if (result[0] != '\0') {
					 trim(result, '/');
				 }
			 }
		 }
		 efree(work);
	 }
 
	 return result ? result : path;
 }
 /* }}} */
 
 /** {{{ static zval *get_path_router_inner(zval *val, char *paths)
  * [GENE_PERF] Replaced php_strtok_r with pointer scanning to avoid
  * string duplication and state maintenance overhead in the hot path.
  */
 static zval *get_path_router_inner(zval *val, char *paths) {
	 zval *ret = NULL, *tmp = NULL, *leaf = NULL;
	 char *seg = NULL, *next_slash = NULL;
	 zend_string *key = NULL;
	 zend_long idx;
	 size_t seg_len;

	if (paths[0] == '\0') {
		leaf = zend_symtable_str_find(Z_ARRVAL_P(val), "leaf", 4);
		return leaf;
	} else {
		 /* Find first '/' to extract current segment */
		 next_slash = strchr(paths, '/');
		 if (next_slash) {
			 seg_len = next_slash - paths;
			 seg = paths;
			 /* Temporarily null-terminate the segment for hash lookup */
			 *next_slash = '\0';
			 ret = zend_symtable_str_find(Z_ARRVAL_P(val), seg, seg_len);
			 if (ret) {
				 leaf = get_path_router_inner(ret, next_slash + 1);
			 }
			 if (!leaf) {
				 ret = zend_symtable_str_find(Z_ARRVAL_P(val), "chird", 5);
				 if (ret) {
					 ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(ret), idx, key, tmp) {
						 if (key) {
							 if (tmp != NULL) {
							 leaf = get_path_router_inner(tmp, next_slash + 1);
							 if (leaf) {
								 setMca(key, seg, seg_len);
								 break;
							 }
						 }

					 } else {
							 if (tmp != NULL) {
								 leaf = get_path_router_inner(tmp, next_slash + 1);
								 if (leaf) {
									 break;
								 }
							 }
						 }
					 }ZEND_HASH_FOREACH_END();
				 }
			 }
			 /* Restore the '/' character */
			 *next_slash = '/';
		 } else {
			 /* No more '/' — this is the last segment */
			 seg_len = strlen(paths);
			 seg = paths;
			 ret = zend_symtable_str_find(Z_ARRVAL_P(val), seg, seg_len);
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
								 setMca(key, seg, seg_len);
								 break;
							 }
						 }
					 } else {
						 if (tmp != NULL) {
								 leaf = zend_symtable_str_find(Z_ARRVAL_P(tmp), "leaf", 4);
								 if (leaf) {
									 break;
								 }
							 }
						 }
					 } ZEND_HASH_FOREACH_END();

				 }
			 }
		 }
	 }
	 return leaf;
 }

zval *get_path_router(zval *val, char *paths) {
	 zval *leaf;
	if (paths[0] == '\0') {
		return zend_symtable_str_find(Z_ARRVAL_P(val), "leaf", 4);
	}
	leaf = get_path_router_inner(val, paths);
	return leaf;
 }
 /* }}} */
 
 /** {{{ gene_router_dispatch_direct
 * Directly dispatch a "Class@action" route via C API, avoiding eval.
 * Performs :m/:c/:a placeholder substitution identical to PHP_METHOD(dispatch).
 * Returns 1 on success, 0 on failure. Result written to *retval.
 */
static int gene_router_dispatch_direct(const char *class_method, zval *retval) {
	 const char *at;
	 char *class_alloc = NULL, *action_alloc = NULL, *tmp_alloc = NULL;
	 size_t class_name_len, action_len;
	 char cls_buf[256], act_buf[128];
	 char *class_name, *action;
	 zval classObject;
	 gene_request_context *ctx = gene_request_ctx();

	 ZVAL_NULL(retval);

	 if (!class_method || class_method[0] == '\0') return 0;

	 at = memchr(class_method, '@', strlen(class_method));
	 if (!at || at == class_method || at[1] == '\0') return 0;

	 class_name_len = at - class_method;
	 action_len = strlen(at + 1);

	 if (class_name_len < sizeof(cls_buf)) {
		 memcpy(cls_buf, class_method, class_name_len);
		 cls_buf[class_name_len] = '\0';
		 class_name = cls_buf;
	 } else {
		 class_name = estrndup(class_method, class_name_len);
		 class_alloc = class_name;
	 }
	 if (action_len < sizeof(act_buf)) {
		 memcpy(act_buf, at + 1, action_len + 1);
		 action = act_buf;
	 } else {
		 action = estrndup(at + 1, action_len);
		 action_alloc = action;
	 }

	 /* [GENE_PERF:2026-04-20] Use cached module_len/controller_len/action_len
	  * instead of calling strlen() on each dispatch. These fields are populated
	  * by setMca() when the route matches, before dispatch_direct is invoked. */
	 if (ctx->module != NULL) {
		 char *tmp_m = gene_strreplace_fast(class_name, class_name_len, ":m", 2, ctx->module, ctx->module_len, &class_name_len);
		 if (tmp_m) {
			 if (class_alloc) efree(class_alloc);
			 class_alloc = tmp_m;
			 class_name = class_alloc;
		 }
	 }
	 if (ctx->controller != NULL) {
		 tmp_alloc = gene_strreplace_fast(class_name, class_name_len, ":c", 2, ctx->controller, ctx->controller_len, &class_name_len);
		 if (tmp_alloc) {
			 if (class_alloc) efree(class_alloc);
			 class_alloc = tmp_alloc;
			 class_name = class_alloc;
		 }
	 }

	 if (!gene_factory(class_name, class_name_len, NULL, &classObject)) {
		 php_error_docref(NULL, E_WARNING, "Gene direct dispatch: unable to init class '%s'.", class_name);
		 if (class_alloc) efree(class_alloc);
		 if (action_alloc) efree(action_alloc);
		 return 0;
	 }

	 if (ctx->action != NULL) {
		 char *tmp = gene_strreplace_fast(action, action_len, ":a", 2, ctx->action, ctx->action_len, &action_len);
		 if (tmp) {
			 if (action_alloc) efree(action_alloc);
			 action_alloc = tmp;
			 action = action_alloc;
		 }
	 }
	 gene_strtolower(action);

	 if (Z_TYPE(classObject) == IS_OBJECT
			 && zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), action, action_len)) {
		 gene_factory_call_1(&classObject, action, action_len, &ctx->path_params, retval);
		 zval_ptr_dtor(&classObject);
		 if (class_alloc) efree(class_alloc);
		 if (action_alloc) efree(action_alloc);
		 return 1;
	 }

	 php_error_docref(NULL, E_WARNING, "Gene direct dispatch: unable to call method '%s' in class '%s'.", action, class_name);
	 zval_ptr_dtor(&classObject);
	 if (class_alloc) efree(class_alloc);
	 if (action_alloc) efree(action_alloc);
	 return 0;
}
/* }}} */

/** {{{ gene_router_exec_hook_direct
 * Directly execute a "HookClass@method" hook via C API.
 * For Gene\Hook subclasses: uses gene_factory_load_class (no constructor)
 * since hooks are stateless and DI is handled via __get/__set.
 * For before/specific hooks: call with no args, return 0 to abort.
 * For after hooks: call with dispatch_result as single arg.
 * Returns 1 to continue, 0 to abort (before hooks only).
 */
static int gene_router_exec_hook_direct(const char *class_method, zval *param, int is_before) {
    const char *at;
    char cls_buf[256], mth_buf[128];
    char *class_name, *method;
    zval classObject, retval;
    size_t class_name_len, method_len;
    int cls_heap = 0, mth_heap = 0;
    zend_class_entry *ce;

    ZVAL_NULL(&retval);

    if (!class_method || class_method[0] == '\0') return 1;

    at = memchr(class_method, '@', strlen(class_method));
    if (!at || at == class_method || at[1] == '\0') return 1;

    class_name_len = at - class_method;
    method_len = strlen(at + 1);

    if (class_name_len < sizeof(cls_buf)) {
        memcpy(cls_buf, class_method, class_name_len);
        cls_buf[class_name_len] = '\0';
        class_name = cls_buf;
    } else {
        class_name = estrndup(class_method, class_name_len);
        cls_heap = 1;
    }
    if (method_len < sizeof(mth_buf)) {
        memcpy(mth_buf, at + 1, method_len + 1);
        method = mth_buf;
    } else {
        method = estrndup(at + 1, method_len);
        mth_heap = 1;
    }
    gene_strtolower(method);

    /* Try lightweight load for Gene\Hook subclasses (skip constructor) */
    if (gene_hook_ce && gene_factory_load_class(class_name, class_name_len, &classObject)) {
        ce = Z_OBJCE(classObject);
        if (instanceof_function(ce, gene_hook_ce)) {
            /* Fast path: Hook subclass, no constructor needed */

            if (zend_hash_str_exists(&ce->function_table, method, method_len)) {
                if (param) {
                    gene_factory_call_1(&classObject, method, method_len, param, &retval);
                } else {
                    gene_factory_call(&classObject, method, method_len, NULL, &retval);
                }
            }
            zval_ptr_dtor(&classObject);
            if (cls_heap) efree(class_name);
            if (mth_heap) efree(method);
            goto check_retval;
        }
        /* Not a Hook subclass, destroy and fall through to gene_factory path */
        zval_ptr_dtor(&classObject);
    }

    /* Standard path: full factory init with constructor */
    if (!gene_factory(class_name, class_name_len, NULL, &classObject)) {
        if (cls_heap) efree(class_name);
        if (mth_heap) efree(method);
        return 1;
    }

    if (Z_TYPE(classObject) == IS_OBJECT
            && zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), method, method_len)) {
        if (param) {
            gene_factory_call_1(&classObject, method, method_len, param, &retval);
        } else {
            gene_factory_call(&classObject, method, method_len, NULL, &retval);
        }
    }

    zval_ptr_dtor(&classObject);
    if (cls_heap) efree(class_name);
    if (mth_heap) efree(method);

check_retval:
    if (is_before) {
        int abort = 0;
        if (Z_TYPE(retval) != IS_NULL && Z_TYPE(retval) != IS_UNDEF) {
            if ((Z_TYPE(retval) == IS_LONG && Z_LVAL(retval) == 0) ||
                (Z_TYPE(retval) == IS_FALSE)) {
                abort = 1;
            }
        }
        zval_ptr_dtor(&retval);
        return abort ? 0 : 1;
    }

    zval_ptr_dtor(&retval);
    return 1;
}
/* }}} */

/** {{{ gene_router_exec_error_direct
 * Directly execute a "Class@method" error handler via C API, avoiding eval.
 * Returns 1 on success, 0 on failure.
 */
static int gene_router_exec_error_direct(const char *class_method) {
    const char *at;
    char cls_buf[256], mth_buf[128];
    char *class_name, *method;
    int cls_heap = 0, mth_heap = 0;
    zval classObject, retval;
    size_t class_name_len, method_len;

    if (!class_method || class_method[0] == '\0') return 0;

    at = memchr(class_method, '@', strlen(class_method));
    if (!at || at == class_method || at[1] == '\0') return 0;

    class_name_len = at - class_method;
    method_len = strlen(at + 1);

    if (class_name_len < sizeof(cls_buf)) {
        memcpy(cls_buf, class_method, class_name_len);
        cls_buf[class_name_len] = '\0';
        class_name = cls_buf;
    } else {
        class_name = estrndup(class_method, class_name_len);
        cls_heap = 1;
    }
    if (method_len < sizeof(mth_buf)) {
        memcpy(mth_buf, at + 1, method_len + 1);
        method = mth_buf;
    } else {
        method = estrndup(at + 1, method_len);
        mth_heap = 1;
    }

    if (!gene_factory(class_name, class_name_len, NULL, &classObject)) {
        if (cls_heap) efree(class_name);
        if (mth_heap) efree(method);
        return 0;
    }

    gene_strtolower(method);

    if (Z_TYPE(classObject) == IS_OBJECT
            && zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), method, method_len)) {
        ZVAL_NULL(&retval);
        gene_factory_call(&classObject, method, method_len, NULL, &retval);
        zval_ptr_dtor(&retval);

        zval_ptr_dtor(&classObject);
        if (cls_heap) efree(class_name);
        if (mth_heap) efree(method);
        return 1;
    }
	 zval_ptr_dtor(&classObject);
	 if (cls_heap) efree(class_name);
	 if (mth_heap) efree(method);
	 return 0;
 }
 /* }}} */

/* {{{ gene_fn_cache_store — store closure in fn_cache, set fid_zv to the ID string
 * [GENE_PERF:2026-04-23] Stable key by closure object handle instead of
 * monotonic fn_cache_id. Re-registering the same closure hits the same key
 * so fn_cache size is bounded by the number of distinct live closures,
 * eliminating the previous unbounded-growth risk in long-running (Swoole)
 * workers. Because the hash table holds a strong ref to the closure, its
 * object handle cannot be reused while cached, so the key is collision-free. */
static void gene_fn_cache_store(zval *closure, zval *fid_zv) {
	 char fid[32];
	 size_t fid_len;
	 zval *existing;
	 uint32_t handle;
	 if (!GENE_G(fn_cache)) {
		 ALLOC_HASHTABLE(GENE_G(fn_cache));
		 zend_hash_init(GENE_G(fn_cache), 16, NULL, ZVAL_PTR_DTOR, 0);
	 }
	 handle = (uint32_t)Z_OBJ_P(closure)->handle;
	 fid_len = snprintf(fid, sizeof(fid), "fn_%u", handle);
	 existing = zend_hash_str_find(GENE_G(fn_cache), fid, fid_len);
	 if (existing == NULL) {
		 Z_TRY_ADDREF_P(closure);
		 zend_hash_str_add_new(GENE_G(fn_cache), fid, fid_len, closure);
	 }
	 /* else: same closure already cached; skip refcount churn + replace */
	 ZVAL_STRINGL(fid_zv, fid, fid_len);
}
 /* }}} */

 /* {{{ gene_router_exec_closure_hook — execute closure as hook, returns 1=continue, 0=abort */
 static int gene_router_exec_closure_hook(zval *closure, zval *param, int is_before) {
	 zval retval;
	 zend_function *func = NULL;
	 zend_object *this_obj = NULL;
	 zend_class_entry *called_scope = NULL;
	 ZVAL_UNDEF(&retval);
	 if (UNEXPECTED(!Z_OBJ_HANDLER_P(closure, get_closure)
		 || Z_OBJ_HANDLER_P(closure, get_closure)(Z_OBJ_P(closure), &called_scope, &func, &this_obj, 0) != SUCCESS)) {
		 return 1;
	 }
	 zend_try {
		 zend_call_known_function(func, this_obj, called_scope, &retval,
			 param ? 1 : 0, param, NULL);
	 } zend_catch {
	 } zend_end_try();
	 if (is_before) {
		 int abort = 0;
		 if (Z_TYPE(retval) != IS_NULL && Z_TYPE(retval) != IS_UNDEF) {
			 if ((Z_TYPE(retval) == IS_LONG && Z_LVAL(retval) == 0) ||
				 (Z_TYPE(retval) == IS_FALSE)) {
				 abort = 1;
			 }
		 }
		 zval_ptr_dtor(&retval);
		 return abort ? 0 : 1;
	 }
	 zval_ptr_dtor(&retval);
	 return 1;
 }
 /* }}} */

 /* {{{ gene_router_dispatch_closure — execute closure as route action with path_params */
 static int gene_router_dispatch_closure(zval *closure, zval *retval) {
	 zend_function *func = NULL;
	 zend_object *this_obj = NULL;
	 zend_class_entry *called_scope = NULL;
	 zval *params_zv;
	 ZVAL_NULL(retval);
	 if (UNEXPECTED(!Z_OBJ_HANDLER_P(closure, get_closure)
		 || Z_OBJ_HANDLER_P(closure, get_closure)(Z_OBJ_P(closure), &called_scope, &func, &this_obj, 0) != SUCCESS)) {
		 return 0;
	 }
	 params_zv = &gene_request_ctx()->path_params;
	 zend_try {
		 zend_call_known_function(func, this_obj, called_scope, retval, 1, params_zv, NULL);
	 } zend_catch {
	 } zend_end_try();
	 return 1;
 }
 /* }}} */
 
 /* {{{ P3 — precompiled route dispatch descriptor + cache.
  *
  * get_router_info_slow() resolves a route leaf into a dispatch plan on EVERY
  * request via 6–10 zend_hash_str_find()s against the leaf and the app-global
  * event/hook array, plus strtok/snprintf. After workerReady() in Swoole the
  * route tree and fn_cache are frozen and a given leaf HashTable* always maps to
  * the same route, so that resolution is a pure function of (leaf, cacheHook)
  * and can be memoized. We cache a gene_route_pc descriptor keyed by the leaf's
  * HashTable pointer in the per-thread GENE_G(route_pc); subsequent dispatches
  * execute the descriptor with zero hash lookups.
  *
  * Safety:
  *  - Swoole-only, post-workerReady only: in FPM the tree is rebuilt and
  *    fn_cache is per-request, so leaf pointers / closure pointers are not
  *    stable — the cache is never populated there (the wrapper falls through to
  *    the slow path).
  *  - Per-thread: GENE_G is per-thread under ZTS, and the descriptor borrows
  *    pointers from the same thread's GENE_G(cache)/fn_cache, so there is no
  *    cross-thread aliasing. Within a worker, Swoole coroutines are scheduled
  *    cooperatively and neither resolve nor execute yields, so the lazy
  *    populate-on-miss is race-free with respect to concurrent reads.
  *  - Borrowed pointers (src/before_src/.../route_cl/...) reference persistent
  *    route-tree strings and fn_cache zvals that are stable for the worker's
  *    lifetime. Only eval_str is owned (a persistent copy of the concatenated
  *    eval program) and is freed by the table dtor.
  *  - Opt-in: gated behind gene.route_precompile (default off) so the proven
  *    slow path is the only code that runs until an operator validates this.
  */
 enum { GENE_PC_DIRECT = 0, GENE_PC_CLOSURE = 1, GENE_PC_EVAL = 2 };

 typedef struct _gene_route_pc {
	 int kind;          /* GENE_PC_DIRECT | GENE_PC_CLOSURE | GENE_PC_EVAL */
	 int is_before;     /* run before-hook?  (hook "@clearBefore/clearAll" clears) */
	 int is_after;      /* run after-hook?   (hook "@clearAfter/clearAll" clears)  */
	 /* Borrowed, NUL-terminated persistent route-tree strings (NULL = absent). */
	 const char *src;        /* route action direct src (MCA dispatch program)   */
	 const char *before_src; /* before-hook direct src                            */
	 const char *after_src;  /* after-hook direct src                             */
	 const char *hook_src;   /* named-hook direct src                             */
	 /* Borrowed fn_cache closure zvals (NULL = absent). */
	 zval *route_cl;
	 zval *before_cl;
	 zval *after_cl;
	 zval *hook_cl;
	 /* Owned persistent copy of the eval program (GENE_PC_EVAL only). */
	 char *eval_str;
	 size_t eval_len;
 } gene_route_pc;

 static void gene_route_pc_dtor(zval *zv) {
	 gene_route_pc *pc = (gene_route_pc *)Z_PTR_P(zv);
	 if (pc) {
		 if (pc->eval_str) {
			 pefree(pc->eval_str, 1);
		 }
		 pefree(pc, 1);
	 }
 }

 void gene_router_pc_destroy(void) {
	 if (GENE_G(route_pc)) {
		 zend_hash_destroy(GENE_G(route_pc));
		 pefree(GENE_G(route_pc), 1);
		 GENE_G(route_pc) = NULL;
	 }
 }

 /* Resolve (leaf, cacheHook) into a descriptor. Mirrors get_router_info_slow()'s
  * resolution exactly but records the outcome instead of executing it. All
  * recorded char* point into persistent strings; eval_str is a persistent copy. */
 static void gene_route_pc_resolve(zval **leaf, zval **cacheHook, gene_route_pc *pc) {
	 zval *hname, *before, *after, *m, *h, *src;
	 zval *before_src = NULL, *after_src = NULL, *hook_src = NULL;
	 int is_before = 1, is_after = 1;
	 char hookname_buf[256];
	 char *hookname = NULL, *hookname_alloc = NULL, *seg = NULL, *ptr = NULL;
	 size_t seg_len = 0;
	 int use_direct = 1;

	 memset(pc, 0, sizeof(*pc));

	 src = zend_hash_str_find(Z_ARRVAL_P(*leaf), "src", 3);
	 if (!src || Z_TYPE_P(src) != IS_STRING || Z_STRLEN_P(src) == 0) {
		 use_direct = 0;
	 }

	 hname = zend_hash_str_find(Z_ARRVAL_P(*leaf), "hook", 4);
	 if (hname && Z_TYPE_P(hname) == IS_STRING && Z_STRLEN_P(hname) > 0) {
		 size_t hookname_len = sizeof("hook:") - 1 + Z_STRLEN_P(hname);
		 if (hookname_len < sizeof(hookname_buf)) {
			 memcpy(hookname_buf, "hook:", sizeof("hook:") - 1);
			 memcpy(hookname_buf + sizeof("hook:") - 1, Z_STRVAL_P(hname), Z_STRLEN_P(hname));
			 hookname_buf[hookname_len] = '\0';
			 hookname = hookname_buf;
		 } else {
			 hookname_alloc = emalloc(hookname_len + 1);
			 memcpy(hookname_alloc, "hook:", sizeof("hook:") - 1);
			 memcpy(hookname_alloc + sizeof("hook:") - 1, Z_STRVAL_P(hname), Z_STRLEN_P(hname));
			 hookname_alloc[hookname_len] = '\0';
			 hookname = hookname_alloc;
		 }
	 }
	 if (hookname) {
		 seg = php_strtok_r(hookname, "@", &ptr);
		 if (seg) {
			 seg_len = strlen(seg);
		 }
	 }
	 if (ptr && strlen(ptr) > 0) {
		 if ((strcmp(ptr, "clearBefore") == 0) || (strcmp(ptr, "clearAll") == 0)) {
			 is_before = 0;
		 }
		 if ((strcmp(ptr, "clearAfter") == 0) || (strcmp(ptr, "clearAll") == 0)) {
			 is_after = 0;
		 }
	 }

	 if (use_direct && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
		 if (is_before) {
			 before = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:before", 11);
			 if (before && Z_TYPE_P(before) == IS_STRING && Z_STRLEN_P(before) > 0) {
				 before_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:before", 11);
				 if (!before_src || Z_TYPE_P(before_src) != IS_STRING) use_direct = 0;
			 }
		 }
		 if (is_after) {
			 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
			 if (after && Z_TYPE_P(after) == IS_STRING && Z_STRLEN_P(after) > 0) {
				 after_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:after", 11);
				 if (!after_src || Z_TYPE_P(after_src) != IS_STRING) use_direct = 0;
			 }
		 }
		 if (seg && seg_len > 0) {
			 char hseg_buf[256];
			 int hseg_key_n = snprintf(hseg_buf, sizeof(hseg_buf), "hsrc%s", seg + 4);
			 size_t hseg_key_len = 0;
			 if (UNEXPECTED(hseg_key_n < 0 || (size_t)hseg_key_n >= sizeof(hseg_buf))) {
				 use_direct = 0;
			 } else {
				 hseg_key_len = (size_t)hseg_key_n;
				 hook_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), hseg_buf, hseg_key_len);
			 }
			 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, seg_len);
			 if (h && Z_TYPE_P(h) == IS_STRING && Z_STRLEN_P(h) > 0) {
				 if (!hook_src || Z_TYPE_P(hook_src) != IS_STRING) use_direct = 0;
			 }
		 }
	 }

	 if (use_direct) {
		 pc->kind = GENE_PC_DIRECT;
		 pc->is_before = is_before;
		 pc->is_after = is_after;
		 pc->src = Z_STRVAL_P(src);
		 pc->before_src = (before_src && Z_TYPE_P(before_src) == IS_STRING) ? Z_STRVAL_P(before_src) : NULL;
		 pc->after_src  = (after_src  && Z_TYPE_P(after_src)  == IS_STRING) ? Z_STRVAL_P(after_src)  : NULL;
		 pc->hook_src   = (hook_src   && Z_TYPE_P(hook_src)   == IS_STRING) ? Z_STRVAL_P(hook_src)   : NULL;
		 if (hookname_alloc) efree(hookname_alloc);
		 return;
	 }

	 if (GENE_G(fn_cache)) {
		 zval *frun = zend_hash_str_find(Z_ARRVAL_P(*leaf), "frun", 4);
		 zval *route_cl = NULL, *before_cl = NULL, *after_cl = NULL, *hook_cl = NULL;
		 int use_closure = 1;

		 if (frun && Z_TYPE_P(frun) == IS_STRING) {
			 route_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(frun), Z_STRLEN_P(frun));
		 }
		 if (!route_cl && (!src || Z_TYPE_P(src) != IS_STRING || Z_STRLEN_P(src) == 0)) {
			 use_closure = 0;
		 }

		 if (use_closure && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 if (is_before) {
				 before = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:before", 11);
				 if (before && Z_TYPE_P(before) == IS_STRING && Z_STRLEN_P(before) > 0) {
					 before_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:before", 11);
					 if (!before_src || Z_TYPE_P(before_src) != IS_STRING) {
						 zval *bfcl = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "fcl:before", 10);
						 if (bfcl && Z_TYPE_P(bfcl) == IS_STRING) {
							 before_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(bfcl), Z_STRLEN_P(bfcl));
							 if (!before_cl) use_closure = 0;
						 } else { use_closure = 0; }
						 before_src = NULL;
					 }
				 }
			 }
			 if (is_after) {
				 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
				 if (after && Z_TYPE_P(after) == IS_STRING && Z_STRLEN_P(after) > 0) {
					 after_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:after", 11);
					 if (!after_src || Z_TYPE_P(after_src) != IS_STRING) {
						 zval *afcl = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "fcl:after", 9);
						 if (afcl && Z_TYPE_P(afcl) == IS_STRING) {
							 after_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(afcl), Z_STRLEN_P(afcl));
							 if (!after_cl) use_closure = 0;
						 } else { use_closure = 0; }
						 after_src = NULL;
					 }
				 }
			 }
			 if (seg && seg_len > 0) {
				 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, seg_len);
				 if (h && Z_TYPE_P(h) == IS_STRING && Z_STRLEN_P(h) > 0 && !hook_src) {
					 char fcl_buf[256];
					 int fcl_n = snprintf(fcl_buf, sizeof(fcl_buf), "fcl%s", seg + 4);
					 zval *hfcl = NULL;
					 if (UNEXPECTED(fcl_n < 0 || (size_t)fcl_n >= sizeof(fcl_buf))) {
						 use_closure = 0;
					 } else {
						 hfcl = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), fcl_buf, (size_t)fcl_n);
					 }
					 if (hfcl && Z_TYPE_P(hfcl) == IS_STRING) {
						 hook_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(hfcl), Z_STRLEN_P(hfcl));
						 if (!hook_cl) use_closure = 0;
					 } else { use_closure = 0; }
				 }
			 }
		 }

		 if (use_closure) {
			 pc->kind = GENE_PC_CLOSURE;
			 pc->is_before = is_before;
			 pc->is_after = is_after;
			 pc->route_cl = route_cl;
			 pc->src = (src && Z_TYPE_P(src) == IS_STRING && Z_STRLEN_P(src) > 0) ? Z_STRVAL_P(src) : NULL;
			 pc->before_cl  = before_cl;
			 pc->before_src = (before_src && Z_TYPE_P(before_src) == IS_STRING) ? Z_STRVAL_P(before_src) : NULL;
			 pc->after_cl   = after_cl;
			 pc->after_src  = (after_src && Z_TYPE_P(after_src) == IS_STRING) ? Z_STRVAL_P(after_src) : NULL;
			 pc->hook_cl    = hook_cl;
			 pc->hook_src   = (hook_src && Z_TYPE_P(hook_src) == IS_STRING) ? Z_STRVAL_P(hook_src) : NULL;
			 if (hookname_alloc) efree(hookname_alloc);
			 return;
		 }
	 }

	 /* EVAL fallback: precompute the concatenated eval program once. */
	 pc->kind = GENE_PC_EVAL;
	 pc->is_before = is_before;
	 pc->is_after = is_after;
	 {
		 smart_str buf = {0};
		 if (is_before && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 before = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:before", 11);
			 if (before && Z_TYPE_P(before) == IS_STRING && Z_STRLEN_P(before) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(before), Z_STRLEN_P(before));
			 }
		 }
		 if (seg && seg_len > 0 && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, seg_len);
			 if (h && Z_TYPE_P(h) == IS_STRING && Z_STRLEN_P(h) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(h), Z_STRLEN_P(h));
			 }
		 }
		 m = zend_hash_str_find(Z_ARRVAL_P(*leaf), "run", 3);
		 if (m && Z_TYPE_P(m) == IS_STRING && Z_STRLEN_P(m) > 0) {
			 smart_str_appendl(&buf, Z_STRVAL_P(m), Z_STRLEN_P(m));
		 }
		 if (is_after && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
			 if (after && Z_TYPE_P(after) == IS_STRING && Z_STRLEN_P(after) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(after), Z_STRLEN_P(after));
			 }
		 }
		 if (buf.s && ZSTR_LEN(buf.s) > 0) {
			 smart_str_0(&buf);
			 pc->eval_len = ZSTR_LEN(buf.s);
			 pc->eval_str = (char *)pemalloc(pc->eval_len + 1, 1);
			 memcpy(pc->eval_str, ZSTR_VAL(buf.s), pc->eval_len);
			 pc->eval_str[pc->eval_len] = '\0';
		 }
		 smart_str_free(&buf);
	 }
	 if (hookname_alloc) efree(hookname_alloc);
 }

 /* Execute a precompiled descriptor. Behaviourally identical to the matching
  * branch of get_router_info_slow(). */
 static int gene_route_pc_execute(const gene_route_pc *pc) {
	 zval dispatch_result;

	 if (pc->kind == GENE_PC_EVAL) {
		 if (pc->eval_str && pc->eval_len > 0) {
			 zend_try {
				 zend_eval_stringl(pc->eval_str, pc->eval_len, NULL, "");
			 } zend_catch {
			 } zend_end_try();
		 }
		 return 1;
	 }

	 ZVAL_NULL(&dispatch_result);

	 if (pc->kind == GENE_PC_DIRECT) {
		 if (pc->is_before && pc->before_src) {
			 if (!gene_router_exec_hook_direct((char *)pc->before_src, NULL, 1)) goto pc_cleanup;
		 }
		 if (pc->hook_src) {
			 if (!gene_router_exec_hook_direct((char *)pc->hook_src, NULL, 1)) goto pc_cleanup;
		 }
		 if (pc->src) {
			 gene_router_dispatch_direct((char *)pc->src, &dispatch_result);
		 }
		 if (pc->is_after && pc->after_src) {
			 gene_router_exec_hook_direct((char *)pc->after_src, &dispatch_result, 0);
		 }
	 } else { /* GENE_PC_CLOSURE */
		 if (pc->is_before) {
			 if (pc->before_cl) {
				 if (!gene_router_exec_closure_hook(pc->before_cl, NULL, 1)) goto pc_cleanup;
			 } else if (pc->before_src) {
				 if (!gene_router_exec_hook_direct((char *)pc->before_src, NULL, 1)) goto pc_cleanup;
			 }
		 }
		 if (pc->hook_cl) {
			 if (!gene_router_exec_closure_hook(pc->hook_cl, NULL, 1)) goto pc_cleanup;
		 } else if (pc->hook_src) {
			 if (!gene_router_exec_hook_direct((char *)pc->hook_src, NULL, 1)) goto pc_cleanup;
		 }
		 if (pc->route_cl) {
			 gene_router_dispatch_closure(pc->route_cl, &dispatch_result);
		 } else if (pc->src) {
			 gene_router_dispatch_direct((char *)pc->src, &dispatch_result);
		 }
		 if (pc->is_after) {
			 if (pc->after_cl) {
				 gene_router_exec_closure_hook(pc->after_cl, &dispatch_result, 0);
			 } else if (pc->after_src) {
				 gene_router_exec_hook_direct((char *)pc->after_src, &dispatch_result, 0);
			 }
		 }
	 }

 pc_cleanup:
	 zval_ptr_dtor(&dispatch_result);
	 return 1;
 }
 /* }}} */

 /** {{{ int get_router_info_slow(zval **leaf, zval **cacheHook)
  * [GENE_PERF:2026-06-19 P3] Renamed from get_router_info(). This is the proven
  * per-request resolve+dispatch path and is left byte-for-byte unchanged; it is
  * the fallback whenever the precompiled-dispatch cache is disabled (default),
  * not in Swoole, or before workerReady(). The new get_router_info() wrapper
  * (below) routes to the precompiled fast path when enabled.
  */
 int get_router_info_slow(zval **leaf, zval **cacheHook) {
	 zval *hname, *before, *after, *m, *h, *src;
	 zval *before_src = NULL, *after_src = NULL, *hook_src = NULL;
	 int is_before = 1, is_after = 1;
	 char hookname_buf[256];
	 char *hookname = NULL, *hookname_alloc = NULL, *seg = NULL, *ptr = NULL;
	 size_t seg_len = 0;
	 int use_direct = 1;
 
	 gene_router_set_uri(leaf);
 
	 src = zend_hash_str_find(Z_ARRVAL_P(*leaf), "src", 3);
	 if (!src || Z_TYPE_P(src) != IS_STRING || Z_STRLEN_P(src) == 0) {
		 use_direct = 0;
	 }
 
	 hname = zend_hash_str_find(Z_ARRVAL_P(*leaf), "hook", 4);
	 if (hname && Z_TYPE_P(hname) == IS_STRING && Z_STRLEN_P(hname) > 0) {
		 size_t hookname_len = sizeof("hook:") - 1 + Z_STRLEN_P(hname);
		 if (hookname_len < sizeof(hookname_buf)) {
			 memcpy(hookname_buf, "hook:", sizeof("hook:") - 1);
			 memcpy(hookname_buf + sizeof("hook:") - 1, Z_STRVAL_P(hname), Z_STRLEN_P(hname));
			 hookname_buf[hookname_len] = '\0';
			 hookname = hookname_buf;
		 } else {
			 hookname_alloc = emalloc(hookname_len + 1);
			 memcpy(hookname_alloc, "hook:", sizeof("hook:") - 1);
			 memcpy(hookname_alloc + sizeof("hook:") - 1, Z_STRVAL_P(hname), Z_STRLEN_P(hname));
			 hookname_alloc[hookname_len] = '\0';
			 hookname = hookname_alloc;
		 }
	 }
	 if (hookname) {
		 /* [GENE_PERF:2026-05-29] Cache seg length once. seg is produced by
		  * php_strtok_r and never mutates for the rest of this function, yet
		  * the prior code re-ran strlen(seg) up to 5 times across the direct,
		  * closure and eval paths. One scan now, reused everywhere. */
		 seg = php_strtok_r(hookname, "@", &ptr);
		 if (seg) {
			 seg_len = strlen(seg);
		 }
	 }
 
	 if (ptr && strlen(ptr) > 0) {
		 if ((strcmp(ptr, "clearBefore") == 0) || (strcmp(ptr, "clearAll") == 0)) {
			 is_before = 0;
		 }
		 if ((strcmp(ptr, "clearAfter") == 0) || (strcmp(ptr, "clearAll") == 0)) {
			 is_after = 0;
		 }
	 }
 
	 if (use_direct && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
		 if (is_before) {
			 before = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:before", 11);
			 if (before && Z_TYPE_P(before) == IS_STRING && Z_STRLEN_P(before) > 0) {
				 before_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:before", 11);
				 if (!before_src || Z_TYPE_P(before_src) != IS_STRING) use_direct = 0;
			 }
		 }
		 if (is_after) {
			 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
			 if (after && Z_TYPE_P(after) == IS_STRING && Z_STRLEN_P(after) > 0) {
				 after_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:after", 11);
				 if (!after_src || Z_TYPE_P(after_src) != IS_STRING) use_direct = 0;
			 }
		 }
		 if (seg && seg_len > 0) {
			 char hseg_buf[256];
			 int hseg_key_n = snprintf(hseg_buf, sizeof(hseg_buf), "hsrc%s", seg + 4);
			 size_t hseg_key_len = 0;
			 if (UNEXPECTED(hseg_key_n < 0 || (size_t)hseg_key_n >= sizeof(hseg_buf))) {
				 use_direct = 0;
			 } else {
				 hseg_key_len = (size_t)hseg_key_n;
				 hook_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), hseg_buf, hseg_key_len);
			 }
			 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, seg_len);
			 if (h && Z_TYPE_P(h) == IS_STRING && Z_STRLEN_P(h) > 0) {
				 if (!hook_src || Z_TYPE_P(hook_src) != IS_STRING) use_direct = 0;
			 }
		 }
	 }
 
	 if (use_direct) {
		 /* === DIRECT DISPATCH PATH (no eval) === */
		 zval dispatch_result;
		 ZVAL_NULL(&dispatch_result);
 
		 if (is_before && before_src) {
			 if (!gene_router_exec_hook_direct(Z_STRVAL_P(before_src), NULL, 1)) {
				 goto direct_cleanup;
			 }
		 }
 
		 if (hook_src) {
			 if (!gene_router_exec_hook_direct(Z_STRVAL_P(hook_src), NULL, 1)) {
				 goto direct_cleanup;
			 }
		 }
 
		 gene_router_dispatch_direct(Z_STRVAL_P(src), &dispatch_result);
 
		 if (is_after && after_src) {
			 gene_router_exec_hook_direct(Z_STRVAL_P(after_src), &dispatch_result, 0);
		 }
 
 direct_cleanup:
		 zval_ptr_dtor(&dispatch_result);
		 if (hookname_alloc) efree(hookname_alloc);
		 return 1;
	 }
 

	 /* === CLOSURE DISPATCH PATH (no eval) === */
	 if (!use_direct && GENE_G(fn_cache)) {
		 zval *frun = zend_hash_str_find(Z_ARRVAL_P(*leaf), "frun", 4);
		 zval *route_cl = NULL, *before_cl = NULL, *after_cl = NULL, *hook_cl = NULL;
		 int use_closure = 1;

		 /* Check route action: needs frun closure OR src for direct dispatch */
		 if (frun && Z_TYPE_P(frun) == IS_STRING) {
			 route_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(frun), Z_STRLEN_P(frun));
		 }
		 if (!route_cl && (!src || Z_TYPE_P(src) != IS_STRING || Z_STRLEN_P(src) == 0)) {
			 use_closure = 0;
		 }

		 /* Check hooks: each needs hsrc (direct) or fcl (closure) to avoid eval */
		 if (use_closure && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 if (is_before) {
				 before = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:before", 11);
				 if (before && Z_TYPE_P(before) == IS_STRING && Z_STRLEN_P(before) > 0) {
					 before_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:before", 11);
					 if (!before_src || Z_TYPE_P(before_src) != IS_STRING) {
						 zval *bfcl = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "fcl:before", 10);
						 if (bfcl && Z_TYPE_P(bfcl) == IS_STRING) {
							 before_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(bfcl), Z_STRLEN_P(bfcl));
							 if (!before_cl) use_closure = 0;
						 } else { use_closure = 0; }
						 before_src = NULL;
					 }
				 }
			 }
			 if (is_after) {
				 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
				 if (after && Z_TYPE_P(after) == IS_STRING && Z_STRLEN_P(after) > 0) {
					 after_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:after", 11);
					 if (!after_src || Z_TYPE_P(after_src) != IS_STRING) {
						 zval *afcl = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "fcl:after", 9);
						 if (afcl && Z_TYPE_P(afcl) == IS_STRING) {
							 after_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(afcl), Z_STRLEN_P(afcl));
							 if (!after_cl) use_closure = 0;
						 } else { use_closure = 0; }
						 after_src = NULL;
					 }
				 }
			 }
			 if (seg && seg_len > 0) {
				 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, seg_len);
				 if (h && Z_TYPE_P(h) == IS_STRING && Z_STRLEN_P(h) > 0 && !hook_src) {
					 char fcl_buf[256];
					 int fcl_n = snprintf(fcl_buf, sizeof(fcl_buf), "fcl%s", seg + 4);
					 zval *hfcl = NULL;
					 if (UNEXPECTED(fcl_n < 0 || (size_t)fcl_n >= sizeof(fcl_buf))) {
						 use_closure = 0;
					 } else {
						 hfcl = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), fcl_buf, (size_t)fcl_n);
					 }
					 if (hfcl && Z_TYPE_P(hfcl) == IS_STRING) {
						 hook_cl = zend_hash_str_find(GENE_G(fn_cache), Z_STRVAL_P(hfcl), Z_STRLEN_P(hfcl));
						 if (!hook_cl) use_closure = 0;
					 } else { use_closure = 0; }
				 }
			 }
		 }

		 if (use_closure) {
			 zval dispatch_result;
			 ZVAL_NULL(&dispatch_result);

			 /* Before hook */
			 if (is_before) {
				 if (before_cl) {
					 if (!gene_router_exec_closure_hook(before_cl, NULL, 1)) goto closure_cleanup;
				 } else if (before_src) {
					 if (!gene_router_exec_hook_direct(Z_STRVAL_P(before_src), NULL, 1)) goto closure_cleanup;
				 }
			 }

			 /* Named hook */
			 if (hook_cl) {
				 if (!gene_router_exec_closure_hook(hook_cl, NULL, 1)) goto closure_cleanup;
			 } else if (hook_src) {
				 if (!gene_router_exec_hook_direct(Z_STRVAL_P(hook_src), NULL, 1)) goto closure_cleanup;
			 }

			 /* Route action */
			 if (route_cl) {
				 gene_router_dispatch_closure(route_cl, &dispatch_result);
			 } else if (src && Z_TYPE_P(src) == IS_STRING && Z_STRLEN_P(src) > 0) {
				 gene_router_dispatch_direct(Z_STRVAL_P(src), &dispatch_result);
			 }

			 /* After hook */
			 if (is_after) {
				 if (after_cl) {
					 gene_router_exec_closure_hook(after_cl, &dispatch_result, 0);
				 } else if (after_src) {
					 gene_router_exec_hook_direct(Z_STRVAL_P(after_src), &dispatch_result, 0);
				 }
			 }

		 closure_cleanup:
			 zval_ptr_dtor(&dispatch_result);
			 if (hookname_alloc) efree(hookname_alloc);
			 return 1;
		 }
	 }
	 /* === EVAL FALLBACK PATH (closures / legacy routes) === */
	 {
		 smart_str buf = {0};
 
		 if (is_before && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 before = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:before", 11);
			 if (before && Z_TYPE_P(before) == IS_STRING && Z_STRLEN_P(before) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(before), Z_STRLEN_P(before));
			 }
		 }
		 if (seg && seg_len > 0 && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, seg_len);
			 if (h && Z_TYPE_P(h) == IS_STRING && Z_STRLEN_P(h) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(h), Z_STRLEN_P(h));
			 }
		 }
		 m = zend_hash_str_find(Z_ARRVAL_P(*leaf), "run", 3);
		 if (m && Z_TYPE_P(m) == IS_STRING && Z_STRLEN_P(m) > 0) {
			 smart_str_appendl(&buf, Z_STRVAL_P(m), Z_STRLEN_P(m));
		 }
		 if (is_after && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
			 if (after && Z_TYPE_P(after) == IS_STRING && Z_STRLEN_P(after) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(after), Z_STRLEN_P(after));
			 }
		 }
 
		 if (buf.s && ZSTR_LEN(buf.s) > 0) {
			 smart_str_0(&buf);
			 zend_try {
				 zend_eval_stringl(ZSTR_VAL(buf.s), ZSTR_LEN(buf.s), NULL, "");
			 } zend_catch {
			 } zend_end_try();
		 }
		 smart_str_free(&buf);
	 }
 
	 if (hookname_alloc) efree(hookname_alloc);
	 return 1;
 }
 /* }}} */
 
 /** {{{ int get_router_info(zval **leaf, zval **cacheHook)
  * [GENE_PERF:2026-06-19 P3] Dispatch entry. When the precompiled cache is
  * enabled (Swoole + workerReady + gene.route_precompile=1) it resolves each
  * leaf once and replays the descriptor on subsequent hits, eliminating the
  * per-request hash lookups. Otherwise it forwards to the unchanged slow path.
  */
 int get_router_info(zval **leaf, zval **cacheHook) {
	 gene_route_pc *pc;
	 zend_ulong key;

	 if (!(GENE_G(route_precompile)
			 && GENE_G(runtime_type) >= 2
			 && GENE_G(worker_ready)
			 && leaf && *leaf && Z_TYPE_P(*leaf) == IS_ARRAY)) {
		 return get_router_info_slow(leaf, cacheHook);
	 }

	 /* set_uri() populates request-scoped ctx->router_path and must run every
	  * request regardless of the descriptor cache (the slow path runs it too,
	  * as its first statement). */
	 gene_router_set_uri(leaf);

	 if (UNEXPECTED(!GENE_G(route_pc))) {
		 PALLOC_HASHTABLE(GENE_G(route_pc));
		 zend_hash_init(GENE_G(route_pc), 16, NULL, gene_route_pc_dtor, 1);
	 }

	 key = (zend_ulong)(uintptr_t)Z_ARRVAL_P(*leaf);
	 pc = (gene_route_pc *)zend_hash_index_find_ptr(GENE_G(route_pc), key);
	 if (!pc) {
		 pc = (gene_route_pc *)pemalloc(sizeof(gene_route_pc), 1);
		 gene_route_pc_resolve(leaf, cacheHook, pc);
		 zend_hash_index_add_new_ptr(GENE_G(route_pc), key, pc);
	 }
	 return gene_route_pc_execute(pc);
 }
 /* }}} */

 /** {{{ static void get_router_info(char *keyString, int keyString_len)
  */
 int get_router_error_run_by_router(zval *cacheHook, char *errorName) {
	 zval *error = NULL, *error_src = NULL;
	 size_t router_e_len, size;
	 char *run = NULL;
	 char err_buf[128], hsrc_buf[128];
	 char *err_key = err_buf, *hsrc_key = hsrc_buf, *fcl_key = NULL;
	 int err_heap = 0, hsrc_heap = 0, fcl_heap = 0;
	 size_t errorName_len = strlen(errorName);
	 size_t hsrc_key_len;
	 if (cacheHook) {
		 router_e_len = sizeof("error:") - 1 + errorName_len;
		 if (router_e_len >= sizeof(err_buf)) {
			 err_key = emalloc(router_e_len + 1);
			 err_heap = 1;
		 }
		 memcpy(err_key, "error:", sizeof("error:") - 1);
		 memcpy(err_key + sizeof("error:") - 1, errorName, errorName_len + 1);
		 error = zend_hash_str_find(Z_ARRVAL_P(cacheHook), err_key, router_e_len);
		 if (error) {
			 /* Try direct dispatch first via hsrc: key */
			 hsrc_key_len = sizeof("hsrc:") - 1 + errorName_len;
			 if (hsrc_key_len >= sizeof(hsrc_buf)) {
				 hsrc_key = emalloc(hsrc_key_len + 1);
				 hsrc_heap = 1;
			 }
			 memcpy(hsrc_key, "hsrc:", sizeof("hsrc:") - 1);
			 memcpy(hsrc_key + sizeof("hsrc:") - 1, errorName, errorName_len + 1);
			 error_src = zend_hash_str_find(Z_ARRVAL_P(cacheHook), hsrc_key, hsrc_key_len);
			 if (error_src && Z_TYPE_P(error_src) == IS_STRING && Z_STRLEN_P(error_src) > 0) {
				 if (gene_router_exec_error_direct(Z_STRVAL_P(error_src))) {
					 if (hsrc_heap) efree(hsrc_key);
					 if (err_heap) efree(err_key);
					 return 1;
				 }
			 }
			 /* Try closure dispatch via fcl: key */
			 if (GENE_G(fn_cache)) {
				 char fcl_buf[128];
				 size_t fcl_len = sizeof("fcl:") - 1 + errorName_len;
				 zval *efcl;
				 fcl_key = fcl_buf;
				 if (fcl_len >= sizeof(fcl_buf)) {
					 fcl_key = emalloc(fcl_len + 1);
					 fcl_heap = 1;
				 }
				 memcpy(fcl_key, "fcl:", sizeof("fcl:") - 1);
				 memcpy(fcl_key + sizeof("fcl:") - 1, errorName, errorName_len + 1);
				 efcl = zend_hash_str_find(Z_ARRVAL_P(cacheHook), fcl_key, fcl_len);
				 if (efcl && Z_TYPE_P(efcl) == IS_STRING) {
					 zval *ecl = zend_hash_find(GENE_G(fn_cache), Z_STR_P(efcl));
					 if (ecl) {
						 gene_router_exec_closure_hook(ecl, NULL, 0);
						 if (fcl_heap) efree(fcl_key);
						 if (hsrc_heap) efree(hsrc_key);
						 if (err_heap) efree(err_key);
						 return 1;
					 }
				 }
				 if (fcl_heap) {
					 efree(fcl_key);
					 fcl_heap = 0;
				 }
			 }
			 /* Fallback to eval */
			 size = Z_STRLEN_P(error);
			 run = estrndup(Z_STRVAL_P(error), size);

			 zend_try {
				 zend_eval_stringl(run, size, NULL, errorName);
			 } zend_catch {
			 } zend_end_try();

			 efree(run);
			 if (hsrc_heap) efree(hsrc_key);
			 if (err_heap) efree(err_key);
			 return 1;
		 }
		 if (err_heap) efree(err_key);
		 return 0;
	 }
	 return 0;
 }
 /* }}} */

 /** {{{ int get_router_error_run(char *errorName, const char *safe_str, size_t safe_len)
  */
 int get_router_error_run(char *errorName, const char *safe_str, size_t safe_len) {
	 zval *cacheHook = NULL, *error = NULL, *error_src = NULL;
	 size_t router_e_len, size;
	 char *run = NULL;
	 char router_e_buf[256];
	 char err_key_buf[128];
	 char hsrc_buf[128];
	 char *router_e_key = router_e_buf, *err_key = err_key_buf, *hsrc_key = hsrc_buf, *fcl_key = NULL;
	 int router_e_heap = 0, err_heap = 0, hsrc_heap = 0, fcl_heap = 0;
	 size_t errorName_len = strlen(errorName);
	 size_t hsrc_key_len;
	 if (safe_str != NULL && safe_len > 0) {
		 router_e_len = safe_len + sizeof(GENE_ROUTER_ROUTER_EVENT) - 1;
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e_key = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 }
		 memcpy(router_e_key, safe_str, safe_len);
		 memcpy(router_e_key + safe_len, GENE_ROUTER_ROUTER_EVENT, sizeof(GENE_ROUTER_ROUTER_EVENT));
	 } else {
		 router_e_len = sizeof(GENE_ROUTER_ROUTER_EVENT) - 1;
		 memcpy(router_e_key, GENE_ROUTER_ROUTER_EVENT, sizeof(GENE_ROUTER_ROUTER_EVENT));
	 }
	 cacheHook = gene_memory_get_quick(router_e_key, router_e_len);
	 if (cacheHook) {
		 router_e_len = sizeof("error:") - 1 + errorName_len;
		 if (router_e_len >= sizeof(err_key_buf)) {
			 err_key = emalloc(router_e_len + 1);
			 err_heap = 1;
		 }
		 memcpy(err_key, "error:", sizeof("error:") - 1);
		 memcpy(err_key + sizeof("error:") - 1, errorName, errorName_len + 1);
		 error = zend_hash_str_find(Z_ARRVAL_P(cacheHook), err_key, router_e_len);
		 if (error) {
			 /* Try direct dispatch first via hsrc: key */
			 hsrc_key_len = sizeof("hsrc:") - 1 + errorName_len;
			 if (hsrc_key_len >= sizeof(hsrc_buf)) {
				 hsrc_key = emalloc(hsrc_key_len + 1);
				 hsrc_heap = 1;
			 }
			 memcpy(hsrc_key, "hsrc:", sizeof("hsrc:") - 1);
			 memcpy(hsrc_key + sizeof("hsrc:") - 1, errorName, errorName_len + 1);
			 error_src = zend_hash_str_find(Z_ARRVAL_P(cacheHook), hsrc_key, hsrc_key_len);
			 if (error_src && Z_TYPE_P(error_src) == IS_STRING && Z_STRLEN_P(error_src) > 0) {
				 if (gene_router_exec_error_direct(Z_STRVAL_P(error_src))) {
					 if (hsrc_heap) efree(hsrc_key);
					 if (err_heap) efree(err_key);
					 if (router_e_heap) efree(router_e_key);
					 return 1;
				 }
			 }
			 /* Try closure dispatch via fcl: key */
			 if (GENE_G(fn_cache)) {
				 char fcl_buf[128];
				 size_t fcl_len = sizeof("fcl:") - 1 + errorName_len;
				 zval *efcl;
				 fcl_key = fcl_buf;
				 if (fcl_len >= sizeof(fcl_buf)) {
					 fcl_key = emalloc(fcl_len + 1);
					 fcl_heap = 1;
				 }
				 memcpy(fcl_key, "fcl:", sizeof("fcl:") - 1);
				 memcpy(fcl_key + sizeof("fcl:") - 1, errorName, errorName_len + 1);
				 efcl = zend_hash_str_find(Z_ARRVAL_P(cacheHook), fcl_key, fcl_len);
				 if (efcl && Z_TYPE_P(efcl) == IS_STRING) {
					 zval *ecl = zend_hash_find(GENE_G(fn_cache), Z_STR_P(efcl));
					 if (ecl) {
						 gene_router_exec_closure_hook(ecl, NULL, 0);
						 if (fcl_heap) efree(fcl_key);
						 if (hsrc_heap) efree(hsrc_key);
						 if (err_heap) efree(err_key);
						 if (router_e_heap) efree(router_e_key);
						 return 1;
					 }
				 }
				 if (fcl_heap) {
					 efree(fcl_key);
					 fcl_heap = 0;
				 }
			 }
			 /* Fallback to eval */
			 size = Z_STRLEN_P(error);
			 run = estrndup(Z_STRVAL_P(error), size);
		 } else {
			 php_error_docref(NULL, E_WARNING, "Gene Unknown Error:%s", errorName);
			 if (err_heap) efree(err_key);
			 if (router_e_heap) efree(router_e_key);
			 return 0;
		 }
	 } else {
		 if (router_e_heap) efree(router_e_key);
		 return 0;
	 }

	 /* [GENE_AUDIT:2026-07-03 P2] Defensive guard: run is only assigned in the
	  * eval-fallback branch above; guard use/free so a future code insertion in
	  * this control flow cannot deref/efree a NULL/uninitialized pointer. */
	 if (run) {
		 zend_try {
			 zend_eval_stringl(run, size, NULL, "");
		 } zend_catch {
		 } zend_end_try();
		 efree(run);
	 }
	 if (hsrc_heap) efree(hsrc_key);
	 if (err_heap) efree(err_key);
	 if (router_e_heap) efree(router_e_key);
	 return 1;
 }
 /* }}} */

 /* [GENE_PERF:2026-06-18 P6] FPM closure-route source cache.
  * In FPM/CLI mode the router is rebuilt on every request, so each closure
  * route re-runs ReflectionFunction + SplFileObject (2 file IOs) just to
  * extract its source text. Cache the *processed* result keyed by
  * "file:start:end" and invalidate when the source mtime changes (same
  * semantics as opcache timestamp validation).
  *
  * Gated on runtime_type < 2: in Swoole the router registers once at worker
  * start, so there is no per-request cost to amortize and we avoid any
  * cross-coroutine sharing concerns. FPM workers are single-process /
  * one-request-at-a-time, so the persistent table is touched sequentially
  * and needs no locking.
  *
  * Each lookup returns a fresh emalloc copy so the caller's efree contract
  * is unchanged; the persistent master copies live for the worker-process
  * lifetime (like the route tree). */
typedef struct _gene_closure_src_node {
	zend_long mtime;
	char     *src;
	size_t    len;
} gene_closure_src_node;

static HashTable *gene_closure_src_cache = NULL;

static void gene_closure_src_node_dtor(zval *zv) {
	gene_closure_src_node *node = (gene_closure_src_node *)Z_PTR_P(zv);
	if (node) {
		if (node->src) pefree(node->src, 1);
		pefree(node, 1);
	}
}

static zend_long gene_router_file_mtime(const char *path) {
	zend_stat_t sb;
	if (!path || VCWD_STAT(path, &sb) == -1) {
		return 0;
	}
	return (zend_long)sb.st_mtime;
}

static gene_closure_src_node *gene_closure_src_cache_get(const char *key, size_t key_len) {
	if (!gene_closure_src_cache) return NULL;
	return (gene_closure_src_node *)zend_hash_str_find_ptr(gene_closure_src_cache, key, key_len);
}

/* [GENE_PERF:2026-06-19 P6] Release the persistent FPM closure-source cache at
 * MSHUTDOWN, matching the cleanup convention of the extension's other
 * persistent caches (route_pc/cache/fn_cache) and keeping valgrind/ASAN clean
 * (no "still reachable" at process exit). No-op when never allocated. */
void gene_closure_src_cache_destroy(void) {
	if (gene_closure_src_cache) {
		zend_hash_destroy(gene_closure_src_cache);
		pefree(gene_closure_src_cache, 1);
		gene_closure_src_cache = NULL;
	}
}

static void gene_closure_src_cache_put(const char *key, size_t key_len, zend_long mtime, const char *src, size_t len) {
	gene_closure_src_node *node;
	if (!gene_closure_src_cache) {
		PALLOC_HASHTABLE(gene_closure_src_cache);
		zend_hash_init(gene_closure_src_cache, 16, NULL, gene_closure_src_node_dtor, 1);
	}
	node = (gene_closure_src_node *)pemalloc(sizeof(gene_closure_src_node), 1);
	node->mtime = mtime;
	node->len = len;
	node->src = (char *)pemalloc(len + 1, 1);
	memcpy(node->src, src, len);
	node->src[len] = '\0';
	/* update_ptr dtors any stale entry for the same key before replacing. */
	zend_hash_str_update_ptr(gene_closure_src_cache, key, key_len, node);
}

 /** {{{ static void get_function_content(char *keyString, int keyString_len)
 */
char * get_function_content(zval *content) {
	zval objEx, ret, fileName, arg1;
	zend_long startline, endline;
	size_t size;
	char *result = NULL, *tmp = NULL;
	/* [GENE_FIX:2026-05-24] gene_lookup_class_str avoids the unsafe
	 * static zend_string* + zend_string_init_interned(...,1) pattern that
	 * dangles across requests under opcache.file_cache_only=1. */
	zend_class_entry *reflection_ptr = gene_lookup_class_str(ZEND_STRL("ReflectionFunction"));

	if (reflection_ptr == NULL) {
		php_error_docref(NULL, E_WARNING, "Unable to start ReflectionFunction");
		return NULL;
	}
	//get file info
	object_init_ex(&objEx, reflection_ptr);
	zend_call_method_with_1_params(gene_strip_obj(&objEx), NULL, NULL, "__construct", NULL,
			content);
	if (EG(exception)) {
		zval_ptr_dtor(&objEx);
		zend_clear_exception();
		return NULL;
	}
	zend_call_method_with_0_params(gene_strip_obj(&objEx), NULL, NULL, "getFileName",
			&fileName);
	if (Z_TYPE(fileName) != IS_STRING) {
		zval_ptr_dtor(&fileName);
		zval_ptr_dtor(&objEx);
		return NULL;
	}
	zend_call_method_with_0_params(gene_strip_obj(&objEx), NULL, NULL, "getStartLine", &ret);
	if (Z_TYPE(ret) != IS_LONG) {
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&fileName);
		zval_ptr_dtor(&objEx);
		return NULL;
	}
	startline = Z_LVAL(ret) - 1;
	zval_ptr_dtor(&ret);
	zend_call_method_with_0_params(gene_strip_obj(&objEx), NULL, NULL, "getEndLine", &ret);
	if (Z_TYPE(ret) != IS_LONG) {
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&fileName);
		zval_ptr_dtor(&objEx);
		return NULL;
	}
	endline = Z_LVAL(ret);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&objEx);

	if (startline < 0 || endline <= 0 || startline >= endline) {
		zval_ptr_dtor(&fileName);
		return NULL;
	}

	/* [GENE_PERF:2026-06-18 P6] Try the FPM source cache before touching the
	 * filesystem. These locals stay in scope until the function returns so the
	 * miss path can populate the cache with the final processed result. */
	char src_key[1024];
	int src_key_len = 0;
	zend_long src_mtime = 0;
	/* [GENE_PERF:2026-06-19 P6] The persistent source cache is a process-wide
	 * (non per-thread) static HashTable mutated with lock-free update/find. That
	 * is safe only when a single thread serves requests, i.e. NTS FPM. Under ZTS
	 * builds multiple worker threads would race on the shared table, so disable
	 * caching there and fall back to the per-request reflection+read path. */
#ifdef ZTS
	int src_cache_on = 0;
#else
	int src_cache_on = (GENE_G(runtime_type) < 2 && Z_TYPE(fileName) == IS_STRING);
#endif
	if (src_cache_on) {
		src_mtime = gene_router_file_mtime(Z_STRVAL(fileName));
		src_key_len = snprintf(src_key, sizeof(src_key),
				"%s:" ZEND_LONG_FMT ":" ZEND_LONG_FMT,
				Z_STRVAL(fileName), startline, endline);
		if (src_mtime == 0 || src_key_len <= 0 || src_key_len >= (int)sizeof(src_key)) {
			/* unstattable file or oversized key: skip caching entirely */
			src_cache_on = 0;
		} else {
			gene_closure_src_node *node = gene_closure_src_cache_get(src_key, (size_t)src_key_len);
			if (node && node->mtime == src_mtime) {
				zval_ptr_dtor(&fileName);
				return estrndup(node->src, node->len);
			}
		}
	}

	//get codestartline
	/* [GENE_FIX:2026-05-24] gene_lookup_class_str avoids the unsafe
	 * static zend_string* + zend_string_init_interned(...,1) pattern that
	 * dangles across requests under opcache.file_cache_only=1. */
	zend_class_entry *spl_file_ptr = gene_lookup_class_str(ZEND_STRL("SplFileObject"));
	if (spl_file_ptr == NULL) {
		zval_ptr_dtor(&fileName);
		php_error_docref(NULL, E_WARNING, "Unable to start SplFileObject");
		return NULL;
	}
	object_init_ex(&objEx, spl_file_ptr);

	zend_call_method_with_1_params(gene_strip_obj(&objEx), NULL, NULL, "__construct", NULL,
			&fileName);
	zval_ptr_dtor(&fileName);
	if (EG(exception)) {
		zval_ptr_dtor(&objEx);
		zend_clear_exception();
		return NULL;
	}

	ZVAL_LONG(&fileName, startline);
	zend_call_method_with_1_params(gene_strip_obj(&objEx), NULL, NULL, "seek", NULL, &fileName);
	zval_ptr_dtor(&fileName);

	{
		smart_str buf = {0};
		while (startline < endline) {
			zend_call_method_with_0_params(gene_strip_obj(&objEx), NULL, NULL, "current", &ret);
			if (Z_TYPE(ret) != IS_STRING) {
				zval_ptr_dtor(&ret);
				++startline;
				zend_call_method_with_0_params(gene_strip_obj(&objEx), NULL, NULL, "next", NULL);
				continue;
			}
			smart_str_appendl(&buf, Z_STRVAL(ret), Z_STRLEN(ret));
			zval_ptr_dtor(&ret);
			++startline;
			zend_call_method_with_0_params(gene_strip_obj(&objEx), NULL, NULL, "next", NULL);
		}
		zval_ptr_dtor(&objEx);
		if (buf.s) {
			smart_str_0(&buf);
			result = estrndup(ZSTR_VAL(buf.s), ZSTR_LEN(buf.s));
			smart_str_free(&buf);
		} else {
			return NULL;
		}
	}

	ZVAL_STRING(&fileName, "function");
	ZVAL_STRING(&arg1, result);
	zend_call_method_with_2_params(NULL, NULL, NULL, "strpos", &ret, &arg1,
			&fileName);
	if (Z_TYPE(ret) == IS_FALSE) {
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&fileName);
		zval_ptr_dtor(&arg1);
		efree(result);
		return NULL;
	}
	startline = Z_LVAL(ret);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&fileName);

	ZVAL_STRING(&fileName, "}");
	zend_call_method_with_2_params(NULL, NULL, NULL, "strrpos", &ret, &arg1,
			&fileName);
	if (Z_TYPE(ret) == IS_FALSE) {
		zval_ptr_dtor(&ret);
		zval_ptr_dtor(&fileName);
		zval_ptr_dtor(&arg1);
		efree(result);
		return NULL;
	}
	endline = Z_LVAL(ret);
	zval_ptr_dtor(&ret);
	zval_ptr_dtor(&fileName);
	zval_ptr_dtor(&arg1);

	if (endline < startline) {
		efree(result);
		return NULL;
	}

	tmp = ecalloc(endline - startline + 2, sizeof(char));
	mid(tmp, result, endline - startline + 1, startline);
	if (result != NULL) {
		efree(result);
		result = NULL;
	}
	remove_extra_space(tmp);
	/* [GENE_PERF:2026-06-18 P6] Populate the FPM source cache with the final
	 * processed text so subsequent requests skip the ReflectionFunction +
	 * SplFileObject file reads. */
	if (src_cache_on && tmp != NULL) {
		gene_closure_src_cache_put(src_key, (size_t)src_key_len, src_mtime, tmp, strlen(tmp));
	}
	return tmp;
}
/* }}} */

/** {{{ char * get_function_content_quik(zval **content)
 */
char *get_function_content_quik(zval **content) {

	return NULL;
}
/* }}} */

/** {{{ char get_router_content(char *content)
 */
char *get_router_content_F(char *src, char *method, char *path) {
	char *dist;
	if (src == NULL) {
		return NULL;
	}
	if (strcmp(method, "hook") == 0) {
		if (strcmp(path, "before") == 0) {
			spprintf(&dist, 0, GENE_ROUTER_CONTENT_FB, src);
		} else if (strcmp(path, "after") == 0) {
			spprintf(&dist, 0, GENE_ROUTER_CONTENT_FA, src);
		} else {
			spprintf(&dist, 0, GENE_ROUTER_CONTENT_FH, src);
		}
	} else {
		spprintf(&dist, 0, GENE_ROUTER_CONTENT_FM, src);
	}
	return dist;
}

/** {{{ char get_router_content(char *content)
 */
char* get_router_content(zval **content, char *method, char *path) {
	char *contents, *seg, *ptr, *tmp;
	contents = estrndup(Z_STRVAL_P(*content), Z_STRLEN_P(*content));
	seg = php_strtok_r(contents, "@", &ptr);
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
	efree(contents);
	return NULL;
}

/*
 *  {{{ void get_router_content_run(char *methodin, char *pathin, const char *safe_str, size_t safe_len)
 */
void get_router_content_run(char *methodin, char *pathin, const char *safe_str, size_t safe_len) {
	 /* [GENE_PERF:2026-04-19] Hot request dispatch path. Two optimizations:
	  *  1. When methodin==NULL (internal dispatch from Application::run), ctx->method is
	  *     already lowercased by gene_ini_router() — use it directly, no emalloc+copy.
	  *  2. Cache method_len once instead of calling strlen() at every hash lookup.
	  * When methodin!=NULL (explicit method in user Router::run), use a 32-byte stack
	  * buffer for the lowercased copy to skip heap alloc for typical HTTP verbs. */
	 const char *method = NULL;
	 char *path = NULL;
	 size_t method_len = 0;
	 int method_heap = 0;
	 char method_buf[32];
	 char *path_new = NULL;
	 size_t router_e_len;
	 zval *temp = NULL, *lead = NULL;
	 zval *conf = NULL, *cache = NULL, *cacheHook = NULL;
	 gene_request_context *ctx = gene_request_ctx();

	 if (methodin == NULL && pathin == NULL) {
		 if (ctx->method) {
			 method = ctx->method; /* already lowercased by gene_ini_router() */
			 /* [GENE_PERF:2026-04-20] Use cached method_len set by gene_ini_router()
			  * — avoids an strlen() scan of a 3-10 byte method string per request. */
			 method_len = ctx->method_len;
		 }
		 if (ctx->path) {
			 /* [GENE_PERF:2026-04-20] Inline emalloc+memcpy avoids str_init()'s
			  * internal strlen() since ctx->path_len is already cached by the
			  * gene_ini_router()/request_set_server_val() path populators. */
			 size_t plen = ctx->path_len;
			 path = (char *)emalloc(plen + 1);
			 memcpy(path, ctx->path, plen + 1);
		 }
	 } else {
		 size_t mlen = methodin ? strlen(methodin) : 0;
		 char *m;
		 if (mlen < sizeof(method_buf)) {
			 m = method_buf;
		 } else {
			 m = emalloc(mlen + 1);
			 method_heap = 1;
		 }
		 /* Inline lowercase copy: fuses str_init + gene_strtolower into a single pass. */
		 {
			 size_t i;
			 unsigned char c;
			 for (i = 0; i < mlen; i++) {
				 c = (unsigned char)methodin[i];
				 m[i] = (c >= 'A' && c <= 'Z') ? (char)(c | 0x20) : (char)c;
			 }
		 }
		 m[mlen] = '\0';
		 method = m;
		 method_len = mlen;
		 path = str_init(pathin);
		 {
			 char *q = strchr(path, '?');
			 if (q) {
				 *q = '\0';
				 gene_merge_query_into_get(q + 1, strlen(q + 1));
			 }
		 }
	 }

	 if (method == NULL || method_len == 0 || path == NULL) {
		 if (method_heap) efree((char *)method);
		 if (path) efree(path);
		 php_error_docref(NULL, E_WARNING, "Gene Unknown Method And Url: NULL");
		 return;
	 }

	 gene_router_reset_path_params();

	 /* [GENE_PERF:2026-04-19 #2] Build the router cache key ONCE with the safe
	  * prefix, then for each of the 3 consecutive lookups (tree/event/conf) just
	  * swap the 3-byte suffix via memcpy. Replaces 3 snprintf calls (~100+ cycles
	  * each, format-string parsing) with 3 x memcpy(3) operations (~3 cycles each).
	  * The three suffixes ":rt", ":re", ":cf" are all exactly 3 bytes.
	  * [GENE_PERF:2026-04-24 v5.5.8] All three keys now resolve under a SINGLE
	  * cache RDLOCK via gene_memory_get_triple() — prior versions took/released
	  * the rwlock three separate times. Under ZTS (or non-ZTS with workerReady
	  * not yet signalled) the contended-atomic cost of the dispatcher hot path
	  * drops from 3× to 1×. Under workerReady==1 non-ZTS the lock is already a
	  * no-op, so the merge is neutral there. */
	 {
		 size_t prefix_len;
		 char *rkey_tree, *rkey_event, *rkey_conf;
		 char rkey_tree_buf[256], rkey_event_buf[256], rkey_conf_buf[256];
		 int rkey_tree_heap = 0, rkey_event_heap = 0, rkey_conf_heap = 0;
		 size_t alloc_len;

		 if (safe_str != NULL && safe_len > 0) {
			 prefix_len = safe_len;
		 } else {
			 prefix_len = 0;
		 }
		 router_e_len = prefix_len + 3;
		 alloc_len = router_e_len + 1;

		 if (alloc_len > sizeof(rkey_tree_buf)) {
			 rkey_tree = emalloc(alloc_len);
			 rkey_tree_heap = 1;
		 } else {
			 rkey_tree = rkey_tree_buf;
		 }
		 if (alloc_len > sizeof(rkey_event_buf)) {
			 rkey_event = emalloc(alloc_len);
			 rkey_event_heap = 1;
		 } else {
			 rkey_event = rkey_event_buf;
		 }
		 if (alloc_len > sizeof(rkey_conf_buf)) {
			 rkey_conf = emalloc(alloc_len);
			 rkey_conf_heap = 1;
		 } else {
			 rkey_conf = rkey_conf_buf;
		 }

		 if (prefix_len > 0) {
			 memcpy(rkey_tree,  safe_str, prefix_len);
			 memcpy(rkey_event, safe_str, prefix_len);
			 memcpy(rkey_conf,  safe_str, prefix_len);
		 }
		 memcpy(rkey_tree  + prefix_len, GENE_ROUTER_ROUTER_TREE,  3);
		 memcpy(rkey_event + prefix_len, GENE_ROUTER_ROUTER_EVENT, 3);
		 memcpy(rkey_conf  + prefix_len, GENE_ROUTER_ROUTER_CONF,  3);
		 rkey_tree[router_e_len]  = '\0';
		 rkey_event[router_e_len] = '\0';
		 rkey_conf[router_e_len]  = '\0';

		 /* Single lock: tree/event/conf resolved in one rwlock span. */
		 gene_memory_get_triple(
			 rkey_tree,  router_e_len, &cache,
			 rkey_event, router_e_len, &cacheHook,
			 rkey_conf,  router_e_len, &conf);

		 if (cache) {
			 temp = zend_symtable_str_find(Z_ARRVAL_P(cache), (char *)method, method_len);
			 if (temp == NULL) {
				 php_error_docref(NULL, E_WARNING, "Gene Unknown Method Cache:%s", method);
				 if (method_heap) efree((char *)method);
				 efree(path);
				 if (rkey_tree_heap)  efree(rkey_tree);
				 if (rkey_event_heap) efree(rkey_event);
				 if (rkey_conf_heap)  efree(rkey_conf);
				 cache = NULL;
				 return;
			 }
			 trim(path, '/');
			 replaceAll(path, '.', '/');
			 if (conf && Z_TYPE_P(conf) == IS_ARRAY) {
				 path_new = get_path_router_init(conf, path);
				 if (path_new != path) {
					 efree(path);
				 }
				 path = path_new;
			 }

			 lead = get_path_router(temp, path);

			 if (lead) {
				 get_router_info(&lead, &cacheHook);
				 lead = NULL;
			 } else {
				 if (!get_router_error_run_by_router(cacheHook, "404")) {
					 if (ctx->path) {
						 php_error_docref(NULL, E_WARNING, "Gene Unknown Url:%s", ctx->path);
					 } else {
						 php_error_docref(NULL, E_WARNING, "Gene Unknown Url:%s", path);
					 }
				 }
			 }
			 cache = NULL;
			 cacheHook = NULL;
		 } else {
			 php_error_docref(NULL, E_WARNING, "Gene Unknown Router Cache");
		 }
		 if (rkey_tree_heap)  efree(rkey_tree);
		 if (rkey_event_heap) efree(rkey_event);
		 if (rkey_conf_heap)  efree(rkey_conf);
	 }
	 if (method_heap) efree((char *)method);
	 efree(path);
	 temp = NULL;
	 return;
 }
 
 /*
  * {{{ gene_router_methods
  */
 PHP_METHOD(gene_router, run) {
	 zend_string *methodin = NULL, *pathin = NULL;
	 zval *self = getThis(), *safe = NULL;
	 char *min = NULL, *pin = NULL;
 
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &methodin, &pathin) == FAILURE) {
		 RETURN_NULL();
	 }
 
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (methodin && ZSTR_LEN(methodin)) {
		 min = ZSTR_VAL(methodin);
	 }
	 if (pathin && ZSTR_LEN(pathin)) {
		 pin = ZSTR_VAL(pathin);
	 }
 
	 if (safe && Z_TYPE_P(safe) == IS_STRING && Z_STRLEN_P(safe) > 0) {
		 get_router_content_run(min, pin, Z_STRVAL_P(safe), Z_STRLEN_P(safe));
	 } else {
		 get_router_content_run(min, pin, NULL, 0);
	 }
	 RETURN_NULL();
 }
 /* }}} */
 
 /*
  * {{{ gene_router_methods
  */
 PHP_METHOD(gene_router, runError) {
	 zend_string *methodin = NULL;
	 const char *safe_str = NULL;
	 size_t safe_len = 0;
 
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &methodin) == FAILURE) {
		 RETURN_NULL();
	 }
	 if (GENE_G(app_key)) {
		 safe_str = GENE_G(app_key);
		 safe_len = GENE_G(app_key_len);
	 } else if (GENE_G(app_root)) {
		 safe_str = GENE_G(app_root);
		 safe_len = GENE_G(app_root_len);
	 }
	 get_router_error_run(ZSTR_VAL(methodin), safe_str, safe_len);
	 RETURN_TRUE;
 }
 /* }}} */
 
 /*
  * {{{ gene_router_methods
  */
 PHP_METHOD(gene_router, __construct) {
	 zval *safe = NULL;
	 int len = 0;
	 zval *obj = getThis();
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		 RETURN_NULL();
	 }
	 /* [GENE_FIX:2026-05-21 F5] zend_parse_parameters("|z") permits non-string
	  * input. zend_update_property_string requires a NUL-terminated C string
	  * via Z_STRVAL_P; a non-string zval would feed strlen() with the wrong
	  * union member (zend_long bytes / pointer interpretation), reading well
	  * past the intended buffer. Gate on IS_STRING and fall through to the
	  * GENE_G(app_key)/app_root defaults otherwise. */
	 if (safe && Z_TYPE_P(safe) == IS_STRING) {
		 zend_update_property_string(gene_router_ce, gene_strip_obj(obj), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), Z_STRVAL_P(safe));
	 } else {
		 if (GENE_G(app_key)) {
			 zend_update_property_string(gene_router_ce, gene_strip_obj(obj),GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE),GENE_G(app_key));
		 } else if (GENE_G(app_root)) {
			 zend_update_property_string(gene_router_ce, gene_strip_obj(obj),GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE),GENE_G(app_root));
		 }
	 }
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::__call($codeString)
  */
static int gene_router_is_http_method(const char *m) {
	/* [GENE_PERF:2026-05-04 #11] Length-aware dispatch: read the
	 * length once, then memcmp the remaining bytes (skipping the
	 * already-checked first character). This is one strlen + one
	 * bounded memcmp per call vs. strcmp's character-at-a-time loop. */
	size_t l = strlen(m);
	switch (m[0]) {
	case 'g': return l == 3 && memcmp(m + 1, "et", 2) == 0;
	case 'p':
		if (l == 4 && m[1] == 'o') return memcmp(m + 2, "st", 2) == 0;
		if (l == 3 && m[1] == 'u') return m[2] == 't';
		if (l == 5 && m[1] == 'a') return memcmp(m + 2, "tch", 3) == 0;
		return 0;
	case 'd': return l == 6 && memcmp(m + 1, "elete", 5) == 0;
	case 't': return l == 5 && memcmp(m + 1, "race", 4) == 0;
	case 'c': return l == 7 && memcmp(m + 1, "onnect", 6) == 0;
	case 'o': return l == 7 && memcmp(m + 1, "ptions", 6) == 0;
	case 'h': return l == 4 && memcmp(m + 1, "ead", 3) == 0;
	default: return 0;
	}
}

static int gene_router_is_event(const char *m) {
	return (strcmp(m, "hook") == 0 || strcmp(m, "error") == 0);
}

PHP_METHOD(gene_router, __call) {
	zval *val = NULL, *group, *safe, *pathVal = NULL, *contentval = NULL, *hook = NULL, *self = getThis();
	zval content;
	ZVAL_UNDEF(&content);
	zend_long methodlen;
	int is_string_route = 0;
	int is_closure_route = 0;
	zval fid_zv;
	ZVAL_UNDEF(&fid_zv);
	size_t router_e_len;
	char *method, *path = NULL, *result = NULL, *tmp = NULL, *router_e, *key = NULL;
	int path_heap = 0; /* 1: path from emalloc/estrdup; 0: stack path_buf in __call */
 
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "sz", &method, &methodlen, &val) == FAILURE) {
		 RETURN_NULL();
	 }
 
	 gene_strtolower(method);
	 if (IS_ARRAY == Z_TYPE_P(val)) {
		 pathVal = zend_hash_index_find(Z_ARRVAL_P(val), 0);
		 if (pathVal != NULL && Z_TYPE_P(pathVal) == IS_STRING) {
			 group = zend_read_property(gene_router_ce, gene_strip_obj(self),GENE_ROUTER_GROUP, strlen(GENE_ROUTER_GROUP), 1,NULL);
			 size_t path_len = Z_STRLEN_P(group) + Z_STRLEN_P(pathVal);
			 char path_buf[512];
			 if (path_len >= sizeof(path_buf)) {
				 path = emalloc(path_len + 1);
				 path_heap = 1;
			 } else {
				 path = path_buf;
				 path_heap = 0;
			 }
			 snprintf(path, path_len + 1, "%s%s", Z_STRVAL_P(group),Z_STRVAL_P(pathVal));
		 }
		 if (path == NULL) {
			 path = estrdup("");
			 path_heap = 1;
		 }
		 contentval = zend_hash_index_find(Z_ARRVAL_P(val), 1);
		 if (contentval != NULL) {
			 if (IS_OBJECT == Z_TYPE_P(contentval)) {
				 tmp = get_function_content(contentval);
				 result = get_router_content_F(tmp, method, path);
				 if (tmp != NULL) {
					 efree(tmp);
					 tmp = NULL;
				 }
				 /* Store closure in fn_cache for direct dispatch */
				 gene_fn_cache_store(contentval, &fid_zv);
				 is_closure_route = 1;
			 } else {
				 result = get_router_content(&contentval, method, path);
				 is_string_route = (Z_TYPE_P(contentval) == IS_STRING);
			 }
		 }
		 if (result) {
			 ZVAL_STRING(&content, result);
			 efree(result);
		 } else {
			 ZVAL_STRING(&content, "");
		 }
 
		 hook = zend_hash_index_find(Z_ARRVAL_P(val), 2);
		 if (hook != NULL && Z_TYPE_P(hook) != IS_STRING) {
			 hook = NULL;
		 }
 
		//call tree
		if (gene_router_is_http_method(method)) {
			{
				 zval pathvals;
				 char router_e_buf[256];
				 char key_buf[256];
				 int router_e_heap = 0;
				 int key_heap = 0;
				 size_t key_len;
				 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
				 if (Z_STRLEN_P(safe)) {
					 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_TREE);
					 if (router_e_len >= sizeof(router_e_buf)) {
						 router_e = emalloc(router_e_len + 1);
						 router_e_heap = 1;
					 } else {
						 router_e = router_e_buf;
					 }
					 snprintf(router_e, router_e_len + 1, "%s%s",Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
				 } else {
					 router_e_len = strlen(GENE_ROUTER_ROUTER_TREE);
					 if (router_e_len >= sizeof(router_e_buf)) {
						 router_e = emalloc(router_e_len + 1);
						 router_e_heap = 1;
					 } else {
						 router_e = router_e_buf;
					 }
					 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_TREE);
				 }
				 if (strlen(path) == 0) {
					 ZVAL_STRING(&pathvals, "");
					 key_len = strlen(method) + 9;
					 if (key_len >= sizeof(key_buf)) {
						 key = emalloc(key_len + 1);
						 key_heap = 1;
					 } else {
						 key = key_buf;
					 }
					 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_KEY, method);
					 gene_memory_set_by_router(router_e, router_e_len, key, &pathvals, 0);
					 if (key_heap) efree(key);
					 key_len = strlen(method) + 9;
					 if (key_len >= sizeof(key_buf)) {
						 key = emalloc(key_len + 1);
						 key_heap = 1;
					 } else {
						 key = key_buf;
					 }
					 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_RUN, method);
					 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
					 if (key_heap) efree(key);
					 if (is_string_route && contentval) {
						 key_len = strlen(method) + 9;
						 if (key_len >= sizeof(key_buf)) {
							 key = emalloc(key_len + 1);
							 key_heap = 1;
						 } else {
							 key = key_buf;
						 }
						 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_SRC, method);
						 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
						 if (key_heap) efree(key);
					 }
					 if (is_closure_route) {
						 key_len = strlen(method) + 10;
						 if (key_len >= sizeof(key_buf)) {
							 key = emalloc(key_len + 1);
							 key_heap = 1;
						 } else {
							 key = key_buf;
						 }
						 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_FRUN, method);
						 gene_memory_set_by_router(router_e, router_e_len, key, &fid_zv, 0);
						 if (key_heap) efree(key);
					 }
					 if (hook) {
						 key_len = strlen(method) + 10;
						 if (key_len >= sizeof(key_buf)) {
							 key = emalloc(key_len + 1);
							 key_heap = 1;
						 } else {
							 key = key_buf;
						 }
						 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_HOOK, method);
						 gene_memory_set_by_router(router_e, router_e_len, key, hook, 0);
						 if (key_heap) efree(key);
					 }
				 } else {
					 trim(path, '/');
					 ZVAL_STRING(&pathvals, path);
					 replaceAll(path, '.', '/');
					 tmp = replace_string(path, ':', GENE_ROUTER_CHIRD);
					 if (tmp == NULL) {
						 key_len = strlen(method) + strlen(path) + 11;
						 if (key_len >= sizeof(key_buf)) {
							 key = emalloc(key_len + 1);
							 key_heap = 1;
						 } else {
							 key = key_buf;
						 }
						 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_KEY_L, method, path);
						 gene_memory_set_by_router(router_e, router_e_len, key, &pathvals, 0);
						 if (key_heap) efree(key);
						 key_len = strlen(method) + strlen(path) + 11;
						 if (key_len >= sizeof(key_buf)) {
							 key = emalloc(key_len + 1);
							 key_heap = 1;
						 } else {
							 key = key_buf;
						 }
						 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_RUN_L, method, path);
						 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
						 if (key_heap) efree(key);
						 if (is_string_route && contentval) {
							 key_len = strlen(method) + strlen(path) + 11;
							 if (key_len >= sizeof(key_buf)) {
								 key = emalloc(key_len + 1);
								 key_heap = 1;
							 } else {
								 key = key_buf;
							 }
							 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_SRC_L, method, path);
							 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
							 if (key_heap) efree(key);
						 }
						 if (is_closure_route) {
							 key_len = strlen(method) + strlen(path) + 12;
							 if (key_len >= sizeof(key_buf)) {
								 key = emalloc(key_len + 1);
								 key_heap = 1;
							 } else {
								 key = key_buf;
							 }
							 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_FRUN_L, method, path);
							 gene_memory_set_by_router(router_e, router_e_len, key, &fid_zv, 0);
							 if (key_heap) efree(key);
						 }
						 if (hook) {
							 key_len = strlen(method) + strlen(path) + 12;
							 if (key_len >= sizeof(key_buf)) {
								 key = emalloc(key_len + 1);
								 key_heap = 1;
							 } else {
								 key = key_buf;
							 }
							 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_HOOK_L, method, path);
							 gene_memory_set_by_router(router_e, router_e_len, key, hook, 0);
							 if (key_heap) efree(key);
						 }
					 } else {
						 key_len = strlen(method) + strlen(tmp) + 11;
						 if (key_len >= sizeof(key_buf)) {
							 key = emalloc(key_len + 1);
							 key_heap = 1;
						 } else {
							 key = key_buf;
						 }
						 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_KEY_L, method, tmp);
						 gene_memory_set_by_router(router_e, router_e_len, key, &pathvals, 0);
						 if (key_heap) efree(key);
						 key_len = strlen(method) + strlen(tmp) + 11;
						 if (key_len >= sizeof(key_buf)) {
							 key = emalloc(key_len + 1);
							 key_heap = 1;
						 } else {
							 key = key_buf;
						 }
						 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_RUN_L, method, tmp);
						 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
						 if (key_heap) efree(key);
						 if (is_string_route && contentval) {
							 key_len = strlen(method) + strlen(tmp) + 11;
							 if (key_len >= sizeof(key_buf)) {
								 key = emalloc(key_len + 1);
								 key_heap = 1;
							 } else {
								 key = key_buf;
							 }
							 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_SRC_L, method, tmp);
							 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
							 if (key_heap) efree(key);
						 }
						 if (is_closure_route) {
							 key_len = strlen(method) + strlen(tmp) + 12;
							 if (key_len >= sizeof(key_buf)) {
								 key = emalloc(key_len + 1);
								 key_heap = 1;
							 } else {
								 key = key_buf;
							 }
							 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_FRUN_L, method, tmp);
							 gene_memory_set_by_router(router_e, router_e_len, key, &fid_zv, 0);
							 if (key_heap) efree(key);
						 }
						 if (hook) {
							 key_len = strlen(method) + strlen(tmp) + 12;
							 if (key_len >= sizeof(key_buf)) {
								 key = emalloc(key_len + 1);
								 key_heap = 1;
							 } else {
								 key = key_buf;
							 }
							 snprintf(key, key_len + 1, GENE_ROUTER_LEAF_HOOK_L, method, tmp);
							 gene_memory_set_by_router(router_e, router_e_len, key, hook, 0);
							 if (key_heap) efree(key);
						 }
						 efree(tmp);
					 }
				 }
			if (router_e_heap) efree(router_e);
			if (path_heap) efree(path);
			zval_ptr_dtor(&content);
			zval_ptr_dtor(&pathvals);
			if (Z_TYPE(fid_zv) != IS_UNDEF) zval_ptr_dtor(&fid_zv);
			RETURN_ZVAL(self, 1, 0);
		}
	}

	//call event
	if (gene_router_is_event(method)) {
		{
				 char router_e_buf[256];
				 char key_buf[256];
				 int router_e_heap = 0;
				 int key_heap = 0;
				 size_t key_len;
				 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
				 if (Z_STRLEN_P(safe)) {
					 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_EVENT);
					 if (router_e_len >= sizeof(router_e_buf)) {
						 router_e = emalloc(router_e_len + 1);
						 router_e_heap = 1;
					 } else {
						 router_e = router_e_buf;
					 }
					 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_EVENT);
				 } else {
					 router_e_len = strlen(GENE_ROUTER_ROUTER_EVENT);
					 if (router_e_len >= sizeof(router_e_buf)) {
						 router_e = emalloc(router_e_len + 1);
						 router_e_heap = 1;
					 } else {
						 router_e = router_e_buf;
					 }
					 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_EVENT);
				 }
				 key_len = strlen(method) + strlen(path) + 2;
				 if (key_len >= sizeof(key_buf)) {
					 key = emalloc(key_len + 1);
					 key_heap = 1;
				 } else {
					 key = key_buf;
				 }
				 snprintf(key, key_len + 1, "%s:%s", method, path);
				 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
				 if (key_heap) efree(key);
				 if (is_string_route && contentval && Z_TYPE_P(contentval) == IS_STRING) {
					 key_len = 6 + strlen(path);
					 if (key_len >= sizeof(key_buf)) {
						 key = emalloc(key_len + 1);
						 key_heap = 1;
					 } else {
						 key = key_buf;
					 }
					 snprintf(key, key_len + 1, "hsrc:%s", path);
					 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
					 if (key_heap) efree(key);
				 }
				 if (is_closure_route) {
					 key_len = 5 + strlen(path);
					 if (key_len >= sizeof(key_buf)) {
						 key = emalloc(key_len + 1);
						 key_heap = 1;
					 } else {
						 key = key_buf;
					 }
					 snprintf(key, key_len + 1, "fcl:%s", path);
					 gene_memory_set_by_router(router_e, router_e_len, key, &fid_zv, 0);
					 if (key_heap) efree(key);
				 }
			 if (router_e_heap) efree(router_e);
			 if (path_heap) efree(path);
			 zval_ptr_dtor(&content);
			 if (Z_TYPE(fid_zv) != IS_UNDEF) zval_ptr_dtor(&fid_zv);
			 RETURN_ZVAL(self, 1, 0);
		 }
	 }
	 if (path_heap) {
		 efree(path);
	 }
	 if (Z_TYPE(content) != IS_UNDEF) {
		 zval_ptr_dtor(&content);
	 }
	 }
	 if (Z_TYPE(fid_zv) != IS_UNDEF) zval_ptr_dtor(&fid_zv);
	 RETURN_ZVAL(self, 1, 0);
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::getEvent()
  */
 PHP_METHOD(gene_router, getEvent) {
	 zval *self = getThis(), *safe, *cache = NULL;
	 size_t router_e_len;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_EVENT);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_EVENT);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_EVENT);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_EVENT);
	 }
	 cache = gene_memory_get(router_e, router_e_len);
	 if (router_e_heap) efree(router_e);
	 if (cache) {
		 gene_memory_zval_local(return_value, cache);
		 return;
	 }
	 RETURN_NULL();
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::getTree()
  */
 PHP_METHOD(gene_router, getTree) {
	 zval *self = getThis(), *safe, *cache = NULL;
	 size_t router_e_len;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_TREE);
	 }
	 cache = gene_memory_get(router_e, router_e_len);
	 if (router_e_heap) efree(router_e);
	 if (cache) {
		 gene_memory_zval_local(return_value, cache);
		 return;
	 }
	 RETURN_NULL();
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::getConf()
  */
 PHP_METHOD(gene_router, getConf) {
	 zval *self = getThis(), *safe = NULL, *cache = NULL;
	 size_t router_e_len;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_CONF);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_CONF);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_CONF);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_CONF);
	 }
	 cache = gene_memory_get(router_e, router_e_len);
	 if (router_e_heap) efree(router_e);
	 if (cache) {
		 gene_memory_zval_local(return_value, cache);
		 return;
	 }
	 RETURN_NULL();
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::delTree()
  */
 PHP_METHOD(gene_router, delTree) {
	 zval *self = getThis(), *safe;
	 size_t router_e_len;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_TREE);
	 }
	 if (gene_memory_del(router_e, router_e_len)) {
		 if (router_e_heap) efree(router_e);
		 RETURN_TRUE;
	 }
	 if (router_e_heap) efree(router_e);
	 RETURN_FALSE;
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::delEvent()
  */
 PHP_METHOD(gene_router, delEvent) {
	 zval *self = getThis(), *safe;
	 size_t router_e_len;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_EVENT);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_EVENT);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_EVENT);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_EVENT);
	 }
	 if (gene_memory_del(router_e, router_e_len)) {
		 if (router_e_heap) efree(router_e);
		 RETURN_TRUE;
	 }
	 if (router_e_heap) efree(router_e);
	 RETURN_FALSE;
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::delConf()
  */
 PHP_METHOD(gene_router, delConf) {
	 zval *self = getThis(), *safe = NULL;
	 size_t router_e_len;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_CONF);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_CONF);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_CONF);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_CONF);
	 }
	 if (gene_memory_del(router_e, router_e_len)) {
		 if (router_e_heap) efree(router_e);
		 RETURN_TRUE;
	 }
	 if (router_e_heap) efree(router_e);
	 RETURN_FALSE;
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::clear()
  */
 PHP_METHOD(gene_router, clear) {
	 zval *self = getThis(), *safe;
	 size_t router_e_len, ret;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_TREE);
	 }
	 ret = gene_memory_del(router_e, router_e_len);
	 if (router_e_heap) efree(router_e);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_EVENT);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
			 router_e_heap = 0;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe),
		 GENE_ROUTER_ROUTER_EVENT);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_EVENT);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
			 router_e_heap = 0;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_EVENT);
	 }
	 ret = gene_memory_del(router_e, router_e_len);
	 if (router_e_heap) efree(router_e);
	 /* [GENE_MEM:2026-04-23] Rebuilding the router tree orphans any closure
	  * refs previously stored in fn_cache (the tree string keys "fn_<handle>"
	  * become unreachable). Wipe fn_cache here to release those refs and
	  * keep long-running workers bounded across reloads. */
	 if (GENE_G(fn_cache)) {
		 zend_hash_destroy(GENE_G(fn_cache));
		 FREE_HASHTABLE(GENE_G(fn_cache));
		 GENE_G(fn_cache) = NULL;
	 }
	 RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_router::getTime()
  */
 PHP_METHOD(gene_router, getTime) {
	 zval *self = getThis(), *safe;
	 size_t router_e_len;
	 zend_long ctime = 0;
	 char *router_e;
	 char router_e_buf[256];
	 int router_e_heap = 0;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_TREE);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_TREE);
	 }
	 ctime = gene_memory_getTime(router_e, router_e_len);
	 if (router_e_heap) efree(router_e);
	 if (ctime > 0) {
		 ctime = time(NULL) - ctime;
		 RETURN_LONG(ctime);
	 }
	 RETURN_NULL();
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
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &fileName)
			 == FAILURE) {
		 RETURN_NULL();
	 }
	 rec = readfilecontent(ZSTR_VAL(fileName));
	 if (rec != NULL) {
		 RETVAL_STRING(rec);
		 efree(rec);
		 return;
	 }
	 RETURN_NULL();
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
 
 PHP_METHOD(gene_router, assign) {
	 zval *value;
	 zend_string *name;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		 return;
	 }
	 gene_view_set_vars(name, value);
	 RETURN_NULL();
 }
 /* }}} */
 
 /** {{{ public gene_router::display(string $file)
  */
 PHP_METHOD(gene_router, display) {
	 zend_string *file, *parent_file = NULL;
	 zend_array *table = NULL;
	 zval *vars = NULL;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S", &file, &parent_file) == FAILURE) {
		 return;
	 }
	 zval *self = getThis();
	 vars = gene_view_get_vars();
	 table = gene_view_build_symbol_table(vars);
 
	 if (parent_file && ZSTR_LEN(parent_file) > 0) {
		 gene_request_context *ctx = gene_request_ctx();
		 if (ctx->child_views) {
			 efree(ctx->child_views);
			 ctx->child_views = NULL;
		 }
		 /* [GENE_PERF:2026-04-20] Cache child_views_len alongside child_views. */
		 ctx->child_views = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		 ctx->child_views_len = ZSTR_LEN(file);
		 gene_view_display(ZSTR_VAL(parent_file), self, table);
	 } else {
		 gene_view_display(ZSTR_VAL(file), self, table);
	 }
	 if (table) {
		 zend_hash_destroy(table);
		 FREE_HASHTABLE(table);
		 gene_view_clear_vars();
	 }
 }
 /* }}} */
 
 /** {{{ public gene_router::display(string $file)
  */
 PHP_METHOD(gene_router, displayExt) {
	 zend_string *file, *parent_file = NULL;
	 bool isCompile = 0;
	 zend_array *table = NULL;
	 zval *vars = NULL;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|Sb", &file, &parent_file, &isCompile) == FAILURE) {
		 return;
	 }
	 zval *self = getThis();
	 vars = gene_view_get_vars();
	 table = gene_view_build_symbol_table(vars);
 
	 if (parent_file && ZSTR_LEN(parent_file)) {
		 gene_request_context *ctx = gene_request_ctx();
		 if (ctx->child_views) {
			 efree(ctx->child_views);
			 ctx->child_views = NULL;
		 }
		 ctx->child_views = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		 ctx->child_views_len = ZSTR_LEN(file);
		 gene_view_display_ext(ZSTR_VAL(parent_file), isCompile, self, table);
	 } else {
		 gene_view_display_ext(ZSTR_VAL(file), isCompile, self, table);
	 }
	 if (table) {
		 zend_hash_destroy(table);
		 FREE_HASHTABLE(table);
		 gene_view_clear_vars();
	 }
 }
 /* }}} */
 
 
 /** {{{ public gene_router::dispatch(string $class, string $action, zval $params)
  */
 PHP_METHOD(gene_router, dispatch) {
	 char *class = NULL, *action = NULL;
	 char *class_alloc = NULL, *action_alloc = NULL, *tmp_alloc = NULL;
	 zend_long class_len = 0, action_len = 0;
	 size_t class_name_len = 0, action_name_len = 0;
	 zval *params = NULL, classObject;
	 gene_request_context *ctx;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssz", &class, &class_len, &action, &action_len, &params) == FAILURE) {
		 return;
	 }
	 class_name_len = (size_t)class_len;
	 action_name_len = (size_t)action_len;
	 ctx = gene_request_ctx();
	 /* [GENE_PERF:2026-04-20] Use cached length fields for module/controller/
	  * action to avoid repeated strlen() on the dispatch hot path. */
	 if (ctx->module != NULL) {
		 class_alloc = gene_strreplace_fast(class, class_name_len, ":m", 2, ctx->module, ctx->module_len, &class_name_len);
		 if (class_alloc) {
			 class = class_alloc;
		 }
	 }
	 if (ctx->controller != NULL) {
		 tmp_alloc = gene_strreplace_fast(class, class_name_len, ":c", 2, ctx->controller, ctx->controller_len, &class_name_len);
		 if (tmp_alloc) {
			 if (class_alloc) efree(class_alloc);
			 class_alloc = tmp_alloc;
			 class = class_alloc;
		 }
	 }
 
	 if (gene_factory(class, class_name_len, NULL, &classObject)) {
		 if (ctx->action != NULL) {
			 action_alloc = gene_strreplace_fast(action, action_name_len, ":a", 2, ctx->action, ctx->action_len, &action_name_len);
			 if (action_alloc) {
				 action = action_alloc;
			 }
		 }
		 gene_strtolower(action);
		 action_len = (zend_long)action_name_len;
		 if (Z_TYPE(classObject) == IS_OBJECT
				 && zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), action, action_name_len)) {
			 zval ret;
			 gene_factory_call_1(&classObject, action, action_name_len, params, &ret);
			 zval_ptr_dtor(&classObject);
			 if (class_alloc) efree(class_alloc);
			 if (action_alloc) efree(action_alloc);
			 RETURN_ZVAL(&ret, 1, 1);
		 } else {
			 php_error_docref(NULL, E_WARNING, "Unable to call method '%s' in class '%s'." , action, class);
		 }
		 zval_ptr_dtor(&classObject);
		 if (class_alloc) efree(class_alloc);
		 if (action_alloc) efree(action_alloc);
		 RETURN_NULL();

	 } else {
		 php_error_docref(NULL, E_WARNING, "Unable to init calss '%s'." , class);
	 }
	 if (class_alloc) efree(class_alloc);
	 RETURN_NULL();
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::params()
  */
 PHP_METHOD(gene_router, params) {
	 zend_string *name = NULL;
	 zval *params = NULL;
	 gene_request_context *ctx;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		 return;
	 }
 
	 ctx = gene_request_ctx();
	 params = &ctx->path_params;
	 if (name == NULL) {
		 RETURN_ZVAL(&ctx->path_params, 1, 0);
	 } else {
		 zval *val = zend_symtable_str_find(Z_ARRVAL_P(params), ZSTR_VAL(name), ZSTR_LEN(name));
		 if (val) {
			 RETURN_ZVAL(val, 1, 0);
		 }
		 RETURN_NULL();
	 }
 }
 /* }}} */
 
 /* }}} */
 
 /*
  * {{{ public gene_router::group()
  */
 PHP_METHOD(gene_router, group) {
	 zend_string *name = NULL;
	 zval *self = getThis();
	 char *group = NULL;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		 return;
	 }
 
	 if (name == NULL) {
		 zend_update_property_string(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_GROUP, strlen(GENE_ROUTER_GROUP), "");
	 } else {
		 group = estrndup(ZSTR_VAL(name), ZSTR_LEN(name));
		 trim(group, '/');
		 zend_update_property_string(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_GROUP, strlen(GENE_ROUTER_GROUP), group);
		 efree(group);
	 }
 
	 RETURN_ZVAL(self, 1, 0);
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::prefix()
  */
 PHP_METHOD(gene_router, prefix) {
	 zend_string *name = NULL;
	 zval *safe = NULL,*self = getThis();
	 char *router_e = NULL,*prefix = NULL,*val = NULL;
	 size_t router_e_len;
 
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		 return;
	 }
 
	 if (name) {
		 char router_e_buf[256];
		 char prefix_buf[64];
		 int router_e_heap = 0;
		 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
		 if (Z_STRLEN_P(safe)) {
			 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_CONF);
			 if (router_e_len >= sizeof(router_e_buf)) {
				 router_e = emalloc(router_e_len + 1);
				 router_e_heap = 1;
			 } else {
				 router_e = router_e_buf;
			 }
			 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_CONF);
		 } else {
			 router_e_len = strlen(GENE_ROUTER_ROUTER_CONF);
			 if (router_e_len >= sizeof(router_e_buf)) {
				 router_e = emalloc(router_e_len + 1);
				 router_e_heap = 1;
			 } else {
				 router_e = router_e_buf;
			 }
			 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_CONF);
		 }
		 prefix = estrdup(GENE_ROUTER_PREFIX);
		 val = estrndup(ZSTR_VAL(name), ZSTR_LEN(name));
		 trim(val, '/');
		 zval content;
		 ZVAL_STRING(&content, val);
		 gene_memory_set_by_router(router_e, router_e_len, prefix, &content, 0);
		 if (router_e_heap) efree(router_e);
		 efree(prefix);
		 efree(val);
		 zval_ptr_dtor(&content);
	 }
	 RETURN_ZVAL(self, 1, 0);
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::lang()
  */
 PHP_METHOD(gene_router, lang) {
	 zend_string *lang_list_tmp = NULL;
	 zval *safe = NULL,*self = getThis();
	 char *router_e = NULL,*lang = NULL,*langs = NULL,*langs_tmp = NULL;
	 size_t router_e_len;
	 char router_e_buf[256];
	 char langs_tmp_buf[128];
	 char langs_buf[64];
	 int router_e_heap = 0;
	 int langs_tmp_heap = 0;
	 int langs_heap = 0;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &lang_list_tmp) == FAILURE) {
		 return;
	 }

	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = Z_STRLEN_P(safe) + strlen(GENE_ROUTER_ROUTER_CONF);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_CONF);
	 } else {
		 router_e_len = strlen(GENE_ROUTER_ROUTER_CONF);
		 if (router_e_len >= sizeof(router_e_buf)) {
			 router_e = emalloc(router_e_len + 1);
			 router_e_heap = 1;
		 } else {
			 router_e = router_e_buf;
		 }
		 snprintf(router_e, router_e_len + 1, "%s", GENE_ROUTER_ROUTER_CONF);
	 }

	 if (lang_list_tmp) {
		 size_t langs_tmp_len = ZSTR_LEN(lang_list_tmp) + 3;
		 if (langs_tmp_len >= sizeof(langs_tmp_buf)) {
			 langs_tmp = emalloc(langs_tmp_len + 1);
			 langs_tmp_heap = 1;
		 } else {
			 langs_tmp = langs_tmp_buf;
		 }
		 snprintf(langs_tmp, langs_tmp_len + 1, ",%s,", ZSTR_VAL(lang_list_tmp));
		 zval content;
		 ZVAL_STRING(&content, langs_tmp);
		 if (langs_tmp_heap) efree(langs_tmp);
		 size_t langs_len = strlen(GENE_ROUTER_LANGS);
		 if (langs_len >= sizeof(langs_buf)) {
			 langs = emalloc(langs_len + 1);
			 langs_heap = 1;
		 } else {
			 langs = langs_buf;
		 }
		 snprintf(langs, langs_len + 1, "%s", GENE_ROUTER_LANGS);
		 gene_memory_set_by_router(router_e, router_e_len, langs, &content, 0);
		 if (langs_heap) efree(langs);
		 zval_ptr_dtor(&content);
	 }
	 if (router_e_heap) efree(router_e);
	 RETURN_ZVAL(self, 1, 0);
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::getLang()
  */
 PHP_METHOD(gene_router, getLang) {
	 gene_request_context *ctx = gene_request_ctx();
	 if (ctx->lang) {
		 RETURN_STRINGL(ctx->lang, ctx->lang_len);
	 }
	 RETURN_NULL();
 }
 /* }}} */
 
 /*
  * {{{ gene_router_methods
  */
 const zend_function_entry gene_router_methods[] = {
	 PHP_ME(gene_router, __construct, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, getEvent, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, getTree, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, getConf, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, delTree, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, delEvent, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, delConf, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, clear, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, getTime, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, getRouter, gene_router_void_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, assign, gene_router_assign, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, display, gene_router_display, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	 PHP_ME(gene_router, displayExt, gene_router_display_ext, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	 PHP_ME(gene_router, runError, gene_router_run_error, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	 PHP_ME(gene_router, run, gene_router_run, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, readFile, gene_router_read_file, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, dispatch, gene_router_dispatch, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	 PHP_ME(gene_router, params, gene_router_params, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	 PHP_ME(gene_router, prefix, gene_router_prefix_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, group, gene_router_group_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, lang, gene_router_lang_arginfo, ZEND_ACC_PUBLIC)
	 PHP_ME(gene_router, getLang, gene_router_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	 PHP_ME(gene_router, __call, gene_router_call_arginfo, ZEND_ACC_PUBLIC)
	 { NULL, NULL, NULL }
 };
 /* }}} */
 
 /*
  * {{{ GENE_MINIT_FUNCTION
  */
 GENE_MINIT_FUNCTION(router) {
	 zend_class_entry gene_router;
	 GENE_INIT_CLASS_ENTRY(gene_router, "Gene_Router", "Gene\\Router", gene_router_methods);
	 gene_router_ce = zend_register_internal_class(&gene_router);
 #if PHP_VERSION_ID >= 80200
	 gene_router_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
 #endif
 
	 //prop
	 zend_declare_property_string(gene_router_ce, GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), "", ZEND_ACC_PUBLIC);
	 zend_declare_property_string(gene_router_ce, GENE_ROUTER_GROUP, strlen(GENE_ROUTER_GROUP), "", ZEND_ACC_PUBLIC);
 
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
 
 
