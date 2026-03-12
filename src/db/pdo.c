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
#include "zend_smart_str.h"
#include <string.h>

#include "../gene.h"
#include "../common/common.h"

void gene_quote_identifier(smart_str *dest, const char *name, size_t len, char oq, char cq) /*{{{*/
{
	size_t i, j, start = 0;
	for (i = 0; i <= len; i++) {
		if (i == len || name[i] == '.') {
			size_t part_len = i - start;
			if (start > 0) {
				smart_str_appendc(dest, '.');
			}
			if (part_len == 0) {
				start = i + 1;
				continue;
			}
			if ((part_len == 1 && name[start] == '*')) {
				smart_str_appendc(dest, '*');
			} else if ((name[start] == oq && name[start + part_len - 1] == cq)
					|| (oq == '[' && name[start] == '[' && name[start + part_len - 1] == ']')) {
				smart_str_appendl(dest, name + start, part_len);
			} else {
				smart_str_appendc(dest, oq);
				for (j = start; j < i; j++) {
					if (name[j] == cq) {
						smart_str_appendc(dest, cq);
					}
					smart_str_appendc(dest, name[j]);
				}
				smart_str_appendc(dest, cq);
			}
			start = i + 1;
		}
	}
}/*}}}*/

static const char *gene_ltrim_ptr(const char *str)
{
	while (*str && isspace((unsigned char)*str)) {
		str++;
	}
	return str;
}

static size_t gene_rtrim_len(const char *str, size_t len)
{
	while (len > 0 && isspace((unsigned char)str[len - 1])) {
		len--;
	}
	return len;
}

static int gene_is_sql_keyword(const char *token, size_t len)
{
	static const char *keywords[] = {
		"as", "left", "right", "inner", "outer", "join", "on", "and", "or",
		"not", "is", "null", "like", "in", "between", "using", "cross", "full",
		"asc", "desc"
	};
	size_t i;
	for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		if (strlen(keywords[i]) == len && strncasecmp(token, keywords[i], len) == 0) {
			return 1;
		}
	}
	return 0;
}

static int gene_is_numeric_token(const char *token, size_t len)
{
	size_t i;
	if (len == 0) {
		return 0;
	}
	for (i = 0; i < len; i++) {
		if (!isdigit((unsigned char)token[i])) {
			return 0;
		}
	}
	return 1;
}

static void gene_append_quoted_token(smart_str *dest, const char *token, size_t len, char oq, char cq)
{
	if (len == 0) {
		return;
	}
	if ((token[0] == oq && token[len - 1] == cq) || (token[0] == '\'' && token[len - 1] == '\'')) {
		smart_str_appendl(dest, token, len);
		return;
	}
	if (len == 1 && token[0] == '*') {
		smart_str_appendc(dest, '*');
		return;
	}
	if (gene_is_sql_keyword(token, len) || gene_is_numeric_token(token, len)) {
		smart_str_appendl(dest, token, len);
		return;
	}
	gene_quote_identifier(dest, token, len, oq, cq);
}

