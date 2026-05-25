/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_globals.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_smart_str.h"

#include "../gene.h"
#include "../http/request.h"
#include "../http/webscan.h"

zend_class_entry *gene_webscan_ce;

static const char *gene_webscan_get_filter = "\\<.+javascript:window\\[.{1}\\\\x|<.*=(&#\\d+?;?)+?>|<.*(data|src)=data:text\\/html.*>|\\b(alert\\(|confirm\\(|expression\\(|prompt\\(|benchmark\\s*?\\(.*\\)|sleep\\s*?\\(.*\\)|\\b(group_)?concat[\\s\\/\\*]*?\\([^\\)]+?\\)|\\bcase[\\s\\/\\*]*?when[\\s\\/\\*]*?\\([^\\)]+?\\)|load_file\\s*?\\()|<[a-z]+?\\b[^>]*?\\bon([a-z]{4,})\\s*?=|^\\+\\/v(8|9)|\\b(and|or)\\b\\s*?([\\(\\)'\"\\d]+?=[\\(\\)'\"\\d]+?|[\\(\\)'\"a-zA-Z]+?=[\\(\\)'\"a-zA-Z]+?|>|<|\\s+?[\\w]+?\\s+?\\bin\\b\\s*?\\(|\\blike\\b\\s+?[\"'])|\\/\\*.*\\*\\/|<\\s*script\\b|\\bEXEC\\b|UNION.+?SELECT\\s*(\\(.+\\)\\s*|@{1,2}.+?\\s*|\\s+?.+?|(`|'|\").*?(`|'|\")\\s*)|UPDATE\\s*(\\(.+\\)\\s*|@{1,2}.+?\\s*|\\s+?.+?|(`|'|\").*?(`|'|\")\\s*)SET|INSERT\\s+INTO.+?VALUES|(SELECT|DELETE)@{0,2}(\\(.+\\)|\\s+?.+?\\s+?|(`|'|\").*?(`|'|\"))FROM(\\(.+\\)|\\s+?.+?|(`|'|\").*?(`|'|\"))|(CREATE|ALTER|DROP|TRUNCATE)\\s+(TABLE|DATABASE)";
static const char *gene_webscan_post_filter = "<.*=(&#\\d+?;?)+?>|<.*data=data:text\\/html.*>|\\b(alert\\(|confirm\\(|expression\\(|prompt\\(|benchmark\\s*?\\(.*\\)|sleep\\s*?\\(.*\\)|\\b(group_)?concat[\\s\\/\\*]*?\\([^\\)]+?\\)|\\bcase[\\s\\/\\*]*?when[\\s\\/\\*]*?\\([^\\)]+?\\)|load_file\\s*?\\()|<[^>]*?\\b(onerror|onmousemove|onload|onclick|onmouseover)\\b|\\b(and|or)\\b\\s*?([\\(\\)'\"\\d]+?=[\\(\\)'\"\\d]+?|[\\(\\)'\"a-zA-Z]+?=[\\(\\)'\"a-zA-Z]+?|>|<|\\s+?[\\w]+?\\s+?\\bin\\b\\s*?\\(|\\blike\\b\\s+?[\"'])|\\/\\*.*\\*\\/|<\\s*script\\b|\\bEXEC\\b|UNION.+?SELECT\\s*(\\(.+\\)\\s*|@{1,2}.+?\\s*|\\s+?.+?|(`|'|\").*?(`|'|\")\\s*)|UPDATE\\s*(\\(.+\\)\\s*|@{1,2}.+?\\s*|\\s+?.+?|(`|'|\").*?(`|'|\")\\s*)SET|INSERT\\s+INTO.+?VALUES|(SELECT|DELETE)(\\(.+\\)|\\s+?.+?\\s+?|(`|'|\").*?(`|'|\"))FROM(\\(.+\\)|\\s+?.+?|(`|'|\").*?(`|'|\"))|(CREATE|ALTER|DROP|TRUNCATE)\\s+(TABLE|DATABASE)";
static const char *gene_webscan_cookie_filter = "benchmark\\s*?\\(.*\\)|sleep\\s*?\\(.*\\)|load_file\\s*?\\(|\\b(and|or)\\b\\s*?([\\(\\)'\"\\d]+?=[\\(\\)'\"\\d]+?|[\\(\\)'\"a-zA-Z]+?=[\\(\\)'\"a-zA-Z]+?|>|<|\\s+?[\\w]+?\\s+?\\bin\\b\\s*?\\(|\\blike\\b\\s+?[\"'])|\\/\\*.*\\*\\/|<\\s*script\\b|\\bEXEC\\b|UNION.+?SELECT\\s*(\\(.+\\)\\s*|@{1,2}.+?\\s*|\\s+?.+?|(`|'|\").*?(`|'|\")\\s*)|UPDATE\\s*(\\(.+\\)\\s*|@{1,2}.+?\\s*|\\s+?.+?|(`|'|\").*?(`|'|\")\\s*)SET|INSERT\\s+INTO.+?VALUES|(SELECT|DELETE)@{0,2}(\\(.+\\)|\\s+?.+?\\s+?|(`|'|\").*?(`|'|\"))FROM(\\(.+\\)|\\s+?.+?|(`|'|\").*?(`|'|\"))|(CREATE|ALTER|DROP|TRUNCATE)\\s+(TABLE|DATABASE)";

