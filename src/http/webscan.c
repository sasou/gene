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
#include "ext/standard/url.h"

#include "../gene.h"
#include "../http/request.h"
#include "../http/webscan.h"
#include "../http/validate.h"

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
    ZEND_ARG_INFO(0, webscan_fail_closed)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_webscan_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

static inline int gene_webscan_is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static const char *gene_webscan_find_close(const char *p, const char *end)
{
    for (; p + 1 < end; p++) {
        if (p[0] == '*' && p[1] == '/') {
            return p;
        }
    }
    return NULL;
}

/* Strip SQL comment obfuscation and collapse whitespace so INSERT#x\nINTO,
 * and#\n1=1, or a=--b etc. line up with the keyword rules. MySQL does not
 * treat # / -- inside string literals as comments, so stripping can also
 * hide payload — callers must match the raw text as well. */
static void gene_webscan_normalize_append(smart_str *out, const char *s, size_t len)
{
    const char *p = s, *end = s + len, *close;
    int in_exec = 0, pending_ws = 0;

    while (p < end) {
        unsigned char c = (unsigned char) *p;
        if (c == '/' && p + 1 < end && p[1] == '*') {
            if (p + 2 < end && p[2] == '!') {
                if (gene_webscan_find_close(p + 3, end)) {
                    p += 3;
                    while (p < end && *p >= '0' && *p <= '9') {
                        p++;
                    }
                    in_exec++;
                    pending_ws = 1;
                    continue;
                }
            } else if ((close = gene_webscan_find_close(p + 2, end)) != NULL) {
                p = close + 2;
                pending_ws = 1;
                continue;
            }
        }
        if (c == '*' && in_exec && p + 1 < end && p[1] == '/') {
            in_exec--;
            p += 2;
            pending_ws = 1;
            continue;
        }
        if (c == '#' || (c == '-' && p + 1 < end && p[1] == '-')) {
            if (c == '-' && p + 2 < end && !gene_webscan_is_space((unsigned char) p[2])) {
                p += 2;
                continue;
            }
            while (p < end && *p != '\n') {
                p++;
            }
            if (p < end) {
                p++;
            }
            pending_ws = 1;
            continue;
        }
        if (gene_webscan_is_space(c)) {
            pending_ws = 1;
            p++;
            continue;
        }
        if (pending_ws) {
            smart_str_appendc(out, ' ');
            pending_ws = 0;
        }
        smart_str_appendc(out, (char) c);
        p++;
    }
    if (pending_ws) {
        smart_str_appendc(out, ' ');
    }
}

static zend_string *gene_webscan_normalize(zend_string *str)
{
    smart_str out = {0};
    gene_webscan_normalize_append(&out, ZSTR_VAL(str), ZSTR_LEN(str));
    smart_str_0(&out);
    return out.s ? out.s : ZSTR_EMPTY_ALLOC();
}

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

/* Leaf-level normalization: normalizing after concatenation would let a #/--
 * in one element swallow its siblings. Each leaf emits value + key path. */
static void gene_webscan_flatten_normalized(zval *value, const char *path, size_t path_len, smart_str *buf)
{
    if (Z_TYPE_P(value) == IS_ARRAY) {
        zval *item;
        zend_string *key;
        zend_ulong idx;
        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(value), idx, key, item) {
            smart_str child = {0};
            if (path_len) {
                smart_str_appendl(&child, path, path_len);
            }
            if (key) {
                gene_webscan_normalize_append(&child, ZSTR_VAL(key), ZSTR_LEN(key));
            } else {
                smart_str_append_long(&child, (zend_long) idx);
            }
            smart_str_0(&child);
            gene_webscan_flatten_normalized(item,
                child.s ? ZSTR_VAL(child.s) : "", child.s ? ZSTR_LEN(child.s) : 0, buf);
            smart_str_free(&child);
        } ZEND_HASH_FOREACH_END();
        return;
    }

    zend_string *str = zval_get_string(value);
    gene_webscan_normalize_append(buf, ZSTR_VAL(str), ZSTR_LEN(str));
    zend_string_release(str);
    if (path_len) {
        smart_str_appendl(buf, path, path_len);
    }
}

/* [GENE_PERF:2026-05-11 v3] regex is a cached interned zend_string produced
 * by gene_webscan_regex_*_cached(). ZVAL_STR skips refcount traffic for
 * interned strings and zval_ptr_dtor is a safe no-op on them.
 * preg_match() returns false on PCRE errors (backtrack / JIT stack limit);
 * a padded payload must not slip through, so that counts as a hit when
 * fail_closed is set. */
