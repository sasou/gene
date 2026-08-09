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
#include "Zend/zend_API.h"
#include "zend_exceptions.h"

#include "../gene.h"
#include "orm.h"

zend_class_entry *gene_orm_query_ce;

#define GENE_ORM_QUERY_WHERE "whereCond"
#define GENE_ORM_QUERY_WHERE_BIND "whereBind"
#define GENE_ORM_QUERY_IN_SQL "inSql"
#define GENE_ORM_QUERY_IN_BIND "inBind"
#define GENE_ORM_QUERY_ORDER "orderBy"
#define GENE_ORM_QUERY_LIMIT_A "limitA"
#define GENE_ORM_QUERY_LIMIT_B "limitB"
#define GENE_ORM_QUERY_HAS_LIMIT "hasLimit"

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_where_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, where)
	ZEND_ARG_INFO(0, bind)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_in_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, in)
	ZEND_ARG_INFO(0, bind)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_order_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, order)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_limit_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

static void gene_orm_query_mark_dirty(zval *self, zend_bool dirty)
{
	zend_update_property_bool(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_DIRTY), dirty ? 1 : 0);
}

static zval *gene_orm_query_db(zval *self)
{
	zval *db = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_DB), 1, NULL);
	if (!db || Z_TYPE_P(db) != IS_OBJECT) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query has no db handle");
		return NULL;
	}
	return db;
}

/* Rebuild select/count chain from stored conditions onto db. */
static int gene_orm_query_apply(zval *self, zval *db, zend_bool for_count)
{
	zval *table_zv, *fields_zv, *where_zv, *bind_zv, *in_sql, *in_bind, *order_zv, *has_limit;
	zval *la, *lb;
	zval args[2], retval;
	uint32_t argc;

	table_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_TABLE), 1, NULL);
	if (!table_zv || Z_TYPE_P(table_zv) != IS_STRING) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query has no table");
		return FAILURE;
	}

	gene_orm_db_reset(db);

	if (for_count) {
		ZVAL_STR_COPY(&args[0], Z_STR_P(table_zv));
		gene_orm_db_call(db, "count", 1, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&retval);
	} else {
		fields_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_FIELDS), 1, NULL);
		if (gene_orm_db_select(db, Z_STR_P(table_zv), fields_zv) != SUCCESS) {
			return FAILURE;
		}
	}
	if (UNEXPECTED(gene_orm_has_exception())) return FAILURE;

	where_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_WHERE), 1, NULL);
	bind_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_WHERE_BIND), 1, NULL);
	if (where_zv && Z_TYPE_P(where_zv) != IS_NULL && Z_TYPE_P(where_zv) != IS_UNDEF) {
		argc = 1;
		ZVAL_COPY(&args[0], where_zv);
		if (bind_zv && Z_TYPE_P(bind_zv) != IS_NULL && Z_TYPE_P(bind_zv) != IS_UNDEF) {
			ZVAL_COPY(&args[1], bind_zv);
			argc = 2;
		}
		gene_orm_db_call(db, "where", argc, args, &retval);
		zval_ptr_dtor(&args[0]);
		if (argc == 2) {
			zval_ptr_dtor(&args[1]);
		}
		zval_ptr_dtor(&retval);
		if (UNEXPECTED(gene_orm_has_exception())) return FAILURE;
	}

	in_sql = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_IN_SQL), 1, NULL);
	in_bind = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_IN_BIND), 1, NULL);
	if (in_sql && Z_TYPE_P(in_sql) == IS_STRING && Z_STRLEN_P(in_sql) > 0) {
		argc = 1;
		ZVAL_COPY(&args[0], in_sql);
		if (in_bind && Z_TYPE_P(in_bind) != IS_NULL && Z_TYPE_P(in_bind) != IS_UNDEF) {
			ZVAL_COPY(&args[1], in_bind);
			argc = 2;
		}
		gene_orm_db_call(db, "in", argc, args, &retval);
		zval_ptr_dtor(&args[0]);
		if (argc == 2) {
			zval_ptr_dtor(&args[1]);
		}
		zval_ptr_dtor(&retval);
		if (UNEXPECTED(gene_orm_has_exception())) return FAILURE;
	}

	if (!for_count) {
		order_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_ORDER), 1, NULL);
		if (order_zv && Z_TYPE_P(order_zv) == IS_STRING && Z_STRLEN_P(order_zv) > 0) {
			ZVAL_COPY(&args[0], order_zv);
			gene_orm_db_call(db, "order", 1, args, &retval);
			zval_ptr_dtor(&args[0]);
			zval_ptr_dtor(&retval);
			if (UNEXPECTED(gene_orm_has_exception())) return FAILURE;
		}

		has_limit = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_HAS_LIMIT), 1, NULL);
		if (has_limit && zend_is_true(has_limit)) {
			la = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
				ZEND_STRL(GENE_ORM_QUERY_LIMIT_A), 1, NULL);
			lb = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
				ZEND_STRL(GENE_ORM_QUERY_LIMIT_B), 1, NULL);
			if (lb && Z_TYPE_P(lb) == IS_LONG) {
				/* Two-arg form: (offset, count) — driver-aware via gene_orm_db_limit */
				gene_orm_db_limit(db, la ? Z_LVAL_P(la) : 0, Z_LVAL_P(lb));
			} else {
				ZVAL_LONG(&args[0], la ? Z_LVAL_P(la) : 0);
				gene_orm_db_call(db, "limit", 1, args, &retval);
				zval_ptr_dtor(&retval);
			}
			if (UNEXPECTED(gene_orm_has_exception())) return FAILURE;
		}
	}

	gene_orm_query_mark_dirty(self, 1);
	return SUCCESS;
}

