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
#include "zend_exceptions.h"
#include "zend_smart_str.h"

#include "../gene.h"
#include "../common/common.h"

/* [GENE_AUDIT:2026-07-03 P1->security] Identifier quoting.
 * Previously gene_quote_table/columns/order/identifier were empty stubs that
 * emitted the raw name with no quoting/escaping, leaving table/column/order
 * identifiers — which may originate from user input (e.g. a sort field passed
 * from the front-end into order()) — exposed to SQL injection. Data values are
 * bound via "?" placeholders and are safe; identifiers are not.
 *
 * Strategy (per audit plan: whitelist-validate first, then full quote):
 *  - Simple identifiers / dot paths (db.table, t.col): wrap each dot-separated
 *    segment in oq..cq, doubling any cq inside. A lone "*" segment passes
 *    through (SELECT *). A segment already wrapped in oq..cq passes through
 *    unchanged to avoid double-quoting pre-quoted input.
 *  - Compound expressions (contain ' ' or '(' — e.g. "col AS alias",
 *    "COUNT(*)", "RAND()"): validate against an extended whitelist
 *    [A-Za-z0-9_.,() *+-/%:=<>!|`'"]; if clean AND quotes are balanced,
 *    emit unchanged (these cannot be auto-quoted); if dirty or quotes are
 *    unbalanced, emit a warning and fall back to wrapping the whole token in
 *    oq..cq so any breakout is neutralized (the statement will error rather
 *    than inject). The extended whitelist covers arithmetic operators (+-/%),
 *    comparison operators (=< >!) used in IF()/CASE WHEN, logical |, colon :
 *    used in CONCAT(ip,port), backtick ` for pre-quoted identifiers inside
 *    expressions, and balanced string-literal quotes ' ". Statement-level
 *    injection markers (;, --, slash-star star-slash) are always rejected.
 *  - JOIN table fragments: if the table string contains the word JOIN
 *    (case-insensitive, word-bounded — including across newlines for
 *    multi-line string literals) — e.g. "t1 a LEFT JOIN t2 b ON a.id=b.id"
 *    — it is treated as a developer-written SQL fragment and passed through
 *    unchanged after an injection-marker safety check (no ;, --, slash-star star-slash).
 *  - ORDER BY: a trailing ASC/DESC keyword is split off, the column part is
 *    dot-path quoted, the keyword is re-attached.
 * oq/cq are the open/close quote chars per driver (MySQL/SQLite `, PgSQL ",
 * MSSQL [ ]). */
static int gene_ident_is_expr_char(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ',' ||
           c == '(' || c == ')' || c == ' ' || c == '*' ||
           c == '+' || c == '-' || c == '/' || c == '%' || c == ':' ||
           c == '=' || c == '<' || c == '>' || c == '!' || c == '|' ||
           c == '`' || c == '\'' || c == '"' ||
           c == '\n' || c == '\r' || c == '\t';
}

static int gene_ident_already_quoted(const char *s, size_t len, char oq, char cq) {
    return len >= 2 && s[0] == oq && s[len - 1] == cq;
}

static int gene_ident_ieq(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
    }
    return 1;
}

static int gene_ident_has_injection_markers(const char *s, size_t len);

/* Quote one dot-separated segment (no '.', no spaces). */
static void gene_quote_segment(smart_str *dest, const char *s, size_t len, char oq, char cq) {
    size_t i;
    /* [GENE_AUDIT:2026-07-05 P2-N1] The pre-quoted passthrough only checked the
     * first/last character, so a crafted token like "`x`; DROP TABLE t; -- `"
     * (first and last char both backtick) sailed through untouched, bypassing
     * the injection-marker/whitelist checks entirely. Require the token to be
     * free of statement-level injection markers before passing it through;
     * otherwise fall to the wrapping branch below, which doubles any embedded
     * cq and neutralizes the payload (the statement errors instead of
     * injecting). */
    if (gene_ident_already_quoted(s, len, oq, cq) &&
        !gene_ident_has_injection_markers(s, len)) {
        smart_str_appendl(dest, s, len);
        return;
    }
    smart_str_appendc(dest, oq);
    for (i = 0; i < len; i++) {
        if (s[i] == cq) smart_str_appendc(dest, cq);  /* double-quote escape */
        smart_str_appendc(dest, s[i]);
    }
    smart_str_appendc(dest, cq);
}

/* Quote a dot path like "db.table" / "t.col" / "u.*". */
static void gene_quote_dotpath(smart_str *dest, const char *s, size_t len, char oq, char cq) {
    size_t i, start = 0;
    int first = 1;
    for (i = 0; i <= len; i++) {
        if (i == len || s[i] == '.') {
            size_t seglen = i - start;
            if (!first) smart_str_appends(dest, ".");
            first = 0;
            if (seglen == 1 && s[start] == '*') {
                smart_str_appendc(dest, '*');
            } else if (seglen > 0) {
                gene_quote_segment(dest, s + start, seglen, oq, cq);
            }
            start = i + 1;
        }
    }
}