static int gene_webscan_match_str(zend_string *regex, zend_string *subject, int fail_closed)
{
    zval retval;
    int matched = 0;

    if (UNEXPECTED(!regex)) {
        return 0;
    }
    ZVAL_UNDEF(&retval);
    gene_pcre_match(regex, subject, &retval);
    if (Z_TYPE(retval) == IS_FALSE) {
        matched = fail_closed;
    } else {
        matched = zend_is_true(&retval);
    }
    zval_ptr_dtor(&retval);
    return matched;
}

static int gene_webscan_match_both(zend_string *regex, zend_string *subject, int fail_closed)
{
    zend_string *norm;
    int matched;

    if (gene_webscan_match_str(regex, subject, fail_closed)) {
        return 1;
    }
    norm = gene_webscan_normalize(subject);
    matched = !zend_string_equals(norm, subject) && gene_webscan_match_str(regex, norm, fail_closed);
    zend_string_release(norm);
    return matched;
}

static int gene_webscan_stop_attack(zend_string *key, zval *value, zend_string *regex, int fail_closed)
{
    smart_str raw = {0}, norm = {0};
    int matched;

    gene_webscan_flatten(value, &raw);
    smart_str_0(&raw);
    matched = gene_webscan_match_str(regex, raw.s ? raw.s : ZSTR_EMPTY_ALLOC(), fail_closed);

    if (!matched) {
        gene_webscan_flatten_normalized(value, "", 0, &norm);
        smart_str_0(&norm);
        if (norm.s && (!raw.s || !zend_string_equals(norm.s, raw.s))) {
            matched = gene_webscan_match_str(regex, norm.s, fail_closed);
        }
    }
    smart_str_free(&raw);
    smart_str_free(&norm);

    return matched || gene_webscan_match_both(regex, key, fail_closed);
}

static int gene_webscan_scan_bag(zval *arr, zend_string *regex, int fail_closed)
{
    zval *value;
    zend_string *key;
    zend_ulong idx;

    if (!arr || Z_TYPE_P(arr) != IS_ARRAY) {
        return 0;
    }
    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arr), idx, key, value) {
        zend_string *k = key ? zend_string_copy(key) : zend_long_to_str((zend_long) idx);
        int hit = gene_webscan_stop_attack(k, value, regex, fail_closed);
        zend_string_release(k);
        if (hit) {
            return 1;
        }
    } ZEND_HASH_FOREACH_END();
    return 0;
}

static zval *gene_webscan_server_val(zval *server, const char *upper, size_t upper_len, const char *lower, size_t lower_len)
{
    zval *v;
    if (!server || Z_TYPE_P(server) != IS_ARRAY) {
        return NULL;
    }
    v = zend_hash_str_find(Z_ARRVAL_P(server), upper, upper_len);
    if (!v) {
        v = zend_hash_str_find(Z_ARRVAL_P(server), lower, lower_len);
    }
    return (v && Z_TYPE_P(v) == IS_STRING) ? v : NULL;
}

/* URL-decoded path portion of REQUEST_URI (no query / fragment). */
static zend_string *gene_webscan_request_path(zval *server)
{
    zval *uri = gene_webscan_server_val(server, ZEND_STRL("REQUEST_URI"), ZEND_STRL("request_uri"));
    const char *s, *q;
    size_t len;
    zend_string *path;

    if (!uri) {
        return zend_string_init(ZEND_STRL("/"), 0);
    }
    s = Z_STRVAL_P(uri);
    len = Z_STRLEN_P(uri);
    for (q = s; q < s + len && *q != '?' && *q != '#'; q++) {}
    path = zend_string_init(s, q - s, 0);
    ZSTR_LEN(path) = php_raw_url_decode(ZSTR_VAL(path), ZSTR_LEN(path));
    return path;
}