/* [GENE_PERF:2026-05-11 v3] Pre-compiled regex zend_string cache.
 * The three filter patterns are process-lifetime constants; previously
 * gene_webscan_match_pattern() built "/pattern/is" via strpprintf() on every
 * call, which allocated a fresh zend_string per scanned value (2x per key +
 * 2x per flattened body per request in Swoole resident mode). Build them
 * once as interned strings on first Webscan::check() invocation. */
/* [GENE_FIX:2026-05-24] gene_interned_str_persistent avoids the unsafe
 * static zend_string* + zend_string_init_interned(...,1) pattern that
 * dangles across requests under opcache.file_cache_only=1. When the
 * runtime cannot grant a permanent interned string the slot stays NULL
 * and the helper returns a request-scope string instead. */
static zend_string *gene_webscan_build_regex(zend_string **slot, const char *raw) {
    if (EXPECTED(*slot != NULL)) {
        return *slot;
    }
    size_t plen = strlen(raw);
    size_t total = plen + 4; /* "/" + pat + "/" + "i" + "s" */
    char *buf = emalloc(total + 1);
    zend_string *out;
    buf[0] = '/';
    memcpy(buf + 1, raw, plen);
    buf[plen + 1] = '/';
    buf[plen + 2] = 'i';
    buf[plen + 3] = 's';
    buf[total] = '\0';
    out = gene_interned_str_persistent(slot, buf, total);
    efree(buf);
    return out;
}

static zend_string *gene_webscan_regex_get_cached(void) {
    static zend_string *cached_slot = NULL;
    return gene_webscan_build_regex(&cached_slot, gene_webscan_get_filter);
}

static zend_string *gene_webscan_regex_post_cached(void) {
    static zend_string *cached_slot = NULL;
    return gene_webscan_build_regex(&cached_slot, gene_webscan_post_filter);
}

static zend_string *gene_webscan_regex_cookie_cached(void) {
    static zend_string *cached_slot = NULL;
    return gene_webscan_build_regex(&cached_slot, gene_webscan_cookie_filter);
}

ZEND_BEGIN_ARG_INFO_EX(gene_webscan_construct_arginfo, 0, 0, 0)
    ZEND_ARG_INFO(0, webscan_switch)
    ZEND_ARG_INFO(0, webscan_white_directory)
    ZEND_ARG_INFO(0, webscan_white_url)
    ZEND_ARG_INFO(0, webscan_get)
    ZEND_ARG_INFO(0, webscan_post)
    ZEND_ARG_INFO(0, webscan_cookie)
    ZEND_ARG_INFO(0, webscan_referer)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_webscan_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

