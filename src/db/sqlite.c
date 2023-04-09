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
#include "../cache/memory.h"
#include "../db/pdo.h"
#include "../db/sqlite.h"
#include "../tool/benchmark.h"

zend_class_entry * gene_db_sqlite_ce;

struct timeval db_start, db_end;
zend_long db_sqlite_memory_start = 0, db_sqlite_memory_end = 0;

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_construct, 0, 0, 1)
	ZEND_ARG_INFO(0, config)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_select, 0, 0, 1)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_count, 0, 0, 1)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_insert, 0, 0, 2)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_batch_insert, 0, 0, 2)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_update, 0, 0, 2)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, table)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_where, 0, 0, 1)
	ZEND_ARG_INFO(0, where)
	ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_in, 0, 0, 1)
	ZEND_ARG_INFO(0, in)
	ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_sql, 0, 0, 1)
	ZEND_ARG_INFO(0, sql)
	ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_group, 0, 0, 1)
	ZEND_ARG_INFO(0, group)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_having, 0, 0, 1)
	ZEND_ARG_INFO(0, having)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_order, 0, 0, 1)
	ZEND_ARG_INFO(0, order)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_limit, 0, 0, 1)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

void sqlite_reset_sql_params(zval *self)
{
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_GROUP));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_HAVING));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_ORDER));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LIMIT));
    zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA));
}

void sqliteSaveHistory(smart_str *sql, zval *param) {
	zval *history = NULL;
	zval params, z_row, z_sql, z_data, z_time, z_memory;
	char *char_t,*char_m;
	history = zend_read_static_property(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_HISTORY), 1);

	ZVAL_STRING(&z_sql, ZSTR_VAL(sql->s));

	jsonEncode(&z_data, param);

	getBenchTime(&db_start, &db_end, &char_t, 1);
	ZVAL_STRING(&z_time, char_t);
	efree(char_t);

    getBenchMemory(&db_sqlite_memory_start, &db_sqlite_memory_end, &char_m, 1);
	ZVAL_STRING(&z_memory, char_m);
	efree(char_m);

	array_init(&z_row);
	add_assoc_zval_ex(&z_row, ZEND_STRL("sql"), &z_sql);
	add_assoc_zval_ex(&z_row, ZEND_STRL("param"), &z_data);
	add_assoc_zval_ex(&z_row, ZEND_STRL("time"), &z_time);
	add_assoc_zval_ex(&z_row, ZEND_STRL("memory"), &z_memory);

	if (history && Z_TYPE_P(history) == IS_ARRAY) {
		add_next_index_zval(history, &z_row);
		Z_TRY_ADDREF_P(&z_row);
	} else {
    	array_init(&params);
    	add_next_index_zval(&params, &z_row);
    	Z_TRY_ADDREF_P(&z_row);
    	zend_update_static_property(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_HISTORY), &params);
    	zval_ptr_dtor(&params);
	}
}

void sqlite_init_where(zval *self, smart_str *where_str) {
	zval *pdo_where = NULL;
	pdo_where = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), 1, NULL);
	if (pdo_where) {
		if (Z_TYPE_P(pdo_where) == IS_STRING) {
			smart_str_appends(where_str, Z_STRVAL_P(pdo_where));
		} else {
			smart_str_appends(where_str, "");
		}
	}
}

zend_bool sqliteInitPdo (zval * self, zval *config) {
	zval  *dsn = NULL, *user = NULL, *pass = NULL, *options = NULL;
	zval pdo_object, option;

	if (config == NULL) {
		config =  zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_CONFIG), 1, NULL);
	}

	zend_string *c_key = zend_string_init(ZEND_STRL("PDO"), 0);
	zend_class_entry *pdo_ptr = zend_lookup_class(c_key);
	zend_string_release(c_key);

	object_init_ex(&pdo_object, pdo_ptr);

	if ((dsn = zend_hash_str_find(config->value.arr, ZEND_STRL("dsn"))) == NULL) {
		 php_error_docref(NULL, E_ERROR, "PDO need a valid dns.");
	}
	if ((user = zend_hash_str_find(config->value.arr, ZEND_STRL("username"))) == NULL) {
		 php_error_docref(NULL, E_ERROR, "PDO need a valid username.");
	}
	if ((pass = zend_hash_str_find(config->value.arr, ZEND_STRL("password"))) == NULL) {
		 php_error_docref(NULL, E_ERROR, "PDO need a valid password.");
	}
	options = zend_hash_str_find(config->value.arr, ZEND_STRL("options"));
    if (options == NULL || Z_TYPE_P(options) == IS_NULL) {
		array_init(&option);
    } else {
		ZVAL_COPY(&option, options);
    }
	#if PHP_VERSION_ID < 70300
		GC_REFCOUNT(Z_ARRVAL_P(&option)) = 1;
	#else
		GC_SET_REFCOUNT(Z_ARRVAL_P(&option), 1);
	#endif
	add_index_long(&option, 3, 2);
	add_index_long(&option, 19, 2);
	gene_pdo_construct(&pdo_object, dsn, user, pass, &option);
	zval_ptr_dtor(&option);

	if (EG(exception)) {
		if (checkPdoError(EG(exception))) {
			EG(exception) = NULL;
		}
	}
    zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), &pdo_object);
    zval_ptr_dtor(&pdo_object);
	return 0;
}