static int gene_webscan_icontains(const char *hay, size_t hay_len, const char *needle, size_t needle_len)
{
    size_t i;
    if (needle_len == 0 || needle_len > hay_len) {
        return 0;
    }
    for (i = 0; i + needle_len <= hay_len; i++) {
        if (strncasecmp(hay + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int gene_webscan_white_dir_hit(const char *p, size_t plen, const char *d, size_t dlen)
{
    while (dlen && *d == '/') {
        d++;
        dlen--;
    }
    while (dlen && d[dlen - 1] == '/') {
        dlen--;
    }
    return dlen && plen > dlen && p[0] == '/' && strncasecmp(p + 1, d, dlen) == 0
        && (plen == dlen + 1 || p[dlen + 1] == '/');
}

/* Whitelists match the path only — a query value such as ?x=admin must not
 * switch the scanner off. The directory must be a leading path segment.
 * white_dir accepts a string or an array of strings; directory matches are
 * query-independent (front-controller rewrites like index.php?/$1 fill
 * QUERY_STRING with the path, so query-gated white_url entries cannot
 * express "this subtree regardless of query"). */
static int gene_webscan_is_white(zval *server, zend_string *path, zval *white_dir, zval *white_urls)
{
    zval *query = gene_webscan_server_val(server, ZEND_STRL("QUERY_STRING"), ZEND_STRL("query_string"));
    const char *qs = query ? Z_STRVAL_P(query) : "";
    size_t qs_len = query ? Z_STRLEN_P(query) : 0;
    const char *p = ZSTR_VAL(path);
    size_t plen = ZSTR_LEN(path);

    if (gene_webscan_icontains(p, plen, ZEND_STRL(".."))) {
        return 0;
    }

    if (white_dir) {
        if (Z_TYPE_P(white_dir) == IS_STRING && Z_STRLEN_P(white_dir) > 0) {
            if (gene_webscan_white_dir_hit(p, plen, Z_STRVAL_P(white_dir), Z_STRLEN_P(white_dir))) {
                return 1;
            }
        } else if (Z_TYPE_P(white_dir) == IS_ARRAY) {
            zval *dv;
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(white_dir), dv) {
                if (Z_TYPE_P(dv) == IS_STRING
                    && gene_webscan_white_dir_hit(p, plen, Z_STRVAL_P(dv), Z_STRLEN_P(dv))) {
                    return 1;
                }
            } ZEND_HASH_FOREACH_END();
        }
    }

    if (white_urls && Z_TYPE_P(white_urls) == IS_ARRAY) {
        zval *item;
        zend_string *key;
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(white_urls), key, item) {
            zend_string *query_item;
            int hit;
            if (!key || !gene_webscan_icontains(p, plen, ZSTR_VAL(key), ZSTR_LEN(key))) {
                continue;
            }
            query_item = zval_get_string(item);
            if (ZSTR_LEN(query_item) == 0) {
                hit = qs_len == 0;
            } else {
                hit = gene_webscan_icontains(qs, qs_len, ZSTR_VAL(query_item), ZSTR_LEN(query_item));
            }
            zend_string_release(query_item);
            if (hit) {
                return 1;
            }
        } ZEND_HASH_FOREACH_END();
    }
    return 0;
}

static int gene_webscan_body_looks_json(void)
{
    zval raw;
    const char *s, *end;
    int looks = 0;

    gene_request_raw_value(&raw);
    if (Z_TYPE(raw) == IS_STRING) {
        s = Z_STRVAL(raw);
        end = s + Z_STRLEN(raw);
        while (s < end && gene_webscan_is_space((unsigned char) *s)) {
            s++;
        }
        looks = s < end && (*s == '{' || *s == '[');
    }
    zval_ptr_dtor(&raw);
    return looks;
}

/* JSON bodies bypass $_POST, yet controllers read them through input() /
 * Request::json(). Decoding goes through the shared request cache; a body
 * that claims JSON but fails to decode is scanned as raw text. */
static int gene_webscan_scan_json(zend_string *regex, int fail_closed)
{
    zval decoded;
    int hit = 0;

    if (!gene_request_json_content_type()) {
        zval *post = getVal(TRACK_VARS_POST, NULL, 0);
        if ((post && Z_TYPE_P(post) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(post)) > 0)
            || !gene_webscan_body_looks_json()) {
            return 0;
        }
    }
    if (gene_request_json_peek(&decoded) == SUCCESS) {
        if (Z_TYPE(decoded) == IS_ARRAY) {
            hit = gene_webscan_scan_bag(&decoded, regex, fail_closed);
        } else if (Z_TYPE(decoded) != IS_NULL && Z_TYPE(decoded) != IS_UNDEF) {
            hit = gene_webscan_stop_attack(ZSTR_EMPTY_ALLOC(), &decoded, regex, fail_closed);
        }
        zval_ptr_dtor(&decoded);
        return hit;
    }

    zval raw;
    gene_request_raw_value(&raw);
    if (Z_TYPE(raw) == IS_STRING && Z_STRLEN(raw) > 0) {
        hit = gene_webscan_match_both(regex, Z_STR(raw), fail_closed);
    }
    zval_ptr_dtor(&raw);
    return hit;
}

PHP_METHOD(gene_webscan, __construct)
{
    zval *self = getThis(), *white_urls = NULL, *white_dir = NULL;
    zend_long webscan_switch = 1, webscan_get = 1, webscan_post = 1, webscan_cookie = 1, webscan_referer = 1, webscan_fail_closed = 1;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|lzalllll", &webscan_switch, &white_dir, &white_urls, &webscan_get, &webscan_post, &webscan_cookie, &webscan_referer, &webscan_fail_closed) == FAILURE) {
        return;
    }
    if (white_dir && Z_TYPE_P(white_dir) != IS_STRING && Z_TYPE_P(white_dir) != IS_ARRAY) {
        zend_type_error("Gene\\Webscan::__construct(): Argument #2 ($webscan_white_directory) must be of type array|string, %s given", zend_zval_type_name(white_dir));
        RETURN_THROWS();
    }

    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_switch"), webscan_switch);
    if (white_dir && Z_TYPE_P(white_dir) == IS_STRING) {
        zend_update_property_str(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_white_directory"), Z_STR_P(white_dir));
    } else if (white_dir && Z_TYPE_P(white_dir) == IS_ARRAY) {
        zend_update_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_white_directory"), white_dir);
    }
    if (white_urls) {
        zend_update_property(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_white_url"), white_urls);
    }
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_get"), webscan_get);
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_post"), webscan_post);
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_cookie"), webscan_cookie);
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_referer"), webscan_referer);
    zend_update_property_long(gene_webscan_ce, gene_strip_obj(self), ZEND_STRL("webscan_fail_closed"), webscan_fail_closed);
}

/* Scanner core shared by Webscan::check() and the Application dispatch path
 * (the latter calls it directly — no per-request object/ctor/8 property reads). */
int gene_webscan_scan(zval *white_dir, zval *white_urls, int scan_get_on, int scan_post_on,
    int scan_cookie_on, int scan_referer_on, int fail_closed)
{
    zval *server;
    zend_string *path;
    int hit = 0;

    server = getVal(TRACK_VARS_SERVER, NULL, 0);
    path = gene_webscan_request_path(server);

    if (gene_webscan_is_white(server, path, white_dir, white_urls)) {
        zend_string_release(path);
        return 0;
    }

    if (scan_get_on) {
        hit = gene_webscan_scan_bag(getVal(TRACK_VARS_GET, NULL, 0), gene_webscan_regex_get_cached(), fail_closed)
            || gene_webscan_match_both(gene_webscan_regex_get_cached(), path, fail_closed);
    }
    zend_string_release(path);
    if (hit) {
        return 1;
    }

    if (scan_post_on) {
        zend_string *re = gene_webscan_regex_post_cached();
        if (gene_webscan_scan_bag(getVal(TRACK_VARS_POST, NULL, 0), re, fail_closed)
            || gene_webscan_scan_json(re, fail_closed)) {
            return 1;
        }
    }

    if (scan_cookie_on
        && gene_webscan_scan_bag(getVal(TRACK_VARS_COOKIE, NULL, 0), gene_webscan_regex_cookie_cached(), fail_closed)) {
        return 1;
    }

    if (scan_referer_on) {
        static const struct { const char *upper; size_t ulen; const char *lower; size_t llen; } headers[] = {
            { ZEND_STRL("HTTP_REFERER"), ZEND_STRL("http_referer") },
            { ZEND_STRL("HTTP_USER_AGENT"), ZEND_STRL("http_user_agent") },
            { ZEND_STRL("HTTP_X_FORWARDED_FOR"), ZEND_STRL("http_x_forwarded_for") },
        };
        size_t i;
        for (i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
            zval *v = gene_webscan_server_val(server, headers[i].upper, headers[i].ulen, headers[i].lower, headers[i].llen);
            if (v && gene_webscan_match_both(gene_webscan_regex_post_cached(), Z_STR_P(v), fail_closed)) {
                return 1;
            }
        }
    }

    return 0;
}

static int gene_webscan_run(zval *self)
{
    zval rv1, rv2, rv3, rv4, rv5, rv6, rv7, rv8;
    zend_object *obj = gene_strip_obj(self);
    zval *enabled = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_switch"), 1, &rv1);
    zval *white_dir = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_white_directory"), 1, &rv2);
    zval *white_urls = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_white_url"), 1, &rv3);
    zval *scan_get = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_get"), 1, &rv4);
    zval *scan_post = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_post"), 1, &rv5);
    zval *scan_cookie = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_cookie"), 1, &rv6);
    zval *scan_referer = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_referer"), 1, &rv7);
    zval *fail_zv = zend_read_property(gene_webscan_ce, obj, ZEND_STRL("webscan_fail_closed"), 1, &rv8);

    if (!zend_is_true(enabled)) {
        return 0;
    }
    return gene_webscan_scan(white_dir, white_urls, zend_is_true(scan_get), zend_is_true(scan_post),
        zend_is_true(scan_cookie), zend_is_true(scan_referer), zend_is_true(fail_zv));
}

PHP_METHOD(gene_webscan, check)
{
    RETURN_BOOL(gene_webscan_run(getThis()));
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
    zend_declare_property_long(gene_webscan_ce, ZEND_STRL("webscan_fail_closed"), 1, ZEND_ACC_PRIVATE);
    return SUCCESS;
}
