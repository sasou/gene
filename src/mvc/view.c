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
#include "main/php_streams.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include "zend_smart_str.h"
#include "ext/pcre/php_pcre.h"
#include "ext/standard/php_filestat.h"
#include "ext/standard/php_string.h"

#include "../gene.h"
#include "../mvc/view.h"
#include "../factory/load.h"
#include "../di/di.h"
#include "../common/common.h"

zend_class_entry * gene_view_ce;


ZEND_BEGIN_ARG_INFO_EX(gene_view_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_view_arg_get, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_view_arg_set, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_view_arg_display, 0, 0, 1)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, parent_file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_view_arg_display_ext, 0, 0, 1)
    ZEND_ARG_INFO(0, file)
    ZEND_ARG_INFO(0, parent_file)
	ZEND_ARG_INFO(0, isCompile)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_view_arg_assign, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(gene_view_arg_scope, 0, 0, 1)
    ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_view_arg_url, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

/** {{{ gene_view_contains
 */
void gene_view_contains(char *file, zval *ret) {
	char *path;
	if (GENE_G(app_root)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root), GENE_G(app_view), file, GENE_G(app_ext));
	} else {
		spprintf(&path, 0, "app/%s/%s%s", GENE_VIEW_VIEW, file, GENE_VIEW_EXT);
	}
	ZVAL_STRING(ret, path);
	efree(path);
}
/* }}} */

/** {{{ gene_view_display_ext
 */
void gene_view_contains_ext(char *file, zend_bool isCompile, zval *ret) {
	char *path, *compile_path, *cpath;
	size_t compile_path_len;
	php_stream *stream = NULL;
	compile_path_len = spprintf(&compile_path, 0, "%s/Cache/Views/%s.php", GENE_G(app_root), file);
	if (isCompile || GENE_G(view_compile)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root), GENE_G(app_view), file, GENE_G(app_ext));
		stream = php_stream_open_wrapper(path, "rb", REPORT_ERRORS, NULL);
		if (stream == NULL) {
			zend_error(E_WARNING, "%s does not read able", path);
			efree(path);
		} else {
			efree(path);
			cpath = estrndup(compile_path, compile_path_len);
			php_dirname(cpath, compile_path_len);
			if (check_folder_exists(cpath) == FAILURE) {
				efree(cpath);
			}
			efree(cpath);
			parser_templates(&stream, compile_path);
			php_stream_close(stream);
		}
	}
	ZVAL_STRING(ret, compile_path);
	efree(compile_path);
}
/* }}} */


/** {{{ gene_view_display
 */
int gene_view_display(char *file, zval *obj, zend_array *symbol_table) {
	char *path;
	if (GENE_G(app_root)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root), GENE_G(app_view), file, GENE_G(app_ext));
	} else {
		spprintf(&path, 0, "app/%s/%s%s", GENE_VIEW_VIEW, file, GENE_VIEW_EXT);
	}
	if(!gene_load_import(path, obj, symbol_table)) {
		php_error_docref(NULL, E_WARNING, "Unable to load view file %s", path);
	}
	efree(path);
	return 1;
}
/* }}} */

/** {{{ gene_view_display_ext
 */
int gene_view_display_ext(char *file, zend_bool isCompile, zval *obj, zend_array *symbol_table) {
	char *path, *compile_path, *cpath;
	size_t compile_path_len;
	php_stream *stream = NULL;
	compile_path_len = spprintf(&compile_path, 0, "%s/Cache/Views/%s.php", GENE_G(app_root), file);
	if (isCompile || GENE_G(view_compile)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root), GENE_G(app_view), file, GENE_G(app_ext));
		stream = php_stream_open_wrapper(path, "rb", REPORT_ERRORS, NULL);
		if (stream == NULL) {
			zend_error(E_WARNING, "%s does not read able", path);
			efree(path);
			return 0;
		} else {
			efree(path);
			cpath = estrndup(compile_path, compile_path_len);
			php_dirname(cpath, compile_path_len);
			if (check_folder_exists(cpath) == FAILURE) {
				efree(cpath);
				return 0;
			}
			efree(cpath);
			parser_templates(&stream, compile_path);
			php_stream_close(stream);
		}
	}
	if (!gene_load_import(compile_path, obj, symbol_table)) {
		php_error_docref(NULL, E_WARNING, "Unable to load view file %s", compile_path);
	}
	efree(compile_path);
	return 1;
}
/* }}} */

