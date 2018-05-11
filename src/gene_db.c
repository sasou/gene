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

#include "php_gene.h"
#include "gene_common.h"
#include "gene_memory.h"
#include "gene_db.h"

zend_class_entry * gene_db_ce;

void reset_sql_params(zval *self)
{
	zend_update_property_null(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL));
	zend_update_property_null(gene_db_ce, self, ZEND_STRL(GENE_DB_WHERE));
	zend_update_property_null(gene_db_ce, self, ZEND_STRL(GENE_DB_ORDER));
	zend_update_property_null(gene_db_ce, self, ZEND_STRL(GENE_DB_LIMIT));
    zend_update_property_null(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA));
}

void set_value_ref(zval *field) {
	if (Z_TYPE_P(field) == IS_STRING) {
		Z_ADDREF_P(field);
	}
}

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

void gene_pdo_error_code(zval *pdostatement_obj, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "errorCode");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
}/*}}}*/

void gene_pdo_error_info(zval *pdostatement_obj, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "errorInfo");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
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


zend_bool gene_pdo_execute (zval *self, zval *statement)
{
	zval *pdo_object = NULL, *params = NULL, *pdo_sql = NULL, *pdo_where = NULL, *pdo_order = NULL, *pdo_limit = NULL;
	zval retval;
	smart_str sql = {0};
	pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	pdo_sql = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), 1, NULL);
	pdo_where = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_WHERE), 1, NULL);
	pdo_order = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_ORDER), 1, NULL);
	pdo_limit = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_LIMIT), 1, NULL);

	if (Z_TYPE_P(pdo_sql) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_sql));
	}
	if (Z_TYPE_P(pdo_where) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_where));
	}
	if (Z_TYPE_P(pdo_order) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_order));
	}
	if (Z_TYPE_P(pdo_limit) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_limit));
	}
	smart_str_0(&sql);
	gene_pdo_prepare(pdo_object, ZSTR_VAL(sql.s), statement);
	smart_str_free(&sql);
	if (statement != NULL) {
		params = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), 1, NULL);
		//execute
		gene_pdo_statement_execute(statement, params, &retval);
		return Z_TYPE(retval) == 3 ? 1 : 0;
	}
    return 0;
}

/*
 * {{{ gene_db
 */
PHP_METHOD(gene_db, __construct)
{
	zval *config = NULL, *dsn = NULL, *user = NULL, *pass = NULL, *options = NULL, *self = getThis();
	zval pdo_object, option;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"z", &config) == FAILURE)
    {
        return;
    }
    if (config) {
		zend_string *c_key = zend_string_init(ZEND_STRL("PDO"), 0);
		zend_class_entry *pdo_ptr = zend_lookup_class(c_key);
		zend_string_free(c_key);
		object_init_ex(&pdo_object, pdo_ptr);

    	if ((dsn = zend_hash_str_find(config->value.arr, ZEND_STRL("dsn"))) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "PDO need a valid dns.");
    		 RETURN_FALSE;
    	}
    	if ((user = zend_hash_str_find(config->value.arr, ZEND_STRL("username"))) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "PDO need a valid username.");
    		 RETURN_FALSE;
    	}
    	if ((pass = zend_hash_str_find(config->value.arr, ZEND_STRL("password"))) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "PDO need a valid password.");
    		 RETURN_FALSE;
    	}
    	options = zend_hash_str_find(config->value.arr, ZEND_STRL("options"));
        if (options == NULL) {
        	array_init(&option);
        	add_index_long(&option, 19, 2);
        	gene_pdo_construct(&pdo_object, dsn, user, pass, &option);
        	zval_ptr_dtor(&option);
        } else {
        	add_index_long(options, 19, 2);
        	gene_pdo_construct(&pdo_object, dsn, user, pass, options);
        }
        zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), &pdo_object);
        zval_ptr_dtor(&pdo_object);
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::getPdo()
 */