static void gene_webscan_flatten(zval *value, smart_str *buf)
{
    if (!value) {
        return;
    }

    if (Z_TYPE_P(value) == IS_ARRAY) {
        zval *item;
        zend_string *key;
        zend_ulong idx;
        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(value), idx, key, item) {
            if (key) {
                smart_str_append(buf, key);
            } else {
                smart_str_append_long(buf, (zend_long) idx);
            }
            gene_webscan_flatten(item, buf);
        } ZEND_HASH_FOREACH_END();
        return;
    }

    zend_string *str = zval_get_string(value);
    smart_str_append(buf, str);
    zend_string_release(str);
}

/* [GENE_PERF:2026-05-11 v3] regex is a cached interned zend_string produced
 * by gene_webscan_regex_*_cached(). ZVAL_STR skips refcount traffic for
 * interned strings and zval_ptr_dtor is a safe no-op on them. */
static int gene_webscan_match_pattern(zend_string *regex, zval *value)
{
    zval retval, params[2];
    int matched = 0;

    static zend_function *preg_fn = NULL;
    if (UNEXPECTED(!preg_fn)) {
        preg_fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("preg_match"));
    }
    if (UNEXPECTED(!preg_fn || !regex)) {
        return 0;
    }
    ZVAL_STR(&params[0], regex);
    ZVAL_COPY(&params[1], value);

    ZVAL_UNDEF(&retval);
    zend_call_known_function(preg_fn, NULL, NULL, &retval, 2, params, NULL);
    matched = zend_is_true(&retval);
    zval_ptr_dtor(&retval);

    zval_ptr_dtor(&params[1]);
    /* params[0] holds an interned string reference — no dtor required. */
    return matched;
}

/* [GENE_PERF:2026-05-11 v3] key is always IS_STRING at the call sites
 * (every caller either ZVAL_STR_COPY's a HashTable string key, converts a
 * numeric index via convert_to_string, or ZVAL_STRING's a literal). The prior
 * ZVAL_COPY + convert_to_string pair was pure overhead per scanned value —
 * pass the key directly. */
static int gene_webscan_stop_attack(zval *key, zval *value, zend_string *regex)
{
    smart_str buf = {0};
    zval flat;
    int matched;

    gene_webscan_flatten(value, &buf);
    smart_str_0(&buf);
    if (buf.s) {
        ZVAL_STR(&flat, buf.s);
    } else {
        ZVAL_EMPTY_STRING(&flat);
    }

    matched = gene_webscan_match_pattern(regex, &flat) || gene_webscan_match_pattern(regex, key);
    zval_ptr_dtor(&flat);
    return matched;
}

static int gene_webscan_string_contains(zend_string *haystack, zend_string *needle)
{
    if (!haystack || !needle || ZSTR_LEN(needle) == 0) {
        return 0;
    }
    return php_memnstr(ZSTR_VAL(haystack), ZSTR_VAL(needle), ZSTR_LEN(needle), ZSTR_VAL(haystack) + ZSTR_LEN(haystack)) != NULL;
}