/** {{{ static int check_folder_exists(char *fullpath)
 */
static int check_folder_exists(char *fullpath) {
	php_stream_statbuf ssb;

	if (FAILURE == php_stream_stat_path(fullpath, &ssb)) {
		if (!php_stream_mkdir(fullpath, 0755, PHP_STREAM_MKDIR_RECURSIVE,
				NULL)) {
			zend_error(E_ERROR, "could not create directory \"%s\"", fullpath);
			return FAILURE;
		}
	}
	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * {{{ gene_view
 */
static int parser_templates(php_stream **stream, char *compile_path) {
	size_t result_len;// @suppress("Type cannot be resolved")
	int i;
	zend_string *arg,*ret;
	php_stream *CacheStream = NULL;
	char *subject = NULL,*result = NULL;

	char regex[PARSER_NUMS][100] = { "/([\\n\\r]+)\\t+/s",
			"/\\<\\!\\-\\-\\{(.+?)\\}\\-\\-\\>/s", "/\\{template\\s+(\\S+)\\}/",
			"/\\{{if\\s+(.+?)\\}}/", "/\\{{else\\}}/", "/\\{{\\/if\\}}/",
			"/\\{if\\s+(.+?)\\}/is", "/\\{elseif\\s+(.+?)\\}/is",
			"/\\{else\\}/i", "/\\{\\/if\\}/i", "/\\``if\\s+(.+?)\\``/",
			"/\\``else``/", "/\\``\\/if\\``/",
			"/\\{loop\\s+(\\S+)\\s+(\\S+)\\}/s",
			"/\\{loop\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\}/s", "/\\{\\/loop\\}/i",
			"/\\{(\\$[a-zA-Z_\\x7f-\\xff][a-zA-Z0-9_\\x7f-\\xff]*)\\}/",
			"/\\{(\\$[a-zA-Z0-9_\\[\\]'\"\\$\\x7f-\\xff]+)\\}/s",
			"/\\{([A-Z_\\x7f-\\xff][A-Z0-9_\x7f-\xff]*)\\}/s",
			"/[\\n\\r\\t]*\\{eval\\s+(.+?)\\s*\\}[\\n\\r\\t]*/is",
			"/\\{(\\$[a-z0-9_]+)\\.([a-z0-9_]+)\\}/i",
			"/\\{(\\$[a-z0-9_]+)\\.([a-z0-9_]+).([a-z0-9_]+)\\}/i",
			"/\\{for\\s+(.+?)\\}/is", "/\\{\\/for\\}/i",
			"/\\{\\:\\s?(.+?)\\}/is", "/\\{\\~\\s?(.+?)\\}/is",
			"/\\{contains\\}/",
			"/\\{containsExt\\}/" };

	char replace[PARSER_NUMS][100] = { "\\1", "{\\1}",
			"<?php gene_view::template('\\1')?>", "``if \\1``", "``else``",
			"``/if``", "<?php if(\\1) { ?>", "<?php } elseif(\\1) { ?>",
			"<?php } else { ?>", "<?php } ?>", "{{if \\1}}", "{{else}}",
			"{{/if}}", "<?php if(is_array(\\1)) { foreach(\\1 as \\2) { ?>",
			"<?php if(is_array(\\1)) { foreach(\\1 as \\2 => \\3) { ?>",
			"<?php } } ?>", "<?php echo htmlspecialchars(\\1, ENT_COMPAT);?>",
			"<?php echo htmlspecialchars(\\1, ENT_COMPAT);?>",
			"<?php echo \\1;?>", "<?php \\1?>", "<?php echo \\1[\'\\2\']; ?>",
			"<?php echo \\1[\'\\2\'][\'\\3\']; ?>", "<?php for(\\1) { ?>",
			"<?php } ?>", "<?php echo \\1; ?>", "<?php \\1; ?>",
			"<?php require $this::contains()?>",
			"<?php require $this::containsExt()?>" };

	smart_str content = { 0 };
	subject = (char *) ecalloc(1024, sizeof(char));

	while (!php_stream_eof(*stream)) {
		if (!php_stream_gets(*stream, subject, 1024)) {
			break;
		}
		smart_str_appendl(&content, subject, strlen(subject));
	}
	efree(subject);
	subject = NULL;
	smart_str_0(&content);

	result = estrndup(content.s->val, content.s->len);
	result_len = content.s->len;
	smart_str_free(&content);

	for (i = 0; i < PARSER_NUMS; i++) {
		arg = zend_string_init(regex[i], strlen(regex[i]), 0);
#if PHP_VERSION_ID < 70300
	int count;
#else
	size_t count;
#endif

#if PHP_VERSION_ID < 70200
		zval replace_val;
		ZVAL_STRINGL(&replace_val, replace[i], strlen(replace[i]));
		if ((ret = php_pcre_replace(arg, NULL, result, result_len, &replace_val, 0, -1, &count)) != NULL) {
			efree(result);
			result = estrndup(ret->val, ret->len);
			result_len = ret->len;
			zend_string_release(ret);
		}
		zend_string_release(arg);
		zval_ptr_dtor(&replace_val);
#else
		zend_string *replace_val;
		replace_val = zend_string_init(replace[i], strlen(replace[i]), 0);
		if ((ret = php_pcre_replace(arg, NULL, result, result_len, replace_val, -1, &count)) != NULL) {
			efree(result);
			result = estrndup(ret->val, ret->len);
			result_len = ret->len;
			zend_string_release(ret);
		}
		zend_string_release(arg);
		zend_string_release(replace_val);
#endif
	}

	CacheStream = php_stream_open_wrapper(compile_path, "wb", REPORT_ERRORS, NULL);
	php_stream_write_string(CacheStream, result);
	if (stream == NULL) {
		zend_error(E_WARNING, "%s does not read able", compile_path);
		return 1;
	}
	php_stream_close(CacheStream);

	if (result != NULL)
		efree(result);
	return 0;
}
/* }}} */


/*
 *  {{{ int gene_view_clear_vars()
 */
void gene_view_clear_vars() {
	zval *no = NULL, *vars = NULL;
	vars = zend_read_static_property(gene_view_ce, GENE_VIEW_VARS, strlen(GENE_VIEW_VARS), 1);
	if (Z_TYPE_P(vars) == IS_ARRAY) {
		no = zend_read_static_property(gene_view_ce, GENE_VIEW_VERSION_NO, strlen(GENE_VIEW_VERSION_NO), 1);
		zend_long num = Z_LVAL_P(no);
		zend_hash_index_del(Z_ARRVAL_P(vars), num);
	}
}
/* }}} */

/*
 *  {{{ int gene_view_get_vars()
 */
zval *gene_view_get_vars() {
	zval *no = NULL, *vars = NULL, *nodata = NULL;
	no = zend_read_static_property(gene_view_ce, GENE_VIEW_VERSION_NO, strlen(GENE_VIEW_VERSION_NO), 1);
	zend_long num = Z_LVAL_P(no);
	vars = zend_read_static_property(gene_view_ce, GENE_VIEW_VARS, strlen(GENE_VIEW_VARS), 1);
	if (Z_TYPE_P(vars) != IS_ARRAY) {
		return NULL;
	}
	nodata = zend_hash_index_find(Z_ARRVAL_P(vars), num);
	return Z_TYPE_P(nodata) == IS_ARRAY ? nodata : NULL;
}
/* }}} */

/*
 *  {{{ int gene_view_set_vars(zend_string *name, zval *value)
 */
int gene_view_set_vars(zend_string *name, zval *value) {
	zval *vars = NULL, *no = NULL, *nodata = NULL;
	vars = zend_read_static_property(gene_view_ce, GENE_VIEW_VARS, strlen(GENE_VIEW_VARS), 1);
	no = zend_read_static_property(gene_view_ce, GENE_VIEW_VERSION_NO, strlen(GENE_VIEW_VERSION_NO), 1);
	zend_long num = Z_LVAL_P(no);

	if (Z_TYPE_P(vars) != IS_ARRAY) {
		zval params, nodata_tmp;
		array_init(&params);
		array_init(&nodata_tmp);
		Z_TRY_ADDREF_P(value);
		zend_hash_update(Z_ARRVAL(nodata_tmp), name, value);
		zend_hash_index_update(Z_ARRVAL(params), num, &nodata_tmp);
		zend_update_static_property(gene_view_ce, GENE_VIEW_VARS, strlen(GENE_VIEW_VARS), &params);
		zval_ptr_dtor(&params);
	} else {
		nodata = zend_hash_index_find(Z_ARRVAL_P(vars), num);
		if (nodata == NULL || Z_TYPE_P(nodata) != IS_ARRAY) {
			zval nodata_tmp;
			array_init(&nodata_tmp);
			Z_TRY_ADDREF_P(value);
			zend_hash_update(Z_ARRVAL(nodata_tmp), name, value);
			zend_hash_index_update(Z_ARRVAL_P(vars), num, &nodata_tmp);
		} else {
			Z_TRY_ADDREF_P(value);
			zend_hash_update(Z_ARRVAL_P(nodata), name, value);
		}
	}
	return 1;
}
/* }}} */

/*
 * {{{ gene_view
 */
PHP_METHOD(gene_view, __construct) {
	long debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}

}
/* }}} */

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, display) {
	zend_string *file, *parent_file = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S", &file, &parent_file) == FAILURE) {
		return;
	}
	zval *self = getThis();
	zval *vars = gene_view_get_vars();
	zend_array *table = (vars && Z_TYPE_P(vars) == IS_ARRAY) ? Z_ARRVAL_P(vars) : NULL;

	if (parent_file && ZSTR_LEN(parent_file) > 0) {
		if (GENE_G(child_views)) {
			efree(GENE_G(child_views));
			GENE_G(child_views) = NULL;
		}
		GENE_G(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display(ZSTR_VAL(parent_file), self, table);
	} else {
		gene_view_display(ZSTR_VAL(file), self, table);
	}
	if (table) {
		gene_view_clear_vars();
	}
}
/* }}} */

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, displayExt) {
	zend_string *file, *parent_file = NULL;
	zend_bool isCompile = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|Sb", &file, &parent_file, &isCompile) == FAILURE) {
		return;
	}
	zval *self = getThis();
	zval *vars = gene_view_get_vars();
	zend_array *table = (vars && Z_TYPE_P(vars) == IS_ARRAY) ? Z_ARRVAL_P(vars) : NULL;

	if (parent_file && ZSTR_LEN(parent_file)) {
		if (GENE_G(child_views)) {
			efree(GENE_G(child_views));
			GENE_G(child_views) = NULL;
		}
		GENE_G(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display_ext(ZSTR_VAL(parent_file), isCompile , self, table);
	} else {
		gene_view_display_ext(ZSTR_VAL(file), isCompile, self, table);
	}
	if (table) {
		gene_view_clear_vars();
	}
}
/* }}} */

PHP_METHOD(gene_view, assign) {
	zval *value;
	zend_string *name;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	gene_view_set_vars(name, value);
	RETURN_NULL();
}
/* }}} */

/** {{{ public gene_view::contains(string $file)
 */
PHP_METHOD(gene_view, contains) {
	zval child;
	gene_view_contains(GENE_G(child_views), &child);
	RETURN_ZVAL(&child, 1, 1);
}
/* }}} */

/** {{{ public gene_view::containsExt(string $file)
 */
PHP_METHOD(gene_view, containsExt) {
	zval child;
	gene_view_contains_ext(GENE_G(child_views), 0, &child);
	RETURN_ZVAL(&child, 1, 1);
}
/* }}} */

/** {{{ public gene_view::url(string $path)
 *  返回带当前语言前缀的 URL，如 url("login.html") => "/en/login.html"
 */
PHP_METHOD(gene_view, url) {
	zend_string *path_str;
	char *out = NULL;
	const char *p;
	size_t path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &path_str) == FAILURE) {
		return;
	}
	p = ZSTR_VAL(path_str);
	path_len = ZSTR_LEN(path_str);
	/* 跳过 path 前导斜杠 */
	for (; path_len > 0 && *p == '/'; p++, path_len--) {}
	if (path_len == 0) {
		/* 如果只有斜杠，也加上语言前缀 */
		if (GENE_G(lang) && GENE_G(lang)[0] != '\0') {
			spprintf(&out, 0, "/%s/", GENE_G(lang));
			RETVAL_STRING(out);
			efree(out);
		} else {
			RETURN_STRING("/");
		}
		return;
	}
	if (GENE_G(lang) && GENE_G(lang)[0] != '\0') {
		spprintf(&out, 0, "/%s/%.*s", GENE_G(lang), (int)path_len, p);
	} else {
		spprintf(&out, 0, "/%.*s", (int)path_len, p);
	}
	RETVAL_STRING(out);
	efree(out);
}
/* }}} */

