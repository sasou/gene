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

zend_class_entry * gene_view_ce;

/** {{{ gene_view_display
 */
int gene_view_display(char *file TSRMLS_DC) {
	char *path;
	int path_len;
	if (GENE_G(app_root)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW,
					strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		path_len = spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root),
				GENE_G(app_view), file, GENE_G(app_ext));
	} else {
		path_len = spprintf(&path, 0, "app/%s/%s%s", GENE_VIEW_VIEW, file,
		GENE_VIEW_EXT);
	}
	gene_load_import(path TSRMLS_CC);
	efree(path);
	return 1;
}
/* }}} */

/** {{{ gene_view_display_ext
 */
int gene_view_display_ext(char *file, zend_bool isCompile TSRMLS_DC) {
	char *path, *compile_path, *cpath;
	int path_len, compile_path_len, cpath_len;
	php_stream *stream;
	compile_path_len = spprintf(&compile_path, 0, "%s/Cache/Views/%s.php",
			GENE_G(app_root), file);
	if (isCompile || GENE_G(view_compile)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW,
					strlen(GENE_VIEW_VIEW));
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, strlen(GENE_VIEW_EXT));
		}
		path_len = spprintf(&path, 0, "%s/%s/%s%s", GENE_G(app_root),
				GENE_G(app_view), file, GENE_G(app_ext));
		stream = php_stream_open_wrapper(path, "rb", REPORT_ERRORS, NULL);
		efree(path);
		if (stream == NULL) {
			zend_error(E_WARNING, "%s does not read able", path);
			return 0;
		} else {
			cpath = estrndup(compile_path, compile_path_len);
			cpath_len = php_dirname(cpath, compile_path_len);
			if (check_folder_exists(cpath) == FAILURE) {
				return 0;
			}
			parser_templates(stream, compile_path);
		}
	}
	gene_load_import(compile_path TSRMLS_CC);
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
static int parser_templates(php_stream *stream, char *compile_path) {
	zval replace_val;
	int result_len, i;
	zend_string *a, *result;

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
			"/\\{contains\\}/" };

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
			"<?php $this::contains()?>" };

	char subject[1024];
	smart_str content = { 0 };

	while (!php_stream_eof(stream)) {
		if (!php_stream_gets(stream, subject, 1024)) {
			break;
		}
		smart_str_appendl(&content, subject, strlen(subject));
	}

	smart_str_0(&content);

	for (i = 0; i < PARSER_NUMS; i++) {
		a = zend_string_init(regex[i], strlen(regex[i]), 0);
		ZVAL_STRINGL(&replace_val, replace[i], strlen(replace[i]));
		if ((result = php_pcre_replace(a, NULL, content.s->val, content.s->len,
				&replace_val, 0, -1, &result_len)) != NULL) {
			smart_str_setl(&content, ZSTR_VAL(result), ZSTR_LEN(result));
		} else {
			smart_str_free(&content);
		}
	}
	php_stream_close(stream);

	stream = php_stream_open_wrapper(compile_path, "wb", REPORT_ERRORS, NULL);

	php_stream_write_string(stream, ZSTR_VAL(result));

	if (stream == NULL) {
		zend_error(E_WARNING, "%s does not read able", compile_path);
		return 1;
	}

	php_stream_close(stream);

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
	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "|l", &debug)
			== FAILURE) {
		RETURN_NULL()
		;
	}
}
/* }}} */

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, display) {
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

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, template) {
	zend_string *file;
	zend_bool isCompile;

	if (zend_parse_parameters(ZEND_NUM_ARGS()TSRMLS_CC, "S|b", &file,
			&isCompile) == FAILURE) {
		return;
	}
	gene_view_display_ext(ZSTR_VAL(file), isCompile TSRMLS_CC);
}
/* }}} */

/*
 * {{{ gene_view_methods
 */
zend_function_entry gene_view_methods[] = {
PHP_ME(gene_view, display, NULL, ZEND_ACC_PUBLIC)
PHP_ME(gene_view, template, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
PHP_ME(gene_view, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR) { NULL,
NULL, NULL } };
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(view) {
	zend_class_entry gene_view;
	GENE_INIT_CLASS_ENTRY(gene_view, "Gene_View", "Gene\\View",
			gene_view_methods);
	gene_view_ce = zend_register_internal_class(&gene_view TSRMLS_CC);

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
