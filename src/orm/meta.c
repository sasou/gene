/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/date/php_date.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include <string.h>

#include "../gene.h"
#include "../di/di.h"
#include "../db/mysql.h"
#include "../db/sqlite.h"
#include "../db/pgsql.h"
#include "../db/mssql.h"
#include "orm.h"

/* Request-scoped meta cache: class name => array{
 *   table, primaryKey, fields, timestamps, connection
 * }. Lives in gene_request_context->orm_meta; freed on cleanup/RSHUTDOWN. */

static zval *gene_orm_meta_cache(void)
{
	gene_request_context *ctx = gene_request_ctx();
	if (!ctx) {
		return NULL;
	}
	if (Z_TYPE(ctx->orm_meta) == IS_UNDEF) {
		array_init(&ctx->orm_meta);
	}
	return &ctx->orm_meta;
}

static void gene_orm_meta_from_array(zval *arr, gene_orm_meta_t *meta)
{
	zval *zv;

	memset(meta, 0, sizeof(*meta));
	ZVAL_NULL(&meta->fields);

	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("table"));
	if (zv && Z_TYPE_P(zv) == IS_STRING) {
		meta->table = zend_string_copy(Z_STR_P(zv));
	}
	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("primaryKey"));
	if (zv && Z_TYPE_P(zv) == IS_STRING) {
		meta->primary_key = zend_string_copy(Z_STR_P(zv));
	}
	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("fields"));
	if (zv) {
		ZVAL_COPY(&meta->fields, zv);
	}
	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("timestamps"));
	if (zv) {
		meta->timestamps = zend_is_true(zv) ? 1 : 0;
	}
	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("connection"));
	if (zv && Z_TYPE_P(zv) == IS_STRING) {
		meta->connection = zend_string_copy(Z_STR_P(zv));
	} else {
		meta->connection = zend_string_init("db", sizeof("db") - 1, 0);
	}
}

static void gene_orm_meta_to_array(gene_orm_meta_t *meta, zval *arr)
{
	array_init(arr);
	if (meta->table) {
		add_assoc_str_ex(arr, ZEND_STRL("table"), zend_string_copy(meta->table));
	}
	if (meta->primary_key) {
		add_assoc_str_ex(arr, ZEND_STRL("primaryKey"), zend_string_copy(meta->primary_key));
	}
	if (Z_TYPE(meta->fields) != IS_UNDEF && Z_TYPE(meta->fields) != IS_NULL) {
		zval tmp;
		ZVAL_COPY(&tmp, &meta->fields);
		add_assoc_zval_ex(arr, ZEND_STRL("fields"), &tmp);
	} else {
		add_assoc_null_ex(arr, ZEND_STRL("fields"));
	}
	add_assoc_bool_ex(arr, ZEND_STRL("timestamps"), meta->timestamps);
	if (meta->connection) {
		add_assoc_str_ex(arr, ZEND_STRL("connection"), zend_string_copy(meta->connection));
	}
}

static zval *gene_orm_read_static(zend_class_entry *ce, const char *name, size_t name_len)
{
	zval *zv;
	zend_class_entry *scope = EG(fake_scope);

	EG(fake_scope) = ce;
	zv = zend_read_static_property(ce, name, name_len, 1);
	EG(fake_scope) = scope;
	return zv;
}

