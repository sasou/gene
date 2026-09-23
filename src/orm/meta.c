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
	if (Z_TYPE(GENE_CTX_COLD(ctx)->orm_meta) == IS_UNDEF) {
		array_init(&GENE_CTX_COLD(ctx)->orm_meta);
	}
	return &GENE_CTX_COLD(ctx)->orm_meta;
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
	 * the same request cache �� to_array always writes the keys, so here a
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
#if PHP_VERSION_ID >= 80500
	const zend_class_entry *scope = EG(fake_scope);
#else
	zend_class_entry *scope = EG(fake_scope);
#endif

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
	 * to_array/from_array �� missing either leaks or loses config per request. */
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
	 * error handlers, __destruct) could Di::del()/Di::set() the service ��
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
		/* Unknown driver �� fall back to public reset(). [GENE_FIX:2026-08-19 N4]
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
	zend_class_entry *ce;
	zend_function *fn;
	zval fname;
	char method_lc[32];
	size_t method_len;
	int r;

	ZVAL_UNDEF(retval);
	if (db && Z_TYPE_P(db) == IS_OBJECT) {
		ce = Z_OBJCE_P(db);
		if (ce == gene_db_mysql_ce || ce == gene_db_sqlite_ce ||
			ce == gene_db_pgsql_ce || ce == gene_db_mssql_ce) {
			method_len = strlen(method);
			if (method_len < sizeof(method_lc)) {
				zend_str_tolower_copy(method_lc, method, method_len);
				fn = zend_hash_str_find_ptr(&ce->function_table, method_lc, method_len);
				if (fn) {
					zend_call_known_function(fn, Z_OBJ_P(db), ce, retval, argc, argv, NULL);
					return SUCCESS;
				}
			}
		}
	}

	ZVAL_STRING(&fname, method);
	r = call_user_function(NULL, db, &fname, retval, argc, argv);
	zval_ptr_dtor(&fname);
	return r;
}

/* [GENE_FEATURE:2026-08-18] Driver identification by class entry (same
 * approach as gene_orm_db_reset �� name substring matching misidentifies
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
 * non-digit, non-dot first char) is rejected �� these APIs must not become a
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
 * same trust level as Db::sql() �� deliberately NOT escaped. */
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
				HashPosition pos;
				/* [GENE_FIX:2026-08-24] fields may be a live reference into a
				 * shared/COW array (e.g. Query::$fields read via
				 * zend_read_property() without a copy) with refcount != 1.
				 * zend_hash_internal_pointer_reset()/_get_current_data()
				 * write through ht->nInternalPointer, mutating the shared
				 * array in place �� undefined under concurrent readers
				 * (Swoole coroutines) and fatal in debug builds
				 * (zend_hash_internal_pointer_reset_ex() assertion). Use a
				 * local HashPosition instead so we never touch the array's
				 * own internal pointer. */
				zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(fields), &pos);
				only = zend_hash_get_current_data_ex(Z_ARRVAL_P(fields), &pos);
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
 * while find() returns int �� the same field with two types. Normalize numeric
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
	 * SAPI request �� under CLI/Swoole that spans the process/worker lifetime, so
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
 * MySQL Db::limit($a,$b) �� LIMIT a,b (offset,count).
 * Sqlite/Pgsql/Mssql Db::limit($a,$b) �� LIMIT a OFFSET b (count,offset). */
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

/* [GENE_FEATURE:2026-09-23 O3] versionKeys live on the model class. Bumps are
 * applied through the request-scoped "cache" component (never a static
 * Cache/Db pointer). While a PDO transaction is open they sit on the
 * request cold block and flush only after commit.
 * [GENE_FIX:2026-09-23 R4] The pending list is bucketed per PDO object
 * handle: { handle => ['pdo' => obj, 'maps' => [map, ...]] }. Committing
 * connection A must not flush (or discard) connection B's still-open
 * transaction bumps. */

static zval *gene_orm_version_pending_slot(void)
{
	gene_request_context *ctx = gene_request_ctx();
	if (!ctx) {
		return NULL;
	}
	return &GENE_CTX_COLD(ctx)->orm_version_pending;
}

