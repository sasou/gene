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
#include "../cache/memory.h"
#include "../db/pdo.h"
#include "../db/sqlite.h"
#include "../db/pool.h"
#include "../factory/factory.h"
#include "../tool/benchmark.h"

zend_class_entry * gene_db_sqlite_ce;

/* [GENE_PERF:2026-04-26] See mysql.c for rationale: strpprintf returns a
 * zend_string* directly; zend_update_property_str only addrefs. Saves the
 * extra strdup that the spprintf+update_property_string pattern incurred. */
#define GENE_DB_SQLITE_SET_PROP(KEY, ...) do { \
    zend_string *_gene_db_s_ = strpprintf(0, __VA_ARGS__); \
    zend_update_property_str(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(KEY), _gene_db_s_); \
    zend_string_release(_gene_db_s_); \
} while (0)

/* Benchmark variables removed from file scope to avoid coroutine data races.
 * They are now local to gene_sqlite_pdo_execute and passed to sqliteSaveHistory. */

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

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_insert, 0, 0, 1)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_upsert, 0, 0, 3)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
    ZEND_ARG_ARRAY_INFO(0, updateCols, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_batch_insert, 0, 0, 1)
	ZEND_ARG_INFO(0, table)
    ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_update, 0, 0, 1)
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
	ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_join, 0, 0, 2)
	ZEND_ARG_INFO(0, table)
	ZEND_ARG_INFO(0, on)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_side_join, 0, 0, 2)
	ZEND_ARG_INFO(0, table)
	ZEND_ARG_INFO(0, on)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_union, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
	ZEND_ARG_INFO(0, all)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_quote, 0, 0, 1)
	ZEND_ARG_INFO(0, str)
	ZEND_ARG_INFO(0, paramType)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_transaction, 0, 0, 1)
	ZEND_ARG_CALLABLE_INFO(0, fn, 0)
ZEND_END_ARG_INFO()

/* [GENE_FEATURE:2026-08-07] attach($path, $schema): attach another SQLite
 * database file to the current connection under $schema name. detach($schema)
 * reverses it. Both validate $schema as an identifier to prevent injection. */
ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_attach, 0, 0, 2)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, schema)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_db_sqlite_detach, 0, 0, 1)
	ZEND_ARG_INFO(0, schema)
ZEND_END_ARG_INFO()

void sqlite_reset_sql_params(zval *self)
{
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_JOIN));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_GROUP));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_HAVING));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_UNION));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_ORDER));
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LIMIT));
	/* [GENE_FEATURE:2026-08-18 3.4] M8: clear the LOCK fragment with every
	 * other SQL part (unused on sqlite — no-op lock methods — but kept for
	 * cross-driver symmetry). */
	zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LOCK));
    zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA));
}

void sqliteSaveHistory(smart_str *sql, zval *param, struct timeval *start, struct timeval *end, zend_long *mem_start, zend_long *mem_end) {
	zval *history = &GENE_REQ(db_sqlite_history);
	zval params, z_row, z_sql, z_data, z_time, z_memory;
	char *char_t,*char_m;

	ZVAL_STRING(&z_sql, ZSTR_VAL(sql->s));

	jsonEncode(&z_data, param);

	getBenchTime(start, end, &char_t, 1);
	ZVAL_STRING(&z_time, char_t);
	efree(char_t);

    getBenchMemory(mem_start, mem_end, &char_m, 1);
	ZVAL_STRING(&z_memory, char_m);
	efree(char_m);

	array_init(&z_row);
	add_assoc_zval_ex(&z_row, ZEND_STRL("sql"), &z_sql);
	add_assoc_zval_ex(&z_row, ZEND_STRL("param"), &z_data);
	add_assoc_zval_ex(&z_row, ZEND_STRL("time"), &z_time);
	add_assoc_zval_ex(&z_row, ZEND_STRL("memory"), &z_memory);

	if (history && Z_TYPE_P(history) == IS_ARRAY) {
		if (zend_hash_num_elements(Z_ARRVAL_P(history)) >= GENE_DB_HISTORY_MAX) {
			zend_ulong _hk; zend_string *_sk;
			ZEND_HASH_FOREACH_KEY(Z_ARRVAL_P(history), _hk, _sk) {
				zend_hash_index_del(Z_ARRVAL_P(history), _hk);
				break;
			} ZEND_HASH_FOREACH_END();
		}
		add_next_index_zval(history, &z_row);
	} else {
    	array_init(&params);
    	add_next_index_zval(&params, &z_row);
    	ZVAL_COPY_VALUE(history, &params);
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

bool sqliteInitPdo (zval * self, zval *config) {
	zval  *dsn = NULL, *user = NULL, *pass = NULL, *options = NULL;
	zval pdo_object, option;

	if (config == NULL) {
		config =  zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_CONFIG), 1, NULL);
	}

	/* Pool mode: in Swoole coroutine mode, if config has 'pool' key,
	 * borrow a PDO connection from the named pool instead of creating one. */
	if (gene_pool_get_pdo(gene_db_sqlite_ce, self, config, ZEND_STRL(GENE_DB_SQLITE_POOL), ZEND_STRL(GENE_DB_SQLITE_PDO))) {
		return 0;
	}

	/* [GENE_FIX:2026-05-24] gene_lookup_class_str avoids the unsafe
	 * static zend_string* + zend_string_init_interned(...,1) pattern that
	 * dangles across requests under opcache.file_cache_only=1. PDO is an
	 * internal class registered at MINIT, so the EG(class_table) fast
	 * path inside gene_lookup_class_str is a single hash hit. */
	zend_class_entry *pdo_ptr = gene_lookup_class_str(ZEND_STRL("PDO"));

	if (!pdo_ptr) {
		php_error_docref(NULL, E_ERROR, "PDO extension is not loaded.");
		return -1;
	}
	object_init_ex(&pdo_object, pdo_ptr);

	/* [GENE_AUDIT:2026-07-13 M2] E_ERROR normally bailouts, but if intercepted
	 * (zend_try / custom error handler) execution would continue and deref NULL.
	 * Return explicitly. */
	if ((dsn = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("dsn"))) == NULL) {
		 php_error_docref(NULL, E_ERROR, "PDO need a valid dsn.");
		 zval_ptr_dtor(&pdo_object);
		 return -1;
	}
	user = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("username"));
	pass = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("password"));
	options = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("options"));
    if (options == NULL || Z_TYPE_P(options) == IS_NULL) {
		array_init(&option);
    } else {
		ZVAL_DUP(&option, options);
    }
	add_index_long(&option, 3, 2);
	add_index_long(&option, 19, 2);
	add_index_long(&option, 11, 2);
	add_index_long(&option, 8, 2);
	/* In Swoole/coroutine mode, do NOT use PDO::ATTR_PERSISTENT.
	 * NOTE: PDO::ATTR_PERSISTENT index is 12. */
	if (GENE_G(runtime_type) >= 2) {
		add_index_bool(&option, 12, 0);
	}
	gene_pdo_construct(&pdo_object, dsn, user, pass, &option);
	zval_ptr_dtor(&option);

	if (EG(exception)) {
		if (checkPdoError(EG(exception))) {
			zend_clear_exception();
		}
	}
    zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), &pdo_object);
    zval_ptr_dtor(&pdo_object);
	return 0;
}