int gene_orm_meta_load(zend_class_entry *ce, gene_orm_meta_t *meta)
{
	zval *cache, *cached, built;
	zval *zv;

	if (!ce || !meta) {
		return FAILURE;
	}

	cache = gene_orm_meta_cache();
	if (cache && Z_TYPE_P(cache) == IS_ARRAY) {
		cached = zend_hash_find(Z_ARRVAL_P(cache), ce->name);
		if (cached && Z_TYPE_P(cached) == IS_ARRAY) {
			gene_orm_meta_from_array(cached, meta);
			if (!meta->table || ZSTR_LEN(meta->table) == 0) {
				gene_orm_meta_release(meta);
				zend_throw_exception_ex(NULL, 0,
					"Gene\\Orm\\Model subclass %s must declare protected static $table",
					ZSTR_VAL(ce->name));
				return FAILURE;
			}
			return SUCCESS;
		}
	}

	memset(meta, 0, sizeof(*meta));
	ZVAL_NULL(&meta->fields);

	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_TABLE));
	if (zv && Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) > 0) {
		meta->table = zend_string_copy(Z_STR_P(zv));
	} else {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Model subclass %s must declare protected static $table",
			ZSTR_VAL(ce->name));
		return FAILURE;
	}

	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_PK));
	if (zv && Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) > 0) {
		meta->primary_key = zend_string_copy(Z_STR_P(zv));
	} else {
		meta->primary_key = zend_string_init("id", sizeof("id") - 1, 0);
	}

	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_FIELDS));
	if (zv && Z_TYPE_P(zv) != IS_NULL && Z_TYPE_P(zv) != IS_UNDEF) {
		ZVAL_COPY(&meta->fields, zv);
	}

	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_TIMESTAMPS));
	meta->timestamps = (zv && zend_is_true(zv)) ? 1 : 0;

	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_CONNECTION));
	if (zv && Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) > 0) {
		meta->connection = zend_string_copy(Z_STR_P(zv));
	} else {
		meta->connection = zend_string_init("db", sizeof("db") - 1, 0);
	}

	if (cache && Z_TYPE_P(cache) == IS_ARRAY) {
		gene_orm_meta_to_array(meta, &built);
		zend_hash_update(Z_ARRVAL_P(cache), ce->name, &built);
	}

	return SUCCESS;
}

void gene_orm_meta_release(gene_orm_meta_t *meta)
{
	if (!meta) {
		return;
	}
	if (meta->table) {
		zend_string_release(meta->table);
		meta->table = NULL;
	}
	if (meta->primary_key) {
		zend_string_release(meta->primary_key);
		meta->primary_key = NULL;
	}
	if (meta->connection) {
		zend_string_release(meta->connection);
		meta->connection = NULL;
	}
	zval_ptr_dtor(&meta->fields);
	ZVAL_UNDEF(&meta->fields);
}

zval *gene_orm_get_db(zend_string *connection)
{
	zval *db;
	zend_string *name = connection;

	if (!name) {
		name = zend_string_init("db", sizeof("db") - 1, 0);
		db = gene_di_get(name);
		zend_string_release(name);
	} else {
		db = gene_di_get(name);
	}
	if (!db || Z_TYPE_P(db) != IS_OBJECT) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm: DI service \"%s\" is not an object (configure db)",
			connection ? ZSTR_VAL(connection) : "db");
		return NULL;
	}
	return db;
}

void gene_orm_db_reset(zval *db)
{
	zend_class_entry *ce;

	if (!db || Z_TYPE_P(db) != IS_OBJECT) {
		return;
	}
	ce = Z_OBJCE_P(db);
	if (ce == gene_db_mysql_ce) {
		mysql_reset_sql_params(db);
	} else if (ce == gene_db_sqlite_ce) {
		sqlite_reset_sql_params(db);
	} else if (ce == gene_db_pgsql_ce) {
		pgsql_reset_sql_params(db);
	} else if (ce == gene_db_mssql_ce) {
		mssql_reset_sql_params(db);
	} else {
		/* Unknown driver — fall back to public reset() */
		zval fname, retval;
		ZVAL_STRING(&fname, "reset");
		ZVAL_UNDEF(&retval);
		call_user_function(NULL, db, &fname, &retval, 0, NULL);
		zval_ptr_dtor(&fname);
		zval_ptr_dtor(&retval);
	}
}

int gene_orm_db_call(zval *db, const char *method, uint32_t argc, zval *argv, zval *retval)
{
	zval fname;
	int r;

	ZVAL_STRING(&fname, method);
	ZVAL_UNDEF(retval);
	r = call_user_function(NULL, db, &fname, retval, argc, argv);
	zval_ptr_dtor(&fname);
	return r;
}