static void gene_orm_version_call_update(zval *map)
{
	zval *cache, rv;
	zend_string *name;
	zend_function *fn;

	if (!map || Z_TYPE_P(map) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(map)) == 0) {
		return;
	}
	name = zend_string_init("cache", sizeof("cache") - 1, 0);
	cache = gene_di_get(name);
	zend_string_release(name);
	if (!cache || Z_TYPE_P(cache) != IS_OBJECT) {
		return;
	}
	fn = zend_hash_str_find_ptr(&Z_OBJCE_P(cache)->function_table, ZEND_STRL("updateversion"));
	if (!fn) {
		return;
	}
	ZVAL_UNDEF(&rv);
	zend_call_known_function(fn, Z_OBJ_P(cache), Z_OBJCE_P(cache), &rv, 1, map, NULL);
	zval_ptr_dtor(&rv);
}

static void gene_orm_version_flush_maps(zval *maps)
{
	zval *map;

	if (!maps || Z_TYPE_P(maps) != IS_ARRAY) {
		return;
	}
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(maps), map) {
		if (map && Z_TYPE_P(map) == IS_ARRAY) {
			gene_orm_version_call_update(map);
		}
	} ZEND_HASH_FOREACH_END();
}

/* Flush only the bucket belonging to this PDO handle. Called from
 * gene_pdo_commit() — B's open transaction is untouched when A commits. */
void gene_orm_version_flush(zval *pdo)
{
	zval *pending, *bucket, *maps;

	if (!pdo || Z_TYPE_P(pdo) != IS_OBJECT) {
		return;
	}
	pending = gene_orm_version_pending_slot();
	if (!pending || Z_TYPE_P(pending) != IS_ARRAY) {
		return;
	}
	bucket = zend_hash_index_find(Z_ARRVAL_P(pending), (zend_ulong)Z_OBJ_HANDLE_P(pdo));
	if (bucket && Z_TYPE_P(bucket) == IS_ARRAY) {
		maps = zend_hash_str_find(Z_ARRVAL_P(bucket), ZEND_STRL("maps"));
		gene_orm_version_flush_maps(maps);
	}
	zend_hash_index_del(Z_ARRVAL_P(pending), (zend_ulong)Z_OBJ_HANDLE_P(pdo));
}

void gene_orm_version_discard(zval *pdo)
{
	zval *pending;

	if (!pdo || Z_TYPE_P(pdo) != IS_OBJECT) {
		return;
	}
	pending = gene_orm_version_pending_slot();
	if (!pending || Z_TYPE_P(pending) != IS_ARRAY) {
		return;
	}
	zend_hash_index_del(Z_ARRVAL_P(pending), (zend_ulong)Z_OBJ_HANDLE_P(pdo));
}

/* [GENE_FIX:2026-09-23 R4] Request-teardown safety net, called from
 * gene_request_context_free_fields() AFTER di_regs transaction hygiene but
 * BEFORE di_regs is destroyed (so gene_di_get("cache") still resolves).
 * A surviving bucket whose PDO is no longer in a transaction means the
 * commit bypassed gene_pdo_commit() (raw $pdo->commit() / sql('COMMIT')):
 * flush it — over-invalidating costs one cache refill, under-invalidating
 * leaves stale rows. A bucket still inside a transaction is discarded; its
 * eventual rollback path discards again harmlessly. */
void gene_orm_version_pending_shutdown(zval *pending)
{
	zval *bucket;

	/* The slot is passed in (not resolved via gene_request_ctx()): ctx
	 * teardown may run for a non-current coroutine or a pooled struct. */
	if (!pending || Z_TYPE_P(pending) != IS_ARRAY) {
		return;
	}
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(pending), bucket) {
		zval *pdo, *maps, in_tx;
		zend_bool open = 0;

		if (!bucket || Z_TYPE_P(bucket) != IS_ARRAY) {
			continue;
		}
		pdo = zend_hash_str_find(Z_ARRVAL_P(bucket), ZEND_STRL("pdo"));
		maps = zend_hash_str_find(Z_ARRVAL_P(bucket), ZEND_STRL("maps"));
		if (pdo && Z_TYPE_P(pdo) == IS_OBJECT) {
			gene_pdo_in_transaction(pdo, &in_tx);
			if (!EG(exception)) {
				open = zend_is_true(&in_tx);
			} else {
				zend_clear_exception();
			}
			zval_ptr_dtor(&in_tx);
		}
		if (!open) {
			gene_orm_version_flush_maps(maps);
		}
	} ZEND_HASH_FOREACH_END();
	zval_ptr_dtor(pending);
	ZVAL_UNDEF(pending);
}