static int gene_ident_expr_clean(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (!gene_ident_is_expr_char((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* [GENE_AUDIT:2026-07-05 8.2.12/8.2.15] Check for SQL injection markers that
 * could break out of the current statement context: semicolon (;), line
 * comment (--), block comment (/* *\/). These are always rejected regardless
 * of context. Backslash is also rejected since it can escape quote characters
 * inside string literals, undermining the balanced-quote check. */
static int gene_ident_has_injection_markers(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (s[i] == ';' || s[i] == '\\') return 1;
        if (s[i] == '-' && i + 1 < len && s[i+1] == '-') return 1;
        if (s[i] == '/' && i + 1 < len && s[i+1] == '*') return 1;
        if (s[i] == '*' && i + 1 < len && s[i+1] == '/') return 1;
    }
    return 0;
}

/* [GENE_AUDIT:2026-07-05 8.2.12/8.2.15] Check that single and double quotes
 * are balanced (even count each). Unbalanced quotes could break out of a
 * string literal context. Since backslash is rejected by
 * gene_ident_has_injection_markers(), we don't need to worry about \' or \"
 * escape sequences — any backslash would already cause rejection. */
static int gene_ident_quotes_balanced(const char *s, size_t len) {
    size_t i, sq = 0, dq = 0;
    for (i = 0; i < len; i++) {
        if (s[i] == '\'') sq++;
        else if (s[i] == '"') dq++;
    }
    return (sq % 2 == 0) && (dq % 2 == 0);
}

/* [GENE_AUDIT:2026-07-05 8.2.12/8.2.15] Detect the word "join" (case-
 * insensitive) as a standalone word — preceded by start-of-string or
 * whitespace (space/tab/newline/CR), followed by whitespace or end-of-string.
 * This identifies developer-written multi-table SQL fragments like
 * "t1 a LEFT JOIN t2 b ON a.id=b.id" that should be passed through as raw
 * SQL rather than being dot-path quoted. Newline/CR are included as word
 * boundaries because real Model code uses multi-line string literals for
 * complex JOIN fragments (e.g. exchange Order.php, Pexch.php). */
static int gene_ident_has_join_keyword(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (i + 3 < len) {
            char prev = i > 0 ? s[i-1] : '\0';
            int at_word_start = (i == 0 || prev == ' ' || prev == '\t' ||
                                 prev == '\n' || prev == '\r');
            if (at_word_start &&
                (s[i] == 'j' || s[i] == 'J') &&
                (s[i+1] == 'o' || s[i+1] == 'O') &&
                (s[i+2] == 'i' || s[i+2] == 'I') &&
                (s[i+3] == 'n' || s[i+3] == 'N')) {
                size_t after = i + 4;
                if (after >= len || s[after] == ' ' || s[after] == '\t' ||
                    s[after] == '\n' || s[after] == '\r') {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* [GENE_AUDIT:2026-07-05 8.2.12/8.2.15] Full expression validation: the
 * token passes the extended character whitelist, has no injection markers,
 * and has balanced quotes. Returns 1 if safe to emit unchanged. */
static int gene_ident_expr_safe(const char *s, size_t len) {
    if (!gene_ident_expr_clean(s, len)) return 0;
    if (gene_ident_has_injection_markers(s, len)) return 0;
    if (!gene_ident_quotes_balanced(s, len)) return 0;
    return 1;
}

/* [GENE_AUDIT:2026-07-05 8.2.12/8.2.15] Detect characters that only appear
 * in expressions, never in a simple identifier or dot path. If present, the
 * token must be treated as an expression (validated + passthrough or
 * fail-safe) rather than dot-path quoted. Note: '-' is NOT included because
 * it can appear in legitimate column names (e.g. "user-name") which should be
 * backtick-quoted, not treated as arithmetic subtraction. Newline/CR are
 * included to handle multi-line expression strings. */
static int gene_ident_has_expr_chars(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        char c = s[i];
        if (c == '(' || c == ' ' || c == '+' || c == '/' || c == '%' ||
            c == '=' || c == '<' || c == '>' || c == '!' || c == '|' ||
            c == '`' || c == '\'' || c == '"' || c == '\n' || c == '\r') {
            return 1;
        }
    }
    return 0;
}

/* [GENE_AUDIT:2026-07-05 8.2.12/8.2.15] Same as gene_ident_has_expr_chars
 * but excludes space — used by gene_quote_order where a space may simply
 * separate the column from a trailing ASC/DESC keyword (handled by the
 * split-off logic), not indicate an expression. */
static int gene_ident_has_expr_chars_nospace(const char *s, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        char c = s[i];
        if (c == '(' || c == '+' || c == '/' || c == '%' ||
            c == '=' || c == '<' || c == '>' || c == '!' || c == '|' ||
            c == '`' || c == '\'' || c == '"') {
            return 1;
        }
    }
    return 0;
}

void gene_quote_identifier(smart_str *dest, const char *name, size_t len, char oq, char cq) /*{{{*/
{
    /* Hash-key column names (INSERT/UPDATE field lists) — simple identifier or
     * "t.col" dot path; quote directly. */
    gene_quote_dotpath(dest, name, len, oq, cq);
}/*}}}*/

char *gene_quote_table(const char *name, char oq, char cq) /*{{{*/
{
    smart_str dst = {0};
    size_t len = strlen(name);
    if (len == 0) return str_init("");
    /* [GENE_AUDIT:2026-07-05 P2-N1] Pre-quoted passthrough requires no
     * injection markers — see gene_quote_segment. Dirty input falls through
     * to the JOIN/expression/dotpath branches, all of which reject or
     * neutralize marker-bearing tokens. */
    if (gene_ident_already_quoted(name, len, oq, cq) &&
        !gene_ident_has_injection_markers(name, len)) return str_init(name);
    /* [GENE_AUDIT:2026-07-05 8.2.12] JOIN table fragments — developer-written
     * multi-table SQL like "t1 a LEFT JOIN t2 b ON a.id=b.id". Pass through
     * unchanged after injection-marker safety check; the =, !, (), etc. in
     * ON clauses are legitimate. Without this, 48 exchange Model calls
     * (17 files) would fail-safe to illegal backtick-wrapped SQL.
     * Guard: require whitespace in the string so that a table literally
     * named "join" (a SQL reserved word) is NOT treated as a JOIN fragment —
     * it should be backtick-quoted via dotpath instead. */
    if (gene_ident_has_join_keyword(name, len) && strpbrk(name, " \t\n\r")) {
        if (!gene_ident_has_injection_markers(name, len)) {
            return str_init(name);
        }
        php_error_docref(NULL, E_WARNING, "Suspicious JOIN table fragment rejected by injection-marker check: %s", name);
        gene_quote_segment(&dst, name, len, oq, cq);  /* fail-safe: neutralize */
        smart_str_0(&dst);
        char *res = str_init(dst.s ? ZSTR_VAL(dst.s) : "");
        smart_str_free(&dst);
        return res;
    }
    /* "table alias" / "table AS alias" / func(...) — pass through if safe.
     * Newline/CR included for multi-line expressions. */
    if (strpbrk(name, " (\t\n\r")) {
        if (gene_ident_expr_safe(name, len)) {
            return str_init(name);
        }
        php_error_docref(NULL, E_WARNING, "Suspicious table identifier rejected by quote whitelist: %s", name);
        gene_quote_segment(&dst, name, len, oq, cq);  /* fail-safe: neutralize */
    } else {
        gene_quote_dotpath(&dst, name, len, oq, cq);
    }
    smart_str_0(&dst);
    char *res = str_init(dst.s ? ZSTR_VAL(dst.s) : "");
    smart_str_free(&dst);
    return res;
}/*}}}*/

char *gene_quote_columns(const char *name, char oq, char cq) /*{{{*/
{
    smart_str dst = {0};
    size_t len = strlen(name);
    size_t i, start = 0;
    int first = 1;
    int paren_depth = 0;
    /* Comma-separated column list: "id,name", "u.*, p.profile_data",
     * "role, COUNT(*) as count". Commas inside parentheses (function
     * argument separators like IF(cond, 0, 1)) are NOT list separators —
     * they belong to the function call and must stay within one token so
     * that the whole IF(...) expression goes through the expr-safe path
     * rather than having "0" split off and backtick-quoted as a column
     * reference. [GENE_AUDIT:2026-07-05 §10] */
    for (i = 0; i <= len; i++) {
        if (i == len || (name[i] == ',' && paren_depth == 0)) {
            const char *item = name + start;
            size_t ilen = i - start, b = 0, e = ilen;
            while (b < e && (item[b] == ' ' || item[b] == '\t' || item[b] == '\n' || item[b] == '\r')) b++;
            while (e > b && (item[e-1] == ' ' || item[e-1] == '\t' || item[e-1] == '\n' || item[e-1] == '\r')) e--;
            if (!first) smart_str_appends(&dst, ",");
            first = 0;
            if (e > b) {
                const char *t = item + b;
                size_t tlen = e - b;
                /* [GENE_AUDIT:2026-07-05 P2-N1] see gene_quote_segment */
                if (gene_ident_already_quoted(t, tlen, oq, cq) &&
                    !gene_ident_has_injection_markers(t, tlen)) {
                    smart_str_appendl(&dst, t, tlen);
                } else if (gene_ident_has_expr_chars(t, tlen)) {
                    /* expression: COUNT(*) ... / "col AS alias" / arithmetic —
                     * passthrough if safe (extended whitelist + balanced quotes) */
                    if (gene_ident_expr_safe(t, tlen)) {
                        smart_str_appendl(&dst, t, tlen);
                    } else {
                        php_error_docref(NULL, E_WARNING, "Suspicious column expression rejected by quote whitelist: %.*s", (int)tlen, t);
                        gene_quote_segment(&dst, t, tlen, oq, cq);
                    }
                } else {
                    gene_quote_dotpath(&dst, t, tlen, oq, cq);
                }
            }
            start = i + 1;
        } else if (name[i] == '(') {
            paren_depth++;
        } else if (name[i] == ')' && paren_depth > 0) {
            paren_depth--;
        }
    }
    smart_str_0(&dst);
    char *res = str_init(dst.s ? ZSTR_VAL(dst.s) : "");
    smart_str_free(&dst);
    return res;
}/*}}}*/

char *gene_quote_order(const char *name, char oq, char cq) /*{{{*/
{
    smart_str dst = {0};
    size_t len = strlen(name);
    size_t i, start = 0;
    int first = 1;
    int paren_depth = 0;
    /* Comma-separated ORDER BY list: "id desc", "sort asc", "id,name", "RAND()".
     * Commas inside parentheses (e.g. IF(cond,0,1) used as an ORDER BY
     * expression) are NOT list separators — same rationale as
     * gene_quote_columns. [GENE_AUDIT:2026-07-05 §10] */
    for (i = 0; i <= len; i++) {
        if (i == len || (name[i] == ',' && paren_depth == 0)) {
            const char *item = name + start;
            size_t ilen = i - start, b = 0, e = ilen;
            while (b < e && (item[b] == ' ' || item[b] == '\t' || item[b] == '\n' || item[b] == '\r')) b++;
            while (e > b && (item[e-1] == ' ' || item[e-1] == '\t' || item[e-1] == '\n' || item[e-1] == '\r')) e--;
            if (!first) smart_str_appends(&dst, ",");
            first = 0;
            if (e > b) {
                const char *t = item + b;
                size_t tlen = e - b;
                /* [GENE_AUDIT:2026-07-05 P2-N1] see gene_quote_segment */
                if (gene_ident_already_quoted(t, tlen, oq, cq) &&
                    !gene_ident_has_injection_markers(t, tlen)) {
                    smart_str_appendl(&dst, t, tlen);
                } else if (gene_ident_has_expr_chars_nospace(t, tlen)) {
                    /* "RAND()" / "LENGTH(name) desc" etc. — passthrough if safe
                     * (extended whitelist + balanced quotes). Space is excluded
                     * so that "id desc" still goes to the ASC/DESC split branch. */
                    if (gene_ident_expr_safe(t, tlen)) {
                        smart_str_appendl(&dst, t, tlen);
                    } else {
                        php_error_docref(NULL, E_WARNING, "Suspicious order expression rejected by quote whitelist: %.*s", (int)tlen, t);
                        gene_quote_segment(&dst, t, tlen, oq, cq);
                    }
                } else {
                    /* split off a trailing ASC/DESC keyword */
                    long lastsp = -1;
                    size_t k;
                    for (k = 0; k < tlen; k++) if (t[k] == ' ') lastsp = (long)k;
                    if (lastsp > 0) {
                        size_t kwlen = tlen - (size_t)lastsp - 1;
                        const char *kw = t + lastsp + 1;
                        if ((kwlen == 3 && gene_ident_ieq(kw, "asc", 3)) ||
                            (kwlen == 4 && gene_ident_ieq(kw, "desc", 4))) {
                            /* [GENE_AUDIT:2026-07-05 P4-3] Trim trailing
                             * whitespace between column and keyword so that
                             * multi-space input like "id  desc" does not
                             * produce a quoted segment with embedded spaces
                             * (e.g. `id `) that would error in SQL. */
                            size_t colend = (size_t)lastsp;
                            while (colend > 0 && (t[colend-1] == ' ' || t[colend-1] == '\t')) colend--;
                            gene_quote_dotpath(&dst, t, colend, oq, cq);
                            smart_str_appends(&dst, " ");
                            smart_str_appendl(&dst, kw, kwlen);
                            start = i + 1;
                            continue;
                        }
                    }
                    gene_quote_dotpath(&dst, t, tlen, oq, cq);
                }
            }
            start = i + 1;
        } else if (name[i] == '(') {
            paren_depth++;
        } else if (name[i] == ')' && paren_depth > 0) {
            paren_depth--;
        }
    }
    smart_str_0(&dst);
    char *res = str_init(dst.s ? ZSTR_VAL(dst.s) : "");
    smart_str_free(&dst);
    return res;
}/*}}}*/

void array_to_string(zval *array, char **result, char oq, char cq)
{
    zval *value;
    smart_str field_str = {0};
    bool pre = 0;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(array), value) {
    	if (pre) {
    		smart_str_appends(&field_str, ",");
    	} else {
    		pre = 1;
    	}
        if ( Z_TYPE_P(value) == IS_OBJECT ) convert_to_string(value);
        if ( (Z_TYPE_P(value) == IS_STRING) && isalpha(*(Z_STRVAL_P(value))) ) {
			char *quoted = gene_quote_columns(Z_STRVAL_P(value), oq, cq);
			smart_str_appends(&field_str, quoted);
			efree(quoted);
        }
    } ZEND_HASH_FOREACH_END();
    smart_str_0(&field_str);
    /* [GENE_FIX:2026-04-27] If no value in the array passed the filter
     * (e.g. all numeric or all leading non-alpha), smart_str_appends was
     * never called and field_str.s is NULL — ZSTR_VAL(NULL) segfaults. */
    *result = str_init(field_str.s ? ZSTR_VAL(field_str.s) : "");
    smart_str_free(&field_str);
}/*}}}*/

void mssql_array_to_string(zval *array, char **result, char oq, char cq)
{
    zval *value;
    smart_str field_str = {0};
    bool pre = 0;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(array), value) {
    	if (pre) {
    		smart_str_appends(&field_str, ",");
    	} else {
    		pre = 1;
    	}
        if ( Z_TYPE_P(value) == IS_OBJECT ) convert_to_string(value);
        if ( (Z_TYPE_P(value) == IS_STRING) && isalpha(*(Z_STRVAL_P(value))) ) {
			char *quoted = gene_quote_columns(Z_STRVAL_P(value), oq, cq);
			smart_str_appends(&field_str, quoted);
			efree(quoted);
        }
    } ZEND_HASH_FOREACH_END();
    smart_str_0(&field_str);
    /* [GENE_FIX:2026-04-27] See array_to_string — same NULL-deref guard. */
    *result = str_init(field_str.s ? ZSTR_VAL(field_str.s) : "");
    smart_str_free(&field_str);
}/*}}}*/

void gene_pdo_construct(zval *pdo_object, zval *dsn, zval *user, zval *pass, zval *options) /*{{{*/
{
    zval retval;
    ZVAL_UNDEF(&retval);
    zend_class_entry *ce = Z_OBJCE_P(pdo_object);
    zend_function *fn = ce->constructor;
    if (EXPECTED(fn)) {
        if (options) {
            zval params[4] = { *dsn, *user, *pass, *options };
            zend_call_known_function(fn, Z_OBJ_P(pdo_object), ce, &retval, 4, params, NULL);
        } else {
            zval params[3] = { *dsn, *user, *pass };
            zend_call_known_function(fn, Z_OBJ_P(pdo_object), ce, &retval, 3, params, NULL);
        }
    }
    if (!Z_ISUNDEF(retval)) {
        zval_ptr_dtor(&retval);
    }
}/*}}}*/

void gene_pdo_begin_transaction(zval *pdo_object, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("begintransaction"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_commit(zval *pdo_object, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("commit"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 0, NULL, NULL);
    }
}/*}}}*/


void gene_pdo_exec(zval *pdo_object, char *sql, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("exec"));
    if (EXPECTED(fn)) {
        zval pdo_sql;
        ZVAL_STRING(&pdo_sql, sql);
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 1, &pdo_sql, NULL);
        zval_ptr_dtor(&pdo_sql);
    }
}/*}}}*/

void gene_pdo_in_transaction(zval *pdo_object, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("intransaction"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_last_insert_id(zval *pdo_object, char *name, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("lastinsertid"));
    if (EXPECTED(fn)) {
        if (name) {
            zval id_name;
            ZVAL_STRING(&id_name, name);
            zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 1, &id_name, NULL);
            zval_ptr_dtor(&id_name);
        } else {
            zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 0, NULL, NULL);
        }
    }
}/*}}}*/

void gene_pdo_error_code(zval *pdo_object, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("errorcode"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_error_info(zval *pdo_object, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("errorinfo"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 0, NULL, NULL);
    }
}/*}}}*/

bool show_sql_errors(zval *pdo_object)
{
    zval retval;
    gene_pdo_error_info(pdo_object, &retval);
    if (Z_TYPE(retval) != IS_ARRAY) {
    	zval_ptr_dtor(&retval);
    	return 0;
    }
    zval *sql_state = zend_hash_index_find(Z_ARRVAL(retval), 0);
    zval *sql_code = zend_hash_index_find(Z_ARRVAL(retval), 1);
    zval *sql_info = zend_hash_index_find(Z_ARRVAL(retval), 2);
    if (!sql_state || Z_TYPE_P(sql_state) != IS_STRING) {
    	zval_ptr_dtor(&retval);
    	return 0;
    }
    if (Z_STRLEN_P(sql_state) != 5 || memcmp(Z_STRVAL_P(sql_state), "00000", 5) != 0) {
    	zend_long code = (sql_code && Z_TYPE_P(sql_code) == IS_LONG) ? Z_LVAL_P(sql_code) : 0;
    	char *info = (sql_info && Z_TYPE_P(sql_info) == IS_STRING) ? Z_STRVAL_P(sql_info) : "Unknown error";
    	zval_ptr_dtor(&retval);
    	php_error_docref(NULL, E_ERROR, "SQL: " ZEND_LONG_FMT " %s", code, info);
        return 1;
    }
    zval_ptr_dtor(&retval);
    return 0;
}/*}}}*/

void gene_pdo_prepare(zval *pdo_object, char *sql, zval *retval) /*{{{ RETURN a PDOStatement */
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("prepare"));
    if (EXPECTED(fn) && sql) {
        zval param;
        ZVAL_STRING(&param, sql);
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 1, &param, NULL);
        zval_ptr_dtor(&param);
    }
}/*}}}*/