bool gene_sqlite_pdo_execute (zval *self, zval *statement)
{
	zval *pdo_object = NULL, *params = NULL, *pdo_sql = NULL, *pdo_join = NULL, *pdo_where = NULL, *pdo_group = NULL,*pdo_having = NULL, *pdo_union = NULL,*pdo_order = NULL, *pdo_limit = NULL, *pdo_lock = NULL;
	zval retval;
	smart_str sql = {0};
	struct timeval db_start, db_end;
	zend_long db_sqlite_memory_start = 0, db_sqlite_memory_end = 0;

	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	pdo_sql = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), 1, NULL);
	pdo_join = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_JOIN), 1, NULL);
	pdo_where = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), 1, NULL);
	pdo_group = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_GROUP), 1, NULL);
	pdo_having = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_HAVING), 1, NULL);
	pdo_union = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_UNION), 1, NULL);
	pdo_order = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_ORDER), 1, NULL);
	pdo_limit = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LIMIT), 1, NULL);
	pdo_lock = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LOCK), 1, NULL);

	/* [GENE_FEATURE:2026-08-06 F0-2] Assembly order: base SQL + JOIN + WHERE
	 * + GROUP + HAVING + UNION + ORDER + LIMIT. JOIN binds to the leading
	 * SELECT's FROM clause; UNION sits before ORDER/LIMIT so ordering and
	 * pagination apply to the union result. */
	if (Z_TYPE_P(pdo_sql) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_sql));
	}
	if (Z_TYPE_P(pdo_join) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_join));
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
	if (Z_TYPE_P(pdo_union) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_union));
	}
	if (Z_TYPE_P(pdo_order) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_order));
	}
	if (Z_TYPE_P(pdo_limit) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_limit));
	}
	/* [GENE_FEATURE:2026-08-18 3.4] LOCK fragment goes last (cross-driver
	 * symmetry; sqlite lock methods are no-ops so this stays empty). */
	if (Z_TYPE_P(pdo_lock) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_lock));
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
    			zend_clear_exception();
    			/* If using pool, notify that the broken connection is lost */
    			gene_pool_notify_remove(gene_db_sqlite_ce, self, ZEND_STRL(GENE_DB_SQLITE_POOL));
    			sqliteInitPdo (self, NULL);
    			pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
    			/* Free old PDOStatement before re-prepare to prevent leak */
    			zval_ptr_dtor(statement);
    			gene_pdo_prepare(pdo_object, ZSTR_VAL(sql.s), statement);
    			zval_ptr_dtor(&retval);
    			gene_pdo_statement_execute(statement, params, &retval);
    		}
    	}
		if (!GENE_G(run_environment)) {
			markEnd(&db_end, &db_sqlite_memory_end);
			sqliteSaveHistory(&sql, params, &db_start, &db_end, &db_sqlite_memory_start, &db_sqlite_memory_end);
		}
		smart_str_free(&sql);
		{
			zend_bool success = Z_TYPE(retval) == IS_TRUE ? 1 : 0;
			zval_ptr_dtor(&retval);
			return success;
		}
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

    {
    	gene_request_context *ctx = gene_request_ctx();
    	if (Z_TYPE(ctx->db_sqlite_history) != IS_UNDEF) {
    		zval_ptr_dtor(&ctx->db_sqlite_history);
    	}
    	ZVAL_UNDEF(&ctx->db_sqlite_history);
    }

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
	char *table = NULL, *select = NULL;
	size_t table_len; // @suppress("Type cannot be resolved")
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params (self);
    if (fields) {
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		mssql_array_to_string(fields, &select, '`', '`');
    		{
    			char *qt = gene_quote_table(table, '`', '`');
    			GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "SELECT %s FROM %s", select, qt);
    			efree(qt);
    		}
            efree(select);
    		break;
    	case IS_STRING:
    		{
    			char *qt = gene_quote_table(table, '`', '`');
    			char *qf = gene_quote_columns(Z_STRVAL_P(fields), '`', '`');
    			GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "SELECT %s FROM %s", qf, qt);
    			efree(qf);
    			efree(qt);
    		}
    		break;
    	default:
    		php_error_docref(NULL, E_ERROR, "Parameter can only be array or string.");
    		break;
    	}

    } else {
    	char *qt = gene_quote_table(table, '`', '`');
    	GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "SELECT * FROM %s", qt);
    	efree(qt);
    }
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_model::count($key)
 */
