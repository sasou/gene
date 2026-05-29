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

static char *gene_view_app_base_path() {
	if (GENE_G(app_root) && GENE_G(app_root)[0] != '\0') {
		return GENE_G(app_root);
	}
	return NULL;
}

/* forward declarations for local helpers */
static int check_folder_exists(char *fullpath);
static int parser_templates(php_stream **stream, char *compile_path);

/* [GENE_PERF:2026-05-29 §3.1] When gene.view_compile_check_mtime is enabled,
 * skip recompilation if the compiled output already exists and is at least as
 * new as the source template. Returns 1 if a (re)compile is required, 0 if the
 * existing compiled file is up to date. When the INI flag is off, always
 * returns 1 to preserve the legacy every-request recompile behavior. */
static int view_compile_needs_rebuild(const char *src_path, const char *compile_path) {
	php_stream_statbuf src_ssb, dst_ssb;

	if (!GENE_G(view_compile_check_mtime)) {
		return 1;
	}
	/* Compiled file missing -> must compile. */
	if (php_stream_stat_path((char *) compile_path, &dst_ssb) != SUCCESS) {
		return 1;
	}
	/* Source missing/unstattable -> fall back to recompile (source open will
	 * surface the real error downstream). */
	if (php_stream_stat_path((char *) src_path, &src_ssb) != SUCCESS) {
		return 1;
	}
	/* Recompile only when the source is strictly newer than the compiled file. */
	if (src_ssb.sb.st_mtime > dst_ssb.sb.st_mtime) {
		return 1;
	}
	return 0;
}

/*
 * NOTE(audit 2026-05-04 #9 "视图变量浅拷贝"):
 *   本函数已经是浅拷贝实现。ZVAL_COPY 对 refcounted 类型（数组/对象/字符串）
 *   仅递增引用计数、对标量直接复制值，不会克隆数组或对象内容。模板在执行过程中
 *   对视图变量的写操作依赖 Zend 的 COW（refcount > 1 时自动分离）保持隔离。
 *
 *   不可进一步"共享 HashTable + GC_ADDREF"：Zend 执行器会原地修改 symbol_table
 *   （新建变量、unset、标量重绑定等），HashTable 无 COW，会污染调用方 vars。
 *
 *   因此此处不存在"深拷贝"优化空间。请勿将此函数重复列入拷贝开销审计项。
 */
zend_array *gene_view_build_symbol_table(zval *vars) {
	zend_array *table;
	zend_ulong idx;
	zend_string *key;
	zval *val;
	uint32_t n;

	if (vars == NULL || Z_TYPE_P(vars) != IS_ARRAY) {
		return NULL;
	}

	/* 空 vars 快速路径：省去 ALLOC_HASHTABLE + zend_hash_init 的零元素分配。
	 * 调用方 gene_load_import 对 NULL symbol_table 已有合法处理。 */
	n = zend_hash_num_elements(Z_ARRVAL_P(vars));
	if (n == 0) {
		return NULL;
	}

	ALLOC_HASHTABLE(table);
	zend_hash_init(table, n, NULL, ZVAL_PTR_DTOR, 0);

	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(vars), idx, key, val) {
		zval tmp;
		ZVAL_COPY(&tmp, val);  /* 浅拷贝：refcount++ / 标量值拷贝 */
		if (key) {
			zend_hash_update(table, key, &tmp);
		} else {
			zend_hash_index_update(table, idx, &tmp);
		}
	} ZEND_HASH_FOREACH_END();

	return table;
}


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
	char *path, *base_path;
	if (!file || file[0] == '\0') {
		ZVAL_FALSE(ret);
		return;
	}
	if (strstr(file, "..") != NULL) {
		php_error_docref(NULL, E_WARNING, "Path traversal detected in view file: %s", file);
		ZVAL_FALSE(ret);
		return;
	}
	base_path = gene_view_app_base_path();
	char path_buf[512];
	int path_heap = 0;
	size_t path_len = 0;
	size_t file_len = strlen(file);
	if (base_path) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, GENE_VIEW_VIEW_LEN);
			GENE_G(app_view_len) = GENE_VIEW_VIEW_LEN;
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, GENE_VIEW_EXT_LEN);
			GENE_G(app_ext_len) = GENE_VIEW_EXT_LEN;
		}
		path_len = GENE_G(app_root_len) + GENE_G(app_view_len) + file_len + GENE_G(app_ext_len) + 3;
		if (path_len >= sizeof(path_buf)) {
			path = emalloc(path_len + 1);
			path_heap = 1;
		} else {
			path = path_buf;
		}
		snprintf(path, path_len + 1, "%s/%s/%s%s", base_path, GENE_G(app_view), file, GENE_G(app_ext));
	} else {
		path_len = sizeof("app/") - 1 + GENE_VIEW_VIEW_LEN + file_len + GENE_VIEW_EXT_LEN + 2;
		if (path_len >= sizeof(path_buf)) {
			path = emalloc(path_len + 1);
			path_heap = 1;
		} else {
			path = path_buf;
		}
		snprintf(path, path_len + 1, "app/%s/%s%s", GENE_VIEW_VIEW, file, GENE_VIEW_EXT);
	}
	ZVAL_STRING(ret, path);
	if (path_heap) {
		efree(path);
	}
}
/* }}} */