static int gene_webscan_is_white(zval *white_dir, zval *white_urls)
{
    zval *uri = getVal(TRACK_VARS_SERVER, NULL, 0);
    zend_string *uri_str = NULL, *query_str = NULL;

    if (uri && Z_TYPE_P(uri) == IS_ARRAY) {
        zval *request_uri = zend_hash_str_find(Z_ARRVAL_P(uri), ZEND_STRL("REQUEST_URI"));
        if (!request_uri) {
            request_uri = zend_hash_str_find(Z_ARRVAL_P(uri), ZEND_STRL("request_uri"));
        }
        zval *query_uri = zend_hash_str_find(Z_ARRVAL_P(uri), ZEND_STRL("QUERY_STRING"));
        if (!query_uri) {
            query_uri = zend_hash_str_find(Z_ARRVAL_P(uri), ZEND_STRL("query_string"));
        }
        uri_str = request_uri ? zval_get_string(request_uri) : zend_string_init(ZEND_STRL("/"), 0);
        query_str = query_uri ? zval_get_string(query_uri) : zend_string_init(ZEND_STRL(""), 0);
    } else {
        uri_str = zend_string_init(ZEND_STRL("/"), 0);
        query_str = zend_string_init(ZEND_STRL(""), 0);
    }

    if (white_dir && Z_TYPE_P(white_dir) == IS_STRING && Z_STRLEN_P(white_dir) > 0) {
        if (gene_webscan_string_contains(uri_str, Z_STR_P(white_dir))) {
            zend_string_release(uri_str);
            zend_string_release(query_str);
            return 1;
        }
    }

    if (white_urls && Z_TYPE_P(white_urls) == IS_ARRAY) {
        zval *item;
        zend_string *key;
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(white_urls), key, item) {
            zend_string *query_item;
            if (!key || !gene_webscan_string_contains(uri_str, key)) {
                continue;
            }
            query_item = zval_get_string(item);
            if (ZSTR_LEN(query_item) == 0 || gene_webscan_string_contains(query_str, query_item)) {
                zend_string_release(query_item);
                zend_string_release(uri_str);
                zend_string_release(query_str);
                return 1;
            }
            zend_string_release(query_item);
        } ZEND_HASH_FOREACH_END();
    }

    zend_string_release(uri_str);
    zend_string_release(query_str);
    return 0;
}

PHP_METHOD(gene_webscan, __construct)
{
    zval *self = getThis(), *white_urls = NULL;
    zend_long webscan_switch = 1, webscan_get = 1, webscan_post = 1, webscan_cookie = 1, webscan_referer = 1;
    zend_string *white_dir = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|lSallll", &webscan_switch, &white_dir, &white_urls, &webscan_get, &webscan_post, &webscan_cookie, &webscan_referer) == FAILURE) {
        return;
    }

    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_switch"), webscan_switch);
    if (white_dir) {
        zend_update_property_str(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_white_directory"), white_dir);
    }
    if (white_urls) {
        zend_update_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_white_url"), white_urls);
    }
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_get"), webscan_get);
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_post"), webscan_post);
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_cookie"), webscan_cookie);
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_referer"), webscan_referer);
}