PHP_METHOD(gene_db_sqlite, count)
{
	zval *self = getThis();
	zend_string *table = NULL,*fields = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S", &table, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
    {
    	char *qt = gene_quote_table(ZSTR_VAL(table), '`', '`');
    	if (fields) {
    		char *qf = gene_quote_columns(ZSTR_VAL(fields), '`', '`');
    		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "SELECT count(%s) AS count FROM %s", qf, qt);
    		efree(qf);
    	} else {
    		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "SELECT count(1) AS count FROM %s", qt);
    	}
    	efree(qt);
    }
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/* Shared INSERT builder. ignore=1 → INSERT OR IGNORE (sqlite idempotent
 * write, equivalent to MySQL INSERT IGNORE). */
static void gene_db_sqlite_do_insert(zval *self, char *table, zval *fields, zend_bool ignore)
{
	smart_str field_str = {0} , value_str = {0};
	zval field_value;
	sqlite_reset_sql_params(self);
	ZVAL_NULL(&field_value);
	smart_str_appends(&field_str, "");
	smart_str_appends(&value_str, "");
    if (fields && Z_TYPE_P(fields) == IS_ARRAY) {
    	gene_insert_field_value (fields, &field_str, &value_str, &field_value, '`', '`');
    	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
	smart_str_0(&field_str);
	smart_str_0(&value_str);
    {
    	char *qt = gene_quote_table(table, '`', '`');
    	if (ignore) {
    		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "INSERT OR IGNORE INTO %s(%s) VALUES(%s)", qt, field_str.s->val, value_str.s->val);
    	} else {
    		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "INSERT INTO %s(%s) VALUES(%s)", qt, field_str.s->val, value_str.s->val);
    	}
    	efree(qt);
    }
    smart_str_free(&field_str);
    smart_str_free(&value_str);
}

/*
 * {{{ public gene_db::insert($key)
 */
PHP_METHOD(gene_db_sqlite, insert)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL;
	size_t table_len;// @suppress("Type cannot be resolved")
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	gene_db_sqlite_do_insert(self, table, fields, 0);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db_sqlite::insertIgnore($table, $fields)
 * [GENE_FEATURE:2026-08-18 3.3] INSERT OR IGNORE — sqlite equivalent of
 * MySQL INSERT IGNORE. Lazy like insert(): execute via lastId()/affectedRows(). */
PHP_METHOD(gene_db_sqlite, insertIgnore)
{
	zval *self = getThis(),*fields = NULL;
	char *table = NULL;
	size_t table_len;// @suppress("Type cannot be resolved")
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &table, &table_len, &fields) == FAILURE) {
		return;
	}
	gene_db_sqlite_do_insert(self, table, fields, 1);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db_sqlite::upsert($table, $fields, $updateCols)
 * [GENE_FEATURE:2026-08-18 3.3] Not folded into the builder: sqlite's
 * ON CONFLICT needs an explicit conflict target, which this API does not
 * model. Use sql() with ON CONFLICT(col) DO UPDATE directly. */
PHP_METHOD(gene_db_sqlite, upsert)
{
	zend_throw_exception_ex(NULL, 0,
		"Gene\\Db\\Sqlite::upsert() is not supported; use sql() with ON CONFLICT(col) DO UPDATE");
}
/* }}} */

/*
 * {{{ public gene_db::batchInsert($key)
 */
PHP_METHOD(gene_db_sqlite, batchInsert)
{
	zval *self = getThis(),*fields = NULL, *row = NULL;
	char *table = NULL;
	size_t table_len;// @suppress("Type cannot be resolved")
	smart_str field_str = {0} , value_str = {0};
	zval field_value;
	bool pre = 0;
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
        		gene_insert_field_value_batch (row, &field_str, &value_str, &field_value, '`', '`');
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
    {
    	char *qt = gene_quote_table(table, '`', '`');
    	GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "INSERT INTO %s(%s) VALUES %s", qt, field_str.s->val, value_str.s->val);
    	efree(qt);
    }
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
	char *table = NULL;
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
    	gene_update_field_value (fields, &field_str, &field_value, '`', '`');
    	zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &field_value);
    	zval_ptr_dtor(&field_value);
    } else {
    	php_error_docref(NULL, E_ERROR, "Data Parameter can only be array.");
    }
	smart_str_0(&field_str);
    {
    	char *qt = gene_quote_table(table, '`', '`');
    	GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "UPDATE %s SET %s", qt, field_str.s->val);
    	efree(qt);
    }
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
	char *table = NULL;
	size_t table_len; // @suppress("Type cannot be resolved")
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &table, &table_len) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
    {
    	char *qt = gene_quote_table(table, '`', '`');
    	GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_SQL, "DELETE FROM %s", qt);
    	efree(qt);
    }
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */


/*
 * {{{ public gene_db::where($key)
 */
PHP_METHOD(gene_db_sqlite, where)
{
	zval *self = getThis(), *where = NULL, *fields = NULL, *data = NULL, *value = NULL;
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
    zend_update_property_str(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), where_str.s);
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
	char *in = NULL, *seg = NULL, *ptr = NULL, *in_tmp = NULL;
	size_t in_len;// @suppress("Type cannot be resolved")
	zval params;
	smart_str where_str = {0},value_str = {0};
	bool pre = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &in, &in_len, &fields) == FAILURE) {
		return;
	}

	sqlite_init_where(self, &where_str);

	if (in_len) {
		if (ZSTR_LEN(where_str.s) == 0) {
			smart_str_appends(&where_str, " WHERE ");
		}
		in_tmp = estrndup(in, in_len);
	}
    if (fields) {
    	data = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
    	switch(Z_TYPE_P(fields)) {
    	case IS_ARRAY:
    		ReplaceStr(in_tmp, in_len + 1, "in(?)", "$");
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
    zend_update_property_str(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), where_str.s);
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
	char *sql = NULL;
	size_t sql_len;// @suppress("Type cannot be resolved")
	zval params;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|z", &sql, &sql_len, &fields) == FAILURE) {
		return;
	}
	sqlite_reset_sql_params(self);
    if (sql_len) {
        zend_update_property_stringl(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), sql, sql_len);
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

/* [GENE_FEATURE:2026-08-06 F0-2] JOIN type whitelist — anything outside this
 * set is rejected instead of interpolated into the SQL. */
static zend_bool gene_db_sqlite_join_type(const char *type, size_t type_len, char *out, size_t out_size) {
	static const char *allowed[] = {
		"INNER", "LEFT", "RIGHT", "CROSS", "FULL",
		"LEFT OUTER", "RIGHT OUTER", "FULL OUTER"
	};
	size_t i, j;
	if (type_len == 0 || type_len >= out_size) {
		return 0;
	}
	for (i = 0; i < type_len; i++) {
		unsigned char c = (unsigned char)type[i];
		out[i] = (c >= 'a' && c <= 'z') ? (char)(c & ~0x20) : (char)c;
	}
	out[type_len] = '\0';
	for (j = 0; j < sizeof(allowed) / sizeof(allowed[0]); j++) {
		if (strcmp(out, allowed[j]) == 0) {
			return 1;
		}
	}
	return 0;
}

/* [GENE_FEATURE:2026-08-06 F0-2] Build the ON clause from a structured
 * assoc array (leftColumn => rightColumn); both sides go through the
 * identifier quoting path. Raw strings are deliberately NOT accepted — a
 * free-form ON string would open a new injection surface around the
 * "?" placeholder binding used for data values. */
static zend_bool gene_db_sqlite_build_on(zval *on, smart_str *out) {
	zend_string *k;
	zval *v;
	int first = 1;
	if (!on || Z_TYPE_P(on) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(on)) == 0) {
		return 0;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(on), k, v) {
		char *lk, *rv;
		if (!k || Z_TYPE_P(v) != IS_STRING || Z_STRLEN_P(v) == 0) {
			return 0;
		}
		lk = gene_quote_columns(ZSTR_VAL(k), '`', '`');
		rv = gene_quote_columns(Z_STRVAL_P(v), '`', '`');
		if (!first) {
			smart_str_appends(out, " AND ");
		}
		smart_str_appends(out, lk);
		smart_str_appends(out, " = ");
		smart_str_appends(out, rv);
		efree(lk);
		efree(rv);
		first = 0;
	} ZEND_HASH_FOREACH_END();
	return !first;
}

/* Append a fragment to an accumulating SQL-part property. */
static void gene_db_sqlite_append_prop(zval *self, const char *prop, size_t prop_len, smart_str *frag) {
	zval *cur = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), prop, prop_len, 1, NULL);
	smart_str out = {0};
	if (cur && Z_TYPE_P(cur) == IS_STRING && Z_STRLEN_P(cur)) {
		smart_str_appends(&out, Z_STRVAL_P(cur));
	}
	if (frag->s) {
		smart_str_appendl(&out, ZSTR_VAL(frag->s), ZSTR_LEN(frag->s));
	}
	smart_str_0(&out);
	zend_update_property_str(gene_db_sqlite_ce, gene_strip_obj(self), prop, prop_len, out.s);
	zend_string_release(out.s);
}