static void gene_orm_version_publish(zval *db, zval *map)
{
	zval *pdo = NULL;
	zval *pending;

	if (!map || Z_TYPE_P(map) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(map)) == 0) {
		if (map && Z_TYPE_P(map) != IS_UNDEF) {
			zval_ptr_dtor(map);
		}
		return;
	}
	if (db && Z_TYPE_P(db) == IS_OBJECT) {
		pdo = zend_read_property(Z_OBJCE_P(db), gene_strip_obj(db), ZEND_STRL("pdo"), 1, NULL);
	}
	if (pdo && Z_TYPE_P(pdo) == IS_OBJECT && !EG(exception)) {
		zval in_tx;
		zend_bool open = 0;
		gene_pdo_in_transaction(pdo, &in_tx);
		if (!EG(exception)) {
			open = zend_is_true(&in_tx);
		} else {
			zend_clear_exception();
		}
		zval_ptr_dtor(&in_tx);
		if (open) {
		pending = gene_orm_version_pending_slot();
		if (pending) {
			zval *bucket, *maps;
			zend_ulong h = (zend_ulong)Z_OBJ_HANDLE_P(pdo);
			if (Z_TYPE_P(pending) != IS_ARRAY) {
				if (Z_TYPE_P(pending) != IS_UNDEF) {
					zval_ptr_dtor(pending);
				}
				array_init(pending);
			}
			bucket = zend_hash_index_find(Z_ARRVAL_P(pending), h);
			if (!bucket || Z_TYPE_P(bucket) != IS_ARRAY) {
				zval nb, pdo_copy, maps_arr;
				array_init(&nb);
				ZVAL_COPY(&pdo_copy, pdo);
				add_assoc_zval_ex(&nb, ZEND_STRL("pdo"), &pdo_copy);
				array_init(&maps_arr);
				add_assoc_zval_ex(&nb, ZEND_STRL("maps"), &maps_arr);
				zend_hash_index_update(Z_ARRVAL_P(pending), h, &nb);
				bucket = zend_hash_index_find(Z_ARRVAL_P(pending), h);
			}
			maps = bucket ? zend_hash_str_find(Z_ARRVAL_P(bucket), ZEND_STRL("maps")) : NULL;
			if (maps && Z_TYPE_P(maps) == IS_ARRAY) {
				add_next_index_zval(maps, map);
				return;
			}
		}
		}
	}
	gene_orm_version_call_update(map);
	zval_ptr_dtor(map);
}

int gene_orm_version_keys(zend_class_entry *ce, zval *keys)
{
	zval *zv, *cache;
	zend_string *name;
	zend_function *fn;

	ZVAL_UNDEF(keys);
	if (!ce) {
		return 0;
	}
	zv = zend_read_static_property(ce, ZEND_STRL(GENE_ORM_VERSION_KEYS), 1);
	if (!zv || Z_TYPE_P(zv) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(zv)) == 0) {
		return 0;
	}
	name = zend_string_init("cache", sizeof("cache") - 1, 0);
	cache = gene_di_get(name);
	zend_string_release(name);
	if (!cache || Z_TYPE_P(cache) != IS_OBJECT) {
		return 0;
	}
	fn = zend_hash_str_find_ptr(&Z_OBJCE_P(cache)->function_table, ZEND_STRL("updateversion"));
	if (!fn) {
		return 0;
	}
	ZVAL_COPY(keys, zv);
	return 1;
}

static zend_bool gene_orm_version_has_secondary(zval *keys, gene_orm_meta_t *meta)
{
	zend_string *field;
	zval *col;

	if (!keys || Z_TYPE_P(keys) != IS_ARRAY) {
		return 0;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(keys), field, col) {
		if (!field || !col || Z_TYPE_P(col) != IS_STRING) {
			continue;
		}
		if (!meta->primary_key || !zend_string_equals(Z_STR_P(col), meta->primary_key)) {
			return 1;
		}
	} ZEND_HASH_FOREACH_END();
	return 0;
}

