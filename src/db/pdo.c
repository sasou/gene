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

#include "../gene.h"
#include "../common/common.h"

void array_to_string(zval *array, char **result)
{
    zval *value;
    smart_str field_str = {0};
    zend_bool pre = 0;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(array), value) {
    	if (pre) {
    		smart_str_appends(&field_str, ",`");
    	} else {
    		smart_str_appendc(&field_str, '`');
    		pre = 1;
    	}
        if ( Z_TYPE_P(value) == IS_OBJECT ) convert_to_string(value);
        if ( (Z_TYPE_P(value) == IS_STRING) && isalpha(*(Z_STRVAL_P(value))) ) {
            smart_str_appends(&field_str, Z_STRVAL_P(value));
        }
        smart_str_appends(&field_str, "`");
    } ZEND_HASH_FOREACH_END();
    smart_str_0(&field_str);
    *result = str_init(ZSTR_VAL(field_str.s));
    smart_str_free(&field_str);
}/*}}}*/

void mssql_array_to_string(zval *array, char **result)
{
    zval *value;
    smart_str field_str = {0};
    zend_bool pre = 0;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(array), value) {
    	if (pre) {
    		smart_str_appends(&field_str, ",");
    	} else {
    		pre = 1;
    	}
        if ( Z_TYPE_P(value) == IS_OBJECT ) convert_to_string(value);
        if ( (Z_TYPE_P(value) == IS_STRING) && isalpha(*(Z_STRVAL_P(value))) ) {
            smart_str_appends(&field_str, Z_STRVAL_P(value));
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

zend_bool show_sql_errors(zval *pdo_object)
{
    zval retval;
    gene_pdo_error_info(pdo_object, &retval);
    zval *sql_state = zend_hash_index_find(Z_ARRVAL(retval), 0);
    zval *sql_code = zend_hash_index_find(Z_ARRVAL_P(&retval), 1);
    zval *sql_info = zend_hash_index_find(Z_ARRVAL_P(&retval), 2);
    if (!zend_string_equals(Z_STR_P(sql_state), strpprintf(0, "00000"))) {
    	php_error_docref(NULL, E_ERROR, "SQL: %d %s", Z_LVAL_P(sql_code), Z_STRVAL_P(sql_info));
    	zval_ptr_dtor(&retval);
        return 1;
    }
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
        zval params[] = { prepare_sql };
        call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
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
        zval params[] = { *bind_parameters };
        call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
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

void gene_mysql_insert_field_value(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value){
	zval *value = NULL;
	zend_bool pre = 0;
	zend_string *key = NULL;
	zend_long id;
	array_init(field_value);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(fields), id, key, value) {
    	if (pre) {
    		smart_str_appends(field_str, ",`");
    		smart_str_appends(value_str, ",");
    	} else {
    		smart_str_appendc(field_str, '`');
    		pre = 1;
    	}
        if (key) {
        	smart_str_append(field_str, key);
        } else {
        	smart_str_append_long(field_str, id);
        }
        smart_str_appends(value_str, "?");
        smart_str_appends(field_str, "`");
    	add_next_index_zval(field_value, value);
    	Z_TRY_ADDREF_P(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_0(field_str);
    smart_str_0(value_str);
}

void gene_mysql_insert_field_value_batch(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value) {
	zval *value = NULL;
	zend_bool pre = 0;
	zend_string *key = NULL;
	zend_long id;
	array_init(field_value);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(fields), id, key, value) {
    	if (pre) {
    		smart_str_appends(field_str, ",`");
    		smart_str_appends(value_str, ",");
    	} else {
    		smart_str_appendc(field_str, '`');
    		smart_str_appends(value_str, "(");
    		pre = 1;
    	}
        if (key) {
        	smart_str_append(field_str, key);
        } else {
        	smart_str_append_long(field_str, id);
        }
        smart_str_appends(value_str, "?");
        smart_str_appends(field_str, "`");
        add_next_index_zval(field_value, value);
        Z_TRY_ADDREF_P(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_appends(value_str, ")");
    smart_str_0(field_str);
    smart_str_0(value_str);
}

void gene_insert_field_value_batch_other(zval *fields, smart_str *value_str, zval *field_value) {
	zval *value = NULL;
	zend_bool pre = 0;
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

void gene_mysql_update_field_value(zval *fields, smart_str *field_str, zval *field_value) {
	zval *value = NULL;
	zend_bool pre = 0;
	zend_string *key = NULL;
	zend_long id;
	array_init(field_value);
	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(fields), id, key, value) {
    	if (pre) {
    		smart_str_appends(field_str, ",`");
    	} else {
    		smart_str_appendc(field_str, '`');
    		pre = 1;
    	}
        if (key) {
        	smart_str_append(field_str, key);
        } else {
        	smart_str_append_long(field_str, id);
        }
        smart_str_appends(field_str, "`=?");
    	add_next_index_zval(field_value, value);
    	Z_TRY_ADDREF_P(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_0(field_str);
}

void gene_insert_field_value(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value){
	zval *value = NULL;
	zend_bool pre = 0;
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
        	smart_str_append(field_str, key);
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

void gene_insert_field_value_batch(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value) {
	zval *value = NULL;
	zend_bool pre = 0;
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
        	smart_str_append(field_str, key);
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

void gene_update_field_value(zval *fields, smart_str *field_str, zval *field_value) {
	zval *value = NULL;
	zend_bool pre = 0;
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
        	smart_str_append(field_str, key);
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
	zend_bool pre = 0;
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


zend_bool checkPdoError(zend_object *ex) {
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