static void gene_db_sqlite_do_join(zval *self, zend_string *table, zval *on, const char *type) {
	smart_str frag = {0}, on_str = {0};
	char tbuf[16];
	const char *ttype = "INNER";
	size_t tlen = 5;
	if (type && type[0]) {
		if (!gene_db_sqlite_join_type(type, strlen(type), tbuf, sizeof(tbuf))) {
			php_error_docref(NULL, E_WARNING, "Invalid JOIN type: %s", type);
			smart_str_free(&on_str);
			smart_str_free(&frag);
			return;
		}
		ttype = tbuf;
		tlen = strlen(tbuf);
	}
	if (!gene_db_sqlite_build_on(on, &on_str)) {
		php_error_docref(NULL, E_WARNING,
			"JOIN ON must be a non-empty assoc array of leftColumn => rightColumn");
		smart_str_free(&on_str);
		smart_str_free(&frag);
		return;
	}
	{
		char *qt = gene_quote_table(ZSTR_VAL(table), '`', '`');
		smart_str_appendc(&frag, ' ');
		smart_str_appendl(&frag, ttype, tlen);
		smart_str_appends(&frag, " JOIN ");
		smart_str_appends(&frag, qt);
		smart_str_appends(&frag, " ON ");
		smart_str_appendl(&frag, ZSTR_VAL(on_str.s), ZSTR_LEN(on_str.s));
		smart_str_0(&frag);
		gene_db_sqlite_append_prop(self, ZEND_STRL(GENE_DB_SQLITE_JOIN), &frag);
		efree(qt);
	}
	smart_str_free(&on_str);
	smart_str_free(&frag);
}

/*
 * {{{ public gene_db_sqlite::reset()
 * [GENE_FEATURE:2026-08-06] Public entry to sqlite_reset_sql_params() so one
 * instance can be reused to build multiple statements. */
PHP_METHOD(gene_db_sqlite, reset)
{
	zval *self = getThis();
	sqlite_reset_sql_params(self);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db_sqlite::join(string $table, array $on, string $type = 'INNER')
 */
PHP_METHOD(gene_db_sqlite, join)
{
	zval *self = getThis(), *on = NULL;
	zend_string *table = NULL, *type = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz|S", &table, &on, &type) == FAILURE) {
		return;
	}
	gene_db_sqlite_do_join(self, table, on, (type && ZSTR_LEN(type)) ? ZSTR_VAL(type) : NULL);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db_sqlite::leftJoin(string $table, array $on)
 */
PHP_METHOD(gene_db_sqlite, leftJoin)
{
	zval *self = getThis(), *on = NULL;
	zend_string *table = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &table, &on) == FAILURE) {
		return;
	}
	gene_db_sqlite_do_join(self, table, on, "LEFT");
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db_sqlite::rightJoin(string $table, array $on)
 */
PHP_METHOD(gene_db_sqlite, rightJoin)
{
	zval *self = getThis(), *on = NULL;
	zend_string *table = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &table, &on) == FAILURE) {
		return;
	}
	gene_db_sqlite_do_join(self, table, on, "RIGHT");
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public gene_db_sqlite::union(string|object $query, bool $all = false)
 * [GENE_FEATURE:2026-08-06 F0-2] A string is treated as developer-written
 * SQL (same trust level as sql()); a Gene\Db\Sqlite object is assembled from
 * its parts, wrapped in parentheses, and its bound params are merged into
 * this query's data array so "?" placeholders stay aligned. */
PHP_METHOD(gene_db_sqlite, union)
{
	zval *self = getThis(), *query = NULL;
	zend_bool all = 0;
	smart_str frag = {0};
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|b", &query, &all) == FAILURE) {
		return;
	}
	smart_str_appends(&frag, all ? " UNION ALL " : " UNION ");
	switch (Z_TYPE_P(query)) {
	case IS_STRING:
		if (Z_STRLEN_P(query)) {
			smart_str_appendl(&frag, Z_STRVAL_P(query), Z_STRLEN_P(query));
		}
		break;
	case IS_OBJECT:
		if (!instanceof_function(Z_OBJCE_P(query), gene_db_sqlite_ce)) {
			php_error_docref(NULL, E_WARNING, "union() expects a SQL string or a Gene\\Db\\Sqlite builder");
			smart_str_free(&frag);
			return;
		}
		{
			zval *sub_sql = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_SQL), 1, NULL);
			zval *sub_join = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_JOIN), 1, NULL);
			zval *sub_where = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_WHERE), 1, NULL);
			zval *sub_group = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_GROUP), 1, NULL);
			zval *sub_having = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_HAVING), 1, NULL);
			zval *sub_order = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_ORDER), 1, NULL);
			zval *sub_limit = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_LIMIT), 1, NULL);
			zval *sub_data = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(query), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
			smart_str_appendc(&frag, '(');
			if (sub_sql && Z_TYPE_P(sub_sql) == IS_STRING) smart_str_appends(&frag, Z_STRVAL_P(sub_sql));
			if (sub_join && Z_TYPE_P(sub_join) == IS_STRING) smart_str_appends(&frag, Z_STRVAL_P(sub_join));
			if (sub_where && Z_TYPE_P(sub_where) == IS_STRING) smart_str_appends(&frag, Z_STRVAL_P(sub_where));
			if (sub_group && Z_TYPE_P(sub_group) == IS_STRING) smart_str_appends(&frag, Z_STRVAL_P(sub_group));
			if (sub_having && Z_TYPE_P(sub_having) == IS_STRING) smart_str_appends(&frag, Z_STRVAL_P(sub_having));
			if (sub_order && Z_TYPE_P(sub_order) == IS_STRING) smart_str_appends(&frag, Z_STRVAL_P(sub_order));
			if (sub_limit && Z_TYPE_P(sub_limit) == IS_STRING) smart_str_appends(&frag, Z_STRVAL_P(sub_limit));
			smart_str_appendc(&frag, ')');
			if (sub_data && Z_TYPE_P(sub_data) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(sub_data)) > 0) {
				zval *data = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);
				zval *value;
				if (Z_TYPE_P(data) == IS_ARRAY) {
					SEPARATE_ARRAY(data);
					ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(sub_data), value) {
						Z_TRY_ADDREF_P(value);
						add_next_index_zval(data, value);
					} ZEND_HASH_FOREACH_END();
				} else {
					zval params;
					gene_memory_zval_local(&params, sub_data);
					zend_update_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), &params);
					zval_ptr_dtor(&params);
				}
			}
		}
		break;
	default:
		php_error_docref(NULL, E_WARNING, "union() expects a SQL string or a Gene\\Db\\Sqlite builder");
		smart_str_free(&frag);
		return;
	}
	smart_str_0(&frag);
	gene_db_sqlite_append_prop(self, ZEND_STRL(GENE_DB_SQLITE_UNION), &frag);
	smart_str_free(&frag);
	RETURN_ZVAL(self, 1, 0);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &group, &group_len) == FAILURE) {
		return;
	}
	if (group_len) {
		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_GROUP, " GROUP BY %s", group);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &having, &having_len) == FAILURE) {
		return;
	}
	if (having_len) {
		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_HAVING, " HAVING %s", having);
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
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &order, &order_len) == FAILURE) {
		return;
	}
	if (order_len) {
		char *qo = gene_quote_order(order, '`', '`');
		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_ORDER, " ORDER BY %s", qo);
		efree(qo);
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
	zend_long num, offset = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l", &num, &offset) == FAILURE) {
		return;
	}
	if (ZEND_NUM_ARGS() > 1) {
		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_LIMIT, " limit " ZEND_LONG_FMT " offset " ZEND_LONG_FMT, num, offset);
	} else {
		GENE_DB_SQLITE_SET_PROP(GENE_DB_SQLITE_LIMIT, " limit " ZEND_LONG_FMT, num);
	}
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/* [GENE_FEATURE:2026-08-18 3.4] SQLite has no row-lock syntax (write locks
 * are database-wide and implicit). Keep the methods as documented no-ops so
 * cross-driver code degrades loudly instead of fataling. */