static void gene_orm_query_finish(zval *self, zval *db)
{
	gene_orm_db_reset(db);
	gene_orm_query_mark_dirty(self, 0);
}

int gene_orm_query_init(zval *query, zval *db, zend_string *table, zval *fields)
{
	object_init_ex(query, gene_orm_query_ce);
	/* zend_update_property ZVAL_COPY's the value; do not ADDREF beforehand
	 * (db from gene_di_get is a borrowed pointer). */
	zend_update_property(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_DB), db);
	zend_update_property_str(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_TABLE), table);
	if (fields && Z_TYPE_P(fields) != IS_UNDEF && Z_TYPE_P(fields) != IS_NULL) {
		zend_update_property(gene_orm_query_ce, gene_strip_obj(query),
			ZEND_STRL(GENE_ORM_QUERY_FIELDS), fields);
	} else {
		zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
			ZEND_STRL(GENE_ORM_QUERY_FIELDS));
	}
	zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_WHERE));
	zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_WHERE_BIND));
	zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_IN_SQL));
	zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_IN_BIND));
	zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_ORDER));
	zend_update_property_long(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_LIMIT_A), 0);
	zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_LIMIT_B));
	zend_update_property_bool(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_HAS_LIMIT), 0);
	gene_orm_query_mark_dirty(query, 0);
	return SUCCESS;
}

PHP_METHOD(gene_orm_query, __construct)
{
	zend_throw_exception_ex(NULL, 0,
		"Gene\\Orm\\Query cannot be constructed directly; use Model::query()");
}

PHP_METHOD(gene_orm_query, __destruct)
{
	zval *self = getThis();
	zval *dirty, *db;

	dirty = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_DIRTY), 1, NULL);
	if (dirty && zend_is_true(dirty)) {
		db = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_DB), 1, NULL);
		if (db && Z_TYPE_P(db) == IS_OBJECT) {
			gene_orm_db_reset(db);
		}
		gene_orm_query_mark_dirty(self, 0);
	}
}

PHP_METHOD(gene_orm_query, where)
{
	zval *self = getThis(), *where = NULL, *bind = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &where, &bind) == FAILURE) {
		return;
	}
	zend_update_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_WHERE), where);
	if (bind) {
		zend_update_property(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_WHERE_BIND), bind);
	} else {
		zend_update_property_null(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_WHERE_BIND));
	}
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, in)
{
	zval *self = getThis(), *bind = NULL;
	zend_string *in = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|z", &in, &bind) == FAILURE) {
		return;
	}
	zend_update_property_str(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_IN_SQL), in);
	if (bind) {
		zend_update_property(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_IN_BIND), bind);
	} else {
		zend_update_property_null(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_IN_BIND));
	}
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, order)
{
	zval *self = getThis();
	zend_string *order = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &order) == FAILURE) {
		return;
	}
	zend_update_property_str(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_ORDER), order);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, limit)
{
	zval *self = getThis();
	zend_long num, limit = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l", &num, &limit) == FAILURE) {
		return;
	}
	/* One arg: take num rows. Two args: (offset, count) like MySQL LIMIT a,b
	 * and Model::paginate — apply() routes via gene_orm_db_limit. */
	zend_update_property_long(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_LIMIT_A), num);
	if (ZEND_NUM_ARGS() > 1) {
		zend_update_property_long(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_LIMIT_B), limit);
	} else {
		zend_update_property_null(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_LIMIT_B));
	}
	zend_update_property_bool(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_HAS_LIMIT), 1);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, all)
{
	zval *self = getThis();
	zval *db, retval;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_NULL();
	}
	if (gene_orm_query_apply(self, db, 0) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	if (gene_orm_db_call(db, "all", 0, NULL, &retval) == SUCCESS) {
		gene_orm_query_finish(self, db);
		if (Z_ISUNDEF(retval)) {
			RETURN_NULL();
		}
		RETURN_ZVAL(&retval, 0, 1);
	}
	gene_orm_query_finish(self, db);
	RETURN_NULL();
}