/** {{{ gene_view_display_ext
 */
void gene_view_contains_ext(char *file, bool isCompile, zval *ret) {
	char *path, *compile_path, *cpath, *base_path;
	size_t compile_path_len;
	php_stream *stream = NULL;
	if (!file || file[0] == '\0') {
		ZVAL_FALSE(ret);
		return;
	}
	if (strstr(file, "..") != NULL) {
		php_error_docref(NULL, E_WARNING, "Path traversal detected in view file: %s", file);
		ZVAL_FALSE(ret);
		return;
	}
	base_path = gene_view_app_base_path();
	char compile_path_buf[512];
	char path_buf[512];
	int compile_path_heap = 0;
	int path_heap = 0;
	size_t file_len = strlen(file);
	if (base_path) {
		compile_path_len = GENE_G(app_root_len) + (sizeof("/Cache/Views/") - 1) + file_len + (sizeof(".php") - 1);
		if (compile_path_len >= sizeof(compile_path_buf)) {
			compile_path = emalloc(compile_path_len + 1);
			compile_path_heap = 1;
		} else {
			compile_path = compile_path_buf;
		}
		snprintf(compile_path, compile_path_len + 1, "%s/Cache/Views/%s.php", base_path, file);
	} else {
		compile_path_len = (sizeof("app/Cache/Views/") - 1) + file_len + (sizeof(".php") - 1);
		if (compile_path_len >= sizeof(compile_path_buf)) {
			compile_path = emalloc(compile_path_len + 1);
			compile_path_heap = 1;
		} else {
			compile_path = compile_path_buf;
		}
		snprintf(compile_path, compile_path_len + 1, "app/Cache/Views/%s.php", file);
	}
	if (isCompile || GENE_G(view_compile)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, GENE_VIEW_VIEW_LEN);
			GENE_G(app_view_len) = GENE_VIEW_VIEW_LEN;
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, GENE_VIEW_EXT_LEN);
			GENE_G(app_ext_len) = GENE_VIEW_EXT_LEN;
		}
		size_t path_len = 0;
		if (base_path) {
			path_len = GENE_G(app_root_len) + GENE_G(app_view_len) + file_len + GENE_G(app_ext_len) + 2;
			if (path_len >= sizeof(path_buf)) {
				path = emalloc(path_len + 1);
				path_heap = 1;
			} else {
				path = path_buf;
			}
			snprintf(path, path_len + 1, "%s/%s/%s%s", base_path, GENE_G(app_view), file, GENE_G(app_ext));
		} else {
			path_len = (sizeof("app/") - 1) + GENE_VIEW_VIEW_LEN + file_len + GENE_VIEW_EXT_LEN + 1;
			if (path_len >= sizeof(path_buf)) {
				path = emalloc(path_len + 1);
				path_heap = 1;
			} else {
				path = path_buf;
			}
			snprintf(path, path_len + 1, "app/%s/%s%s", GENE_VIEW_VIEW, file, GENE_VIEW_EXT);
		}
		if (view_compile_needs_rebuild(path, compile_path)) {
			stream = php_stream_open_wrapper(path, "rb", REPORT_ERRORS, NULL);
			if (stream == NULL) {
				zend_error(E_WARNING, "%s does not read able", path);
				if (path_heap) {
					efree(path);
				}
			} else {
				if (path_heap) {
					efree(path);
				}
				cpath = estrndup(compile_path, compile_path_len);
				php_dirname(cpath, compile_path_len);
				check_folder_exists(cpath);
				efree(cpath);
				parser_templates(&stream, compile_path);
				php_stream_close(stream);
			}
		} else if (path_heap) {
			efree(path);
		}
	}
	ZVAL_STRING(ret, compile_path);
	if (compile_path_heap) {
		efree(compile_path);
	}
}
/* }}} */