static void gene_quote_expression_range(smart_str *dest, const char *expr, size_t len, char oq, char cq)
{
	size_t i = 0;
	while (i < len) {
		if (isspace((unsigned char)expr[i])) {
			smart_str_appendc(dest, expr[i]);
			i++;
			continue;
		}
		if (expr[i] == '\'' || expr[i] == '"' || (oq == '`' && expr[i] == '`') || (oq == '[' && expr[i] == '[')) {
			char quote = expr[i++];
			char close_quote = (quote == '[') ? ']' : quote;
			smart_str_appendc(dest, quote);
			while (i < len) {
				smart_str_appendc(dest, expr[i]);
				if (expr[i] == close_quote) {
					if (quote != '[' && i + 1 < len && expr[i + 1] == close_quote) {
						i++;
						smart_str_appendc(dest, expr[i]);
					} else if (quote == '[' && i + 1 < len && expr[i + 1] == close_quote) {
						i++;
						smart_str_appendc(dest, expr[i]);
					} else {
						i++;
						break;
					}
				}
				i++;
			}
			continue;
		}
		if (isalnum((unsigned char)expr[i]) || expr[i] == '_') {
			size_t start = i;
			size_t j;
			while (i < len) {
				if (isalnum((unsigned char)expr[i]) || expr[i] == '_') {
					i++;
					continue;
				}
				if (expr[i] == '.') {
					if (i + 1 < len && (isalnum((unsigned char)expr[i + 1]) || expr[i + 1] == '_'
							|| expr[i + 1] == '*')) {
						i++;
						continue;
					}
					break;
				}
				if (expr[i] == '*' && i > start && expr[i - 1] == '.') {
					i++;
					continue;
				}
				break;
			}
			j = i;
			while (j < len && isspace((unsigned char)expr[j])) {
				j++;
			}
			if (j < len && expr[j] == '(' && !gene_is_sql_keyword(expr + start, i - start)) {
				smart_str_appendl(dest, expr + start, i - start);
			} else {
				gene_append_quoted_token(dest, expr + start, i - start, oq, cq);
			}
			continue;
		}
		smart_str_appendc(dest, expr[i]);
		i++;
	}
}

static void gene_quote_field_item(smart_str *dest, const char *item, size_t len, char oq, char cq)
{
	const char *start = gene_ltrim_ptr(item);
	size_t trimmed_len = gene_rtrim_len(start, len - (start - item));
	const char *as_pos = NULL;
	size_t i;
	int depth = 0;

	if (trimmed_len == 0) {
		return;
	}

	for (i = 0; i + 3 < trimmed_len; i++) {
		char ch = start[i];
		if (ch == '(') depth++;
		else if (ch == ')') depth--;
		if (depth == 0 && i > 0 &&
			isspace((unsigned char)start[i]) &&
			(start[i + 1] == 'a' || start[i + 1] == 'A') &&
			(start[i + 2] == 's' || start[i + 2] == 'S') &&
			isspace((unsigned char)start[i + 3])) {
			as_pos = start + i;
			break;
		}
	}

	if (as_pos) {
		size_t expr_len = gene_rtrim_len(start, as_pos - start);
		const char *alias = gene_ltrim_ptr(as_pos + 4);
		size_t alias_len = gene_rtrim_len(alias, trimmed_len - (alias - start));
		gene_quote_expression_range(dest, start, expr_len, oq, cq);
		smart_str_appends(dest, " AS ");
		gene_append_quoted_token(dest, alias, alias_len, oq, cq);
		return;
	}

	depth = 0;
	for (i = trimmed_len; i > 0; i--) {
		char ch = start[i - 1];
		if (ch == ')') depth++;
		else if (ch == '(') depth--;
		if (depth == 0 && isspace((unsigned char)ch)) {
			const char *alias = gene_ltrim_ptr(start + i);
			size_t expr_len = gene_rtrim_len(start, i - 1);
			size_t alias_len = gene_rtrim_len(alias, trimmed_len - (alias - start));
			if (alias_len > 0 && expr_len > 0) {
				gene_quote_expression_range(dest, start, expr_len, oq, cq);
				smart_str_appendc(dest, ' ');
				gene_append_quoted_token(dest, alias, alias_len, oq, cq);
				return;
			}
			break;
		}
	}

	gene_quote_expression_range(dest, start, trimmed_len, oq, cq);
}

static char *gene_quote_field_list_string(const char *name, char oq, char cq)
{
	smart_str buf = {0};
	size_t len = strlen(name), start = 0, i, depth = 0;
	int pre = 0;
	for (i = 0; i <= len; i++) {
		char ch = (i < len) ? name[i] : ',';
		if (ch == '(') depth++;
		else if (ch == ')') depth--;
		if (i == len || (ch == ',' && depth == 0)) {
			if (pre) {
				smart_str_appends(&buf, ",");
			}
			gene_quote_field_item(&buf, name + start, i - start, oq, cq);
			pre = 1;
			start = i + 1;
		}
	}
	smart_str_0(&buf);
	if (!buf.s) {
		return str_init("");
	}
	{
		char *result = str_init(ZSTR_VAL(buf.s));
		smart_str_free(&buf);
		return result;
	}
}

