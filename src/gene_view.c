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

#include "php_gene.h"
#include "gene_view.h"
#include "gene_load.h"
#include "gene_di.h"
#include "gene_common.h"

zend_class_entry * gene_view_ce;


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

/** {{{ gene_view_display
 */
int gene_view_display(char *file TSRMLS_DC) {
	char *path;
	int path_len;
	if (GENE_G(app_root)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		path_len = spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root), GENE_G(app_view), file, GENE_G(app_ext));
	} else {
		path_len = spprintf(&path, 0, "app/%s/%s%s", GENE_VIEW_VIEW, file, GENE_VIEW_EXT);
	}
	if(!gene_load_import(path TSRMLS_CC)) {
		php_error_docref(NULL, E_WARNING, "Unable to load view file %s", path);
	}
	efree(path);
	return 1;
}
/* }}} */

/** {{{ gene_view_display_ext
 */
int gene_view_display_ext(char *file, zend_bool isCompile TSRMLS_DC) {
	char *path, *compile_path, *cpath;
	int path_len, compile_path_len, cpath_len;
	php_stream *stream = NULL;
	compile_path_len = spprintf(&compile_path, 0, "%s/Cache/Views/%s.php", GENE_G(app_root), file);
	if (isCompile || GENE_G(view_compile)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		path_len = spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root), GENE_G(app_view), file, GENE_G(app_ext));
		stream = php_stream_open_wrapper(path, "rb", REPORT_ERRORS, NULL);
		if (stream == NULL) {
			zend_error(E_WARNING, "%s does not read able", path);
			efree(path);
			return 0;
		} else {
			efree(path);
			cpath = estrndup(compile_path, compile_path_len);
			cpath_len = php_dirname(cpath, compile_path_len);
			if (check_folder_exists(cpath) == FAILURE) {
				efree(cpath);
				return 0;
			}
			efree(cpath);
			parser_templates(&stream, compile_path);
			php_stream_close(stream);
		}
	}
	if (!gene_load_import(compile_path TSRMLS_CC)) {
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
	return SUCCESS;
}
/* }}} */

/*
 * {{{ gene_view
 */
static int parser_templates(php_stream **stream, char *compile_path) {
	zval replace_val;
	int result_len, i;
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
			"<?php $this::contains()?>",
			"<?php $this::containsExt()?>" };

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
		ZVAL_STRINGL(&replace_val, replace[i], strlen(replace[i]));
		if ((ret = php_pcre_replace(arg, NULL, result, result_len, &replace_val, 0, -1, &result_len)) != NULL) {
			efree(result);
			result = estrndup(ret->val, ret->len);
			result_len = ret->len;
			zend_string_free(ret);
		}
		zend_string_free(arg);
		zval_ptr_dtor(&replace_val);
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
 * {{{ gene_view
 */
PHP_METHOD(gene_view, __construct) {
	long debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}
}
/* }}} */

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, display) {
	zend_string *file, *parent_file = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|S", &file, &parent_file) == FAILURE) {
		return;
	}
	if (parent_file && ZSTR_LEN(parent_file) > 0) {
		if (GENE_G(child_views)) {
			efree(GENE_G(child_views));
			GENE_G(child_views) = NULL;
		}
		GENE_G(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display(ZSTR_VAL(parent_file) TSRMLS_CC);
	} else {
		gene_view_display(ZSTR_VAL(file) TSRMLS_CC);
	}
}
/* }}} */

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, displayExt) {
	zend_string *file, *parent_file = NULL;
	zend_bool isCompile = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|Sb", &file, &parent_file, &isCompile) == FAILURE) {
		return;
	}
	if (parent_file && ZSTR_LEN(parent_file)) {
		if (GENE_G(child_views)) {
			efree(GENE_G(child_views));
			GENE_G(child_views) = NULL;
		}
		GENE_G(child_views) = estrndup(ZSTR_VAL(file), ZSTR_LEN(file));
		gene_view_display_ext(ZSTR_VAL(parent_file), isCompile TSRMLS_CC);
	} else {
		gene_view_display_ext(ZSTR_VAL(file), isCompile TSRMLS_CC);
	}
}
/* }}} */

/** {{{ public gene_view::contains(string $file)
 */
PHP_METHOD(gene_view, contains) {
	gene_view_display(GENE_G(child_views) TSRMLS_CC);
}
/* }}} */

/** {{{ public gene_view::containsExt(string $file)
 */
PHP_METHOD(gene_view, containsExt) {
	gene_view_display_ext(GENE_G(child_views), 0 TSRMLS_CC);
}
/* }}} */

/*
 * {{{ gene_view
 */
PHP_METHOD(gene_view, __set)
{
	zend_string *name;
	zval *value, *props = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	props = zend_read_property(gene_view_ce, getThis(), ZEND_STRL(GENE_VIEW_ATTR), 1, NULL);
	if (Z_TYPE_P(props) == IS_NULL) {
		zval prop;
		array_init(&prop);
		if (zend_hash_update(Z_ARRVAL(prop), name, value) != NULL) {
			Z_TRY_ADDREF_P(value);
		}
		zend_update_property(gene_view_ce, getThis(), ZEND_STRL(GENE_VIEW_ATTR), &prop);
		zval_ptr_dtor(&prop);
	} else {
		if (zend_hash_update(Z_ARRVAL_P(props), name, value) != NULL) {
			Z_TRY_ADDREF_P(value);
			RETURN_TRUE;
		}
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_view
 */
PHP_METHOD(gene_view, __get)
{
	zval *pzval = NULL, *props = NULL,*obj = getThis();
	zend_string *name = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}

	if (!name) {
		RETURN_NULL();
	} else {
		props = zend_read_property(gene_view_ce, obj, ZEND_STRL(GENE_VIEW_ATTR), 1, NULL);
		if (Z_TYPE_P(props) == IS_NULL) {
			zval prop;
			array_init(&prop);
			zend_update_property(gene_view_ce, obj, ZEND_STRL(GENE_VIEW_ATTR), &prop);
			zval_ptr_dtor(&prop);
			pzval = gene_di_get_easy(name);
			if (pzval) {
				RETURN_ZVAL(pzval, 1, 0);
			}
			RETURN_NULL();
		} else {
			pzval = zend_hash_find(Z_ARRVAL_P(props), name);
			if (pzval == NULL) {
				pzval = gene_di_get_easy(name);
				if (pzval) {
					RETURN_ZVAL(pzval, 1, 0);
				}
				RETURN_NULL();
			}
			RETURN_ZVAL(pzval, 1, 0);
		}
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_view_methods
 */
zend_function_entry gene_view_methods[] = {
	PHP_ME(gene_view, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(gene_view, display, gene_view_arg_display, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, displayExt, gene_view_arg_display_ext, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, contains, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(gene_view, containsExt, NULL, ZEND_ACC_PUBLIC)
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
	gene_view_ce = zend_register_internal_class(&gene_view TSRMLS_CC);

	//var
	zend_declare_property_null(gene_view_ce, ZEND_STRL(GENE_VIEW_ATTR), ZEND_ACC_PUBLIC);
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