zend_bool gene_sqlite_pdo_execute (zval *self, zval *statement)
{
	zval *pdo_object = NULL, *params = NULL, *pdo_sql = NULL, *pdo_where = NULL, *pdo_group = NULL,*pdo_having = NULL,*pdo_order = NULL, *pdo_limit = NULL;
	zval retval;
	smart_str sql = {0};

	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	pdo_sql = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), 1, NULL);
	pdo_where = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), 1, NULL);
	pdo_group = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_GROUP), 1, NULL);
	pdo_having = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_HAVING), 1, NULL);
	pdo_order = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_ORDER), 1, NULL);
	pdo_limit = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LIMIT), 1, NULL);

	if (Z_TYPE_P(pdo_sql) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_sql));
	}
	if (Z_TYPE_P(pdo_where) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_where));
	}
	if (Z_TYPE_P(pdo_group) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_group));
	}
	if (Z_TYPE_P(pdo_having) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_having));
	}
	if (Z_TYPE_P(pdo_order) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_order));
	}
	if (Z_TYPE_P(pdo_limit) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_limit));
	}
	smart_str_0(&sql);
	if (!GENE_G(run_environment)) {
		markStart(&db_start, &db_sqlite_memory_start);
	}

	gene_pdo_prepare(pdo_object, ZSTR_VAL(sql.s), statement);
	if (Z_TYPE_P(statement) == IS_OBJECT) {
		params = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
		//execute
		ZVAL_NULL(&retval);
		gene_pdo_statement_execute(statement, params, &retval);

    	if (EG(exception)) {
    		if (checkPdoError(EG(exception))) {
    			EG(exception) = NULL;
    			sqliteInitPdo (self, NULL);
    			gene_pdo_prepare(pdo_object, ZSTR_VAL(sql.s), statement);
    			ZVAL_NULL(&retval);
    			gene_pdo_statement_execute(statement, params, &retval);
    		}
    	}
		if (!GENE_G(run_environment)) {
			markEnd(&db_end, &db_sqlite_memory_end);
			sqliteSaveHistory(&sql, params);
		}
		smart_str_free(&sql);
		return Z_TYPE(retval) == 3 ? 1 : 0;
	}
	smart_str_free(&sql);
    return 0;
}

/*
 * {{{ gene_db
 */
PHP_METHOD(gene_db_sqlite, __construct)
{
	zval *config = NULL, *self = getThis();
    if (zend_parse_parameters(ZEND_NUM_ARGS(),"z", &config) == FAILURE)
    {
        return;
    }

    zend_update_static_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_HISTORY));

    if (config) {
    	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_CONFIG), config);
    	sqliteInitPdo (self, config);
    }
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::getPdo()
 */