/*
 * {{{ gene_view
 */
PHP_METHOD(gene_view, __set)
{
	zend_string *name;
	zval *value;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	zval class_name;
	gene_class_name(&class_name);
	if (gene_di_set_class(Z_STR(class_name), name, value)) {
		zval_ptr_dtor(&class_name);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&class_name);
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_view
 */
PHP_METHOD(gene_view, scope)
{
	zend_long num = 0;
	zval *no = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &num) == FAILURE) {
		return;
	}
	if (num == 0) {
		no = zend_read_static_property(gene_view_ce, GENE_VIEW_VERSION_NO, strlen(GENE_VIEW_VERSION_NO), 1);
		num = Z_LVAL_P(no);
		num = num + 1;
	}
	zend_update_static_property_long(gene_view_ce, GENE_VIEW_VERSION_NO, strlen(GENE_VIEW_VERSION_NO), num);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_view
 */
PHP_METHOD(gene_view, __get)
{
	zval *pzval = NULL;
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		zval class_name;
		gene_class_name(&class_name);
		pzval = gene_di_get_class(Z_STR(class_name), name);
		if (pzval) {
			zval_ptr_dtor(&class_name);
			RETURN_ZVAL(pzval, 1, 0);
		}
		zval_ptr_dtor(&class_name);
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_view_methods
 */
zend_function_entry gene_view_methods[] = {
	PHP_ME(gene_view, __construct, gene_view_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_view, display, gene_view_arg_display, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, displayExt, gene_view_arg_display_ext, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, assign, gene_view_arg_assign, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, contains, gene_view_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, containsExt, gene_view_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, url, gene_view_arg_url, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, scope, gene_view_arg_scope, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, __get, gene_view_arg_get, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, __set, gene_view_arg_set, ZEND_ACC_PUBLIC)
	{ NULL,NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(view) {
	zend_class_entry gene_view;
	GENE_INIT_CLASS_ENTRY(gene_view, "Gene_View", "Gene\\View", gene_view_methods);
	gene_view_ce = zend_register_internal_class(&gene_view);

	zend_declare_property_null(gene_view_ce, GENE_VIEW_VARS, strlen(GENE_VIEW_VARS), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_long(gene_view_ce, GENE_VIEW_VERSION_NO, strlen(GENE_VIEW_VERSION_NO), 0, ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
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
