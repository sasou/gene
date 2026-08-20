/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifndef GENE_ORM_H
#define GENE_ORM_H

#define GENE_ORM_ATTRS "attributes"
#define GENE_ORM_EXISTS "exists"
#define GENE_ORM_TABLE "table"
#define GENE_ORM_PK "primaryKey"
#define GENE_ORM_FIELDS "fields"
#define GENE_ORM_TIMESTAMPS "timestamps"
#define GENE_ORM_CONNECTION "connection"

#define GENE_ORM_QUERY_DB "db"
#define GENE_ORM_QUERY_TABLE "table"
#define GENE_ORM_QUERY_FIELDS "fields"
#define GENE_ORM_QUERY_DIRTY "dirty"
/* [GENE_FEATURE:2026-08-18 A0] Query v2: conditions live in a single ordered
 * `ops` array property (each op = [tag, ...args]) instead of one slot per
 * condition type — repeated where()/join() no longer silently overwrite. */
#define GENE_ORM_QUERY_OPS "ops"
#define GENE_ORM_QUERY_PK "primaryKey"
/* Set by Query::in($col, []) — terminal methods must return an empty result
 * WITHOUT issuing SQL (an empty IN must never degrade to "no condition"). */
#define GENE_ORM_QUERY_EMPTY "emptyResult"

#define GENE_ORM_CREATED_AT "createdAt"
#define GENE_ORM_UPDATED_AT "updatedAt"
#define GENE_ORM_TS_FORMAT "timestampFormat"

extern zend_class_entry *gene_orm_model_ce;
extern zend_class_entry *gene_orm_query_ce;

typedef struct _gene_orm_meta {
	zend_string *table;
	zend_string *primary_key;
	zval fields; /* array|string|IS_NULL — owned copy when from cache */
	zend_bool timestamps;
	zend_string *connection;
	/* [GENE_FEATURE:2026-08-18 3.2] Configurable timestamp columns.
	 * NULL created_at/updated_at = that column is never written.
	 * ts_unix: 1 → time() int, 0 → "Y-m-d H:i:s" string. */
	zend_string *created_at;
	zend_string *updated_at;
	zend_bool ts_unix;
} gene_orm_meta_t;

/* driver identification for driver-aware SQL fragments (meta.c) */
#define GENE_ORM_DB_UNKNOWN 0
#define GENE_ORM_DB_MYSQL   1
#define GENE_ORM_DB_SQLITE  2
#define GENE_ORM_DB_PGSQL   3
#define GENE_ORM_DB_MSSQL   4

GENE_MINIT_FUNCTION(orm);

/* [GENE_FIX:2026-08-09 M3] call_user_function reports SUCCESS even when the
 * callee threw — gate every db call sequence on the pending exception so the
 * first error is not masked by follow-up SQL. */
static zend_always_inline bool gene_orm_has_exception(void)
{
	return EG(exception) != NULL;
}

/* meta.c */
int gene_orm_meta_load(zend_class_entry *ce, gene_orm_meta_t *meta);
void gene_orm_meta_release(gene_orm_meta_t *meta);
int gene_orm_get_db(zend_string *connection, zval *out);
void gene_orm_db_reset(zval *db);
int gene_orm_db_kind(zval *db);
int gene_orm_db_call(zval *db, const char *method, uint32_t argc, zval *argv, zval *retval);
int gene_orm_db_select(zval *db, zend_string *table, zval *fields);
int gene_orm_db_select_sub(zval *db, zend_string *sub, zend_string *alias);
void gene_orm_apply_timestamps(zval *data, zend_bool is_insert, gene_orm_meta_t *meta);
void gene_orm_db_limit(zval *db, zend_long offset, zend_long limit);
void gene_orm_normalize_id(zval *id);
zend_bool gene_orm_valid_ident(zend_string *s);

/* query.c */
int gene_orm_query_init(zval *query, zval *db, zend_string *table, zval *fields, zend_string *primary_key);
void gene_orm_query_register(void);

#endif
