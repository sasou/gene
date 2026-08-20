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
#include "zend_smart_str.h"
#include <string.h>

#include "../gene.h"
#include "../di/di.h"
#include "../db/pdo.h"
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
	/* [GENE_FEATURE:2026-08-18 3.2] createdAt/updatedAt/timestampFormat ride
	 * the same request cache — to_array always writes the keys, so here a
	 * NULL zval means "column disabled", a missing key means default. */
	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("createdAt"));
	if (zv && Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) > 0) {
		meta->created_at = zend_string_copy(Z_STR_P(zv));
	} else if (!zv) {
		meta->created_at = zend_string_init(ZEND_STRL("created_at"), 0);
	}
	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("updatedAt"));
	if (zv && Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) > 0) {
		meta->updated_at = zend_string_copy(Z_STR_P(zv));
	} else if (!zv) {
		meta->updated_at = zend_string_init(ZEND_STRL("updated_at"), 0);
	}
	zv = zend_hash_str_find(Z_ARRVAL_P(arr), ZEND_STRL("timestampFormat"));
	meta->ts_unix = (zv && Z_TYPE_P(zv) == IS_STRING &&
		zend_string_equals_literal(Z_STR_P(zv), "unix")) ? 1 : 0;
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
	/* 3.2: always persist all three keys so from_array can distinguish
	 * "disabled" (null) from "not configured" (absent). */
	if (meta->created_at) {
		add_assoc_str_ex(arr, ZEND_STRL("createdAt"), zend_string_copy(meta->created_at));
	} else {
		add_assoc_null_ex(arr, ZEND_STRL("createdAt"));
	}
	if (meta->updated_at) {
		add_assoc_str_ex(arr, ZEND_STRL("updatedAt"), zend_string_copy(meta->updated_at));
	} else {
		add_assoc_null_ex(arr, ZEND_STRL("updatedAt"));
	}
	add_assoc_stringl_ex(arr, ZEND_STRL("timestampFormat"),
		meta->ts_unix ? "unix" : "datetime", meta->ts_unix ? 4 : 8);
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

	/* [GENE_FEATURE:2026-08-18 3.2] Column names are configurable; null/'' on
	 * the static property disables that column. Defaults match the pre-3.2
	 * hardcoded behaviour so existing $timestamps=true models are unaffected. */
	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_CREATED_AT));
	if (zv && Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) > 0) {
		meta->created_at = zend_string_copy(Z_STR_P(zv));
	} else if (zv && (Z_TYPE_P(zv) == IS_NULL || Z_TYPE_P(zv) == IS_STRING)) {
		meta->created_at = NULL; /* explicitly disabled */
	} else {
		meta->created_at = zend_string_init(ZEND_STRL("created_at"), 0);
	}
	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_UPDATED_AT));
	if (zv && Z_TYPE_P(zv) == IS_STRING && Z_STRLEN_P(zv) > 0) {
		meta->updated_at = zend_string_copy(Z_STR_P(zv));
	} else if (zv && (Z_TYPE_P(zv) == IS_NULL || Z_TYPE_P(zv) == IS_STRING)) {
		meta->updated_at = NULL;
	} else {
		meta->updated_at = zend_string_init(ZEND_STRL("updated_at"), 0);
	}
	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_TS_FORMAT));
	meta->ts_unix = (zv && Z_TYPE_P(zv) == IS_STRING &&
		zend_string_equals_literal(Z_STR_P(zv), "unix")) ? 1 : 0;

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
	/* [GENE_FEATURE:2026-08-18 3.2] M3: every new zend_string* in the meta
	 * struct must be released here AND round-tripped through
	 * to_array/from_array — missing either leaks or loses config per request. */
	if (meta->created_at) {
		zend_string_release(meta->created_at);
		meta->created_at = NULL;
	}
	if (meta->updated_at) {
		zend_string_release(meta->updated_at);
		meta->updated_at = NULL;
	}
	zval_ptr_dtor(&meta->fields);
	ZVAL_UNDEF(&meta->fields);
}