void gene_pdo_rollback(zval *pdo_object, zval *retval) /*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdo_object)->function_table, ZEND_STRL("rollback"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdo_object), Z_OBJCE_P(pdo_object), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_statement_execute(zval *pdostatement_obj, zval *bind_parameters, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdostatement_obj)->function_table, ZEND_STRL("execute"));
    if (EXPECTED(fn)) {
        if (bind_parameters) {
            zval params[1];
            ZVAL_COPY(&params[0], bind_parameters);
            zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 1, params, NULL);
            zval_ptr_dtor(&params[0]);
        } else {
            zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 0, NULL, NULL);
        }
    }
}/*}}}*/

void gene_pdo_statement_fetch(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdostatement_obj)->function_table, ZEND_STRL("fetch"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_statement_fetch_all(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdostatement_obj)->function_table, ZEND_STRL("fetchall"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_statement_fetch_column(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdostatement_obj)->function_table, ZEND_STRL("fetchcolumn"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_statement_fetch_object(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdostatement_obj)->function_table, ZEND_STRL("fetchobject"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 0, NULL, NULL);
    }
}/*}}}*/


void gene_pdo_statement_row_count(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdostatement_obj)->function_table, ZEND_STRL("rowcount"));
    if (EXPECTED(fn)) {
        zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 0, NULL, NULL);
    }
}/*}}}*/