static char *gene_quote_order_list_string(const char *name, char oq, char cq)
{
	smart_str buf = {0};
	size_t len = strlen(name), start = 0, i, depth = 0;
	int pre = 0;
	for (i = 0; i <= len; i++) {
		char ch = (i < len) ? name[i] : ',';
		if (ch == '(') depth++;
		else if (ch == ')') depth--;
		if (i == len || (ch == ',' && depth == 0)) {
			const char *item_start = name + start;
			const char *item = gene_ltrim_ptr(item_start);
			size_t item_len = i - start;
			item_len -= (size_t) (item - item_start);
			item_len = gene_rtrim_len(item, item_len);
			if (pre) {
				smart_str_appends(&buf, ",");
			}
			gene_quote_expression_range(&buf, item, item_len, oq, cq);
			pre = 1;
			start = i + 1;
		}
	}
	smart_str_0(&buf);
	if (!buf.s) {
		return str_init("");
	}
	{
		char *result = str_init(ZSTR_VAL(buf.s));
		smart_str_free(&buf);
		return result;
	}
}

char *gene_quote_table(const char *name, char oq, char cq) /*{{{*/
{
	smart_str buf = {0};
	gene_quote_expression_range(&buf, name, strlen(name), oq, cq);
	smart_str_0(&buf);
	char *result = str_init(ZSTR_VAL(buf.s));
	smart_str_free(&buf);
	return result;
}/*}}}*/

char *gene_quote_columns(const char *name, char oq, char cq) /*{{{*/
{
	return gene_quote_field_list_string(name, oq, cq);
}/*}}}*/

char *gene_quote_order(const char *name, char oq, char cq) /*{{{*/
{
	return gene_quote_order_list_string(name, oq, cq);
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
    *result = str_init(ZSTR_VAL(field_str.s));
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
    *result = str_init(ZSTR_VAL(field_str.s));
    smart_str_free(&field_str);
}/*}}}*/

void gene_pdo_construct(zval *pdo_object, zval *dsn, zval *user, zval *pass, zval *options) /*{{{*/
{
    zval retval;
    zval function_name;
    ZVAL_STRING(&function_name, "__construct");
    if (options) {
        zval params[4] = { *dsn, *user, *pass, *options };
        uint32_t param_count = 4;
        call_user_function(NULL, pdo_object, &function_name, &retval, param_count, params);
    } else {
        zval params[3] = { *dsn, *user, *pass};
        uint32_t param_count = 3;
        call_user_function(NULL, pdo_object, &function_name, &retval, param_count, params);
    }
    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_begin_transaction(zval *pdo_object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "beginTransaction");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_commit(zval *pdo_object, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "commit");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_pdo_exec(zval *pdo_object, char *sql, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "exec");
    uint32_t param_count = 1;
    zval pdo_sql;
    ZVAL_STRING(&pdo_sql, sql);
    zval params[] = { pdo_sql };
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    zval_ptr_dtor(&pdo_sql);
}/*}}}*/

void gene_pdo_in_transaction(zval *pdo_object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "inTransaction");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_last_insert_id(zval *pdo_object, char *name, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "lastInsertId");
    if (name) {
        uint32_t param_count = 1;
        zval id_name;
        ZVAL_STRING(&id_name, name);
        zval params[] = { id_name };
        call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
        zval_ptr_dtor(&id_name);
    } else {
        uint32_t param_count = 0;
        zval *params = NULL;
        call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    }
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_error_code(zval *pdo_object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "errorCode");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_error_info(zval *pdo_object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "errorInfo");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