/* [GENE_FIX:2026-09-23 R2] Any valid column mapping (pk or secondary) —
 * used to decide whether a non-pk where needs the prefetch SELECT at all. */
static zend_bool gene_orm_version_has_any_column(zval *keys, gene_orm_meta_t *meta)
{
	zend_string *field;
	zval *col;

	if (!keys || Z_TYPE_P(keys) != IS_ARRAY) {
		return 0;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(keys), field, col) {
		if (!field || !col || Z_TYPE_P(col) != IS_STRING) {
			continue;
		}
		if (gene_orm_valid_ident(Z_STR_P(col))) {
			return 1;
		}
	} ZEND_HASH_FOREACH_END();
	return 0;
}

/* [GENE_FIX:2026-09-23 R6] 1 when col_name is mapped as a secondary
 * version column (flip() then supplies the post-update pair). */
zend_bool gene_orm_version_col_mapped(zval *keys, gene_orm_meta_t *meta, zend_string *col_name)
{
	zend_string *field;
	zval *col;

	if (!keys || Z_TYPE_P(keys) != IS_ARRAY || !col_name) {
		return 0;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(keys), field, col) {
		if (!field || !col || Z_TYPE_P(col) != IS_STRING) {
			continue;
		}
		if (meta->primary_key && zend_string_equals(Z_STR_P(col), meta->primary_key)) {
			continue;
		}
		if (zend_string_equals(Z_STR_P(col), col_name)) {
			return 1;
		}
	} ZEND_HASH_FOREACH_END();
	return 0;
}

zend_long gene_orm_version_scan_limit(zend_class_entry *ce)
{
	zval *zv;
	zend_long n = 1000;

	if (!ce) {
		return n;
	}
	zv = gene_orm_read_static(ce, ZEND_STRL(GENE_ORM_VERSION_SCAN_LIMIT));
	if (zv && Z_TYPE_P(zv) == IS_LONG && Z_LVAL_P(zv) > 0) {
		n = Z_LVAL_P(zv);
	}
	return n;
}

static void gene_orm_version_reset_db(zval *db)
{
	gene_orm_db_reset(db);
}

/* Build "pk,col1,col2" (pk first, deduped) for the prefetch SELECT. */
static zend_string *gene_orm_version_prefetch_cols(zval *keys, gene_orm_meta_t *meta)
{
	smart_str cols = {0};
	zend_string *field;
	zval *col;
	zend_bool first = 1;
	zend_bool pk_in = 0;

	if (meta->primary_key && gene_orm_valid_ident(meta->primary_key)) {
		smart_str_append(&cols, meta->primary_key);
		first = 0;
	}
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(keys), field, col) {
		if (!field || !col || Z_TYPE_P(col) != IS_STRING) {
			continue;
		}
		if (!gene_orm_valid_ident(Z_STR_P(col))) {
			continue;
		}
		if (meta->primary_key && zend_string_equals(Z_STR_P(col), meta->primary_key)) {
			pk_in = 1;
			continue;
		}
		if (!first) {
			smart_str_appendc(&cols, ',');
		}
		first = 0;
		smart_str_append(&cols, Z_STR_P(col));
	} ZEND_HASH_FOREACH_END();
	smart_str_0(&cols);
	return cols.s;
}

/* [GENE_FIX:2026-09-23 R2] Pre-write read by primary key. Updates prefetch
 * whenever a secondary column is mapped (row-level invalidation needs the
 * current values regardless of payload), deletes likewise. Returns a single
 * assoc row (scalar pk) or a list of rows (array pk) in `old`. */