PHP_METHOD(gene_db_sqlite, lockForUpdate)
{
	zval *self = getThis();
	php_error_docref(NULL, E_NOTICE,
		"Gene\\Db\\Sqlite::lockForUpdate() is a no-op — SQLite has no FOR UPDATE syntax");
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_db_sqlite, sharedLock)
{
	zval *self = getThis();
	php_error_docref(NULL, E_NOTICE,
		"Gene\\Db\\Sqlite::sharedLock() is a no-op — SQLite has no shared-lock syntax");
	RETURN_ZVAL(self, 1, 0);
}


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
 * {{{ public gene_db::quote($str, $paramType)
 * [GENE_FEATURE:2026-08-07 F1-8] PDO::quote pass-through for string literal
 * escaping. $paramType defaults to PDO::PARAM_STR (2).
 */
PHP_METHOD(gene_db_sqlite, quote)
{
	zval *self = getThis(), *pdo_object = NULL;
	zend_string *str = NULL;
	zend_long param_type = 2; /* PDO::PARAM_STR */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|l", &str, &param_type) == FAILURE) {
		return;
	}
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	if (!pdo_object || Z_TYPE_P(pdo_object) != IS_OBJECT) {
		RETURN_FALSE;
	}
	gene_pdo_quote(pdo_object, str, param_type, return_value);
}
/* }}} */

/*
 * {{{ public gene_db::print($key)
 */
PHP_METHOD(gene_db_sqlite, print)
{
	zval *self = getThis(),*pdo_object = NULL, *pdo_sql = NULL, *pdo_join = NULL, *pdo_where = NULL, *pdo_order = NULL,*pdo_group = NULL,*pdo_having = NULL, *pdo_union = NULL, *pdo_limit = NULL, *pdo_lock = NULL, *params = NULL;
	smart_str sql = {0};
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	pdo_sql = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_SQL), 1, NULL);
	pdo_join = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_JOIN), 1, NULL);
	pdo_where = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_WHERE), 1, NULL);
	pdo_group = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_GROUP), 1, NULL);
	pdo_having = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_HAVING), 1, NULL);
	pdo_union = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_UNION), 1, NULL);
	pdo_order = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_ORDER), 1, NULL);
	pdo_limit = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LIMIT), 1, NULL);
	pdo_lock = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_LOCK), 1, NULL);
	params = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_DATA), 1, NULL);

	if (Z_TYPE_P(pdo_sql) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_sql));
	}
	if (Z_TYPE_P(pdo_join) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_join));
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
	if (Z_TYPE_P(pdo_union) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_union));
	}
	if (Z_TYPE_P(pdo_order) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_order));
	}
	if (Z_TYPE_P(pdo_limit) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_limit));
	}
	if (Z_TYPE_P(pdo_lock) == IS_STRING) {
		smart_str_appends(&sql, Z_STRVAL_P(pdo_lock));
	}
	smart_str_0(&sql);
	zval z_row, z_sql;
	/* [GENE_FIX:2026-08-19] sql.s stays NULL when no statement was built
	 * (fresh handle / right after reset) — ZSTR_VAL(NULL) segfaults. */
	ZVAL_STRING(&z_sql, sql.s ? ZSTR_VAL(sql.s) : "");
	smart_str_free(&sql);

	array_init(&z_row);
	add_assoc_zval_ex(&z_row, ZEND_STRL("sql"), &z_sql);
	Z_TRY_ADDREF_P(params);
	add_assoc_zval_ex(&z_row, ZEND_STRL("param"), params);
	RETURN_ZVAL(&z_row, 1, 1);
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
 * {{{ public gene_db::transaction(callable $fn)
 */
PHP_METHOD(gene_db_sqlite, transaction)
{
	zend_fcall_info fci;
	zend_fcall_info_cache fcc = empty_fcall_info_cache;
	zval *self = getThis(), *pdo_object = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "f", &fci, &fcc) == FAILURE) {
		return;
	}
	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	gene_pdo_run_transaction(pdo_object, &fci, &fcc, return_value);
}
/* }}} */

/*
 * {{{ public gene_db::release()
 */
PHP_METHOD(gene_db_sqlite, release)
{
	zval *self = getThis();
	gene_pool_return_pdo(gene_db_sqlite_ce, self, ZEND_STRL(GENE_DB_SQLITE_POOL), ZEND_STRL(GENE_DB_SQLITE_PDO));
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_db::free()
 */
PHP_METHOD(gene_db_sqlite, free)
{
	zval *self = getThis();
	zval *pool = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_POOL), 1, NULL);
	if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
		gene_pool_return_pdo(gene_db_sqlite_ce, self, ZEND_STRL(GENE_DB_SQLITE_POOL), ZEND_STRL(GENE_DB_SQLITE_PDO));
	} else {
		/* [GENE_FIX:2026-08-19 N3] No-pool handle — see Db\Mysql::free(). */
		zval *pdo = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
		gene_db_tx_hygiene(pdo, "Db\\Sqlite handle freed");
		zend_update_property_null(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO));
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public gene_db::__destruct()
 */
