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
 
 #include "../gene.h"
 #include "../factory/factory.h"
 #include "../router/router.h"
 #include "../cache/memory.h"
 #include "../common/common.h"
 #include "../app/application.h"
 #include "../mvc/view.h"
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
  */
 int setMca(zend_string *key, char *val) {
	 zval sval, *params = NULL;
	 gene_request_context *ctx = gene_request_ctx();
	 if (key->len == 1) {
		 switch (key->val[0]) {
		 case 'm':
			 if (ctx->module) {
				 efree(ctx->module);
				 ctx->module = NULL;
			 }
			 ctx->module = str_init(val);
			 firstToUpper(ctx->module);
			 break;
		 case 'c':
			 if (ctx->controller) {
				 efree(ctx->controller);
				 ctx->controller = NULL;
			 }
			 ctx->controller = str_init(val);
			 firstToUpper(ctx->controller);
			 break;
		 case 'a':
			 if (ctx->action) {
				 efree(ctx->action);
				 ctx->action = NULL;
			 }
			 ctx->action = str_init(val);
			 break;
		 }
	 } else {
		 params = ctx->path_params;
		 if (params != NULL) {
			 ZVAL_STRING(&sval, val);
			 zend_symtable_update(Z_ARRVAL_P(params), key, &sval);
		 }
	 }
	 return 1;
 }
 /* }}} */
 
 /** {{{ void gene_router_reset_path_params()
  */
 static void gene_router_reset_path_params() {
	 gene_request_context *ctx = gene_request_ctx();
	 zval *params = ctx->path_params;
 
	 if (params == NULL) {
		 ctx->path_params = (zval *) emalloc(sizeof(zval));
		 array_init(ctx->path_params);
		 return;
	 }
 
	 if (Z_TYPE_P(params) != IS_ARRAY) {
		 zval_ptr_dtor(params);
		 array_init(params);
		 return;
	 }
 
	 zend_hash_clean(Z_ARRVAL_P(params));
 }
 /* }}} */
 
 /** {{{ void gene_router_set_uri(zval **leaf)
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
		 } else {
			 ctx->router_path = str_init("");
		 }
	 }
 }
 /* }}} */
 
 /** {{{ static void get_path_router(char *keyString, int keyString_len)
  */
 char *get_path_router_init(zval *conf, char *path) {
	 zval *prefix = NULL, *langs = NULL;
	 char *seg = NULL, *ptr = NULL, *path_prefix = NULL, *result = NULL, *lang_tmp = NULL, *search = NULL, *work = NULL;
	 zend_long path_len = 0;
	 path_len = strlen(path);
	 if (path_len == 0) {
		 return path;
	 }
 
	 result = NULL;
	 prefix = zend_symtable_str_find(Z_ARRVAL_P(conf), "prefix", 6);
	 if (prefix) {
		 if (Z_STRLEN_P(prefix) <= path_len) {
			 path_prefix = str_sub(path, Z_STRLEN_P(prefix));
			 if (strcmp(path_prefix, Z_STRVAL_P(prefix)) == 0) {
				 result = str_sub_len(path, Z_STRLEN_P(prefix), path_len - Z_STRLEN_P(prefix));
				 trim(result, '/');
				 if (strlen(result) < 1) {
					 efree(path_prefix);
					 return result;
				 }
			 }
			 efree(path_prefix);
		 }
	 }
	 langs = zend_symtable_str_find(Z_ARRVAL_P(conf), "langs", 5);
	 if (langs) {
		 work = str_init(result ? result : path);
		 seg = php_strtok_r(work, "/", &ptr);
		 if (seg && strlen(seg) > 0) {
			 if (ptr == NULL) {
				 ptr = "";
			 }
			 spprintf(&lang_tmp, 0, ",%s,", seg);
			 search = strstr(Z_STRVAL_P(langs), lang_tmp);
			 efree(lang_tmp);
			 if (search != NULL) {
				 if (GENE_REQ(lang)) {
					 efree(GENE_REQ(lang));
					 GENE_REQ(lang) = NULL;
				 }
				 GENE_REQ(lang) = str_init(seg);
				 if (result) {
					 efree(result);
				 }
				 spprintf(&result, 0, "%s", ptr);
				 if (strlen(result) > 0) {
					 trim(result, '/');
				 }
			 }
		 }
		 efree(work);
	 }
 
	 return result ? result : path;
 }
 /* }}} */
 
 /** {{{ static void get_path_router(char *keyString, int keyString_len)
  */
 zval *get_path_router(zval *val, char *paths) {
	 zval *ret = NULL, *tmp = NULL, *leaf = NULL;
	 char *seg = NULL, *ptr = NULL, *path = NULL;
	 zend_string *key = NULL;
	 zend_long idx;
	if (strlen(paths) == 0) {
		leaf = zend_symtable_str_find(Z_ARRVAL_P(val), "leaf", 4);
		return leaf;
	} else {
		path = estrdup(paths);
		 seg = php_strtok_r(path, "/", &ptr);
		 if (ptr && strlen(seg) > 0) {
			 ret = zend_symtable_str_find(Z_ARRVAL_P(val), seg, strlen(seg));
			 if (ret) {
				 leaf = get_path_router(ret, ptr);
			 }
			 if (!leaf) {
				 ret = zend_symtable_str_find(Z_ARRVAL_P(val), "chird", 5);
				 if (ret) {
					 ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(ret), idx, key, tmp) {
						 if (key) {
							 if (tmp != NULL) {
								 leaf = get_path_router(tmp, ptr);
								 if (leaf) {
									 setMca(key, seg);
									 break;
								 }
							 }
 
						 } else {
							 if (tmp != NULL) {
								 leaf = get_path_router(tmp, ptr);
								 if (leaf) {
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
									 setMca(key, seg);
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
	 efree(path);
	 path = NULL;
	 return leaf;
 }
 /* }}} */
 
 /** {{{ gene_router_dispatch_direct
  * Directly dispatch a "Class@action" route via C API, avoiding eval.
  * Performs :m/:c/:a placeholder substitution identical to PHP_METHOD(dispatch).
  * Returns 1 on success, 0 on failure. Result written to *retval.
  */
 static int gene_router_dispatch_direct(const char *class_method, zval *retval) {
	 char *copy, *class_name, *action, *ptr;
	 char *class_alloc = NULL, *action_alloc = NULL, *tmp_alloc = NULL;
	 zval classObject;
	 gene_request_context *ctx = gene_request_ctx();
 
	 ZVAL_NULL(retval);
 
	 if (!class_method || strlen(class_method) == 0) return 0;
 
	 copy = estrdup(class_method);
	 class_name = php_strtok_r(copy, "@", &ptr);
	 action = ptr;
 
	 if (!class_name || !action || strlen(action) == 0) {
		 efree(copy);
		 return 0;
	 }
 
	 if (ctx->module != NULL) {
		 class_alloc = strreplace2(class_name, ":m", ctx->module);
		 class_name = class_alloc;
	 }
	 if (ctx->controller != NULL) {
		 tmp_alloc = strreplace2(class_name, ":c", ctx->controller);
		 if (class_alloc) efree(class_alloc);
		 class_alloc = tmp_alloc;
		 class_name = class_alloc;
	 }
 
	 if (!gene_factory(class_name, strlen(class_name), NULL, &classObject)) {
		 php_error_docref(NULL, E_WARNING, "Gene direct dispatch: unable to init class '%s'.", class_name);
		 if (class_alloc) efree(class_alloc);
		 efree(copy);
		 return 0;
	 }
 
	 action_alloc = estrdup(action);
	 if (ctx->action != NULL) {
		 char *tmp = strreplace2(action_alloc, ":a", ctx->action);
		 efree(action_alloc);
		 action_alloc = tmp;
	 }
	 gene_strtolower(action_alloc);
 
	 if (Z_TYPE(classObject) == IS_OBJECT
			 && zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), action_alloc, strlen(action_alloc))) {
		 gene_factory_call_1(&classObject, action_alloc, ctx->path_params, retval);
		 zval_ptr_dtor(&classObject);
		 if (class_alloc) efree(class_alloc);
		 efree(action_alloc);
		 efree(copy);
		 return 1;
	 }

	 php_error_docref(NULL, E_WARNING, "Gene direct dispatch: unable to call method '%s' in class '%s'.", action_alloc, class_name);
	 zval_ptr_dtor(&classObject);
	 if (class_alloc) efree(class_alloc);
	 efree(action_alloc);
	 efree(copy);
	 return 0;
}
 /* }}} */
 
 /** {{{ gene_router_exec_hook_direct
  * Directly execute a "HookClass@method" hook via C API.
  * For before/specific hooks: call with no args, return 0 to abort.
  * For after hooks: call with dispatch_result as single arg.
  * Returns 1 to continue, 0 to abort (before hooks only).
  */
 static int gene_router_exec_hook_direct(const char *class_method, zval *param, int is_before) {
	 char *copy, *class_name, *method, *ptr;
	 zval classObject, retval;
 
	 ZVAL_NULL(&retval);
 
	 if (!class_method || strlen(class_method) == 0) return 1;
 
	 copy = estrdup(class_method);
	 class_name = php_strtok_r(copy, "@", &ptr);
	 method = ptr;
 
	 if (!class_name || !method || strlen(method) == 0) {
		 efree(copy);
		 return 1;
	 }
 
	 if (!gene_factory(class_name, strlen(class_name), NULL, &classObject)) {
		 efree(copy);
		 return 1;
	 }
 
	 gene_strtolower(method);

	 if (Z_TYPE(classObject) == IS_OBJECT
			 && zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), method, strlen(method))) {
		 if (param) {
			 gene_factory_call_1(&classObject, method, param, &retval);
		 } else {
			 gene_factory_call(&classObject, method, NULL, &retval);
		 }
	 }

	 zval_ptr_dtor(&classObject);
	 efree(copy);

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
 
 /** {{{ static void get_router_info(char *keyString, int keyString_len)
  */
 int get_router_info(zval **leaf, zval **cacheHook) {
	 zval *hname, *before, *after, *m, *h, *src;
	 zval *before_src = NULL, *after_src = NULL, *hook_src = NULL;
	 int is_before = 1, is_after = 1;
	 char *hookname = NULL, *seg = NULL, *ptr = NULL;
	 int use_direct = 1;
 
	 gene_router_set_uri(leaf);
 
	 src = zend_hash_str_find(Z_ARRVAL_P(*leaf), "src", 3);
	 if (!src || Z_TYPE_P(src) != IS_STRING || Z_STRLEN_P(src) == 0) {
		 use_direct = 0;
	 }
 
	 hname = zend_hash_str_find(Z_ARRVAL_P(*leaf), "hook", 4);
	 if (hname) {
		 spprintf(&hookname, 0, "hook:%s", Z_STRVAL_P(hname));
	 }
	 if (hookname) {
		 seg = php_strtok_r(hookname, "@", &ptr);
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
			 if (before && Z_STRLEN_P(before) > 0) {
				 before_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:before", 11);
				 if (!before_src || Z_TYPE_P(before_src) != IS_STRING) use_direct = 0;
			 }
		 }
		 if (is_after) {
			 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
			 if (after && Z_STRLEN_P(after) > 0) {
				 after_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hsrc:after", 11);
				 if (!after_src || Z_TYPE_P(after_src) != IS_STRING) use_direct = 0;
			 }
		 }
		 if (seg && strlen(seg) > 0) {
			 char *hseg_key;
			 size_t hseg_key_len = spprintf(&hseg_key, 0, "hsrc%s", seg + 4);
			 hook_src = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), hseg_key, hseg_key_len);
			 efree(hseg_key);
			 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, strlen(seg));
			 if (h && Z_STRLEN_P(h) > 0) {
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
		 if (hookname) efree(hookname);
		 return 1;
	 }
 
	 /* === EVAL FALLBACK PATH (closures / legacy routes) === */
	 {
		 smart_str buf = {0};
 
		 if (is_before && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 before = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:before", 11);
			 if (before && Z_STRLEN_P(before) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(before), Z_STRLEN_P(before));
			 }
		 }
		 if (seg && strlen(seg) > 0 && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 h = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), seg, strlen(seg));
			 if (h && Z_STRLEN_P(h) > 0) {
				 smart_str_appendl(&buf, Z_STRVAL_P(h), Z_STRLEN_P(h));
			 }
		 }
		 m = zend_hash_str_find(Z_ARRVAL_P(*leaf), "run", 3);
		 if (m && Z_STRLEN_P(m) > 0) {
			 smart_str_appendl(&buf, Z_STRVAL_P(m), Z_STRLEN_P(m));
		 }
		 if (is_after && *cacheHook && Z_TYPE_P(*cacheHook) == IS_ARRAY) {
			 after = zend_hash_str_find(Z_ARRVAL_P(*cacheHook), "hook:after", 10);
			 if (after && Z_STRLEN_P(after) > 0) {
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
 
	 if (hookname) {
		 efree(hookname);
	 }
	 return 1;
 }
 /* }}} */
 
 /** {{{ static void get_router_info(char *keyString, int keyString_len)
  */
 int get_router_error_run_by_router(zval *cacheHook, char *errorName) {
	 zval *error = NULL;
	 size_t router_e_len, size = 0;
	 char *run = NULL, *router_e;
	 if (cacheHook) {
		 router_e_len = spprintf(&router_e, 0, "error:%s", errorName);
		 error = zend_hash_str_find(Z_ARRVAL_P(cacheHook), router_e, router_e_len);
		 if (error) {
			 size = Z_STRLEN_P(error) + 1;
			 run = (char *) ecalloc(size, sizeof(char));
			 strcat(run, Z_STRVAL_P(error));
			 run[size - 1] = 0;
 
			 zend_try {
				 zend_eval_stringl(run, strlen(run), NULL, errorName);
			 } zend_catch {
			 } zend_end_try();
 
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
 
 /** {{{ static void get_router_info(char *keyString, int keyString_len)
  */
 int get_router_error_run(char *errorName, zval *safe) {
	 zval *cacheHook = NULL, *error = NULL;
	 size_t router_e_len, size = 0;
	 char *run = NULL, *router_e;
	 if (safe != NULL && Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		 GENE_ROUTER_ROUTER_EVENT);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
	 }
	 cacheHook = gene_memory_get_quick(router_e, router_e_len);
	 efree(router_e);
	 if (cacheHook) {
		 router_e_len = spprintf(&router_e, 0, "error:%s", errorName);
		 error = zend_hash_str_find(Z_ARRVAL_P(cacheHook), router_e, router_e_len);
		 if (error) {
			 size = Z_STRLEN_P(error) + 1;
			 run = (char *) ecalloc(size, sizeof(char));
			 strcat(run, Z_STRVAL_P(error));
			 run[size - 1] = 0;
		 } else {
			 efree(router_e);
			 php_error_docref(NULL, E_WARNING, "Gene Unknown Error:%s", errorName);
			 return 0;
		 }
		 efree(router_e);
	 } else {
		 return 0;
	 }
 
	 zend_try {
		 zend_eval_stringl(run, strlen(run), NULL, "");
	 } zend_catch {
	 } zend_end_try();
 
	 efree(run);
	 run = NULL;
	 return 1;
 }
 /* }}} */
 
 /** {{{ static void get_function_content(char *keyString, int keyString_len)
  */
 char * get_function_content(zval *content) {
	 zval objEx, ret, fileName, arg, arg1;
	 zend_long startline, endline;
	 size_t size;
	 char *result = NULL, *tmp = NULL;
	 zend_string *c_key = zend_string_init(ZEND_STRL("ReflectionFunction"), 0);
	 zend_class_entry *reflection_ptr = zend_lookup_class(c_key);
 
	 if (reflection_ptr == NULL) {
		 zend_string_release(c_key);
		 php_error_docref(NULL, E_WARNING, "Unable to start ReflectionFunction");
		 return NULL;
	 }
	 zend_string_release(c_key);
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
 
	 //get codestartline
	 c_key = zend_string_init(ZEND_STRL("SplFileObject"), 0);
	 zend_class_entry *spl_file_ptr = zend_lookup_class(c_key);
	 if (spl_file_ptr == NULL) {
		 zend_string_release(c_key);
		 zval_ptr_dtor(&fileName);
		 php_error_docref(NULL, E_WARNING, "Unable to start SplFileObject");
		 return NULL;
	 }
	 zend_string_release(c_key);
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
 
	 ZVAL_STRING(&arg, "strrpos");
	 ZVAL_STRING(&fileName, "}");
	 zend_call_method_with_2_params(NULL, NULL, NULL, "strrpos", &ret, &arg1,
			 &fileName);
	 if (Z_TYPE(ret) == IS_FALSE) {
		 zval_ptr_dtor(&ret);
		 zval_ptr_dtor(&fileName);
		 zval_ptr_dtor(&arg);
		 zval_ptr_dtor(&arg1);
		 efree(result);
		 return NULL;
	 }
	 endline = Z_LVAL(ret);
	 zval_ptr_dtor(&ret);
	 zval_ptr_dtor(&fileName);
	 zval_ptr_dtor(&arg);
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
	 spprintf(&contents, 0, "%s", Z_STRVAL_P(*content));
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
  *  {{{ void get_router_content_run(char *methodin,char *pathin,zval *safe)
  */
 void get_router_content_run(char *methodin, char *pathin, zval *safe) {
	 char *method = NULL, *path = NULL, *run = NULL, *hook = NULL, *router_e;
	 char *path_new = NULL;
	 size_t router_e_len;// @suppress("Type cannot be resolved")
	 zval *temp = NULL, *lead = NULL;
	 zval *conf = NULL,*cache = NULL, *cacheHook = NULL;
	 gene_request_context *ctx = gene_request_ctx();
 
	 if (methodin == NULL && pathin == NULL) {
		 if (ctx->method) {
			 method = str_init(ctx->method);
		 }
		 if (ctx->path) {
			 path = str_init(ctx->path);
		 }
	 } else {
		 method = str_init(methodin);
		 gene_strtolower(method);
		 path = str_init(pathin);
	 }
 
	 if (method == NULL || path == NULL) {
		 if (method) efree(method);
		 if (path) efree(path);
		 php_error_docref(NULL, E_WARNING, "Gene Unknown Method And Url: NULL");
		 return;
	 }
 
	 gene_router_reset_path_params();
 
	 if (safe != NULL && Z_STRLEN_P(safe)) {
		 router_e = str_concat(Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_TREE);
		 router_e_len = strlen(router_e);
	 } else {
		 router_e = str_init(GENE_ROUTER_ROUTER_TREE);
		 router_e_len = strlen(router_e);
	 }
	 cache = gene_memory_get_quick(router_e, router_e_len);
	 efree(router_e);
 
	 if (cache) {
		 temp = zend_symtable_str_find(Z_ARRVAL_P(cache), method, strlen(method));
		 if (temp == NULL) {
			 php_error_docref(NULL, E_WARNING, "Gene Unknown Method Cache:%s", method);
			 efree(method);
			 efree(path);
			 cache = NULL;
			 return;
		 }
		 if (safe != NULL && Z_STRLEN_P(safe)) {
			 router_e = str_concat(Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_EVENT);
			 router_e_len = strlen(router_e);
		 } else {
			 router_e = str_init(GENE_ROUTER_ROUTER_EVENT);
			 router_e_len = strlen(router_e);
		 }
		 cacheHook = gene_memory_get_quick(router_e, router_e_len);
		 efree(router_e);
		 trim(path, '/');
		 replaceAll(path, '.', '/');
		 if (safe != NULL && Z_STRLEN_P(safe)) {
			 router_e = str_concat(Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_CONF);
			 router_e_len = strlen(router_e);
		 } else {
			 router_e = str_init(GENE_ROUTER_ROUTER_CONF);
			 router_e_len = strlen(router_e);
		 }
		 conf = gene_memory_get_quick(router_e, router_e_len);
		 if (conf && Z_TYPE_P(conf) == IS_ARRAY) {
			 path_new = get_path_router_init(conf, path);
			 if (path_new != path) {
				 efree(path);
			 }
			 path = path_new;
		 }
		 efree(router_e);
		 
 
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
	 efree(method);
	 efree(path);
	 temp = NULL;
	 return;
 }
 
 /*
  * {{{ gene_router_methods
  */
 PHP_METHOD(gene_router, run) {
	 zend_string *methodin = NULL, *pathin = NULL;
	 zval *self = getThis(), *safe = NULL, safein;
	 char *min = NULL, *pin = NULL;
 
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|SS", &methodin, &pathin) == FAILURE) {
		 RETURN_NULL();
	 }
 
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
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
 
	 get_router_content_run(min, pin, &safein);
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
 
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &methodin) == FAILURE) {
		 RETURN_NULL();
	 }
	 if (GENE_G(app_key)) {
		 ZVAL_STRING(&safe, GENE_G(app_key));
	 } else if (GENE_G(app_root)) {
		 ZVAL_STRING(&safe, GENE_G(app_root));
	 } else {
		 ZVAL_STRING(&safe, "");
	 }
	 get_router_error_run(ZSTR_VAL(methodin), &safe);
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
	 zval *obj = getThis();
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &safe) == FAILURE) {
		 RETURN_NULL();
	 }
	 if (safe) {
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
	switch (m[0]) {
	case 'g': return strcmp(m, "get") == 0;
	case 'p':
		if (m[1] == 'o') return strcmp(m, "post") == 0;
		if (m[1] == 'u') return strcmp(m, "put") == 0;
		if (m[1] == 'a') return strcmp(m, "patch") == 0;
		return 0;
	case 'd': return strcmp(m, "delete") == 0;
	case 't': return strcmp(m, "trace") == 0;
	case 'c': return strcmp(m, "connect") == 0;
	case 'o': return strcmp(m, "options") == 0;
	case 'h': return strcmp(m, "head") == 0;
	default: return 0;
	}
}

static int gene_router_is_event(const char *m) {
	return (strcmp(m, "hook") == 0 || strcmp(m, "error") == 0);
}

PHP_METHOD(gene_router, __call) {
	zval *val = NULL, *group, *safe, *pathVal = NULL, *contentval = NULL, *hook = NULL, *self = getThis();
	zval content;
	zend_long methodlen;
	int is_string_route = 0;
	size_t router_e_len;
	char *method, *path = NULL, *result = NULL, *tmp = NULL, *router_e, *key = NULL;
 
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "sz", &method, &methodlen, &val) == FAILURE) {
		 RETURN_NULL();
	 }
 
	 gene_strtolower(method);
	 if (IS_ARRAY == Z_TYPE_P(val)) {
		 pathVal = zend_hash_index_find(Z_ARRVAL_P(val), 0);
		 if (pathVal != NULL && Z_TYPE_P(pathVal) == IS_STRING) {
			 group = zend_read_property(gene_router_ce, gene_strip_obj(self),GENE_ROUTER_GROUP, strlen(GENE_ROUTER_GROUP), 1,NULL);
			 spprintf(&path, 0, "%s%s", Z_STRVAL_P(group),Z_STRVAL_P(pathVal));
		 }
		 if (path == NULL) {
			 spprintf(&path, 0, "");
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
				 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
				 if (Z_STRLEN_P(safe)) {
					 router_e_len = spprintf(&router_e, 0, "%s%s",Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
				 } else {
					 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
				 }
				 if (strlen(path) == 0) {
					 ZVAL_STRING(&pathvals, "");
					 spprintf(&key, 0, GENE_ROUTER_LEAF_KEY, method);
					 gene_memory_set_by_router(router_e, router_e_len, key, &pathvals, 0);
					 efree(key);
					 spprintf(&key, 0, GENE_ROUTER_LEAF_RUN, method);
					 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
					 efree(key);
					 if (is_string_route && contentval) {
						 spprintf(&key, 0, GENE_ROUTER_LEAF_SRC, method);
						 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
						 efree(key);
					 }
					 if (hook) {
						 spprintf(&key, 0, GENE_ROUTER_LEAF_HOOK, method);
						 gene_memory_set_by_router(router_e, router_e_len, key, hook, 0);
						 efree(key);
					 }
				 } else {
					 trim(path, '/');
					 ZVAL_STRING(&pathvals, path);
					 replaceAll(path, '.', '/');
					 tmp = replace_string(path, ':', GENE_ROUTER_CHIRD);
					 if (tmp == NULL) {
						 spprintf(&key, 0, GENE_ROUTER_LEAF_KEY_L, method, path);
						 gene_memory_set_by_router(router_e, router_e_len, key, &pathvals, 0);
						 efree(key);
						 spprintf(&key, 0, GENE_ROUTER_LEAF_RUN_L, method, path);
						 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
						 efree(key);
						 if (is_string_route && contentval) {
							 spprintf(&key, 0, GENE_ROUTER_LEAF_SRC_L, method, path);
							 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
							 efree(key);
						 }
						 if (hook) {
							 spprintf(&key, 0, GENE_ROUTER_LEAF_HOOK_L, method, path);
							 gene_memory_set_by_router(router_e, router_e_len, key, hook, 0);
							 efree(key);
						 }
					 } else {
						 spprintf(&key, 0, GENE_ROUTER_LEAF_KEY_L, method, tmp);
						 gene_memory_set_by_router(router_e, router_e_len, key, &pathvals, 0);
						 efree(key);
						 spprintf(&key, 0, GENE_ROUTER_LEAF_RUN_L, method, tmp);
						 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
						 efree(key);
						 if (is_string_route && contentval) {
							 spprintf(&key, 0, GENE_ROUTER_LEAF_SRC_L, method, tmp);
							 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
							 efree(key);
						 }
						 if (hook) {
							 spprintf(&key, 0, GENE_ROUTER_LEAF_HOOK_L, method, tmp);
							 gene_memory_set_by_router(router_e, router_e_len, key, hook, 0);
							 efree(key);
						 }
						 efree(tmp);
					 }
				 }
			efree(router_e);
			efree(path);
			zval_ptr_dtor(&content);
			zval_ptr_dtor(&pathvals);
			RETURN_ZVAL(self, 1, 0);
		}
	}

	//call event
	if (gene_router_is_event(method)) {
		{
				 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
				 if (Z_STRLEN_P(safe)) {
					 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_EVENT);
				 } else {
					 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
				 }
				 spprintf(&key, 0, "%s:%s", method, path);
				 gene_memory_set_by_router(router_e, router_e_len, key, &content, 0);
				 efree(key);
				 if (is_string_route && contentval && Z_TYPE_P(contentval) == IS_STRING) {
					 spprintf(&key, 0, "hsrc:%s", path);
					 gene_memory_set_by_router(router_e, router_e_len, key, contentval, 0);
					 efree(key);
				 }
			 efree(router_e);
			 efree(path);
			 zval_ptr_dtor(&content);
			 RETURN_ZVAL(self, 1, 0);
		 }
	 }
	 if (path) {
		 efree(path);
	 }
	 if (Z_TYPE(content) == IS_STRING) {
		 zval_ptr_dtor(&content);
	 }
	 }
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
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_EVENT);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
	 }
	 cache = gene_memory_get(router_e, router_e_len);
	 efree(router_e);
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
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	 }
	 cache = gene_memory_get(router_e, router_e_len);
	 efree(router_e);
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
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_CONF);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_CONF);
	 }
	 cache = gene_memory_get(router_e, router_e_len);
	 efree(router_e);
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
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
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
  * {{{ public gene_router::delEvent()
  */
 PHP_METHOD(gene_router, delEvent) {
	 zval *self = getThis(), *safe;
	 size_t router_e_len;
	 char *router_e;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_EVENT);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
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
  * {{{ public gene_router::delConf()
  */
 PHP_METHOD(gene_router, delConf) {
	 zval *self = getThis(), *safe = NULL;
	 size_t router_e_len;
	 char *router_e;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_CONF);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_CONF);
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
  * {{{ public gene_router::clear()
  */
 PHP_METHOD(gene_router, clear) {
	 zval *self = getThis(), *safe;
	 size_t router_e_len, ret;
	 char *router_e;
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	 }
	 ret = gene_memory_del(router_e, router_e_len);
	 efree(router_e);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),
		 GENE_ROUTER_ROUTER_EVENT);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_EVENT);
	 }
	 ret = gene_memory_del(router_e, router_e_len);
	 efree(router_e);
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
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe),GENE_ROUTER_ROUTER_TREE);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_TREE);
	 }
	 ctime = gene_memory_getTime(router_e, router_e_len);
	 efree(router_e);
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
		 if (GENE_REQ(child_views)) {
			 efree(GENE_REQ(child_views));
			 GENE_REQ(child_views) = NULL;
		 }
		 GENE_REQ(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
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
		 if (GENE_REQ(child_views)) {
			 efree(GENE_REQ(child_views));
			 GENE_REQ(child_views) = NULL;
		 }
		 GENE_REQ(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
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
	 zval *params = NULL, classObject;
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "ssz", &class, &class_len, &action, &action_len, &params) == FAILURE) {
		 return;
	 }
	 if (GENE_REQ(module) != NULL) {
		 class_alloc = strreplace2(class, ":m", GENE_REQ(module));
		 class = class_alloc;
	 }
	 if (GENE_REQ(controller) != NULL) {
		 tmp_alloc = strreplace2(class, ":c", GENE_REQ(controller));
		 if (class_alloc) efree(class_alloc);
		 class_alloc = tmp_alloc;
		 class = class_alloc;
	 }
 
	 if (gene_factory(class, strlen(class), NULL, &classObject)) {
		 if (GENE_REQ(action) != NULL) {
			 action_alloc = strreplace2(action, ":a", GENE_REQ(action));
			 action = action_alloc;
		 }
		 gene_strtolower(action);
		 if (Z_TYPE(classObject) == IS_OBJECT
				 && zend_hash_str_exists(&(Z_OBJCE(classObject)->function_table), action, strlen(action))) {
			 zval ret;
			 gene_factory_call_1(&classObject, action, params, &ret);
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
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		 return;
	 }
 
	 params = GENE_REQ(path_params);
	 if (name == NULL) {
		 RETURN_ZVAL(GENE_REQ(path_params), 1, 0);
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
		 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
		 if (Z_STRLEN_P(safe)) {
			 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_CONF);
		 } else {
			 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_CONF);
		 }
		 spprintf(&prefix, 0, "%s", GENE_ROUTER_PREFIX);
		 val = estrndup(ZSTR_VAL(name), ZSTR_LEN(name));
		 trim(val, '/');
		 zval content;
		 ZVAL_STRING(&content, val);
		 gene_memory_set_by_router(router_e, router_e_len, prefix, &content, 0);
		 efree(router_e);
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
	 if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &lang_list_tmp) == FAILURE) {
		 return;
	 }
 
	 safe = zend_read_property(gene_router_ce, gene_strip_obj(self), GENE_ROUTER_SAFE, strlen(GENE_ROUTER_SAFE), 1, NULL);
	 if (Z_STRLEN_P(safe)) {
		 router_e_len = spprintf(&router_e, 0, "%s%s", Z_STRVAL_P(safe), GENE_ROUTER_ROUTER_CONF);
	 } else {
		 router_e_len = spprintf(&router_e, 0, "%s", GENE_ROUTER_ROUTER_CONF);
	 }
 
	 if (lang_list_tmp) {
		 spprintf(&langs_tmp, 0, ",%s,", ZSTR_VAL(lang_list_tmp));
		 zval content;
		 ZVAL_STRING(&content, langs_tmp);
		 efree(langs_tmp);
		 spprintf(&langs, 0, "%s", GENE_ROUTER_LANGS);
		 gene_memory_set_by_router(router_e, router_e_len, langs, &content, 0);
		 efree(langs);
		 zval_ptr_dtor(&content);
	 }
	 efree(router_e);
	 RETURN_ZVAL(self, 1, 0);
 }
 /* }}} */
 
 /*
  * {{{ public gene_router::getLang()
  */
 PHP_METHOD(gene_router, getLang) {
	 if (GENE_REQ(lang)) {
		 RETURN_STRING(GENE_REQ(lang));
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
 
 