PHP_METHOD(gene_db_sqlite, getPdo)
{
	zval *self = getThis();
	zval *pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	RETURN_ZVAL(pdo_object, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_model::select($key)
 */
PHP_METHOD(gene_db_sqlite, select)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len; // @suppress("Type cannot be resolved")
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params (self);
    if (fields) {
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		mssql_array_to_string(fields, &select);
            spprintf(&sql, 0, "SELECT %s FROM %s", select, table);
            efree(select);
    		break;
    	case IS_STRING:
    		spprintf(&sql, 0, "SELECT %s FROM %s", Z_STRVAL_P(fields), table);
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter can only be array or string.");
    		break;
    	}

    } else {
    	spprintf(&sql, 0, "SELECT * FROM %s", table);
    }
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), sql);
    efree(sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_model::count($key)
 */
PHP_METHOD(gene_db_sqlite, count)
{
	zval *self = getThis();
	char *sql = NULL;
	zend_string *table = NULL,*fields = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S", &table, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
    if (fields) {
    	spprintf(&sql, 0, "SELECT count(%s) AS count FROM %s", ZSTR_VAL(fields), ZSTR_VAL(table));
    } else {
    	spprintf(&sql, 0, "SELECT count(1) AS count FROM %s", ZSTR_VAL(table));
    }
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), sql);
    efree(sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::insert($key)
 */
PHP_METHOD(gene_db_sqlite, insert)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len;// @suppress("Type cannot be resolved")
	smart_str field_str = {0} , value_str = {0};
	zval field_value;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
	ZVAL_NULL(&field_value);
	smart_str_appends(&field_str, "");
	smart_str_appends(&value_str, "");
    if (fields && Z_TYPE_P(fields) == IS_ARRAY) {
    	gene_insert_field_value (fields, &field_str, &value_str, &field_value);
    	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
	smart_str_0(&field_str);
	smart_str_0(&value_str);
    spprintf(&sql, 0, "INSERT INTO %s(%s) VALUES(%s)", table, field_str.s->val, value_str.s->val);
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), sql);
    efree(sql);
    smart_str_free(&field_str);
    smart_str_free(&value_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::batchInsert($key)
 */
PHP_METHOD(gene_db_sqlite, batchInsert)
{
	zval *self = getThis(),*fields = NULL, *row = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len;// @suppress("Type cannot be resolved")
	smart_str field_str = {0} , value_str = {0};
	zval field_value;
	zend_bool pre = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
	ZVAL_NULL(&field_value);
	smart_str_appends(&field_str, "");
	smart_str_appends(&value_str, "");
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
    	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
	smart_str_0(&field_str);
	smart_str_0(&value_str);
    spprintf(&sql, 0, "INSERT INTO %s(%s) VALUES %s", table, field_str.s->val, value_str.s->val);
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), sql);
    efree(sql);
    smart_str_free(&field_str);
    smart_str_free(&value_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::update($key)
 */
PHP_METHOD(gene_db_sqlite, update)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL,*select = NULL, *sql = NULL;
	size_t table_len; // @suppress("Type cannot be resolved")
	smart_str field_str = {0};
	zval field_value;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
	ZVAL_NULL(&field_value);
	smart_str_appends(&field_str, "");
    if (fields && Z_TYPE_P(fields) == IS_ARRAY) {
    	gene_update_field_value (fields, &field_str, &field_value);
    	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
	smart_str_0(&field_str);
    spprintf(&sql, 0, "UPDATE %s SET %s", table, field_str.s->val);
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), sql);
    efree(sql);
    smart_str_free(&field_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::delete($key)
 */
PHP_METHOD(gene_db_sqlite, delete)
{
	zval *self = getThis();
	char *table = NULL, *sql = NULL;
	size_t table_len; // @suppress("Type cannot be resolved")
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &table, &table_len) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
    spprintf(&sql, 0, "DELETE FROM %s", table);
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), sql);
    efree(sql);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::where($key)
 */