void gene_pdo_statement_set_fetch_mode(zval *pdostatement_obj, int fetch_style, zval *retval)/*{{{*/
{
    ZVAL_UNDEF(retval);
    zend_function *fn = zend_hash_str_find_ptr(&Z_OBJCE_P(pdostatement_obj)->function_table, ZEND_STRL("setfetchmode"));
    if (EXPECTED(fn)) {
        zval param;
        ZVAL_LONG(&param, fetch_style);
        zend_call_known_function(fn, Z_OBJ_P(pdostatement_obj), Z_OBJCE_P(pdostatement_obj), retval, 1, &param, NULL);
    }
}/*}}}*/

void jsonEncode(zval *data, zval *param) {
	if (Z_TYPE_P(param) == IS_ARRAY) {
		GENE_CG_FN_DECL(fn);
		fn = GENE_CG_FN_LOOKUP(fn, "json_encode");
		zval ret;
		ZVAL_NULL(&ret);
		if (EXPECTED(fn)) {
			zend_call_known_function(fn, NULL, NULL, &ret, 1, param, NULL);
		}
		if (Z_TYPE(ret) == IS_STRING) {
			ZVAL_STRING(data, Z_STRVAL(ret));
		} else {
			ZVAL_NULL(data);
		}
		zval_ptr_dtor(&ret);
	} else {
		ZVAL_NULL(data);
	}
}