PHP_METHOD(gene_webscan, check)
{
    zval *self = getThis();
    zval rv1, rv2, rv3, rv4, rv5, rv6, rv7;
    zval *enabled = zend_read_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_switch"), 1, &rv1);
    zval *white_dir = zend_read_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_white_directory"), 1, &rv2);
    zval *white_urls = zend_read_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_white_url"), 1, &rv3);
    zval *scan_get = zend_read_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_get"), 1, &rv4);
    zval *scan_post = zend_read_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_post"), 1, &rv5);
    zval *scan_cookie = zend_read_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_cookie"), 1, &rv6);
    zval *scan_referer = zend_read_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_referer"), 1, &rv7);

    if (!zend_is_true(enabled)) {
        RETURN_FALSE;
    }

    if (gene_webscan_is_white(white_dir, white_urls)) {
        RETURN_FALSE;
    }

    if (zend_is_true(scan_get)) {
        zval *arr = getVal(TRACK_VARS_GET, NULL, 0);
        if (arr && Z_TYPE_P(arr) == IS_ARRAY) {
            zval *value;
            zend_string *key;
            zend_ulong idx;
            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arr), idx, key, value) {
                zval k;
                if (key) {
                    ZVAL_STR_COPY(&k, key);
                } else {
                    ZVAL_LONG(&k, (zend_long) idx);
                    convert_to_string(&k);
                }
                if (gene_webscan_stop_attack(&k, value, gene_webscan_regex_get_cached())) {
                    zval_ptr_dtor(&k);
                    RETURN_TRUE;
                }
                zval_ptr_dtor(&k);
            } ZEND_HASH_FOREACH_END();
        }
    }

    if (zend_is_true(scan_post)) {
        zval *arr = getVal(TRACK_VARS_POST, NULL, 0);
        if (arr && Z_TYPE_P(arr) == IS_ARRAY) {
            zval *value;
            zend_string *key;
            zend_ulong idx;
            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arr), idx, key, value) {
                zval k;
                if (key) {
                    ZVAL_STR_COPY(&k, key);
                } else {
                    ZVAL_LONG(&k, (zend_long) idx);
                    convert_to_string(&k);
                }
                if (gene_webscan_stop_attack(&k, value, gene_webscan_regex_post_cached())) {
                    zval_ptr_dtor(&k);
                    RETURN_TRUE;
                }
                zval_ptr_dtor(&k);
            } ZEND_HASH_FOREACH_END();
        }
    }

    if (zend_is_true(scan_cookie)) {
        zval *arr = getVal(TRACK_VARS_COOKIE, NULL, 0);
        if (arr && Z_TYPE_P(arr) == IS_ARRAY) {
            zval *value;
            zend_string *key;
            zend_ulong idx;
            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arr), idx, key, value) {
                zval k;
                if (key) {
                    ZVAL_STR_COPY(&k, key);
                } else {
                    ZVAL_LONG(&k, (zend_long) idx);
                    convert_to_string(&k);
                }
                if (gene_webscan_stop_attack(&k, value, gene_webscan_regex_cookie_cached())) {
                    zval_ptr_dtor(&k);
                    RETURN_TRUE;
                }
                zval_ptr_dtor(&k);
            } ZEND_HASH_FOREACH_END();
        }
    }

    if (zend_is_true(scan_referer)) {
        zval *server = getVal(TRACK_VARS_SERVER, NULL, 0);
        if (server && Z_TYPE_P(server) == IS_ARRAY) {
            zval *referer = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("HTTP_REFERER"));
            if (!referer) {
                referer = zend_hash_str_find(Z_ARRVAL_P(server), ZEND_STRL("http_referer"));
            }
            if (referer) {
                zval key;
                ZVAL_STRING(&key, "HTTP_REFERER");
                if (gene_webscan_stop_attack(&key, referer, gene_webscan_regex_post_cached())) {
                    zval_ptr_dtor(&key);
                    RETURN_TRUE;
                }
                zval_ptr_dtor(&key);
            }
        }
    }

    RETURN_FALSE;
}

const zend_function_entry gene_webscan_methods[] = {
    PHP_ME(gene_webscan, __construct, gene_webscan_construct_arginfo, ZEND_ACC_PUBLIC)
    PHP_ME(gene_webscan, check, gene_webscan_void_arginfo, ZEND_ACC_PUBLIC)
    { NULL, NULL, NULL }
};

GENE_MINIT_FUNCTION(webscan) {
    zend_class_entry gene_webscan;
    GENE_INIT_CLASS_ENTRY(gene_webscan, "Gene_Webscan", "Gene\\Webscan", gene_webscan_methods);
    gene_webscan_ce = zend_register_internal_class(&gene_webscan);
#if PHP_VERSION_ID >= 80200
    gene_webscan_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif
    zend_declare_property_long(gene_webscan_ce, ZEND_STRL("webscan_switch"), 1, ZEND_ACC_PRIVATE);
    zend_declare_property_string(gene_webscan_ce, ZEND_STRL("webscan_white_directory"), "", ZEND_ACC_PRIVATE);
    zend_declare_property_null(gene_webscan_ce, ZEND_STRL("webscan_white_url"), ZEND_ACC_PRIVATE);
    zend_declare_property_long(gene_webscan_ce, ZEND_STRL("webscan_get"), 1, ZEND_ACC_PRIVATE);
    zend_declare_property_long(gene_webscan_ce, ZEND_STRL("webscan_post"), 1, ZEND_ACC_PRIVATE);
    zend_declare_property_long(gene_webscan_ce, ZEND_STRL("webscan_cookie"), 1, ZEND_ACC_PRIVATE);
    zend_declare_property_long(gene_webscan_ce, ZEND_STRL("webscan_referer"), 1, ZEND_ACC_PRIVATE);
    return SUCCESS;
}