PHP_METHOD(gene_db_sqlite, where)
{
	zval *self = getThis(), *where = NULL, *fields = NULL, *data = NULL, *value = NULL;
	char  *sql_where = NULL;
	zval params;
	smart_str where_str = {0};

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &where, &fields) == FAILURE) {
		return;
	}

	sqlite_init_where(self, &where_str);

	switch(Z_TYPE_P(where)) {
	case IS_ARRAY:
        data = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
        if (Z_TYPE_P(data) == IS_ARRAY) {
        	makeWhere(self, &where_str, where, data);
        } else {
            array_init(&params);
            makeWhere(self, &where_str, where, &params);
            zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &params);
            zval_ptr_dtor(&params);
        }
		break;
	case IS_STRING:
		if (Z_STRLEN_P(where)) {
			if (ZSTR_LEN(where_str.s) == 0) {
				smart_str_appends(&where_str, " WHERE ");
			}
			smart_str_appends(&where_str, Z_STRVAL_P(where));
		}
	    if (fields) {
	    	data = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
	    	switch(Z_TYPE_P(fields)) {
	    	case IS_ARRAY:
	    		if (Z_TYPE_P(data) == IS_ARRAY) {
	    			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), value) {
	    				add_next_index_zval(data, value);
	    				Z_TRY_ADDREF_P(value);
	    			} ZEND_HASH_FOREACH_END();
	    		} else {
	    			gene_memory_zval_local(&params, fields);
	    			zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &params);
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
	    			Z_TRY_ADDREF_P(fields);
	    		} else {
	            	array_init(&params);
	            	add_next_index_zval(&params, fields);
	            	Z_TRY_ADDREF_P(fields);
	            	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &params);
	            	zval_ptr_dtor(&params);
	    		}
	    		break;
	    	default:
	    		php_error_docref(NULL, E_ERROR, "Parameter error.");
	    		break;
	    	}
	    }
		break;
	default:
		php_error_docref(NULL, E_ERROR, "Parameter error.");
		break;
	}

    smart_str_0(&where_str);
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), ZSTR_VAL(where_str.s));
    smart_str_free(&where_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::in($key)
 */
PHP_METHOD(gene_db_sqlite, in)
{
	zval *self = getThis(), *fields = NULL, *data = NULL, *value = NULL;
	char *in = NULL, *sql_in = NULL, *seg = NULL, *ptr = NULL, *in_tmp = NULL;
	size_t in_len;// @suppress("Type cannot be resolved")
	zval params;
	smart_str where_str = {0},value_str = {0};
	zend_bool pre = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &in, &in_len, &fields) == FAILURE) {
		return;
	}

	sqlite_init_where(self, &where_str);

	if (in_len) {
		if (ZSTR_LEN(where_str.s) == 0) {
			smart_str_appends(&where_str, " WHERE ");
		}
		spprintf(&in_tmp, 0, "%s", in, in_len);
	}
    if (fields) {
    	data = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		ReplaceStr(in_tmp, "in(?)", "$");
    		seg = php_strtok_r(in_tmp, "$", &ptr);
    		if (seg) {
    			smart_str_appends(&where_str, seg);
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
    				Z_TRY_ADDREF_P(value);
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
    			zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), fields);
    		}
    		smart_str_appends(&value_str, ")");
    		smart_str_0(&value_str);
    		smart_str_appends(&where_str, ZSTR_VAL(value_str.s));
    		smart_str_free(&value_str);
    		if (ptr) {
    			smart_str_appends(&where_str, ptr);
    		}
    		break;
    	case IS_STRING:
    	case IS_LONG:
    	case IS_DOUBLE:
    	case IS_FALSE:
    	case IS_TRUE:
    	case IS_NULL:
    		smart_str_appends(&where_str, in_tmp);
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			add_next_index_zval(data, fields);
    			Z_TRY_ADDREF_P(fields);
    		} else {
            	array_init(&params);
            	add_next_index_zval(&params, fields);
            	Z_TRY_ADDREF_P(fields);
            	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &params);
            	zval_ptr_dtor(&params);
    		}
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter error.");
    		break;
    	}
    }

    if (in_len) {
    	efree(in_tmp);
    }
    smart_str_0(&where_str);
    zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), ZSTR_VAL(where_str.s));
    smart_str_free(&where_str);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::sql($key)
 */
PHP_METHOD(gene_db_sqlite, sql)
{
	zval *self = getThis(), *fields = NULL, *data = NULL, *value = NULL;
	char *sql = NULL, *pdo_sql = NULL;
	size_t sql_len;// @suppress("Type cannot be resolved")
	zval params;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &sql, &sql_len, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
    if (sql_len) {
        spprintf(&pdo_sql, 0, "%s", sql);
        zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), pdo_sql);
        efree(pdo_sql);
    }
    if (fields) {
    	data = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		if (Z_TYPE_P(data) == IS_ARRAY) {
    			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(fields), value) {
    				add_next_index_zval(data, value);
    				Z_TRY_ADDREF_P(value);
    			} ZEND_HASH_FOREACH_END();
    		} else {
    			gene_memory_zval_local(&params, fields);
    			zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &params);
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
    			Z_TRY_ADDREF_P(fields);
    		} else {
            	array_init(&params);
            	add_next_index_zval(&params, fields);
            	Z_TRY_ADDREF_P(fields);
            	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &params);
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
PHP_METHOD(gene_db_sqlite, execute)
{
	zval *self = getThis();
	zval statement;
	gene_sqlite_pdo_execute(self, &statement);
	RETURN_ZVAL(&statement, 1, 1);
}
/* }}} */

