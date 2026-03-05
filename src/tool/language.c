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

#include "../gene.h"
#include "language.h"
#include "../di/di.h"

zend_class_entry *gene_language_ce;

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
    zval *dir_zv = NULL;
    zval *lang_zv = NULL;
    zval *default_lang_zv = NULL;

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

    if (lang_zv == NULL) {
        if (default_lang_zv && Z_TYPE_P(default_lang_zv) == IS_STRING && Z_STRLEN_P(default_lang_zv) > 0) {
            zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_LANG), default_lang_zv);
        } else {
            zval tmp;
            ZVAL_STRING(&tmp, "en");
            zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_LANG), &tmp);
            zval_ptr_dtor(&tmp);
        }
    } else {
        zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_LANG), lang_zv);
    }

    zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_DIR), dir_zv);

    RETURN_NULL();
}
/* }}} */

/*
 * {{{ public Gene\Language::lang($lang = null)
 */
PHP_METHOD(gene_language, lang) {
    zval *lang = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &lang) == FAILURE) {
        RETURN_NULL();
    }

    if (lang && Z_TYPE_P(lang) == IS_STRING && Z_STRLEN_P(lang) > 0) {
        zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_LANG), lang);
    }

    RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/*
 * {{{ public Gene\Language::__call($name, $args)
 */
PHP_METHOD(gene_language, __call) {
    zend_string *name = NULL;
    zval *args = NULL;
    zval *first = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &args) == FAILURE) {
        RETURN_NULL();
    }

    /* self::$dir = $name; */
    {
        zval dir;
        ZVAL_STR_COPY(&dir, name);
        zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_DIR), &dir);
        zval_ptr_dtor(&dir);
    }

    /* isset($lang[0]) && self::$lang = $lang[0]; */
    if (args && Z_TYPE_P(args) == IS_ARRAY) {
        first = zend_hash_index_find(Z_ARRVAL_P(args), 0);
        if (first && Z_TYPE_P(first) == IS_STRING && Z_STRLEN_P(first) > 0) {
            zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_LANG), first);
        }
    }

    RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/*
 * {{{ public Gene\Language::__get($name)
 */
PHP_METHOD(gene_language, __get) {
    zend_string *name = NULL;
    zval *config, *dir_zv, *lang_zv, *dir_conf, *val;
    char *file = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
        RETURN_NULL();
    }

    /* 读取静态 config、dir、lang */
    config  = zend_read_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_CONFIG), 1);
    dir_zv  = zend_read_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_DIR), 1);
    lang_zv = zend_read_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_LANG), 1);

    if (Z_TYPE_P(config) != IS_ARRAY) {
        zval tmp;
        array_init(&tmp);
        zend_update_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_CONFIG), &tmp);
        zval_ptr_dtor(&tmp);
        config = zend_read_static_property(gene_language_ce, ZEND_STRL(GENE_LANGUAGE_CONFIG), 1);
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

    dir_conf = zend_hash_find(Z_ARRVAL_P(config), Z_STR_P(dir_zv));

    if (dir_conf == NULL) {
        /* 相当于 init(): APP_ROOT/Language/Dir/Lang.php */
        int len = spprintf(&file, 0, "%s/Language/%c%.*s/%c%.*s.php",
                           GENE_G(app_root),
                           toupper(Z_STRVAL_P(dir_zv)[0]), (int)(Z_STRLEN_P(dir_zv) - 1), Z_STRVAL_P(dir_zv) + 1,
                           toupper(Z_STRVAL_P(lang_zv)[0]), (int)(Z_STRLEN_P(lang_zv) - 1), Z_STRVAL_P(lang_zv) + 1);
        if (len > 0) {
            zval retval;
            zend_file_handle file_handle;
            ZVAL_UNDEF(&retval);
            zend_stream_init_filename(&file_handle, file);
            if (zend_execute_scripts(ZEND_REQUIRE, &retval, 1, &file_handle) == SUCCESS) {
                if (Z_TYPE(retval) == IS_ARRAY) {
                    zval tmp;
                    ZVAL_COPY(&tmp, &retval);
                    zend_hash_update(Z_ARRVAL_P(config), Z_STR_P(dir_zv), &tmp);
                    dir_conf = zend_hash_find(Z_ARRVAL_P(config), Z_STR_P(dir_zv));
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
        val = zend_hash_find(Z_ARRVAL_P(dir_conf), name);
        if (val) {
            RETURN_ZVAL(val, 1, 0);
        }
    }

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
    INIT_CLASS_ENTRY(gene_language,"Gene_Language",gene_language_methods);
    GENE_INIT_CLASS_ENTRY(gene_language, "Gene_Language", "Gene\\Language", gene_language_methods);
    gene_language_ce = zend_register_internal_class(&gene_language);

    /* static properties: dir, lang, config */
    zend_declare_property_string(gene_language_ce, GENE_LANGUAGE_DIR,  strlen(GENE_LANGUAGE_DIR), "", ZEND_ACC_PUBLIC | ZEND_ACC_STATIC);
    zend_declare_property_string(gene_language_ce, GENE_LANGUAGE_LANG, strlen(GENE_LANGUAGE_LANG), "en", ZEND_ACC_PUBLIC | ZEND_ACC_STATIC);
    zend_declare_property_null(gene_language_ce,  GENE_LANGUAGE_CONFIG, strlen(GENE_LANGUAGE_CONFIG), ZEND_ACC_PUBLIC | ZEND_ACC_STATIC);

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