PHP_METHOD(gene_db, getPdo)
{
	zval *self = getThis();
	zval *pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	RETURN_ZVAL(pdo_object, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, select)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	reset_sql_params (self);
    if (fields) {
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		array_to_string(fields, &select);
            spprintf(&sql, 0, "SELECT %s FROM `%s`", select, table);
            efree(select);
    		break;
    	case IS_STRING:
    		spprintf(&sql, 0, "SELECT %s FROM `%s`", Z_STRVAL_P(fields), table);
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter can only be array or string.");
    		break;
    	}

    } else {
    	spprintf(&sql, 0, "SELECT * FROM `%s`", table);
    }
    zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), sql);
    efree(sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_model::count($key)
 */
PHP_METHOD(gene_db, count)
{
	zval *self = getThis();
	char *sql = NULL;
	zend_string *table = NULL,*fields = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S|S", &table, &fields) == FAILURE) {
		return;
	}
	reset_sql_params(self);
    if (fields) {
    	spprintf(&sql, 0, "SELECT count(%s) AS count FROM `%s`", ZSTR_VAL(fields), ZSTR_VAL(table));
    } else {
    	spprintf(&sql, 0, "SELECT count(1) AS count FROM `%s`", ZSTR_VAL(table));
    }
    zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), sql);
    efree(sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

void gene_insert_field_value (zval *fields, smart_str *field_str, smart_str *value_str,zval *field_value) {
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
    	set_value_ref(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_0(field_str);
    smart_str_0(value_str);
}

void gene_insert_field_value_batch (zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value) {
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
        set_value_ref(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_appends(value_str, ")");
    smart_str_0(field_str);
    smart_str_0(value_str);
}

void gene_insert_field_value_batch_other (zval *fields, smart_str *value_str, zval *field_value) {
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
        set_value_ref(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_appends(value_str, ")");
    smart_str_0(value_str);
}

void gene_update_field_value (zval *fields, smart_str *field_str, zval *field_value) {
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
    	set_value_ref(value);
    } ZEND_HASH_FOREACH_END();
    smart_str_0(field_str);
}

/*
 * {{{ public gene_db::insert($key)
 */
PHP_METHOD(gene_db, insert)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len;
	smart_str field_str = {0} , value_str = {0};
	zval field_value;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	reset_sql_params(self);
    if (fields && Z_TYPE_P(fields) == IS_ARRAY) {
    	gene_insert_field_value (fields, &field_str, &value_str, &field_value);
    	zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
    spprintf(&sql, 0, "INSERT INTO `%s`(%s) VALUES(%s)", table, field_str.s->val, value_str.s->val);
    zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), sql);
    efree(sql);
    smart_str_free(&field_str);
    smart_str_free(&value_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::batchInsert($key)
 */
PHP_METHOD(gene_db, batchInsert)
{
	zval *self = getThis(),*fields = NULL, *row = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len;
	smart_str field_str = {0} , value_str = {0};
	zval field_value;
	zend_bool pre = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	reset_sql_params(self);
    if (fields && Z_TYPE_P(fields) == IS_ARRAY) {
    	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), row) {
        	if (pre) {
        		smart_str_appends(&value_str, ",");
        		gene_insert_field_value_batch_other (row, &value_str, &field_value);
        	} else {
        		gene_insert_field_value_batch (row, &field_str, &value_str, &field_value);
        		pre = 1;
        	}
        } ZEND_HASH_FOREACH_END();
    	zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
    spprintf(&sql, 0, "INSERT INTO `%s`(%s) VALUES %s", table, field_str.s->val, value_str.s->val);
    zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), sql);
    efree(sql);
    smart_str_free(&field_str);
    smart_str_free(&value_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::update($key)
 */
PHP_METHOD(gene_db, update)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len;
	smart_str field_str = {0};
	zval field_value;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	reset_sql_params(self);
    if (fields && Z_TYPE_P(fields) == IS_ARRAY) {
    	gene_update_field_value (fields, &field_str, &field_value);
    	zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
    spprintf(&sql, 0, "UPDATE `%s` SET %s", table, field_str.s->val);
    zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), sql);
    efree(sql);
    smart_str_free(&field_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::delete($key)
 */
PHP_METHOD(gene_db, delete)
{
	zval *self = getThis();
	char *table = NULL, *sql = NULL;
	size_t table_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &table, &table_len) == FAILURE) {
		return;
	}
	reset_sql_params(self);
    spprintf(&sql, 0, "DELETE FROM `%s`", table);
    zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), sql);
    efree(sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::where($key)
 */
PHP_METHOD(gene_db, where)
{
	zval *self = getThis(), *fields = NULL, *data = NULL, *value = NULL;
	char *where = NULL, *sql_where = NULL;
	size_t where_len;
	zval params;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &where, &where_len, &fields) == FAILURE) {
		return;
	}

    if (where_len) {
        spprintf(&sql_where, 0, " WHERE %s", where);
        zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_WHERE), sql_where);
        efree(sql_where);
    }
    if (fields) {
    	data = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), 1, NULL);
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), value) {
    				add_next_index_zval(data, value);
    				set_value_ref(value);
    			} ZEND_HASH_FOREACH_END();
    		} else {
    			gene_memory_zval_local(&params, fields);
    			zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &params);
    			zval_ptr_dtor(&params);
    		}
    		break;
    	case IS_STRING:
    	case IS_LONG:
    	case IS_DOUBLE:
    	case IS_FALSE:
    	case IS_TRUE:
    	case IS_NULL:
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			add_next_index_zval(data, fields);
    			set_value_ref(fields);
    		} else {
            	array_init(&params);
            	add_next_index_zval(&params, fields);
            	set_value_ref(fields);
            	zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &params);
            	zval_ptr_dtor(&params);
    		}
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter error.");
    		break;
    	}
    }
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::in($key)
 */
