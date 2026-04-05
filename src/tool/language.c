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
#include "zend_exceptions.h"
#include "zend_smart_str.h" /* for smart_str (used in file-load path) */

#include "../gene.h"
#include "language.h"
#include "../di/di.h"

zend_class_entry *gene_language_ce;
uint32_t gene_language_offset_dir;
uint32_t gene_language_offset_lang;
uint32_t gene_language_offset_config;
uint32_t gene_language_offset_cached;

static void gene_language_smart_str_release(smart_str *str) {
    if (str->s) {
        zend_string_release(str->s);
        str->s = NULL;
    }
}

static zend_string *gene_language_build_cache_key_zstr(const char *dir, size_t dir_len, const char *lang, size_t lang_len) {
    size_t key_len = dir_len + 1 + lang_len;
    zend_string *s = zend_string_alloc(key_len, 0);
    memcpy(ZSTR_VAL(s), dir, dir_len);
    ZSTR_VAL(s)[dir_len] = ':';
    memcpy(ZSTR_VAL(s) + dir_len + 1, lang, lang_len);
    ZSTR_VAL(s)[key_len] = '\0';
    return s;
}

/* {{{ ARG_INFO */
ZEND_BEGIN_ARG_INFO_EX(gene_language_construct_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, dir)
    ZEND_ARG_INFO(0, defaultLang)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_language_lang_arginfo, 0, 0, 0)
    ZEND_ARG_INFO(0, lang)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_language_call_arginfo, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_language_get_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()
/* }}} */

/*
 * {{{ public Gene\Language::__construct($dir, $defaultLang = 'en')
 */
PHP_METHOD(gene_language, __construct) {
    zval *self = getThis();
    zend_object *obj = Z_OBJ_P(self);
    zval *dir_zv = NULL;
    zval *lang_zv = NULL;
    zval *default_lang_zv = NULL;
    zval *slot;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &dir_zv, &default_lang_zv) == FAILURE) {
        RETURN_NULL();
    }

    /* self::$lang = \Gene\Di::get('lang') ?? 'en'; */
    {
        zend_string *name = zend_string_init(ZEND_STRL("lang"), 0);
        zval *val = gene_di_get(name);
        if (val && Z_TYPE_P(val) == IS_STRING && Z_STRLEN_P(val) > 0) {
            lang_zv = val;
        }
        zend_string_release(name);
    }

    slot = OBJ_PROP(obj, gene_language_offset_lang);
    zval_ptr_dtor(slot);
    if (lang_zv == NULL) {
        if (default_lang_zv && Z_TYPE_P(default_lang_zv) == IS_STRING && Z_STRLEN_P(default_lang_zv) > 0) {
            ZVAL_COPY(slot, default_lang_zv);
        } else {
            ZVAL_STRING(slot, "en");
        }
    } else {
        ZVAL_COPY(slot, lang_zv);
    }

    slot = OBJ_PROP(obj, gene_language_offset_dir);
    zval_ptr_dtor(slot);
    ZVAL_COPY(slot, dir_zv);

    RETURN_NULL();
}
/* }}} */

/*
 * {{{ public Gene\Language::lang($lang = null)
 */
PHP_METHOD(gene_language, lang) {
    zval *self = getThis();
    zval *lang = NULL;
    zend_object *obj = Z_OBJ_P(self);

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &lang) == FAILURE) {
        RETURN_NULL();
    }

    if (lang && Z_TYPE_P(lang) == IS_STRING && Z_STRLEN_P(lang) > 0) {
        zval *slot = OBJ_PROP(obj, gene_language_offset_lang);
        zval_ptr_dtor(slot);
        ZVAL_COPY(slot, lang);

        /* Invalidate cached dir_conf since lang changed */
        zval *cached = OBJ_PROP(obj, gene_language_offset_cached);
        zval_ptr_dtor(cached);
        ZVAL_NULL(cached);
    }

    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public Gene\Language::__call($name, $args)
 */