/*
 * {{{ public gene_db::group()
 */
PHP_METHOD(gene_db_sqlite, group)
{
	zval *self = getThis();
	char *group = NULL;
	size_t group_len = 0;// @suppress("Type cannot be resolved")
	char *group_tmp;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &group, &group_len) == FAILURE) {
		return;
	}
	if (group_len) {
		spprintf(&group_tmp, 0, " GROUP BY %s", group);
		zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_GROUP), group_tmp);
		efree(group_tmp);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::having()
 */
PHP_METHOD(gene_db_sqlite, having)
{
	zval *self = getThis();
	char *having = NULL;
	size_t having_len = 0;// @suppress("Type cannot be resolved")
	char *having_tmp;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &having, &having_len) == FAILURE) {
		return;
	}
	if (having_len) {
		spprintf(&having_tmp, 0, " HAVING %s", having);
		zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_HAVING), having_tmp);
		efree(having_tmp);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::order()
 */
PHP_METHOD(gene_db_sqlite, order)
{
	zval *self = getThis();
	char *order = NULL;
	size_t order_len = 0;// @suppress("Type cannot be resolved")
	char *order_tmp;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &order, &order_len) == FAILURE) {
		return;
	}
	if (order_len) {
		spprintf(&order_tmp, 0, " ORDER BY %s", order);
		zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_ORDER), order_tmp);
		efree(order_tmp);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::limit()
 */
PHP_METHOD(gene_db_sqlite, limit)
{
	zval *self = getThis();
	zend_long *num, *offset = NULL;
	char *limit;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l", &num, &offset) == FAILURE) {
		return;
	}
	if (offset) {
		spprintf(&limit, 0, " limit %d offset %d", offset, num);
	} else {
		spprintf(&limit, 0, " limit %d", num);
	}
	zend_update_property_string(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LIMIT), limit);
	efree(limit);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::all()
 */
PHP_METHOD(gene_db_sqlite, all)
{
	zval *self = getThis();
	zval statement;
	if (gene_sqlite_pdo_execute(self, &statement)) {
		gene_pdo_statement_fetch_all(&statement, return_value);
		zval_ptr_dtor(&statement);
		return;
	}
	RETURN_NULL();
}
/* }}} */


/*
 * {{{ public gene_db::row()
 */
PHP_METHOD(gene_db_sqlite, row)
{
	zval *self = getThis();
	zval statement;
	if (gene_sqlite_pdo_execute(self, &statement)) {
		gene_pdo_statement_fetch(&statement, return_value);
		zval_ptr_dtor(&statement);
		return;
	}
	RETURN_NULL();
}
/* }}} */


/*
 * {{{ public gene_db::cell()
 */
PHP_METHOD(gene_db_sqlite, cell)
{
	zval *self = getThis();
	zval statement;
	if (gene_sqlite_pdo_execute(self, &statement)) {
		gene_pdo_statement_fetch_column(&statement, return_value);
		zval_ptr_dtor(&statement);
		return;
	}
	RETURN_NULL();
}
/* }}} */


/*
 * {{{ public gene_db::lastId()
 */