PHP_METHOD(gene_db, in)
{
	zval *self = getThis(), *fields = NULL, *data = NULL, *value = NULL, *pdo_where = NULL;
	char *in = NULL, *sql_in = NULL, *seg = NULL, *ptr = NULL, *in_tmp = NULL;
	size_t in_len;
	zval params;
	smart_str field_str = {0},value_str = {0};
	zend_bool pre = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &in, &in_len, &fields) == FAILURE) {
		return;
	}
	pdo_where = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_WHERE), 1, NULL);
	if (pdo_where && Z_TYPE_P(pdo_where) == IS_STRING) {
		smart_str_appends(&field_str, Z_STRVAL_P(pdo_where));
	}
	if (in_len) {
		spprintf(&in_tmp, 0, "%s", in, in_len);
	}
    if (fields) {
    	data = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), 1, NULL);
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		ReplaceStr(in_tmp, "in(?)", "$");
    		seg = php_strtok_r(in_tmp, "$", &ptr);
    		if (seg) {
    			smart_str_appends(&field_str, seg);
    		}
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), value) {
    				if (pre) {
    					smart_str_appends(&value_str, ",?");
    				} else {
    					smart_str_appends(&value_str, " in(?");
    					pre = 1;
    				}
    				add_next_index_zval(data, value);
    				set_value_ref(value);
    			} ZEND_HASH_FOREACH_END();
    		} else {
    			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), value) {
    				if (pre) {
    					smart_str_appends(&value_str, ",?");
    				} else {
    					smart_str_appends(&value_str, " in(?");
    					pre = 1;
    				}
    			} ZEND_HASH_FOREACH_END();
    			zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), fields);
    		}
    		smart_str_appends(&value_str, ")");
    		smart_str_0(&value_str);
    		smart_str_appends(&field_str, ZSTR_VAL(value_str.s));
    		smart_str_free(&value_str);
    		if (ptr) {
    			smart_str_appends(&field_str, ptr);
    		}
    		break;
    	case IS_STRING:
    	case IS_LONG:
    	case IS_DOUBLE:
    	case IS_FALSE:
    	case IS_TRUE:
    	case IS_NULL:
    		smart_str_appends(&field_str, in_tmp);
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			add_next_index_zval(data, fields);
    			set_value_ref(fields);
    		} else {
            	array_init(&params);
            	add_next_index_zval(&params, fields);
            	set_value_ref(fields);
            	zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &params);
            	zval_ptr_dtor(&params);
    		}
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter error.");
    		break;
    	}
    }
	smart_str_0(&field_str);
    if (in_len) {
    	efree(in_tmp);
        zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_WHERE), ZSTR_VAL(field_str.s));
        smart_str_free(&field_str);
    }
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::sql($key)
 */