int gene_orm_get_db(zend_string *connection, zval *out)
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
		return FAILURE;
	}
	/* [GENE_FIX:2026-08-10 N1] gene_di_get() returns a borrowed pointer to a
	 * DI registry hashtable *slot*. The 2026-08-09 M5 fix only ADDREF'd the
	 * object but kept handing out the slot pointer: callers hold it across
	 * several call_user_function round-trips during which user code (getters,
	 * error handlers, __destruct) could Di::del()/Di::set() the service —
	 * deleting/replacing the slot so the later gene_orm_db_reset()/dtor acted
	 * on the replacement object or freed memory (UAF + leak). Copy the zval
	 * itself so the caller owns an independent handle; no user code runs
	 * between gene_di_get() and this copy, so the slot cannot dangle here.
	 * Every caller must zval_ptr_dtor() the out zval when done. */
	ZVAL_COPY(out, db);
	return SUCCESS;
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
		/* Unknown driver — fall back to public reset(). [GENE_FIX:2026-08-19 N4]
		 * Even with a pending exception we MUST clean: a guard exception
		 * (e.g. P0-2/N1) leaves a built but unexecuted WHERE-less UPDATE on
		 * the handle, and skipping reset would let a later read terminal
		 * execute it. Save the in-flight exception, run reset(), discard
		 * only an exception reset() itself raises, then restore.
		 * [GENE_FIX:2026-08-19 N8a] The exception is parked in a LOCAL
		 * variable instead of EG(prev_exception) (zend_exception_save) so
		 * the window is reentrant and cannot disturb an outer window. */
		zval fname, retval;
		zend_object *saved_exception = EG(exception);
		EG(exception) = NULL;
		ZVAL_STRING(&fname, "reset");
		ZVAL_UNDEF(&retval);
		call_user_function(NULL, db, &fname, &retval, 0, NULL);
		zval_ptr_dtor(&fname);
		zval_ptr_dtor(&retval);
		gene_discard_current_exception();
		EG(exception) = saved_exception;
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

/* [GENE_FEATURE:2026-08-18] Driver identification by class entry (same
 * approach as gene_orm_db_reset — name substring matching misidentifies
 * subclasses/pool wrappers). Used for driver-aware SQL fragments. */
int gene_orm_db_kind(zval *db)
{
	zend_class_entry *ce;

	if (!db || Z_TYPE_P(db) != IS_OBJECT) {
		return GENE_ORM_DB_UNKNOWN;
	}
	ce = Z_OBJCE_P(db);
	if (ce == gene_db_mysql_ce) {
		return GENE_ORM_DB_MYSQL;
	}
	if (ce == gene_db_sqlite_ce) {
		return GENE_ORM_DB_SQLITE;
	}
	if (ce == gene_db_pgsql_ce) {
		return GENE_ORM_DB_PGSQL;
	}
	if (ce == gene_db_mssql_ce) {
		return GENE_ORM_DB_MSSQL;
	}
	return GENE_ORM_DB_UNKNOWN;
}

/* [GENE_FEATURE:2026-08-18 3.1/3.5] Identifier whitelist for API surfaces
 * that splice a column name into a SQL fragment (where 3-arg, in column
 * form, whereLike, selectSub alias). Anything outside [A-Za-z0-9_.] (with a
 * non-digit, non-dot first char) is rejected — these APIs must not become a
 * new injection surface around the raw string-where path. */
zend_bool gene_orm_valid_ident(zend_string *s)
{
	size_t i;

	if (!s || ZSTR_LEN(s) == 0 || ZSTR_LEN(s) > 128) {
		return 0;
	}
	for (i = 0; i < ZSTR_LEN(s); i++) {
		unsigned char c = (unsigned char)ZSTR_VAL(s)[i];
		zend_bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9' && i > 0) || c == '_' || (c == '.' && i > 0);
		if (!ok) {
			return 0;
		}
	}
	return 1;
}

/* [GENE_FEATURE:2026-08-18 4.2] Append ", (<sub>) AS <alias>" to the SELECT
 * field list of the db's assembled SQL property. All four drivers build the
 * base SQL as "SELECT <fields> FROM <table>"; the main FROM is the LAST
 * " FROM " occurrence (a subquery appended earlier would introduce its own,
 * so we anchor at the end; a quoted identifier literally containing
 * " FROM " is pathological and out of scope). $sql is developer-written,
 * same trust level as Db::sql() — deliberately NOT escaped. */
int gene_orm_db_select_sub(zval *db, zend_string *sub, zend_string *alias)
{
	zval *sql_zv;
	zend_string *sql;
	const char *p, *last = NULL;
	smart_str buf = {0};
	char oq = '`', cq = '`';

	if (!db || Z_TYPE_P(db) != IS_OBJECT) {
		return FAILURE;
	}
	switch (gene_orm_db_kind(db)) {
	case GENE_ORM_DB_PGSQL:
		oq = cq = '"';
		break;
	case GENE_ORM_DB_MSSQL:
		oq = '['; cq = ']';
		break;
	default:
		break;
	}
	sql_zv = zend_read_property(Z_OBJCE_P(db), gene_strip_obj(db), ZEND_STRL("sql"), 1, NULL);
	if (!sql_zv || Z_TYPE_P(sql_zv) != IS_STRING || Z_STRLEN_P(sql_zv) == 0) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Query::selectSub() requires an active select() verb");
		return FAILURE;
	}
	sql = Z_STR_P(sql_zv);
	p = ZSTR_VAL(sql);
	while ((p = strstr(p, " FROM ")) != NULL) {
		last = p;
		p += 6;
	}
	if (!last) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Query::selectSub() cannot locate the FROM clause");
		return FAILURE;
	}
	smart_str_appendl(&buf, ZSTR_VAL(sql), last - ZSTR_VAL(sql));
	smart_str_appends(&buf, ", (");
	smart_str_appendl(&buf, ZSTR_VAL(sub), ZSTR_LEN(sub));
	smart_str_appends(&buf, ") AS ");
	smart_str_appendc(&buf, oq);
	smart_str_appendl(&buf, ZSTR_VAL(alias), ZSTR_LEN(alias));
	smart_str_appendc(&buf, cq);
	smart_str_appends(&buf, last);
	smart_str_0(&buf);
	zend_update_property_str(Z_OBJCE_P(db), gene_strip_obj(db), ZEND_STRL("sql"), buf.s);
	smart_str_free(&buf);
	return SUCCESS;
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