/** {{{ gene_view_display
 */
int gene_view_display(char *file, zval *obj, zend_array *symbol_table) {
	char *path, *base_path;
	int path_heap = 0;
	char path_buf[512];
	size_t file_len;
	if (!file || file[0] == '\0') {
		php_error_docref(NULL, E_WARNING, "Empty view file");
		return 0;
	}
	if (strstr(file, "..") != NULL) {
		php_error_docref(NULL, E_WARNING, "Path traversal detected in view file: %s", file);
		return 0;
	}
	file_len = strlen(file);
	base_path = gene_view_app_base_path();
	if (base_path) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, GENE_VIEW_VIEW_LEN);
			GENE_G(app_view_len) = GENE_VIEW_VIEW_LEN;
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, GENE_VIEW_EXT_LEN);
			GENE_G(app_ext_len) = GENE_VIEW_EXT_LEN;
		}
		size_t path_len = GENE_G(app_root_len) + GENE_G(app_view_len) + file_len + GENE_G(app_ext_len) + 3;
		if (path_len >= sizeof(path_buf)) {
			path = emalloc(path_len + 1);
			path_heap = 1;
		} else {
			path = path_buf;
		}
		snprintf(path, path_len + 1, "%s/%s/%s%s", base_path, GENE_G(app_view), file, GENE_G(app_ext));
	} else {
		size_t path_len = (sizeof("app/") - 1) + GENE_VIEW_VIEW_LEN + file_len + GENE_VIEW_EXT_LEN + 2;
		if (path_len >= sizeof(path_buf)) {
			path = emalloc(path_len + 1);
			path_heap = 1;
		} else {
			path = path_buf;
		}
		snprintf(path, path_len + 1, "app/%s/%s%s", GENE_VIEW_VIEW, file, GENE_VIEW_EXT);
	}
	if(!gene_load_import(path, obj, symbol_table)) {
		php_error_docref(NULL, E_WARNING, "Unable to load view file %s", path);
	}
	if (path_heap) {
		efree(path);
	}
	return 1;
}
/* }}} */

/** {{{ gene_view_display_ext
 */