void gene_orm_version_prefetch(zval *db, gene_orm_meta_t *meta, zval *keys, zval *pk, zend_bool is_delete, zval *old)
{
	zend_string *cols;
	zval fields, args[2], retval, lim;

	ZVAL_UNDEF(old);
	if (!db || !meta || !keys || Z_TYPE_P(keys) != IS_ARRAY || !pk) {
		return;
	}
	if (!gene_orm_version_has_secondary(keys, meta)) {
		return;
	}
	cols = gene_orm_version_prefetch_cols(keys, meta);
	if (!cols) {
		return;
	}
	ZVAL_STR(&fields, cols);
	gene_orm_db_select(db, meta->table, &fields);
	zval_ptr_dtor(&fields);
	if (gene_orm_has_exception()) {
		gene_orm_version_reset_db(db);
		return;
	}
	if (Z_TYPE_P(pk) == IS_ARRAY) {
		smart_str buf = {0};
		smart_str_append(&buf, meta->primary_key);
		smart_str_appends(&buf, " in(?)");
		smart_str_0(&buf);
		ZVAL_STR(&args[0], buf.s);
		ZVAL_COPY(&args[1], pk);
		gene_orm_db_call(db, "in", 2, args, &retval);
		zval_ptr_dtor(&args[0]); /* consumes buf.s — do NOT smart_str_free */
		zval_ptr_dtor(&args[1]);
		zval_ptr_dtor(&retval);
		if (!gene_orm_has_exception() &&
			gene_orm_db_call(db, "all", 0, NULL, old) == SUCCESS &&
			Z_TYPE_P(old) != IS_ARRAY) {
			zval_ptr_dtor(old);
			ZVAL_UNDEF(old);
		}
	} else {
		smart_str buf = {0};
		smart_str_append(&buf, meta->primary_key);
		smart_str_appends(&buf, "=?");
		smart_str_0(&buf);
		ZVAL_STR(&args[0], buf.s);
		ZVAL_COPY(&args[1], pk);
		gene_orm_db_call(db, "where", 2, args, &retval);
		zval_ptr_dtor(&args[0]); /* consumes buf.s — do NOT smart_str_free */
		zval_ptr_dtor(&args[1]);
		zval_ptr_dtor(&retval);
		ZVAL_LONG(&lim, 1);
		gene_orm_db_call(db, "limit", 1, &lim, &retval);
		zval_ptr_dtor(&retval);
		if (!gene_orm_has_exception() &&
			gene_orm_db_call(db, "row", 0, NULL, old) == SUCCESS &&
			Z_TYPE_P(old) != IS_ARRAY) {
			zval_ptr_dtor(old);
			ZVAL_UNDEF(old);
		}
	}
	gene_orm_version_reset_db(db);
}

/* [GENE_FIX:2026-09-23 R3] Pre-write read for an arbitrary $where clause
 * (non-pk updateBy). Applies the same where semantics as the UPDATE via
 * gene_orm_apply_where() and caps the candidate set at `limit` rows
 * (LIMIT limit+1). On overflow sets *overflowed and returns no rows —
 * the caller warns and skips invalidation instead of bumping a partial set. */
void gene_orm_version_prefetch_where(zval *db, gene_orm_meta_t *meta, zval *keys, zval *where, zend_long limit, zval *old, zend_bool *overflowed)
{
	zend_string *cols;
	zval fields, retval;
	zend_bool emitted = 0;

	ZVAL_UNDEF(old);
	if (overflowed) {
		*overflowed = 0;
	}
	if (!db || !meta || !keys || Z_TYPE_P(keys) != IS_ARRAY ||
		!where || Z_TYPE_P(where) == IS_UNDEF || Z_TYPE_P(where) == IS_NULL) {
		return;
	}
	if (Z_TYPE_P(where) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(where)) == 0) {
		return;
	}
	if (!gene_orm_version_has_any_column(keys, meta)) {
		return;
	}
	cols = gene_orm_version_prefetch_cols(keys, meta);
	if (!cols) {
		return;
	}
	ZVAL_STR(&fields, cols);
	gene_orm_db_select(db, meta->table, &fields);
	zval_ptr_dtor(&fields);
	if (gene_orm_has_exception()) {
		gene_orm_version_reset_db(db);
		return;
	}
	gene_orm_apply_where(db, where, meta, &emitted);
	if (!emitted || gene_orm_has_exception()) {
		gene_orm_version_reset_db(db);
		return;
	}
	gene_orm_db_limit(db, 0, limit + 1);
	if (gene_orm_has_exception()) {
		gene_orm_version_reset_db(db);
		return;
	}
	if (gene_orm_db_call(db, "all", 0, NULL, old) == SUCCESS &&
		Z_TYPE_P(old) == IS_ARRAY) {
		if (overflowed && (zend_long)zend_hash_num_elements(Z_ARRVAL_P(old)) > limit) {
			zval_ptr_dtor(old);
			ZVAL_UNDEF(old);
			*overflowed = 1;
		}
	} else {
		if (Z_TYPE_P(old) != IS_UNDEF) {
			zval_ptr_dtor(old);
		}
		ZVAL_UNDEF(old);
	}
	gene_orm_version_reset_db(db);
}

