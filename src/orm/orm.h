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

extern zend_class_entry *gene_orm_model_ce;
extern zend_class_entry *gene_orm_query_ce;

typedef struct _gene_orm_meta {
	zend_string *table;
	zend_string *primary_key;
	zval fields; /* array|string|IS_NULL — owned copy when from cache */
	zend_bool timestamps;
	zend_string *connection;
} gene_orm_meta_t;

GENE_MINIT_FUNCTION(orm);

/* meta.c */
int gene_orm_meta_load(zend_class_entry *ce, gene_orm_meta_t *meta);
void gene_orm_meta_release(gene_orm_meta_t *meta);
zval *gene_orm_get_db(zend_string *connection);
void gene_orm_db_reset(zval *db);
int gene_orm_db_call(zval *db, const char *method, uint32_t argc, zval *argv, zval *retval);
int gene_orm_db_select(zval *db, zend_string *table, zval *fields);
void gene_orm_apply_timestamps(zval *data, zend_bool is_insert);
void gene_orm_db_limit(zval *db, zend_long offset, zend_long limit);

/* query.c */
int gene_orm_query_init(zval *query, zval *db, zend_string *table, zval *fields);
void gene_orm_query_register(void);

#endif