bool show_sql_errors(zval *pdo_object)
{
    zval retval;
    zend_string *ok_state;
    gene_pdo_error_info(pdo_object, &retval);
    zval *sql_state = zend_hash_index_find(Z_ARRVAL(retval), 0);
    zval *sql_code = zend_hash_index_find(Z_ARRVAL_P(&retval), 1);
    zval *sql_info = zend_hash_index_find(Z_ARRVAL_P(&retval), 2);
    ok_state = zend_string_init(ZEND_STRL("00000"), 0);
    if (!zend_string_equals(Z_STR_P(sql_state), ok_state)) {
    	zend_string_release(ok_state);
    	php_error_docref(NULL, E_ERROR, "SQL: %d %s", Z_LVAL_P(sql_code), Z_STRVAL_P(sql_info));
    	zval_ptr_dtor(&retval);
        return 1;
    }
    zend_string_release(ok_state);
    zval_ptr_dtor(&retval);
    return 0;
}/*}}}*/

void gene_pdo_prepare(zval *pdo_object, char *sql, zval *retval) /*{{{ RETURN a PDOStatement */
{
    zval function_name;
    ZVAL_STRING(&function_name, "prepare");
    if (sql) {
        uint32_t param_count = 1;
        zval prepare_sql;
        ZVAL_STRING(&prepare_sql, sql);
        zval params[1];
        ZVAL_COPY(&params[0], &prepare_sql);
        call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
        zval_ptr_dtor(&params[0]);
        zval_ptr_dtor(&prepare_sql);
    }
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_rollback(zval *pdo_object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "rollBack");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_statement_execute(zval *pdostatement_obj, zval *bind_parameters, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "execute");
    if (bind_parameters){
        uint32_t param_count = 1;
        zval params[1];
        ZVAL_COPY(&params[0], bind_parameters);
        call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
        zval_ptr_dtor(&params[0]);
    } else {
        uint32_t param_count = 0;
        zval *params = NULL;
        call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
    }
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_statement_fetch(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "fetch");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_statement_fetch_all(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "fetchAll");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_statement_fetch_column(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "fetchColumn");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_statement_fetch_object(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "fetchObject");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/


void gene_pdo_statement_row_count(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "rowCount");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_pdo_statement_set_fetch_mode(zval *pdostatement_obj, int fetch_style, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "setFetchMode");
    uint32_t param_count = 1;
    zval pdo_fetch_style;
    ZVAL_LONG(&pdo_fetch_style, fetch_style);
    zval params[] = { pdo_fetch_style };
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void jsonEncode(zval *data, zval *param) {
	zval func, ret;
	ZVAL_STRING(&func, "json_encode");
	ZVAL_NULL(&ret);
	if (Z_TYPE_P(param) == IS_ARRAY) {
		call_user_function(EG(function_table), NULL, &func, &ret, 1, param);
		if (Z_TYPE(ret) == IS_STRING) {
			ZVAL_STRING(data, Z_STRVAL(ret));
		}
	} else {
		ZVAL_NULL(data);
	}
	zval_ptr_dtor(&func);
	zval_ptr_dtor(&ret);
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
	if (ZSTR_LEN(where_str->s) == 0 && zend_hash_num_elements(Z_ARRVAL_P(where)) > 0) {
		smart_str_appends(where_str, " WHERE ");
	}
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(where), id, key, obj) {
		if (Z_TYPE_P(obj) == IS_ARRAY) {
			value = zend_hash_index_find(Z_ARRVAL_P(obj), 0);
			ops = zend_hash_index_find(Z_ARRVAL_P(obj), 1);
			condition = zend_hash_index_find(Z_ARRVAL_P(obj), 2);
			other = zend_hash_index_find(Z_ARRVAL_P(obj), 3);
			if (value == NULL) {
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
	ce = Z_OBJCE(zv);

	msg = zend_read_property(ce, gene_strip_obj(&zv), ZEND_STRL("message"), 0, &rv);
	for (i = 0; i < 9; i++) {
		if (strstr(Z_STRVAL_P(msg), pdoErrorStr[i]) != NULL) {
			return 1;
		}
	}
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
