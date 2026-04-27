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

void gene_quote_identifier(smart_str *dest, const char *name, size_t len, char oq, char cq) /*{{{*/
{
	smart_str_appendl(dest, name, len);
}/*}}}*/

char *gene_quote_table(const char *name, char oq, char cq) /*{{{*/
{
	return str_init(name);
}/*}}}*/

char *gene_quote_columns(const char *name, char oq, char cq) /*{{{*/
{
	return str_init(name);
}/*}}}*/

char *gene_quote_order(const char *name, char oq, char cq) /*{{{*/
{
	return str_init(name);
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
		static zend_function *fn = NULL;
		if (UNEXPECTED(!fn)) {
			fn = zend_hash_str_find_ptr(CG(function_table), ZEND_STRL("json_encode"));
		}
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
	ce = Z_OBJCE(zv);

	msg = zend_read_property(ce, gene_strip_obj(&zv), ZEND_STRL("message"), 0, &rv);
	if (!msg || Z_TYPE_P(msg) != IS_STRING) {
		return 0;
	}
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