PHP_METHOD(gene_db_sqlite, lastId)
{
	zval *self = getThis(), *pdo_object = NULL;
	zval statement;
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	if (gene_sqlite_pdo_execute(self, &statement)) {
		gene_pdo_last_insert_id(pdo_object, NULL, return_value);
		zval_ptr_dtor(&statement);
		return;
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_db::affectedRows($key)
 */
PHP_METHOD(gene_db_sqlite, affectedRows)
{
	zval *self = getThis();
	zval statement;
	if (gene_sqlite_pdo_execute(self, &statement)) {
		gene_pdo_statement_row_count(&statement, return_value);
		zval_ptr_dtor(&statement);
		return;
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_db::print($key)
 */
PHP_METHOD(gene_db_sqlite, print)
{
	zval *self = getThis(),*pdo_object = NULL, *pdo_sql = NULL, *pdo_where = NULL, *pdo_order = NULL,*pdo_group = NULL,*pdo_having = NULL, *pdo_limit = NULL, *params = NULL;
	smart_str sql = {0};
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	pdo_sql = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), 1, NULL);
	pdo_where = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), 1, NULL);
	pdo_group = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_GROUP), 1, NULL);
	pdo_having = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_HAVING), 1, NULL);
	pdo_order = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_ORDER), 1, NULL);
	pdo_limit = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LIMIT), 1, NULL);
	params = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);

	if (Z_TYPE_P(pdo_sql) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_sql));
	}
	if (Z_TYPE_P(pdo_where) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_where));
	}
	if (Z_TYPE_P(pdo_group) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_group));
	}
	if (Z_TYPE_P(pdo_having) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_having));
	}
	if (Z_TYPE_P(pdo_order) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_order));
	}
	if (Z_TYPE_P(pdo_limit) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_limit));
	}
	smart_str_0(&sql);
	zval z_row, z_sql;
	ZVAL_STRING(&z_sql, ZSTR_VAL(sql.s));
	smart_str_free(&sql);

	array_init(&z_row);
	add_assoc_zval_ex(&z_row, ZEND_STRL("sql"), &z_sql);
	add_assoc_zval_ex(&z_row, ZEND_STRL("param"), params);
	RETURN_ZVAL(&z_row, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db::beginTransaction()
 */
PHP_METHOD(gene_db_sqlite, beginTransaction)
{
	zval *self = getThis(), *pdo_object = NULL;
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	gene_pdo_begin_transaction(pdo_object, return_value);
}
/* }}} */

/*
 * {{{ public gene_db::inTransaction()
 */
PHP_METHOD(gene_db_sqlite, inTransaction)
{
	zval *self = getThis(), *pdo_object = NULL;
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	gene_pdo_in_transaction(pdo_object, return_value);
}
/* }}} */

/*
 * {{{ public gene_db::rollBack()
 */
PHP_METHOD(gene_db_sqlite, rollBack)
{
	zval *self = getThis(), *pdo_object = NULL;
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	gene_pdo_rollback(pdo_object, return_value);
}
/* }}} */


/*
 * {{{ public gene_db::commit()
 */
PHP_METHOD(gene_db_sqlite, commit)
{
	zval *self = getThis(), *pdo_object = NULL;
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	gene_pdo_commit(pdo_object, return_value);
}
/* }}} */

/*
 * {{{ public gene_db::free()
 */
PHP_METHOD(gene_db_sqlite, free)
{
	zval *self = getThis();
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO));
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_db::history()
 */
PHP_METHOD(gene_db_sqlite, history)
{
	zval *history = NULL;
	history = zend_read_static_property(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_HISTORY), 1);
	RETURN_ZVAL(history, 1, 0);
}
/* }}} */

/*
 * {{{ gene_db_sqlite_methods
 */
zend_function_entry gene_db_sqlite_methods[] = {
		PHP_ME(gene_db_sqlite, __construct, gene_db_sqlite_construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(gene_db_sqlite, getPdo, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, select, gene_db_sqlite_select, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, count, gene_db_sqlite_count, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, insert, gene_db_sqlite_insert, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, batchInsert, gene_db_sqlite_batch_insert, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, update, gene_db_sqlite_update, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, delete, gene_db_sqlite_delete, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, where, gene_db_sqlite_where, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, in, gene_db_sqlite_in, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, sql, gene_db_sqlite_sql, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, limit, gene_db_sqlite_limit, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, order, gene_db_sqlite_order, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, group, gene_db_sqlite_group, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, having, gene_db_sqlite_having, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, execute, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, all, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, row, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, cell, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, lastId, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, affectedRows, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, print, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, beginTransaction, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, inTransaction, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, rollBack, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, commit, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, free, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, history, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(db_sqlite)
{
    zend_class_entry gene_db_sqlite;
	GENE_INIT_CLASS_ENTRY(gene_db_sqlite, "Gene_Db_Sqlite", "Gene\\Db\\Sqlite", gene_db_sqlite_methods);
	gene_db_sqlite_ce = zend_register_internal_class_ex(&gene_db_sqlite, NULL);
	gene_db_sqlite_ce->ce_flags |= ZEND_ACC_FINAL;

	//pdo
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_CONFIG), ZEND_ACC_PUBLIC);
	zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_PDO), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_SQL), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_WHERE), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_GROUP), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_HAVING), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_ORDER), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_LIMIT), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_DATA), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_HISTORY), ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);

	return SUCCESS;// @suppress("Symbol is not resolved")
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