PHP_METHOD(gene_language, __call) {
    zval *self = getThis();
    zend_string *name = NULL;
    zval *args = NULL;
    zval *first = NULL;
    zend_object *obj = Z_OBJ_P(self);

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &args) == FAILURE) {
        RETURN_NULL();
    }

    /* self::$dir = $name; */
    {
        zval *slot = OBJ_PROP(obj, gene_language_offset_dir);
        zval_ptr_dtor(slot);
        ZVAL_STR_COPY(slot, name);
    }

    /* isset($lang[0]) && self::$lang = $lang[0]; */
    if (args && Z_TYPE_P(args) == IS_ARRAY) {
        first = zend_hash_index_find(Z_ARRVAL_P(args), 0);
        if (first && Z_TYPE_P(first) == IS_STRING && Z_STRLEN_P(first) > 0) {
            zval *slot = OBJ_PROP(obj, gene_language_offset_lang);
            zval_ptr_dtor(slot);
            ZVAL_COPY(slot, first);
        }
    }

    /* Invalidate cached dir_conf since dir (and possibly lang) changed */
    {
        zval *cached = OBJ_PROP(obj, gene_language_offset_cached);
        zval_ptr_dtor(cached);
        ZVAL_NULL(cached);
    }

    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public Gene\Language::__get($name)
 */
PHP_METHOD(gene_language, __get) {
    zend_string *name = NULL;
    zval *self = getThis();
    zend_object *obj = Z_OBJ_P(self);
    zval *config, *dir_zv, *lang_zv, *cached, *dir_conf, *val;
    char *file = NULL;
    char key_stack[128];
    char *key_ptr = key_stack;
    size_t key_len;
    int key_heap = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
        RETURN_NULL();
    }

    /* Fast path: if _cached holds a valid array, look up directly (no key build needed) */
    cached = OBJ_PROP(obj, gene_language_offset_cached);
    if (Z_TYPE_P(cached) == IS_ARRAY) {
        val = zend_hash_find(Z_ARRVAL_P(cached), name);
        if (val) {
            RETURN_ZVAL(val, 1, 0);
        }
        ZVAL_EMPTY_STRING(return_value);
        return;
    }

    /* Read properties via pre-computed offsets (avoids 3 hash lookups per call) */
    dir_zv  = OBJ_PROP(obj, gene_language_offset_dir);
    lang_zv = OBJ_PROP(obj, gene_language_offset_lang);
    config  = OBJ_PROP(obj, gene_language_offset_config);

    if (Z_TYPE_P(config) != IS_ARRAY) {
        zval tmp;
        array_init(&tmp);
        zval_ptr_dtor(config);
        ZVAL_COPY_VALUE(config, &tmp);
    }

    if (Z_TYPE_P(dir_zv) != IS_STRING || Z_STRLEN_P(dir_zv) == 0 ||
        Z_TYPE_P(lang_zv) != IS_STRING || Z_STRLEN_P(lang_zv) == 0) {
        ZVAL_EMPTY_STRING(return_value);
        return;
    }

    /* app_root 未初始化时不尝试加载语言文件，避免异常 */
    if (!GENE_G(app_root)) {
        ZVAL_EMPTY_STRING(return_value);
        return;
    }

    /* Build cache key "dir:lang" using stack buffer (avoids smart_str heap alloc) */
    key_len = Z_STRLEN_P(dir_zv) + 1 + Z_STRLEN_P(lang_zv);
    if (key_len >= sizeof(key_stack)) {
        key_ptr = emalloc(key_len + 1);
        key_heap = 1;
    }
    memcpy(key_ptr, Z_STRVAL_P(dir_zv), Z_STRLEN_P(dir_zv));
    key_ptr[Z_STRLEN_P(dir_zv)] = ':';
    memcpy(key_ptr + Z_STRLEN_P(dir_zv) + 1, Z_STRVAL_P(lang_zv), Z_STRLEN_P(lang_zv));
    key_ptr[key_len] = '\0';

    dir_conf = zend_hash_str_find(Z_ARRVAL_P(config), key_ptr, key_len);

    if (dir_conf == NULL) {
        /* 相当于 init(): APP_ROOT/Language/Dir/Lang.php */
        int len = spprintf(&file, 0, "%s/Language/%c%.*s/%c%.*s.php",
                           GENE_G(app_root),
                           toupper(Z_STRVAL_P(dir_zv)[0]), (int)(Z_STRLEN_P(dir_zv) - 1), Z_STRVAL_P(dir_zv) + 1,
                           toupper(Z_STRVAL_P(lang_zv)[0]), (int)(Z_STRLEN_P(lang_zv) - 1), Z_STRVAL_P(lang_zv) + 1);
        if (len > 0) {
            zval retval;
            zend_file_handle file_handle;
            zend_string *cache_key_zstr;
            ZVAL_UNDEF(&retval);
            zend_stream_init_filename(&file_handle, file);
            if (zend_execute_scripts(ZEND_REQUIRE, &retval, 1, &file_handle) == SUCCESS) {
                if (Z_TYPE(retval) == IS_ARRAY) {
                    zval tmp;
                    ZVAL_COPY(&tmp, &retval);
                    cache_key_zstr = gene_language_build_cache_key_zstr(
                        Z_STRVAL_P(dir_zv), Z_STRLEN_P(dir_zv),
                        Z_STRVAL_P(lang_zv), Z_STRLEN_P(lang_zv));
                    zend_hash_update(Z_ARRVAL_P(config), cache_key_zstr, &tmp);
                    dir_conf = zend_hash_str_find(Z_ARRVAL_P(config), key_ptr, key_len);
                    zend_string_release(cache_key_zstr);
                }
            } else {
                php_error_docref(NULL, E_WARNING, "Unable to load language file %s", file);
            }
            if (Z_TYPE(retval) != IS_UNDEF) {
                zval_ptr_dtor(&retval);
            }
            zend_destroy_file_handle(&file_handle);
            efree(file);
        }
    }

    if (dir_conf && Z_TYPE_P(dir_conf) == IS_ARRAY) {
        /* Cache the resolved dir_conf in _cached for subsequent fast-path lookups */
        ZVAL_COPY(cached, dir_conf);

        val = zend_hash_find(Z_ARRVAL_P(dir_conf), name);
        if (val) {
            if (key_heap) efree(key_ptr);
            RETURN_ZVAL(val, 1, 0);
        }
    }

    if (key_heap) efree(key_ptr);
    ZVAL_EMPTY_STRING(return_value);
}
/* }}} */