PHP_METHOD(gene_db, sql)
{
	zval *self = getThis(), *fields = NULL, *data = NULL, *value = NULL;
	char *sql = NULL, *pdo_sql = NULL;
	size_t sql_len;
	zval params;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &sql, &sql_len, &fields) == FAILURE) {
		return;
	}
	reset_sql_params(self);
    if (sql_len) {
        spprintf(&pdo_sql, 0, "%s", sql);
        zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_WHERE), pdo_sql);
        efree(pdo_sql);
    }
    if (fields) {
    	data = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), 1, NULL);
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), value) {
    				add_next_index_zval(data, value);
    				set_value_ref(value);
    			} ZEND_HASH_FOREACH_END();
    		} else {
    			gene_memory_zval_local(&params, fields);
    			set_value_ref(fields);
    			zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &params);
    			zval_ptr_dtor(&params);
    		}
    		break;
    	case IS_STRING:
    	case IS_LONG:
    	case IS_DOUBLE:
    	case IS_FALSE:
    	case IS_TRUE:
    	case IS_NULL:
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			add_next_index_zval(data, fields);
    			set_value_ref(fields);
    		} else {
            	array_init(&params);
            	add_next_index_zval(&params, fields);
            	set_value_ref(fields);
            	zend_update_property(gene_db_ce, self, ZEND_STRL(GENE_DB_DATA), &params);
            	zval_ptr_dtor(&params);
    		}
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter error.");
    		break;
    	}
    }
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::execute()
 */
PHP_METHOD(gene_db, execute)
{
	zval *self = getThis();
	zval statement;
	gene_pdo_execute(self, &statement);
	RETURN_ZVAL(&statement, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_db::order()
 */
PHP_METHOD(gene_db, order)
{
	zval *self = getThis();
	char *order = NULL;
	size_t order_len = 0;
	char *order_tmp;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &order, &order_len) == FAILURE) {
		return;
	}
	if (order_len) {
		spprintf(&order_tmp, 0, " ORDER BY %s", order);
		zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_ORDER), order_tmp);
		efree(order_tmp);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::limit()
 */
PHP_METHOD(gene_db, limit)
{
	zval *self = getThis();
	zend_long *num, *offset = NULL;
	char *limit;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|l", &num, &offset) == FAILURE) {
		return;
	}
	if (offset) {
		spprintf(&limit, 0, " LIMIT %d,%d", num, offset);
	} else {
		spprintf(&limit, 0, " LIMIT %d", num);
	}
	zend_update_property_string(gene_db_ce, self, ZEND_STRL(GENE_DB_LIMIT), limit);
	efree(limit);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::all()
 */