/* [GENE_FIX:2026-08-09 M2] PDO lastInsertId() is always a string, which made
 * create()/save() return a string id and store a string pk into attributes
 * while find() returns int — the same field with two types. Normalize numeric
 * strings to long, mirroring Query::count()'s 2026-08-08 hardening. */
void gene_orm_normalize_id(zval *id)
{
	zend_long l;
	double d;

	if (id && Z_TYPE_P(id) == IS_STRING) {
		if (is_numeric_string(Z_STRVAL_P(id), Z_STRLEN_P(id), &l, &d, 0) == IS_LONG) {
			zval_ptr_dtor(id);
			ZVAL_LONG(id, l);
		}
	}
}

/* [GENE_FEATURE:2026-08-18 3.2] Column names + format come from the model
 * meta ($createdAt/$updatedAt/$timestampFormat). A NULL column is skipped;
 * a payload-supplied value is never overwritten (unchanged semantics). */
void gene_orm_apply_timestamps(zval *data, zend_bool is_insert, gene_orm_meta_t *meta)
{
	time_t t;

	if (!data || Z_TYPE_P(data) != IS_ARRAY || !meta) {
		return;
	}
	if (!meta->created_at && !meta->updated_at) {
		return;
	}
	/* [GENE_FIX:2026-08-09 H3] sapi_get_request_time() is constant for the whole
	 * SAPI request — under CLI/Swoole that spans the process/worker lifetime, so
	 * created_at/updated_at froze at worker start. Use wall clock like the rest
	 * of the codebase (memory.c, pool.c, session.c, ...). */
	t = time(NULL);
	if (meta->ts_unix) {
		if (is_insert && meta->created_at &&
			!zend_hash_exists(Z_ARRVAL_P(data), meta->created_at)) {
			add_assoc_long_ex(data, ZSTR_VAL(meta->created_at), ZSTR_LEN(meta->created_at), (zend_long)t);
		}
		if (meta->updated_at &&
			!zend_hash_exists(Z_ARRVAL_P(data), meta->updated_at)) {
			add_assoc_long_ex(data, ZSTR_VAL(meta->updated_at), ZSTR_LEN(meta->updated_at), (zend_long)t);
		}
	} else {
		zend_string *now = php_format_date("Y-m-d H:i:s", sizeof("Y-m-d H:i:s") - 1, t, 1);
		if (is_insert && meta->created_at &&
			!zend_hash_exists(Z_ARRVAL_P(data), meta->created_at)) {
			add_assoc_str_ex(data, ZSTR_VAL(meta->created_at), ZSTR_LEN(meta->created_at),
				zend_string_copy(now));
		}
		if (meta->updated_at &&
			!zend_hash_exists(Z_ARRVAL_P(data), meta->updated_at)) {
			add_assoc_str_ex(data, ZSTR_VAL(meta->updated_at), ZSTR_LEN(meta->updated_at),
				zend_string_copy(now));
		}
		zend_string_release(now);
	}
}

/* Paginate-friendly limit: ORM API is always (offset, limit).
 * MySQL Db::limit($a,$b) → LIMIT a,b (offset,count).
 * Sqlite/Pgsql/Mssql Db::limit($a,$b) → LIMIT a OFFSET b (count,offset). */
void gene_orm_db_limit(zval *db, zend_long offset, zend_long limit)
{
	zval args[2], retval;
	zend_class_entry *ce;
	zend_bool mysql_style = 0;

	if (!db || Z_TYPE_P(db) != IS_OBJECT) {
		return;
	}
	/* [GENE_FIX:2026-08-09 M4] Class-name substring matching misidentified
	 * custom subclasses, raw Gene\Db\Pdo handles and pool wrappers, silently
	 * swapping offset/limit for real MySQL handles (and vice versa). Compare
	 * class entries like gene_orm_db_reset() does; unknown drivers get the
	 * documented default: LIMIT count OFFSET offset. */
	ce = Z_OBJCE_P(db);
	if (ce == gene_db_mysql_ce) {
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