PHP_METHOD(gene_db_sqlite, __destruct)
{
	zval *self = getThis();
	zval *pool = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_POOL), 1, NULL);
	if (pool && Z_TYPE_P(pool) == IS_OBJECT) {
		gene_pool_return_pdo(gene_db_sqlite_ce, self, ZEND_STRL(GENE_DB_SQLITE_POOL), ZEND_STRL(GENE_DB_SQLITE_PDO));
	} else {
		zval *pdo = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
		gene_db_tx_hygiene(pdo, "Db\\Sqlite handle destructed");
	}
}
/* }}} */

/* [GENE_FEATURE:2026-08-07] Validate a schema identifier: only [A-Za-z_][A-Za-z0-9_]*
 * is accepted. Returns 1 on success. Rejects empty, overlong (>63), or
 * non-identifier chars to prevent SQL injection via ATTACH/DETACH. */
static zend_bool gene_db_sqlite_valid_schema(const char *s, size_t len) {
	size_t i;
	if (len == 0 || len > 63) return 0;
	if (!((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z') || s[0] == '_')) return 0;
	for (i = 1; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		      (c >= '0' && c <= '9') || c == '_')) return 0;
	}
	return 1;
}

/*
 * {{{ public gene_db::attach(string $path, string $schema)
 * [GENE_FEATURE:2026-08-07] Attach another SQLite database file to the
 * current connection under $schema. $schema is validated as an identifier;
 * single quotes in $path are doubled (SQLite string-literal escaping).
 * Returns true on success, false on failure.
 */
PHP_METHOD(gene_db_sqlite, attach)
{
	zval *self = getThis();
	zend_string *path = NULL, *schema = NULL;
	zval *pdo_object = NULL, retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &path, &schema) == FAILURE) {
		return;
	}
	if (!gene_db_sqlite_valid_schema(ZSTR_VAL(schema), ZSTR_LEN(schema))) {
		php_error_docref(NULL, E_WARNING, "Attach schema name must be a valid identifier [A-Za-z_][A-Za-z0-9_]*");
		RETURN_FALSE;
	}
	if (ZSTR_LEN(path) == 0) {
		php_error_docref(NULL, E_WARNING, "Attach path must not be empty");
		RETURN_FALSE;
	}
	/* [GENE_FIX:2026-08-07] Reject NUL bytes: the SQL is passed to
	 * gene_pdo_exec() as a C string, so an embedded NUL would silently
	 * truncate the path. */
	if (memchr(ZSTR_VAL(path), '\0', ZSTR_LEN(path)) != NULL) {
		php_error_docref(NULL, E_WARNING, "Attach path must not contain NUL bytes");
		RETURN_FALSE;
	}

	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	if (!pdo_object || Z_TYPE_P(pdo_object) != IS_OBJECT) {
		php_error_docref(NULL, E_WARNING, "PDO connection is not initialized");
		RETURN_FALSE;
	}

	/* Escape single quotes in path by doubling them (SQLite literal rule). */
	smart_str sql = {0};
	smart_str_appends(&sql, "ATTACH DATABASE '");
	size_t i;
	for (i = 0; i < ZSTR_LEN(path); i++) {
		char c = ZSTR_VAL(path)[i];
		smart_str_appendc(&sql, c);
		if (c == '\'') smart_str_appendc(&sql, '\'');
	}
	smart_str_appends(&sql, "' AS ");
	smart_str_appends(&sql, ZSTR_VAL(schema));
	smart_str_0(&sql);

	ZVAL_UNDEF(&retval);
	gene_pdo_exec(pdo_object, ZSTR_VAL(sql.s), &retval);
	smart_str_free(&sql);

	/* [GENE_FIX:2026-08-07] PDO::exec() returns false on failure; anything
	 * other than IS_FALSE/IS_UNDEF counts as success. */
	zend_bool ok = (Z_TYPE(retval) != IS_UNDEF && Z_TYPE(retval) != IS_FALSE) ? 1 : 0;
	if (Z_TYPE(retval) != IS_UNDEF) zval_ptr_dtor(&retval);
	RETURN_BOOL(ok);
}
/* }}} */