int gene_view_display_ext(char *file, bool isCompile, zval *obj, zend_array *symbol_table) {
	char *path, *compile_path, *cpath, *base_path;
	size_t compile_path_len;
	php_stream *stream = NULL;
	if (!file || file[0] == '\0') {
		php_error_docref(NULL, E_WARNING, "Empty view file");
		return 0;
	}
	if (strstr(file, "..") != NULL) {
		php_error_docref(NULL, E_WARNING, "Path traversal detected in view file: %s", file);
		return 0;
	}
	base_path = gene_view_app_base_path();
	char compile_path_buf[512];
	char path_buf[512];
	int compile_path_heap = 0;
	int path_heap = 0;
	size_t file_len = strlen(file);
	if (base_path) {
		compile_path_len = GENE_G(app_root_len) + (sizeof("/Cache/Views/") - 1) + file_len + (sizeof(".php") - 1);
		if (compile_path_len >= sizeof(compile_path_buf)) {
			compile_path = emalloc(compile_path_len + 1);
			compile_path_heap = 1;
		} else {
			compile_path = compile_path_buf;
		}
		snprintf(compile_path, compile_path_len + 1, "%s/Cache/Views/%s.php", base_path, file);
	} else {
		compile_path_len = (sizeof("app/Cache/Views/") - 1) + file_len + (sizeof(".php") - 1);
		if (compile_path_len >= sizeof(compile_path_buf)) {
			compile_path = emalloc(compile_path_len + 1);
			compile_path_heap = 1;
		} else {
			compile_path = compile_path_buf;
		}
		snprintf(compile_path, compile_path_len + 1, "app/Cache/Views/%s.php", file);
	}
	if (isCompile || GENE_G(view_compile)) {
		if (!GENE_G(app_view)) {
			GENE_G(app_view) = estrndup(GENE_VIEW_VIEW, GENE_VIEW_VIEW_LEN);
			GENE_G(app_view_len) = GENE_VIEW_VIEW_LEN;
		}
		if (!GENE_G(app_ext)) {
			GENE_G(app_ext) = estrndup(GENE_VIEW_EXT, GENE_VIEW_EXT_LEN);
			GENE_G(app_ext_len) = GENE_VIEW_EXT_LEN;
		}
		size_t path_len = 0;
		if (base_path) {
			path_len = GENE_G(app_root_len) + GENE_G(app_view_len) + file_len + GENE_G(app_ext_len) + 2;
			if (path_len >= sizeof(path_buf)) {
				path = emalloc(path_len + 1);
				path_heap = 1;
			} else {
				path = path_buf;
			}
			snprintf(path, path_len + 1, "%s/%s/%s%s", base_path, GENE_G(app_view), file, GENE_G(app_ext));
		} else {
			path_len = (sizeof("app/") - 1) + GENE_VIEW_VIEW_LEN + file_len + GENE_VIEW_EXT_LEN + 1;
			if (path_len >= sizeof(path_buf)) {
				path = emalloc(path_len + 1);
				path_heap = 1;
			} else {
				path = path_buf;
			}
			snprintf(path, path_len + 1, "app/%s/%s%s", GENE_VIEW_VIEW, file, GENE_VIEW_EXT);
		}
		if (view_compile_needs_rebuild(path, compile_path)) {
			stream = php_stream_open_wrapper(path, "rb", REPORT_ERRORS, NULL);
			if (stream == NULL) {
				zend_error(E_WARNING, "%s does not read able", path);
				if (path_heap) {
					efree(path);
				}
			} else {
				if (path_heap) {
					efree(path);
				}
				cpath = estrndup(compile_path, compile_path_len);
				php_dirname(cpath, compile_path_len);
				check_folder_exists(cpath);
				efree(cpath);
				parser_templates(&stream, compile_path);
				php_stream_close(stream);
			}
		} else if (path_heap) {
			efree(path);
		}
	}
	if(!gene_load_import(compile_path, obj, symbol_table)) {
		php_error_docref(NULL, E_WARNING, "Unable to load view file %s", compile_path);
	}
	if (compile_path_heap) {
		efree(compile_path);
	}
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
	int i;
	zend_string *ret;
	php_stream *CacheStream = NULL;
	zend_string *result = NULL;

	/* [GENE_PERF:2026-05] Cache regex/replace zend_strings as process-lifetime
	 * statics. Eliminates 56× zend_string_init + 56× zend_string_release per
	 * template compile. [GENE_FIX:2026-05-24] gene_interned_str_persistent avoids
	 * the unsafe zend_string_init_interned(...,1) pattern that dangles across
	 * requests under opcache.file_cache_only=1. */
	static zend_string *regex_strs[PARSER_NUMS] = {0};
	static zend_string *replace_strs[PARSER_NUMS] = {0};
	/* [GENE_FIX:2026-05-29] Per-call working arrays. The replace loop below must
	 * NEVER read the static slots directly: gene_interned_str_persistent() only
	 * populates a static slot when the interned string is IS_STR_PERMANENT. Under
	 * opcache.file_cache_only=1 / CLI / no-opcache it returns a REQUEST-SCOPE
	 * string and leaves the slot NULL on purpose. The previous code assigned that
	 * return value back into the static array (regex_strs[i] = ...), so the
	 * statics ended up holding request-scoped pointers that dangle on the next
	 * request — the `if (!regex_strs[0])` guard then skipped re-init and fed freed
	 * zend_strings to php_pcre_replace (use-after-free). We keep the statics for
	 * the permanent fast path only and drive php_pcre_replace from these locals,
	 * which are always valid for the current request. */
	zend_string *regex_use[PARSER_NUMS];
	zend_string *replace_use[PARSER_NUMS];

	if (UNEXPECTED(!regex_strs[0])) {
		static const char *regex_raw[PARSER_NUMS] = {
			"/([\\n\\r]+)\\t+/s",
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
			"/\\{containsExt\\}/"
		};
		static const char *replace_raw[PARSER_NUMS] = {
			"\\1", "{\\1}",
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
			"<?php require $this::containsExt()?>"
		};
		for (i = 0; i < PARSER_NUMS; i++) {
			/* gene_interned_str_persistent() sets the static slot only when the
			 * resolved string is permanent; otherwise the slot stays NULL and a
			 * request-scope string is returned. Capture the return value in the
			 * per-call array so this request always has a valid pointer, while
			 * the statics remain NULL (and the guard re-resolves next request)
			 * in non-permanent environments. */
			regex_use[i] = gene_interned_str_persistent(&regex_strs[i], regex_raw[i], strlen(regex_raw[i]));
			replace_use[i] = gene_interned_str_persistent(&replace_strs[i], replace_raw[i], strlen(replace_raw[i]));
		}
	} else {
		/* Permanent fast path: statics are valid process-lifetime strings. */
		memcpy(regex_use, regex_strs, sizeof(regex_use));
		memcpy(replace_use, replace_strs, sizeof(replace_use));
	}

	/* [GENE_PERF:2026-05] Track result as zend_string* directly — eliminates
	 * 28× estrndup + 28× efree per compile. php_pcre_replace already returns
	 * a zend_string, so we just swap pointers. */
	{
		zend_string *file_content = php_stream_copy_to_mem(*stream, PHP_STREAM_COPY_ALL, 0);
		if (file_content) {
			result = file_content;
		} else {
			result = ZSTR_EMPTY_ALLOC();
			zend_string_addref(result);
		}
	}

	for (i = 0; i < PARSER_NUMS; i++) {
		size_t count;
		ret = php_pcre_replace(regex_use[i], NULL, ZSTR_VAL(result), ZSTR_LEN(result), replace_use[i], -1, &count);
		if (ret != NULL) {
			zend_string_release(result);
			result = ret;
		}
	}

	CacheStream = php_stream_open_wrapper(compile_path, "wb", REPORT_ERRORS, NULL);
	if (CacheStream == NULL) {
		zend_error(E_WARNING, "%s does not write able", compile_path);
		zend_string_release(result);
		return 1;
	}
	php_stream_write(CacheStream, ZSTR_VAL(result), ZSTR_LEN(result));
	php_stream_close(CacheStream);

	zend_string_release(result);
	return 0;
}
/* }}} */