/*
 * {{{ gene_language_methods
 */
const zend_function_entry gene_language_methods[] = {
    PHP_ME(gene_language, __construct, gene_language_construct_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(gene_language, lang,        gene_language_lang_arginfo,      ZEND_ACC_PUBLIC)
    PHP_ME(gene_language, __call,      gene_language_call_arginfo,      ZEND_ACC_PUBLIC)
    PHP_ME(gene_language, __get,       gene_language_get_arginfo,       ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION(language)
 */
GENE_MINIT_FUNCTION(language) {
    zend_class_entry gene_language;
    zend_property_info *pi;
    GENE_INIT_CLASS_ENTRY(gene_language, "Gene_Language", "Gene\\Language", gene_language_methods);
    gene_language_ce = zend_register_internal_class(&gene_language);
#if PHP_VERSION_ID >= 80200
    gene_language_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

    zend_declare_property_string(gene_language_ce, GENE_LANGUAGE_DIR,  strlen(GENE_LANGUAGE_DIR), "", ZEND_ACC_PUBLIC);
    zend_declare_property_string(gene_language_ce, GENE_LANGUAGE_LANG, strlen(GENE_LANGUAGE_LANG), "en", ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_language_ce,  GENE_LANGUAGE_CONFIG, strlen(GENE_LANGUAGE_CONFIG), ZEND_ACC_PROTECTED);
    zend_declare_property_null(gene_language_ce,  GENE_LANGUAGE_CACHED, strlen(GENE_LANGUAGE_CACHED), ZEND_ACC_PROTECTED);

    /* Cache property offsets for fast OBJ_PROP access in __get/__call/lang */
    pi = zend_hash_str_find_ptr(&gene_language_ce->properties_info, GENE_LANGUAGE_DIR, strlen(GENE_LANGUAGE_DIR));
    gene_language_offset_dir = pi->offset;
    pi = zend_hash_str_find_ptr(&gene_language_ce->properties_info, GENE_LANGUAGE_LANG, strlen(GENE_LANGUAGE_LANG));
    gene_language_offset_lang = pi->offset;
    pi = zend_hash_str_find_ptr(&gene_language_ce->properties_info, GENE_LANGUAGE_CONFIG, strlen(GENE_LANGUAGE_CONFIG));
    gene_language_offset_config = pi->offset;
    pi = zend_hash_str_find_ptr(&gene_language_ce->properties_info, GENE_LANGUAGE_CACHED, strlen(GENE_LANGUAGE_CACHED));
    gene_language_offset_cached = pi->offset;

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