void gene_insert_field_value(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value, char oq, char cq){
	zval *value = NULL;
	bool pre = 0;
	zend_string *key = NULL;
	zend_long id;
	array_init(field_value);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(fields), id, key, value) {
    	if (pre) {
    		smart_str_appends(field_str, ",");
    		smart_str_appends(value_str, ",");
    	} else {
    		pre = 1;
    	}
        if (key) {
        	gene_quote_identifier(field_str, ZSTR_VAL(key), ZSTR_LEN(key), oq, cq);
        } else {
        	smart_str_append_long(field_str, id);
        }
        smart_str_appends(value_str, "?");
    	add_next_index_zval(field_value, value);
    	Z_TRY_ADDREF_P(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_0(field_str);
    smart_str_0(value_str);
}

void gene_insert_field_value_batch(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value, char oq, char cq) {
	zval *value = NULL;
	bool pre = 0;
	zend_string *key = NULL;
	zend_long id;
	array_init(field_value);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(fields), id, key, value) {
    	if (pre) {
    		smart_str_appends(field_str, ",");
    		smart_str_appends(value_str, ",");
    	} else {
    		smart_str_appends(value_str, "(");
    		pre = 1;
    	}
        if (key) {
        	gene_quote_identifier(field_str, ZSTR_VAL(key), ZSTR_LEN(key), oq, cq);
        } else {
        	smart_str_append_long(field_str, id);
        }
        smart_str_appends(value_str, "?");
        add_next_index_zval(field_value, value);
        Z_TRY_ADDREF_P(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_appends(value_str, ")");
    smart_str_0(field_str);
    smart_str_0(value_str);
}

void gene_insert_field_value_batch_other(zval *fields, smart_str *value_str, zval *field_value) {
	zval *value = NULL;
	bool pre = 0;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), value) {
    	if (pre) {
    		smart_str_appends(value_str, ",");
    	} else {
    		smart_str_appends(value_str, "(");
    		pre = 1;
    	}
        smart_str_appends(value_str, "?");
        add_next_index_zval(field_value, value);
        Z_TRY_ADDREF_P(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_appends(value_str, ")");
    smart_str_0(value_str);
}

void gene_update_field_value(zval *fields, smart_str *field_str, zval *field_value, char oq, char cq) {
	zval *value = NULL;
	bool pre = 0;
	zend_string *key = NULL;
	zend_long id;
	array_init(field_value);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(fields), id, key, value) {
    	if (pre) {
    		smart_str_appends(field_str, ",");
    	} else {
    		pre = 1;
    	}
        if (key) {
        	gene_quote_identifier(field_str, ZSTR_VAL(key), ZSTR_LEN(key), oq, cq);
        } else {
        	smart_str_append_long(field_str, id);
        }
        smart_str_appends(field_str, "=?");
    	add_next_index_zval(field_value, value);
    	Z_TRY_ADDREF_P(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_0(field_str);
}

void makeWhere(zval *self, smart_str *where_str, zval *where, zval *field_value) {
	zval *obj = NULL, *value = NULL,  *ops = NULL, *condition = NULL, *other, *tmp = NULL;
	bool pre = 0;
	zend_string *key = NULL;
	zend_long id;
	/* [GENE_FIX:2026-04-27] Defensive NULL guard: ZSTR_LEN(where_str->s)
	 * dereferences ->s. All current callers go through *_init_where which
	 * allocates, but this prevents a future regression. */
	if ((where_str->s == NULL || ZSTR_LEN(where_str->s) == 0)
	    && zend_hash_num_elements(Z_ARRVAL_P(where)) > 0) {
		smart_str_appends(where_str, " WHERE ");
	}
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(where), id, key, obj) {
		if (Z_TYPE_P(obj) == IS_ARRAY) {
			value = zend_hash_index_find(Z_ARRVAL_P(obj), 0);
			ops = zend_hash_index_find(Z_ARRVAL_P(obj), 1);
			condition = zend_hash_index_find(Z_ARRVAL_P(obj), 2);
			other = zend_hash_index_find(Z_ARRVAL_P(obj), 3);
			if (value == NULL) {
				/* [GENE_FIX:2026-04-27] Must null-terminate via smart_str_0
				 * before bailing; callers pass where_str.s->val to %s. */
				smart_str_0(where_str);
				return;
			}
	        if (key) {
		    	if (pre) {
		    		if (condition && Z_TYPE_P(condition) == IS_STRING) {
		    			smart_str_appends(where_str, " ");
		    			smart_str_appends(where_str, Z_STRVAL_P(condition));
		    			smart_str_appends(where_str, " ");
		    		} else {
		    			smart_str_appends(where_str, " and ");
		    		}
		    	} else {
		    		pre = 1;
		    	}
	        	if (other && Z_TYPE_P(other) == IS_STRING) {
	        		if (strcmp("(", Z_STRVAL_P(other)) == 0) {
	        			smart_str_appends(where_str, Z_STRVAL_P(other));
	        		}
	        	}
	        	smart_str_append(where_str, key);
		        if (ops && Z_TYPE_P(ops) == IS_STRING) {
		        	if (strcmp("in", Z_STRVAL_P(ops)) == 0) {
		        		if (Z_TYPE_P(value) == IS_ARRAY) {
		        			pre = 0;
		        			smart_str_appends(where_str, " in(");
		        			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(value), tmp) {
		        				if (pre) {
		        					smart_str_appends(where_str, ",?");
		        				} else {
		        					smart_str_appends(where_str, "?");
		        					pre = 1;
		        				}
				    	    	add_next_index_zval(field_value, tmp);
				    	    	Z_TRY_ADDREF_P(tmp);
		        			} ZEND_HASH_FOREACH_END();
		            		smart_str_appends(where_str, ")");
		        		} else {
		        			smart_str_appends(where_str, " in(?)");
			    	    	add_next_index_zval(field_value, value);
			    	    	Z_TRY_ADDREF_P(value);
		        		}
		        	} else {
		    			smart_str_appends(where_str, " ");
		    			smart_str_appends(where_str, Z_STRVAL_P(ops));
		    			smart_str_appends(where_str, " ?");
		    	    	add_next_index_zval(field_value, value);
		    	    	Z_TRY_ADDREF_P(value);
		        	}
		        } else {
		        	smart_str_appends(where_str, " = ?");
			    	add_next_index_zval(field_value, value);
			    	Z_TRY_ADDREF_P(value);
		        }
	        } else {
		    	if (pre) {
		    		if (condition && Z_TYPE_P(condition) == IS_STRING) {
		    			smart_str_appends(where_str, " ");
		    			smart_str_appends(where_str, Z_STRVAL_P(condition));
		    			smart_str_appends(where_str, " ");
		    		} else {
		    			smart_str_appends(where_str, " and ");
		    		}
		    	} else {
		    		pre = 1;
		    	}
	        	if (other && Z_TYPE_P(other) == IS_STRING) {
	        		if (strcmp("(", Z_STRVAL_P(other)) == 0) {
	        			smart_str_appends(where_str, Z_STRVAL_P(other));
	        		}
	        	}
        		if (Z_TYPE_P(value) == IS_ARRAY) {
        			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(value), tmp) {
		    	    	add_next_index_zval(field_value, tmp);
		    	    	Z_TRY_ADDREF_P(tmp);
        			} ZEND_HASH_FOREACH_END();
        		} else {
	    	    	add_next_index_zval(field_value, value);
	    	    	Z_TRY_ADDREF_P(value);
        		}
		    	smart_str_appends(where_str, Z_STRVAL_P(ops));
	        }
        	if (other && Z_TYPE_P(other) == IS_STRING) {
        		if (strcmp(")", Z_STRVAL_P(other)) == 0) {
        			smart_str_appends(where_str, Z_STRVAL_P(other));
        		}
        	}
		} else {
	        if (key) {
	        	if (obj && Z_TYPE_P(obj) == IS_STRING) {
	        		if (strcmp("(", Z_STRVAL_P(obj)) == 0) {
				    	if (pre) {
				    		smart_str_appends(where_str, " and ");
				    	} else {
				    		pre = 1;
				    	}
	        			smart_str_appends(where_str, Z_STRVAL_P(obj));
	        		} else if (strcmp(")", Z_STRVAL_P(obj)) == 0) {
	        			smart_str_appends(where_str, Z_STRVAL_P(obj));
	        		} else {
	    		    	if (pre) {
	    		    		smart_str_appends(where_str, " and ");
	    		    	} else {
	    		    		pre = 1;
	    		    	}
	    	        	smart_str_append(where_str, key);
	    		        smart_str_appends(where_str, " = ?");
	    		    	add_next_index_zval(field_value, obj);
	    		    	Z_TRY_ADDREF_P(obj);
	        		}
	        	} else {
    		    	if (pre) {
    		    		smart_str_appends(where_str, " and ");
    		    	} else {
    		    		pre = 1;
    		    	}
    	        	smart_str_append(where_str, key);
    		        smart_str_appends(where_str, " = ?");
    		    	add_next_index_zval(field_value, obj);
    		    	Z_TRY_ADDREF_P(obj);
	        	}
	        } else {
	        	if (obj && Z_TYPE_P(obj) == IS_STRING) {
	        		if (strcmp("(", Z_STRVAL_P(obj)) == 0) {
				    	if (pre) {
				    		smart_str_appends(where_str, " and ");
				    	} else {
				    		pre = 1;
				    	}
	        			smart_str_appends(where_str, Z_STRVAL_P(obj));
	        		} else if (strcmp(")", Z_STRVAL_P(obj)) == 0) {
	        			smart_str_appends(where_str, Z_STRVAL_P(obj));
	        		} else {
				    	if (pre) {
				    		smart_str_appends(where_str, " and ");
				    	} else {
				    		pre = 1;
				    	}
				    	smart_str_appends(where_str, Z_STRVAL_P(obj));
	        		}
	        	}
	        }
		}
    } ZEND_HASH_FOREACH_END();
    smart_str_0(where_str);
}


bool checkPdoError(zend_object *ex) {
	zval *msg;
	zend_class_entry *ce;
	zval zv, rv;
	int i;
	const char *pdoErrorStr[9] = { "server has gone away", "no connection to the server", "Lost connection",
			"is dead or not enabled", "Error while sending", "server closed the connection unexpectedly",
			"Error writing data to the connection", "Resource deadlock avoided", "failed with errno" };

	ZVAL_OBJ(&zv, ex);
	ZVAL_UNDEF(&rv);
	ce = Z_OBJCE(zv);

	msg = zend_read_property(ce, gene_strip_obj(&zv), ZEND_STRL("message"), 0, &rv);
	if (msg && Z_TYPE_P(msg) == IS_STRING) {
		for (i = 0; i < 9; i++) {
			if (strstr(Z_STRVAL_P(msg), pdoErrorStr[i]) != NULL) {
				if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
				return 1;
			}
		}
	}
	if (!Z_ISUNDEF(rv)) zval_ptr_dtor(&rv);
	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