PHP_METHOD(gene_db, all)
{
	zval *self = getThis();
	zval statement, retval;
	if (gene_pdo_execute(self, &statement)) {
		gene_pdo_statement_fetch_all(&statement, &retval);
	}
	zval_ptr_dtor(&statement);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */


/*
 * {{{ public gene_db::row()
 */
PHP_METHOD(gene_db, row)
{
	zval *self = getThis();
	zval statement, retval;
	if (gene_pdo_execute(self, &statement)) {
		gene_pdo_statement_fetch(&statement, &retval);
	}
	zval_ptr_dtor(&statement);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */


/*
 * {{{ public gene_db::cell()
 */
PHP_METHOD(gene_db, cell)
{
	zval *self = getThis();
	zval statement, retval;
	if (gene_pdo_execute(self, &statement)) {
		gene_pdo_statement_fetch_column(&statement, &retval);
	}
	zval_ptr_dtor(&statement);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */


/*
 * {{{ public gene_db::lastId()
 */
PHP_METHOD(gene_db, lastId)
{
	zval *self = getThis(), *pdo_object = NULL;
	zval statement, retval;
	pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	if (gene_pdo_execute(self, &statement)) {
		gene_pdo_last_insert_id(pdo_object, NULL, &retval);
	}
	zval_ptr_dtor(&statement);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_db::affectedRows($key)
 */
PHP_METHOD(gene_db, affectedRows)
{
	zval *self = getThis();
	zval statement, retval;
	if (gene_pdo_execute(self, &statement)) {
		gene_pdo_statement_row_count(&statement, &retval);
	}
	zval_ptr_dtor(&statement);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_db::print($key)
 */
PHP_METHOD(gene_db, print)
{
	zval *self = getThis(),*pdo_object = NULL, *pdo_sql = NULL, *pdo_where = NULL, *pdo_order = NULL, *pdo_limit = NULL;
	smart_str sql = {0};
	pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	pdo_sql = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_SQL), 1, NULL);
	pdo_where = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_WHERE), 1, NULL);
	pdo_order = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_ORDER), 1, NULL);
	pdo_limit = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_LIMIT), 1, NULL);

	if (Z_TYPE_P(pdo_sql) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_sql));
	}
	if (Z_TYPE_P(pdo_where) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_where));
	}
	if (Z_TYPE_P(pdo_order) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_order));
	}
	if (Z_TYPE_P(pdo_limit) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_limit));
	}
	smart_str_0(&sql);
	php_printf(" SQL:%s ", ZSTR_VAL(sql.s));
	smart_str_free(&sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::beginTransaction()
 */
PHP_METHOD(gene_db, beginTransaction)
{
	zval *self = getThis(), *pdo_object = NULL;
	zval retval;
	pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	gene_pdo_begin_transaction(pdo_object, &retval);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_db::inTransaction()
 */
PHP_METHOD(gene_db, inTransaction)
{
	zval *self = getThis(), *pdo_object = NULL;
	zval retval;
	pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	gene_pdo_in_transaction(pdo_object, &retval);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_db::rollBack()
 */
PHP_METHOD(gene_db, rollBack)
{
	zval *self = getThis(), *pdo_object = NULL;
	zval retval;
	pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	gene_pdo_rollback(pdo_object, &retval);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */


/*
 * {{{ public gene_db::commit()
 */
PHP_METHOD(gene_db, commit)
{
	zval *self = getThis(), *pdo_object = NULL;
	zval retval;
	pdo_object = zend_read_property(gene_db_ce, self, ZEND_STRL(GENE_DB_PDO), 1, NULL);
	gene_pdo_commit(pdo_object, &retval);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */

/*
 * {{{ gene_db_methods
 */
zend_function_entry gene_db_methods[] = {
		PHP_ME(gene_db, getPdo, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, select, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, count, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, insert, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, batchInsert, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, update, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, delete, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, where, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, in, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, sql, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, limit, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, order, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, execute, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, all, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, row, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, cell, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, lastId, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, affectedRows, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, print, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, beginTransaction, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, inTransaction, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, rollBack, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, commit, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(db)
{
    zend_class_entry gene_db;
	GENE_INIT_CLASS_ENTRY(gene_db, "Gene_Db", "Gene\\Db", gene_db_methods);
	gene_db_ce = zend_register_internal_class_ex(&gene_db, NULL);
	gene_db_ce->ce_flags |= ZEND_ACC_FINAL;

	//pdo
	zend_declare_property_null(gene_db_ce, ZEND_STRL(GENE_DB_PDO), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, ZEND_STRL(GENE_DB_SQL), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, ZEND_STRL(GENE_DB_WHERE), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, ZEND_STRL(GENE_DB_ORDER), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, ZEND_STRL(GENE_DB_LIMIT), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, ZEND_STRL(GENE_DB_DATA), ZEND_ACC_PUBLIC TSRMLS_CC);

	return SUCCESS;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