static void gene_orm_version_add_value(zval *map, zend_string *field, zval *value)
{
	zval copy;
	if (!value) {
		add_assoc_null_ex(map, ZSTR_VAL(field), ZSTR_LEN(field));
		return;
	}
	ZVAL_COPY(&copy, value);
	add_assoc_zval_ex(map, ZSTR_VAL(field), ZSTR_LEN(field), &copy);
}

/* [GENE_FIX:2026-09-23 S1] Loose equality for bump values: a prefetched
 * row value and the payload's new value may differ only in zval type
 * (e.g. sqlite numeric string vs int). Restricted to scalars so
 * zend_compare() never sees array/object pairs. */
static zend_bool gene_orm_version_same(zval *a, zval *b)
{
	if (!a || !b) {
		return 0;
	}
	if (zend_is_identical(a, b)) {
		return 1;
	}
	if (Z_TYPE_P(a) == IS_REFERENCE) {
		a = Z_REFVAL_P(a);
	}
	if (Z_TYPE_P(b) == IS_REFERENCE) {
		b = Z_REFVAL_P(b);
	}
	if (Z_TYPE_P(a) >= IS_NULL && Z_TYPE_P(a) <= IS_STRING &&
		Z_TYPE_P(b) >= IS_NULL && Z_TYPE_P(b) <= IS_STRING) {
		return zend_compare(a, b) == 0;
	}
	return 0;
}

static void gene_orm_version_add_pair(zval *map, zend_string *field, zval *oldv, zval *newv)
{
	zval pair, a, b;
	/* [GENE_FIX:2026-09-23 S1] Identical prev/next collapses to a single
	 * value — ["new","new"] would bump the same key twice for no reason. */
	if (gene_orm_version_same(oldv, newv)) {
		gene_orm_version_add_value(map, field, newv);
		return;
	}
	array_init_size(&pair, 2);
	if (oldv) {
		ZVAL_COPY(&a, oldv);
	} else {
		ZVAL_NULL(&a);
	}
	if (newv) {
		ZVAL_COPY(&b, newv);
	} else {
		ZVAL_NULL(&b);
	}
	add_next_index_zval(&pair, &a);
	add_next_index_zval(&pair, &b);
	add_assoc_zval_ex(map, ZSTR_VAL(field), ZSTR_LEN(field), &pair);
}

static zval *gene_orm_version_row_col(zval *row, zend_string *col)
{
	if (!row || Z_TYPE_P(row) != IS_ARRAY || !col) {
		return NULL;
	}
	return zend_hash_find(Z_ARRVAL_P(row), col);
}

/* `old` carries either one assoc row (scalar-pk prefetch / hydrated attrs)
 * or a list of assoc rows (pk-array prefetch / where prefetch). */
static zend_bool gene_orm_version_old_is_rows(zval *old)
{
	HashPosition pos;
	zval *first;

	if (!old || Z_TYPE_P(old) != IS_ARRAY ||
		zend_hash_num_elements(Z_ARRVAL_P(old)) == 0) {
		return 0;
	}
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(old), &pos);
	first = zend_hash_get_current_data_ex(Z_ARRVAL_P(old), &pos);
	return (first && Z_TYPE_P(first) == IS_ARRAY) ? 1 : 0;
}