/*
 *  {{{ int gene_view_clear_vars()
 */
void gene_view_clear_vars() {
	gene_request_context *ctx = gene_request_ctx();
	zval *vars = &ctx->view_vars;
	if (Z_TYPE_P(vars) == IS_ARRAY) {
		zend_hash_index_del(Z_ARRVAL_P(vars), ctx->view_scope_no);
	}
}
/* }}} */

/*
 *  {{{ int gene_view_reset_vars()
 */
void gene_view_reset_vars() {
	gene_request_context *ctx = gene_request_ctx();
	if (Z_TYPE(ctx->view_vars) != IS_UNDEF) {
		zval_ptr_dtor(&ctx->view_vars);
	}
	ZVAL_UNDEF(&ctx->view_vars);
	ctx->view_scope_no = 0;
}
/* }}} */

/*
 *  {{{ int gene_view_get_vars()
 */
zval *gene_view_get_vars() {
	gene_request_context *ctx = gene_request_ctx();
	zval *vars = &ctx->view_vars, *nodata = NULL;
	if (Z_TYPE_P(vars) != IS_ARRAY) {
		return NULL;
	}
	nodata = zend_hash_index_find(Z_ARRVAL_P(vars), ctx->view_scope_no);
	return (nodata && Z_TYPE_P(nodata) == IS_ARRAY) ? nodata : NULL;
}
/* }}} */