/*
 * {{{ public gene_db::detach(string $schema)
 * [GENE_FEATURE:2026-08-07] Detach a previously-attached schema. Same
 * identifier validation as attach(). Returns true on success.
 */
PHP_METHOD(gene_db_sqlite, detach)
{
	zval *self = getThis();
	zend_string *schema = NULL;
	zval *pdo_object = NULL, retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &schema) == FAILURE) {
		return;
	}
	if (!gene_db_sqlite_valid_schema(ZSTR_VAL(schema), ZSTR_LEN(schema))) {
		php_error_docref(NULL, E_WARNING, "Detach schema name must be a valid identifier [A-Za-z_][A-Za-z0-9_]*");
		RETURN_FALSE;
	}

	pdo_object = zend_read_property(gene_db_sqlite_ce, gene_strip_obj(self), ZEND_STRL(GENE_DB_SQLITE_PDO), 1, NULL);
	if (!pdo_object || Z_TYPE_P(pdo_object) != IS_OBJECT) {
		php_error_docref(NULL, E_WARNING, "PDO connection is not initialized");
		RETURN_FALSE;
	}

	smart_str sql = {0};
	smart_str_appends(&sql, "DETACH DATABASE ");
	smart_str_appends(&sql, ZSTR_VAL(schema));
	smart_str_0(&sql);

	ZVAL_UNDEF(&retval);
	gene_pdo_exec(pdo_object, ZSTR_VAL(sql.s), &retval);
	smart_str_free(&sql);

	/* [GENE_FIX:2026-08-07] PDO::exec() returns false on failure; anything
	 * other than IS_FALSE/IS_UNDEF counts as success. */
	zend_bool ok = (Z_TYPE(retval) != IS_UNDEF && Z_TYPE(retval) != IS_FALSE) ? 1 : 0;
	if (Z_TYPE(retval) != IS_UNDEF) zval_ptr_dtor(&retval);
	RETURN_BOOL(ok);
}
/* }}} */

/*
 * {{{ public gene_db::history()
 */
PHP_METHOD(gene_db_sqlite, history)
{
	gene_request_context *ctx = gene_request_ctx();
	if (Z_TYPE(ctx->db_sqlite_history) == IS_UNDEF) {
		RETURN_NULL();
	}
	RETURN_ZVAL(&ctx->db_sqlite_history, 1, 0);
}
/* }}} */

/*
 * {{{ gene_db_sqlite_methods
 */
const zend_function_entry gene_db_sqlite_methods[] = {
		PHP_ME(gene_db_sqlite, __construct, gene_db_sqlite_construct, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, getPdo, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, select, gene_db_sqlite_select, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, count, gene_db_sqlite_count, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, insert, gene_db_sqlite_insert, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, insertIgnore, gene_db_sqlite_insert, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, upsert, gene_db_sqlite_upsert, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, batchInsert, gene_db_sqlite_batch_insert, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, update, gene_db_sqlite_update, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, delete, gene_db_sqlite_delete, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, where, gene_db_sqlite_where, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, in, gene_db_sqlite_in, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, join, gene_db_sqlite_join, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, leftJoin, gene_db_sqlite_side_join, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, rightJoin, gene_db_sqlite_side_join, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, union, gene_db_sqlite_union, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, reset, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, sql, gene_db_sqlite_sql, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, limit, gene_db_sqlite_limit, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, lockForUpdate, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, sharedLock, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, order, gene_db_sqlite_order, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, group, gene_db_sqlite_group, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, having, gene_db_sqlite_having, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, execute, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, all, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, row, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, cell, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, lastId, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_MALIAS(gene_db_sqlite, lastInsertId, lastId, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, affectedRows, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_MALIAS(gene_db_sqlite, rowCount, affectedRows, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, quote, gene_db_sqlite_quote, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, print, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, beginTransaction, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, inTransaction, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, rollBack, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, commit, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, transaction, gene_db_sqlite_transaction, ZEND_ACC_PUBLIC)
		PHP_MALIAS(gene_db_sqlite, transact, transaction, gene_db_sqlite_transaction, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, release, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, free, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, attach, gene_db_sqlite_attach, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, detach, gene_db_sqlite_detach, ZEND_ACC_PUBLIC)
		PHP_ME(gene_db_sqlite, __destruct, gene_db_sqlite_void_arginfo, ZEND_ACC_PUBLIC)
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
#if PHP_VERSION_ID >= 80200
	gene_db_sqlite_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

	//pdo
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_CONFIG), ZEND_ACC_PUBLIC);
	zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_PDO), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_SQL), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_JOIN), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_WHERE), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_GROUP), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_HAVING), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_UNION), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_ORDER), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_LIMIT), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_LOCK), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_DATA), ZEND_ACC_PUBLIC);
	zend_declare_property_null(gene_db_sqlite_ce, ZEND_STRL(GENE_DB_SQLITE_POOL), ZEND_ACC_PROTECTED);
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