int gene_orm_db_select(zval *db, zend_string *table, zval *fields)
{
	zval args[2], retval;
	uint32_t argc = 1;
	int r;

	ZVAL_STR_COPY(&args[0], table);
	if (fields && Z_TYPE_P(fields) != IS_NULL && Z_TYPE_P(fields) != IS_UNDEF) {
		if (Z_TYPE_P(fields) == IS_STRING) {
			if (Z_STRLEN_P(fields) == 0 ||
				(Z_STRLEN_P(fields) == 1 && Z_STRVAL_P(fields)[0] == '*')) {
				/* SELECT * */
			} else {
				ZVAL_COPY(&args[1], fields);
				argc = 2;
			}
		} else if (Z_TYPE_P(fields) == IS_ARRAY) {
			uint32_t n = zend_hash_num_elements(Z_ARRVAL_P(fields));
			if (n == 1) {
				zval *only;
				zend_hash_internal_pointer_reset(Z_ARRVAL_P(fields));
				only = zend_hash_get_current_data(Z_ARRVAL_P(fields));
				if (!(only && Z_TYPE_P(only) == IS_STRING &&
					Z_STRLEN_P(only) == 1 && Z_STRVAL_P(only)[0] == '*')) {
					ZVAL_COPY(&args[1], fields);
					argc = 2;
				}
			} else if (n > 1) {
				ZVAL_COPY(&args[1], fields);
				argc = 2;
			}
		} else {
			ZVAL_COPY(&args[1], fields);
			argc = 2;
		}
	}

	r = gene_orm_db_call(db, "select", argc, args, &retval);
	zval_ptr_dtor(&args[0]);
	if (argc == 2) {
		zval_ptr_dtor(&args[1]);
	}
	zval_ptr_dtor(&retval);
	return r;
}

void gene_orm_apply_timestamps(zval *data, zend_bool is_insert)
{
	zend_string *now;
	time_t t;

	if (!data || Z_TYPE_P(data) != IS_ARRAY) {
		return;
	}
	t = (time_t)sapi_get_request_time();
	now = php_format_date("Y-m-d H:i:s", sizeof("Y-m-d H:i:s") - 1, t, 1);
	if (is_insert) {
		if (!zend_hash_str_exists(Z_ARRVAL_P(data), ZEND_STRL("created_at"))) {
			add_assoc_str_ex(data, ZEND_STRL("created_at"), zend_string_copy(now));
		}
	}
	if (!zend_hash_str_exists(Z_ARRVAL_P(data), ZEND_STRL("updated_at"))) {
		add_assoc_str_ex(data, ZEND_STRL("updated_at"), zend_string_copy(now));
	}
	zend_string_release(now);
}

/* Paginate-friendly limit: ORM API is always (offset, limit).
 * MySQL Db::limit($a,$b) → LIMIT a,b (offset,count).
 * Sqlite/Pgsql/Mssql Db::limit($a,$b) → LIMIT a OFFSET b (count,offset). */
void gene_orm_db_limit(zval *db, zend_long offset, zend_long limit)
{
	zval args[2], retval;
	const char *cname;
	zend_bool mysql_style = 0;

	if (!db || Z_TYPE_P(db) != IS_OBJECT) {
		return;
	}
	cname = ZSTR_VAL(Z_OBJCE_P(db)->name);
	if (strstr(cname, "Mysql") || strstr(cname, "mysql")) {
		mysql_style = 1;
	}
	if (mysql_style) {
		ZVAL_LONG(&args[0], offset);
		ZVAL_LONG(&args[1], limit);
	} else {
		ZVAL_LONG(&args[0], limit);
		ZVAL_LONG(&args[1], offset);
	}
	gene_orm_db_call(db, "limit", 2, args, &retval);
	zval_ptr_dtor(&retval);
}
