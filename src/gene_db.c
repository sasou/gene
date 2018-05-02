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
#include "gene_db.h"

zend_class_entry * gene_db_ce;

void reset_sql_params (zval *self)
{
	zend_update_property_long(gene_db_ce, self, GENE_DB_TYPE, strlen(GENE_DB_TYPE), 0);
	zend_update_property_string(gene_db_ce, self, GENE_DB_SQL, strlen(GENE_DB_SQL), "");
	zend_update_property_string(gene_db_ce, self, GENE_DB_WHERE, strlen(GENE_DB_WHERE), "");
	zend_update_property_string(gene_db_ce, self, GENE_DB_ORDER, strlen(GENE_DB_ORDER), "");
	zend_update_property_string(gene_db_ce, self, GENE_DB_LIMIT, strlen(GENE_DB_LIMIT), "");
    zend_update_property_null(gene_db_ce, self, GENE_DB_DATA, strlen(GENE_DB_DATA));
}

void array_to_string(zval *array, char **result)
{
    zval *value;
    smart_str field_str = {0};
    zend_bool pre = FALSE;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(array), value) {
    	if (pre) {
    		smart_str_appends(&field_str, ",`");
    	} else {
    		smart_str_appendc(&field_str, '`');
    		pre = TRUE;
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
}/*}}}*/

void gene_pdo_commit(zval *pdo_object, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "commit");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
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
    } else {
        uint32_t param_count = 0;
        zval *params = NULL;
        call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
    }
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

void gene_pdo_roll_back(zval *pdo_object, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "rollBack");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdo_object, &function_name, retval, param_count, params);
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
    return ;
}/*}}}*/

void gene_pdo_statement_fetch(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "fetch");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
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

void gene_pdo_statement_fetch_object(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "fetchObject");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
}/*}}}*/


void gene_pdo_statement_row_count(zval *pdostatement_obj, zval *retval)/*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "rowCount");
    uint32_t param_count = 0;
    zval *params = NULL;
    call_user_function(NULL, pdostatement_obj, &function_name, retval, param_count, params);
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
}/*}}}*/


void gene_pdo_execute (zval *self, zval *statement)
{
	zval *pdo_object = NULL, *params = NULL, *type = NULL, *pdo_sql = NULL;
	zval retval;
	char *sql = NULL;
	pdo_object = zend_read_property(gene_db_ce, self, GENE_DB_PDO, strlen(GENE_DB_PDO), 1, NULL);
	pdo_sql = zend_read_property(gene_db_ce, self, GENE_DB_SQL, strlen(GENE_DB_SQL), 1, NULL);

	spprintf(&sql, 0, "%s", Z_STRVAL_P(pdo_sql));
	gene_pdo_prepare(pdo_object, sql, statement);
	efree(sql);
	if (statement != NULL) {
		params = zend_read_property(gene_db_ce, self, GENE_DB_DATA, strlen(GENE_DB_DATA), 1, NULL);
		//execute
		gene_pdo_statement_execute(statement, params, &retval);
	}
    zval_ptr_dtor(&retval);
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

    	if (EXPECTED(dsn = zend_hash_str_find(config->value.arr, "dsn", 3)) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "PDO need a valid dns.");
    		 RETURN_FALSE;
    	}
    	if (EXPECTED(user = zend_hash_str_find(config->value.arr, "username", 8)) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "PDO need a valid username.");
    		 RETURN_FALSE;
    	}
    	if (EXPECTED(pass = zend_hash_str_find(config->value.arr, "password", 8)) == NULL) {
    		 php_error_docref(NULL, E_ERROR, "PDO need a valid password.");
    		 RETURN_FALSE;
    	}
    	options = zend_hash_str_find(config->value.arr, "options", 7);
        if (options == NULL) {
        	array_init(&option);
        	add_index_long(&option, 19, 2);
        	gene_pdo_construct(&pdo_object, dsn, user, pass, &option);
        	zval_ptr_dtor(&option);
        } else {
        	add_index_long(options, 19, 2);
        	gene_pdo_construct(&pdo_object, dsn, user, pass, options);
        }
        zend_update_property(gene_db_ce, self, GENE_DB_PDO, strlen(GENE_DB_PDO), &pdo_object);
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
	zval *pdo_object = zend_read_property(gene_db_ce, self, GENE_DB_PDO, strlen(GENE_DB_PDO), 1, NULL);
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
	int table_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	reset_sql_params (self);
    if (fields) {
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		array_to_string(fields, &select);
            spprintf(&sql, 0, "SELECT %s FROM `%s` ", select, table);
            efree(select);
    		break;
    	case IS_STRING:
    		spprintf(&sql, 0, "SELECT %s FROM `%s` ", Z_STRVAL_P(fields), table);
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter can only be array or string.");
    		break;
    	}

    } else {
    	spprintf(&sql, 0, "SELECT * FROM `%s` ", table);
    }
    php_printf(" sql:%s ", sql);
    zend_update_property_string(gene_db_ce, self, GENE_DB_SQL, strlen(GENE_DB_SQL), sql);
    efree(sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, insert)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, update)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, delete)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, where)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, in)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, sql)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, execute)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, order)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, limit)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, all)
{
	zval *self = getThis();
	zval statement, retval;
	gene_pdo_execute(self, &statement);
	gene_pdo_statement_fetch_all(&statement, &retval);
	zval_ptr_dtor(&statement);
	RETURN_ZVAL(&retval, 1, 1);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, row)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, cell)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db, lastId)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::affectedRows($key)
 */
PHP_METHOD(gene_db, affectedRows)
{
	zval *self = getThis();
	char *php_script;
	int php_script_len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &php_script, &php_script_len) == FAILURE) {
		return;
	}
	if (php_script_len) {
		php_printf(" key:%s ",php_script);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ gene_db_methods
 */
zend_function_entry gene_db_methods[] = {
		PHP_ME(gene_db, getPdo, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, select, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db, insert, NULL, ZEND_ACC_PUBLIC)
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
	zend_declare_property_null(gene_db_ce, GENE_DB_PDO, strlen(GENE_DB_PDO), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, GENE_DB_TYPE, strlen(GENE_DB_TYPE), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, GENE_DB_SQL, strlen(GENE_DB_SQL), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, GENE_DB_WHERE, strlen(GENE_DB_WHERE), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, GENE_DB_ORDER, strlen(GENE_DB_ORDER), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, GENE_DB_LIMIT, strlen(GENE_DB_LIMIT), ZEND_ACC_PUBLIC TSRMLS_CC);
    zend_declare_property_null(gene_db_ce, GENE_DB_DATA, strlen(GENE_DB_DATA), ZEND_ACC_PUBLIC TSRMLS_CC);

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