/*
 *  {{{ int gene_view_set_vars(zend_string *name, zval *value)
 * [GENE_PERF:2026-04-24 v5.5.8] Pre-size both HashTables to amortize the
 * common 5-20 vars per scope / 1-3 scopes per render. Avoids default-8 initial
 * grow on the 9th assign() call for modestly-sized templates.
 */
int gene_view_set_vars(zend_string *name, zval *value) {
	gene_request_context *ctx = gene_request_ctx();
	zval *vars = &ctx->view_vars, *nodata = NULL;
	zend_long num = ctx->view_scope_no;

	if (Z_TYPE_P(vars) != IS_ARRAY) {
		zval params, nodata_tmp;
		array_init_size(&params, 4);        /* scopes: usually 1, rarely > 4 */
		array_init_size(&nodata_tmp, 16);   /* vars per scope: typical 5-20 */
		Z_TRY_ADDREF_P(value);
		zend_hash_update(Z_ARRVAL(nodata_tmp), name, value);
		zend_hash_index_update(Z_ARRVAL(params), num, &nodata_tmp);
		ZVAL_COPY_VALUE(vars, &params);
	} else {
		nodata = zend_hash_index_find(Z_ARRVAL_P(vars), num);
		if (nodata == NULL || Z_TYPE_P(nodata) != IS_ARRAY) {
			zval nodata_tmp;
			array_init_size(&nodata_tmp, 16);
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
	zend_long debug = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &debug) == FAILURE) {
		RETURN_NULL();
	}

}
/* }}} */

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, display) {
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

/** {{{ public gene_view::display(string $file)
 */
PHP_METHOD(gene_view, displayExt) {
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
		gene_view_display_ext(ZSTR_VAL(parent_file), isCompile , self, table);
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
	gene_view_contains(GENE_REQ(child_views), &child);
	RETURN_ZVAL(&child, 1, 1);
}
/* }}} */

/** {{{ public gene_view::containsExt(string $file)
 */
PHP_METHOD(gene_view, containsExt) {
	zval child;
	gene_view_contains_ext(GENE_REQ(child_views), 0, &child);
	RETURN_ZVAL(&child, 1, 1);
}
/* }}} */

/** {{{ public gene_view::url(string $path)
 *  返回带当前语言前缀的 URL，如 url("login.html") => "/en/login.html"
 */
PHP_METHOD(gene_view, url) {
	zend_string *path_str;
	const char *p;
	size_t path_len;
	gene_request_context *ctx;
	const char *lang;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &path_str) == FAILURE) {
		return;
	}
	p = ZSTR_VAL(path_str);
	path_len = ZSTR_LEN(path_str);
	/* 跳过 path 前导斜杠 */
	for (; path_len > 0 && *p == '/'; p++, path_len--) {}
	ctx = gene_request_ctx();
	/* [GENE_PERF:2026-04-27] Use cached ctx->lang_len; eliminates strlen per call. */
	size_t lang_len;
	if (ctx->lang && ctx->lang[0] != '\0') {
		lang = ctx->lang;
		lang_len = ctx->lang_len;
	} else {
		lang = NULL;
		lang_len = 0;
	}
	if (path_len == 0) {
		/* 如果只有斜杠，也加上语言前缀 */
		if (lang) {
			size_t out_len = lang_len + 2;
			char out_buf[256];
			char *out_ptr = out_buf;
			int out_heap = 0;
			if (out_len >= sizeof(out_buf)) {
				out_ptr = emalloc(out_len + 1);
				out_heap = 1;
			}
			out_ptr[0] = '/';
			memcpy(out_ptr + 1, lang, lang_len);
			out_ptr[lang_len + 1] = '/';
			out_ptr[lang_len + 2] = '\0';
			RETVAL_STRINGL(out_ptr, out_len);
			if (out_heap) {
				efree(out_ptr);
			}
		} else {
			RETURN_STRING("/");
		}
		return;
	}
	if (lang) {
		/* [GENE_PERF:2026-04-27] memcpy + RETVAL_STRINGL — same pattern as Gene\Controller::url(). */
		size_t out_len = lang_len + path_len + 2;
		char out_buf[512];
		char *out_ptr = out_buf;
		int out_heap = 0;
		if (out_len >= sizeof(out_buf)) {
			out_ptr = emalloc(out_len + 1);
			out_heap = 1;
		}
		out_ptr[0] = '/';
		memcpy(out_ptr + 1, lang, lang_len);
		out_ptr[lang_len + 1] = '/';
		memcpy(out_ptr + lang_len + 2, p, path_len);
		out_ptr[out_len] = '\0';
		RETVAL_STRINGL(out_ptr, out_len);
		if (out_heap) {
			efree(out_ptr);
		}
	} else {
		size_t out_len = path_len + 1;
		char out_buf[512];
		char *out_ptr = out_buf;
		int out_heap = 0;
		if (out_len >= sizeof(out_buf)) {
			out_ptr = emalloc(out_len + 1);
			out_heap = 1;
		}
		out_ptr[0] = '/';
		memcpy(out_ptr + 1, p, path_len);
		out_ptr[out_len] = '\0';
		RETVAL_STRINGL(out_ptr, out_len);
		if (out_heap) {
			efree(out_ptr);
		}
	}
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
	zend_string *class_name = gene_get_class_name_fast();
	if (!class_name) { RETURN_FALSE; }
	if (gene_di_set_class(class_name, name, value)) {
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_view
 */
PHP_METHOD(gene_view, scope)
{
	zend_long num = 0;
	gene_request_context *ctx = gene_request_ctx();
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &num) == FAILURE) {
		return;
	}
	if (num == 0) {
		num = ctx->view_scope_no + 1;
	}
	ctx->view_scope_no = num;
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
		zend_string *class_name = gene_get_class_name_fast();
		if (!class_name) { RETURN_NULL(); }
		pzval = gene_di_get_class(class_name, name);
		if (pzval) {
			RETURN_ZVAL(pzval, 1, 0);
		}
		RETURN_NULL();
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ gene_view_methods
 */
const zend_function_entry gene_view_methods[] = {
	PHP_ME(gene_view, __construct, gene_view_void_arginfo, ZEND_ACC_PUBLIC)
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
#if PHP_VERSION_ID >= 80200
	gene_view_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

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