static void gene_orm_version_gather_col(zval *map, zend_string *field, zval *rows, zend_string *col)
{
	zval gathered, *row;

	array_init(&gathered);
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(rows), row) {
		zval *v = gene_orm_version_row_col(row, col);
		zval copy;
		if (!v || Z_TYPE_P(v) == IS_NULL) {
			continue;
		}
		ZVAL_COPY(&copy, v);
		add_next_index_zval(&gathered, &copy);
	} ZEND_HASH_FOREACH_END();
	if (zend_hash_num_elements(Z_ARRVAL(gathered)) > 0) {
		add_assoc_zval_ex(map, ZSTR_VAL(field), ZSTR_LEN(field), &gathered);
	} else {
		zval_ptr_dtor(&gathered);
	}
}

/* [GENE_FIX:2026-09-23 R2/R3] Row-level invalidation: any successful write
 * bumps the row's CURRENT key under every mapped version column — not just
 * columns present in the payload. A renamed column adds its new value on
 * top of the old one. */
void gene_orm_version_commit_write(zval *db, gene_orm_meta_t *meta, zval *keys, zval *pk, zval *data, zval *old, zend_bool is_delete, zend_long affected)
{
	zval map;
	zend_string *field;
	zval *col;
	zend_bool rows;

	if (affected <= 0 || !keys || Z_TYPE_P(keys) != IS_ARRAY || !meta) {
		return;
	}
	rows = gene_orm_version_old_is_rows(old);
	array_init(&map);
	ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(keys), field, col) {
		if (!field) {
			continue;
		}
		if (!col || Z_TYPE_P(col) == IS_NULL) {
			add_assoc_null_ex(&map, ZSTR_VAL(field), ZSTR_LEN(field));
			continue;
		}
		if (Z_TYPE_P(col) != IS_STRING || !gene_orm_valid_ident(Z_STR_P(col))) {
			continue;
		}
		if (meta->primary_key && zend_string_equals(Z_STR_P(col), meta->primary_key)) {
			if (pk && Z_TYPE_P(pk) != IS_NULL && Z_TYPE_P(pk) != IS_UNDEF) {
				gene_orm_version_add_value(&map, field, pk);
			} else if (rows) {
				gene_orm_version_gather_col(&map, field, old, meta->primary_key);
			} else {
				zval *v = gene_orm_version_row_col(old, meta->primary_key);
				if (v) {
					gene_orm_version_add_value(&map, field, v);
				}
			}
			continue;
		}
		if (is_delete) {
			if (rows) {
				gene_orm_version_gather_col(&map, field, old, Z_STR_P(col));
			} else {
				zval *v = gene_orm_version_row_col(old, Z_STR_P(col));
				if (v) {
					gene_orm_version_add_value(&map, field, v);
				}
			}
			continue;
		}
		{
			zval *neu = (data && Z_TYPE_P(data) == IS_ARRAY)
				? zend_hash_find(Z_ARRVAL_P(data), Z_STR_P(col)) : NULL;
			if (rows) {
				/* Batch where: every prefetched row's current value plus the
				 * payload's new value (once). */
				zval gathered, *row;
				array_init(&gathered);
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(old), row) {
					zval *v = gene_orm_version_row_col(row, Z_STR_P(col));
					zval copy;
					if (!v || Z_TYPE_P(v) == IS_NULL) {
						continue;
					}
					ZVAL_COPY(&copy, v);
					add_next_index_zval(&gathered, &copy);
				} ZEND_HASH_FOREACH_END();
				if (neu) {
					zval copy;
					ZVAL_COPY(&copy, neu);
					add_next_index_zval(&gathered, &copy);
				}
				if (zend_hash_num_elements(Z_ARRVAL(gathered)) > 0) {
					add_assoc_zval_ex(&map, ZSTR_VAL(field), ZSTR_LEN(field), &gathered);
				} else {
					zval_ptr_dtor(&gathered);
				}
			} else {
				zval *prev = gene_orm_version_row_col(old, Z_STR_P(col));
				if (neu) {
					if (prev) {
						gene_orm_version_add_pair(&map, field, prev, neu);
					} else {
						gene_orm_version_add_value(&map, field, neu);
					}
				} else if (prev) {
					gene_orm_version_add_value(&map, field, prev);
				}
			}
		}
	} ZEND_HASH_FOREACH_END();
	gene_orm_version_publish(db, &map);
}