PHP_METHOD(gene_orm_query, row)
{
	zval *self = getThis();
	zval *db, retval;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_NULL();
	}
	if (gene_orm_query_apply(self, db, 0) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	if (gene_orm_db_call(db, "row", 0, NULL, &retval) == SUCCESS) {
		gene_orm_query_finish(self, db);
		if (Z_ISUNDEF(retval)) {
			RETURN_NULL();
		}
		RETURN_ZVAL(&retval, 0, 1);
	}
	gene_orm_query_finish(self, db);
	RETURN_NULL();
}

PHP_METHOD(gene_orm_query, cell)
{
	zval *self = getThis();
	zval *db, retval;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_NULL();
	}
	if (gene_orm_query_apply(self, db, 0) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	if (gene_orm_db_call(db, "cell", 0, NULL, &retval) == SUCCESS) {
		gene_orm_query_finish(self, db);
		if (Z_ISUNDEF(retval)) {
			RETURN_NULL();
		}
		RETURN_ZVAL(&retval, 0, 1);
	}
	gene_orm_query_finish(self, db);
	RETURN_NULL();
}

PHP_METHOD(gene_orm_query, count)
{
	zval *self = getThis();
	zval *db, retval;
	zend_long n = 0;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_LONG(0);
	}
	if (gene_orm_query_apply(self, db, 1) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_LONG(0);
	}
	if (gene_orm_db_call(db, "cell", 0, NULL, &retval) == SUCCESS) {
		gene_orm_query_finish(self, db);
		if (Z_ISUNDEF(retval)) {
			RETURN_LONG(0);
		}
		if (Z_TYPE(retval) == IS_LONG) {
			n = Z_LVAL(retval);
		} else if (Z_TYPE(retval) == IS_STRING) {
			n = zend_atol(Z_STRVAL(retval), Z_STRLEN(retval));
		} else if (Z_TYPE(retval) == IS_DOUBLE) {
			n = (zend_long)Z_DVAL(retval);
		} else {
			n = zval_get_long(&retval);
		}
		zval_ptr_dtor(&retval);
		RETURN_LONG(n);
	}
	gene_orm_query_finish(self, db);
	RETURN_LONG(0);
}

const zend_function_entry gene_orm_query_methods[] = {
	PHP_ME(gene_orm_query, __construct, gene_orm_query_void_arginfo, ZEND_ACC_PRIVATE)
	PHP_ME(gene_orm_query, __destruct, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, where, gene_orm_query_where_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, in, gene_orm_query_in_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, order, gene_orm_query_order_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, limit, gene_orm_query_limit_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, all, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, row, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, cell, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, count, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

void gene_orm_query_register(void)
{
	zend_class_entry ce;

	GENE_INIT_CLASS_ENTRY(ce, "Gene_Orm_Query", "Gene\\Orm\\Query", gene_orm_query_methods);
	gene_orm_query_ce = zend_register_internal_class_ex(&ce, NULL);
	gene_orm_query_ce->ce_flags |= ZEND_ACC_FINAL;
	/* [GENE_FIX:2026-08-09] Dropped ALLOW_DYNAMIC_PROPERTIES: a final class
	 * with protected internal state should stay closed; dynamic props would
	 * bypass the dirty-latch bookkeeping. */

	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_DB), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_TABLE), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_FIELDS), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_DIRTY), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_WHERE), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_WHERE_BIND), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_IN_SQL), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_IN_BIND), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_ORDER), ZEND_ACC_PROTECTED);
	zend_declare_property_long(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_LIMIT_A), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_LIMIT_B), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_HAS_LIMIT), 0, ZEND_ACC_PROTECTED);
}
