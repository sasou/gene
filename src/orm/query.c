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
#include "zend_smart_str.h"

#include "../gene.h"
#include "../db/pdo.h"
#include "orm.h"

zend_class_entry *gene_orm_query_ce;

/* [GENE_FEATURE:2026-08-18 A0] Query v2 — ordered op list.
 * Every chainable method appends one op array [tag, ...] to the `ops`
 * property; terminal methods replay the ops onto the Db handle in call
 * order. Repeated where()/join()/in() ACCUMULATE instead of silently
 * overwriting (the v1 single-slot behaviour dropped conditions — a wrong
 * results bug, worse than an error). String conditions are joined with
 * " AND " generated HERE, because Db::where()/Db::in() append raw text to
 * the WHERE slot without any connector (plan C2). */

/* apply() modes */
#define GENE_ORM_Q_SELECT 0
#define GENE_ORM_Q_COUNT  1
#define GENE_ORM_Q_UPDATE 2
#define GENE_ORM_Q_DELETE 3

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_where_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, where)
	ZEND_ARG_INFO(0, op_or_bind)
	ZEND_ARG_INFO(0, val)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_in_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, in)
	ZEND_ARG_INFO(0, bind)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_str_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_limit_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_join_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, table)
	ZEND_ARG_INFO(0, on)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_fields_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, fields)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_paginate_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_update_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, data, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_selectsub_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, sql)
	ZEND_ARG_INFO(0, alias)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_wherelike_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, col)
	ZEND_ARG_INFO(0, keyword)
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

/* Push one op array onto the ops property. Takes ownership of `op`.
 * Ops are plain zval arrays in an object property (M1): freed with the
 * object, no extra dtor, no C-side allocation state. */
static void gene_orm_query_push(zval *self, zval *op)
{
	zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
	zval copy;

	if (ops && Z_TYPE_P(ops) == IS_ARRAY) {
		ZVAL_COPY(&copy, ops);
		SEPARATE_ARRAY(&copy);
	} else {
		array_init(&copy);
	}
	add_next_index_zval(&copy, op);
	zend_update_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_OPS), &copy);
	zval_ptr_dtor(&copy);
}

static zend_always_inline void gene_orm_query_op_tag(zval *op, const char *tag, size_t len)
{
	zval t;
	ZVAL_STRINGL(&t, tag, len);
	add_next_index_zval(op, &t);
}

static zend_always_inline void gene_orm_query_op_val(zval *op, zval *v)
{
	zval t;
	if (v) {
		ZVAL_COPY(&t, v);
	} else {
		ZVAL_NULL(&t);
	}
	add_next_index_zval(op, &t);
}

static zend_always_inline void gene_orm_query_op_str(zval *op, zend_string *s)
{
	zval t;
	if (s) {
		ZVAL_STR_COPY(&t, s);
	} else {
		ZVAL_NULL(&t);
	}
	add_next_index_zval(op, &t);
}

static zend_bool gene_orm_query_is_empty(zval *self)
{
	zval *e = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_EMPTY), 1, NULL);
	return e && zend_is_true(e);
}

/* Rebuild the db chain from the op list.
 * mode: GENE_ORM_Q_*; data: payload for UPDATE; force_limit overrides any
 * limit op (paginate list phase / first()). */
static int gene_orm_query_apply(zval *self, zval *db, int mode, zval *data,
		zend_bool force_limit, zend_long fl_off, zend_long fl_lim)
{
	zval *table_zv, *fields_zv, *ops_zv, *op;
	zval args[3], retval, merged;
	zend_bool merged_init = 0, where_started = 0;
	zend_bool has_limit = 0;
	int lock_mode = 0; /* 0 none, 1 FOR UPDATE, 2 shared */
	zend_long la = 0, lb = -1; /* lb < 0 → single-arg limit */
	smart_str group_buf = {0}, having_buf = {0}, order_buf = {0};
	int status = FAILURE;

	ZVAL_UNDEF(&merged);

	table_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_TABLE), 1, NULL);
	if (!table_zv || Z_TYPE_P(table_zv) != IS_STRING) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query has no table");
		return FAILURE;
	}

	gene_orm_db_reset(db);

	/* --- verb first: every Db verb entry resets the SQL params itself, and
	 * for UPDATE the SET values must reach the DATA property before any
	 * where bind is appended (bind order = placeholder order). --- */
	switch (mode) {
	case GENE_ORM_Q_COUNT: {
		ZVAL_STR_COPY(&args[0], Z_STR_P(table_zv));
		gene_orm_db_call(db, "count", 1, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&retval);
		break;
	}
	case GENE_ORM_Q_UPDATE:
	case GENE_ORM_Q_DELETE: {
		if (mode == GENE_ORM_Q_UPDATE) {
			ZVAL_STR_COPY(&args[0], Z_STR_P(table_zv));
			ZVAL_COPY(&args[1], data);
			gene_orm_db_call(db, "update", 2, args, &retval);
			zval_ptr_dtor(&args[0]);
			zval_ptr_dtor(&args[1]);
			zval_ptr_dtor(&retval);
		} else {
			ZVAL_STR_COPY(&args[0], Z_STR_P(table_zv));
			gene_orm_db_call(db, "delete", 1, args, &retval);
			zval_ptr_dtor(&args[0]);
			zval_ptr_dtor(&retval);
		}
		break;
	}
	default: {
		/* SELECT — a fields() op overrides the model's $fields projection */
		zval *fields_sel = NULL;
		ops_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (ops_zv && Z_TYPE_P(ops_zv) == IS_ARRAY) {
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops_zv), op) {
				zval *tag;
				if (Z_TYPE_P(op) != IS_ARRAY) {
					continue;
				}
				tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
				if (tag && Z_TYPE_P(tag) == IS_STRING &&
					zend_string_equals_literal(Z_STR_P(tag), "fields")) {
					fields_sel = zend_hash_index_find(Z_ARRVAL_P(op), 1);
				}
			} ZEND_HASH_FOREACH_END();
		}
		if (!fields_sel || Z_TYPE_P(fields_sel) == IS_NULL || Z_TYPE_P(fields_sel) == IS_UNDEF) {
			fields_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
				ZEND_STRL(GENE_ORM_QUERY_FIELDS), 1, NULL);
			fields_sel = fields_zv;
		}
		if (gene_orm_db_select(db, Z_STR_P(table_zv), fields_sel) != SUCCESS) {
			return FAILURE;
		}
		break;
	}
	}
	if (UNEXPECTED(gene_orm_has_exception())) return FAILURE;

	ops_zv = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);

	/* --- pass 1: merge all ARRAY where conditions into a single db->where()
	 * call. Db's makeWhere() does NOT emit a connector when the WHERE slot
	 * already holds text (plan C2), so interleaving string and array wheres
	 * at the Db level would produce "WHERE a=?`b` = ?". Merging is safe:
	 * every connector is AND, so relative order vs string wheres is
	 * semantically neutral. Later ops with the same key overwrite earlier
	 * ones (documented). --- */
	if (ops_zv && Z_TYPE_P(ops_zv) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops_zv), op) {
			zval *tag, *cond;
			zend_string *k;
			zend_ulong idx;
			zval *v;
			if (Z_TYPE_P(op) != IS_ARRAY) {
				continue;
			}
			tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
			cond = zend_hash_index_find(Z_ARRVAL_P(op), 1);
			if (!tag || Z_TYPE_P(tag) != IS_STRING ||
				!zend_string_equals_literal(Z_STR_P(tag), "where") ||
				!cond || Z_TYPE_P(cond) != IS_ARRAY) {
				continue;
			}
			if (!merged_init) {
				array_init(&merged);
				merged_init = 1;
			}
			ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(cond), idx, k, v) {
				zval tmp;
				ZVAL_COPY(&tmp, v);
				if (k) {
					zend_hash_update(Z_ARRVAL(merged), k, &tmp);
				} else {
					zend_hash_index_update(Z_ARRVAL(merged), idx, &tmp);
				}
			} ZEND_HASH_FOREACH_END();
		} ZEND_HASH_FOREACH_END();
	}
	if (merged_init && zend_hash_num_elements(Z_ARRVAL(merged)) > 0) {
		ZVAL_COPY(&args[0], &merged);
		gene_orm_db_call(db, "where", 1, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&retval);
		where_started = 1;
		if (UNEXPECTED(gene_orm_has_exception())) goto out;
	}

	/* --- pass 2: replay ops in call order --- */
	if (ops_zv && Z_TYPE_P(ops_zv) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops_zv), op) {
			zval *tag, *a1, *a2, *a3;
			const char *t;
			if (Z_TYPE_P(op) != IS_ARRAY) {
				continue;
			}
			tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
			if (!tag || Z_TYPE_P(tag) != IS_STRING) {
				continue;
			}
			t = Z_STRVAL_P(tag);
			a1 = zend_hash_index_find(Z_ARRVAL_P(op), 1);
			a2 = zend_hash_index_find(Z_ARRVAL_P(op), 2);
			a3 = zend_hash_index_find(Z_ARRVAL_P(op), 3);

			if (strcmp(t, "where") == 0) {
				if (!a1 || Z_TYPE_P(a1) != IS_STRING || Z_STRLEN_P(a1) == 0) {
					continue; /* array wheres handled in pass 1 */
				}
				if (where_started) {
					smart_str frag = {0};
					smart_str_appends(&frag, " AND ");
					smart_str_appendl(&frag, Z_STRVAL_P(a1), Z_STRLEN_P(a1));
					smart_str_0(&frag);
					ZVAL_STR(&args[0], frag.s);
				} else {
					ZVAL_COPY(&args[0], a1);
				}
				if (a2 && Z_TYPE_P(a2) != IS_NULL && Z_TYPE_P(a2) != IS_UNDEF) {
					ZVAL_COPY(&args[1], a2);
					gene_orm_db_call(db, "where", 2, args, &retval);
					zval_ptr_dtor(&args[1]);
				} else {
					gene_orm_db_call(db, "where", 1, args, &retval);
				}
				zval_ptr_dtor(&args[0]);
				zval_ptr_dtor(&retval);
				where_started = 1;
				if (UNEXPECTED(gene_orm_has_exception())) goto out;
			} else if (strcmp(t, "in") == 0 || strcmp(t, "inraw") == 0) {
				smart_str frag = {0};
				uint32_t argc = 1;
				if (!a1 || Z_TYPE_P(a1) != IS_STRING || Z_STRLEN_P(a1) == 0) {
					continue;
				}
				if (where_started) {
					smart_str_appends(&frag, " AND ");
				}
				smart_str_appendl(&frag, Z_STRVAL_P(a1), Z_STRLEN_P(a1));
				if (strcmp(t, "in") == 0) {
					smart_str_appends(&frag, " in(?)");
				}
				smart_str_0(&frag);
				ZVAL_STR(&args[0], frag.s);
				if (a2 && Z_TYPE_P(a2) != IS_NULL && Z_TYPE_P(a2) != IS_UNDEF) {
					ZVAL_COPY(&args[1], a2);
					argc = 2;
				}
				gene_orm_db_call(db, "in", argc, args, &retval);
				zval_ptr_dtor(&args[0]);
				if (argc == 2) {
					zval_ptr_dtor(&args[1]);
				}
				zval_ptr_dtor(&retval);
				where_started = 1;
				if (UNEXPECTED(gene_orm_has_exception())) goto out;
			} else if (strcmp(t, "join") == 0) {
				if (mode == GENE_ORM_Q_UPDATE || mode == GENE_ORM_Q_DELETE) {
					zend_throw_exception_ex(NULL, 0,
						"Gene\\Orm\\Query: join() is not supported by update()/delete()");
					goto out;
				}
				{
					uint32_t argc = 2;
					ZVAL_COPY(&args[0], a1);
					ZVAL_COPY(&args[1], a2);
					if (a3 && Z_TYPE_P(a3) == IS_STRING && Z_STRLEN_P(a3) > 0) {
						ZVAL_COPY(&args[2], a3);
						argc = 3;
					}
					gene_orm_db_call(db, "join", argc, args, &retval);
					zval_ptr_dtor(&args[0]);
					zval_ptr_dtor(&args[1]);
					if (argc == 3) {
						zval_ptr_dtor(&args[2]);
					}
					zval_ptr_dtor(&retval);
					if (UNEXPECTED(gene_orm_has_exception())) goto out;
				}
			} else if (strcmp(t, "group") == 0) {
				if (a1 && Z_TYPE_P(a1) == IS_STRING && Z_STRLEN_P(a1) > 0) {
					/* [GENE_FIX:2026-08-19 P2-5] count over GROUP BY makes
					 * cell() return the FIRST GROUP's row count — a silently
					 * wrong number (count=3 vs list of 2 groups, verified by
					 * audit/repro/group_count_semantics.php). Refuse loudly;
					 * callers keep the explicit count()+all() two-step. */
					if (mode == GENE_ORM_Q_COUNT) {
						zend_throw_exception_ex(NULL, 0,
							"Gene\\Orm\\Query: group() cannot be combined with count()/paginate(); "
							"use count() and all() as separate steps");
						goto out;
					}
					if (group_buf.s && ZSTR_LEN(group_buf.s) > 0) {
						smart_str_appends(&group_buf, ", ");
					}
					smart_str_appendl(&group_buf, Z_STRVAL_P(a1), Z_STRLEN_P(a1));
				}
			} else if (strcmp(t, "having") == 0) {
				if (a1 && Z_TYPE_P(a1) == IS_STRING && Z_STRLEN_P(a1) > 0) {
					if (having_buf.s && ZSTR_LEN(having_buf.s) > 0) {
						smart_str_appends(&having_buf, " AND ");
					}
					smart_str_appendl(&having_buf, Z_STRVAL_P(a1), Z_STRLEN_P(a1));
				}
			} else if (strcmp(t, "order") == 0) {
				if (a1 && Z_TYPE_P(a1) == IS_STRING && Z_STRLEN_P(a1) > 0) {
					if (order_buf.s && ZSTR_LEN(order_buf.s) > 0) {
						smart_str_appends(&order_buf, ", ");
					}
					smart_str_appendl(&order_buf, Z_STRVAL_P(a1), Z_STRLEN_P(a1));
				}
			} else if (strcmp(t, "limit") == 0) {
				la = (a1 && Z_TYPE_P(a1) == IS_LONG) ? Z_LVAL_P(a1) : 0;
				lb = (a2 && Z_TYPE_P(a2) == IS_LONG) ? Z_LVAL_P(a2) : -1;
				has_limit = 1;
			} else if (strcmp(t, "lock") == 0) {
				if (a1 && Z_TYPE_P(a1) == IS_STRING &&
					zend_string_equals_literal(Z_STR_P(a1), "share")) {
					lock_mode = 2;
				} else {
					lock_mode = 1;
				}
			} else if (strcmp(t, "selectsub") == 0) {
				if (mode == GENE_ORM_Q_SELECT && a1 && a2 &&
					Z_TYPE_P(a1) == IS_STRING && Z_TYPE_P(a2) == IS_STRING) {
					gene_orm_db_select_sub(db, Z_STR_P(a1), Z_STR_P(a2));
					if (UNEXPECTED(gene_orm_has_exception())) goto out;
				}
			}
			/* "fields" handled by the verb */
		} ZEND_HASH_FOREACH_END();
	}

	/* --- [GENE_FIX:2026-08-19 P0-2] semantic write guard ---
	 * The previous pre-check only looked at op TAGS: a where([]) (dropped
	 * by pass 1's non-empty test) or where('') (skipped by pass 2) still
	 * carried a "where" tag and passed — producing an unscoped
	 * UPDATE/DELETE (full-table rewrite, verified by
	 * audit/repro/guard_unscoped_write.php). Decide HERE, on where_started,
	 * i.e. on what the generated SQL actually contains, so the guard can
	 * never drift from the generator. The in([]) emptyResult early-exit in
	 * the terminal methods runs BEFORE apply(), so a safe no-op does not
	 * turn into an exception. The verb call above only BUILT sql (lazy);
	 * nothing has executed yet. */
	if ((mode == GENE_ORM_Q_UPDATE || mode == GENE_ORM_Q_DELETE) && !where_started) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Query::%s() requires at least one effective where()/in() condition",
			mode == GENE_ORM_Q_UPDATE ? "update" : "delete");
		goto out;
	}

	/* --- flush accumulated group/having/order/limit/lock ---
	 * smart_str_0() first: Db::order() hands the char* to gene_quote_order()
	 * which strlen()s it — an unterminated smart_str buffer reads stale heap
	 * past ZSTR_LEN (observed as garbage tail like "id desc`status` = ?"). */
	if (group_buf.s && ZSTR_LEN(group_buf.s) > 0) {
		smart_str_0(&group_buf);
		ZVAL_STR(&args[0], group_buf.s);
		gene_orm_db_call(db, "group", 1, args, &retval);
		/* args[0] borrows group_buf.s — do NOT dtor it, freed below */
		zval_ptr_dtor(&retval);
		if (UNEXPECTED(gene_orm_has_exception())) goto out;
	}
	if (having_buf.s && ZSTR_LEN(having_buf.s) > 0) {
		smart_str_0(&having_buf);
		ZVAL_STR(&args[0], having_buf.s);
		gene_orm_db_call(db, "having", 1, args, &retval);
		zval_ptr_dtor(&retval);
		if (UNEXPECTED(gene_orm_has_exception())) goto out;
	}
	if (mode != GENE_ORM_Q_COUNT) {
		if (order_buf.s && ZSTR_LEN(order_buf.s) > 0) {
			smart_str_0(&order_buf);
			ZVAL_STR(&args[0], order_buf.s);
			gene_orm_db_call(db, "order", 1, args, &retval);
			zval_ptr_dtor(&retval);
			if (UNEXPECTED(gene_orm_has_exception())) goto out;
		}
		if (force_limit) {
			gene_orm_db_limit(db, fl_off, fl_lim);
			if (UNEXPECTED(gene_orm_has_exception())) goto out;
		} else if (has_limit) {
			if (lb >= 0) {
				/* (offset, count) — driver-aware via gene_orm_db_limit */
				gene_orm_db_limit(db, la, lb);
			} else {
				ZVAL_LONG(&args[0], la);
				gene_orm_db_call(db, "limit", 1, args, &retval);
				zval_ptr_dtor(&retval);
			}
			if (UNEXPECTED(gene_orm_has_exception())) goto out;
		}
		if (mode == GENE_ORM_Q_SELECT && lock_mode) {
			gene_orm_db_call(db, lock_mode == 1 ? "lockForUpdate" : "sharedLock",
				0, NULL, &retval);
			zval_ptr_dtor(&retval);
			if (UNEXPECTED(gene_orm_has_exception())) goto out;
		}
	}

	status = SUCCESS;

out:
	smart_str_free(&group_buf);
	smart_str_free(&having_buf);
	smart_str_free(&order_buf);
	if (merged_init) {
		zval_ptr_dtor(&merged);
	}
	if (status == SUCCESS) {
		gene_orm_query_mark_dirty(self, 1);
	}
	return status;
}

static void gene_orm_query_finish(zval *self, zval *db)
{
	gene_orm_db_reset(db);
	gene_orm_query_mark_dirty(self, 0);
}

int gene_orm_query_init(zval *query, zval *db, zend_string *table, zval *fields)
{
	zval ops;

	object_init_ex(query, gene_orm_query_ce);
	/* zend_update_property ZVAL_COPY's the value; callers pass an owned copy
	 * obtained from gene_orm_get_db() and zval_ptr_dtor() it after init
	 * (see [GENE_FIX:2026-08-10 N1] in meta.c). */
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
	array_init(&ops);
	zend_update_property(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_OPS), &ops);
	zval_ptr_dtor(&ops);
	zend_update_property_bool(gene_orm_query_ce, gene_strip_obj(query),
		ZEND_STRL(GENE_ORM_QUERY_EMPTY), 0);
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

/*
 * {{{ where($cond, $bind) | where(array $cond) | where($col, $op, $val)
 * 1-arg/2-arg forms keep v1 semantics (raw fragment or assoc array).
 * 3-arg form is the comparison-operator shorthand: $op is whitelisted
 * (> >= < <= != =) and $col must pass the identifier whitelist — anything
 * else throws instead of being spliced into SQL. */
PHP_METHOD(gene_orm_query, where)
{
	zval *self = getThis(), *where = NULL, *bind = NULL, *val = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|zz", &where, &bind, &val) == FAILURE) {
		return;
	}
	if (ZEND_NUM_ARGS() >= 3) {
		static const char *allowed[] = { ">", ">=", "<", "<=", "!=", "=" };
		size_t i;
		zend_bool ok = 0;
		if (Z_TYPE_P(where) != IS_STRING || !gene_orm_valid_ident(Z_STR_P(where))) {
			zend_throw_exception_ex(NULL, 0,
				"Gene\\Orm\\Query::where() 3-arg form expects a plain column name");
			RETURN_NULL();
		}
		if (Z_TYPE_P(bind) == IS_STRING) {
			for (i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
				/* NOT zend_string_equals_literal: allowed[i] is a char*
				 * variable, and that macro sizeof()s its argument (would
				 * compare against sizeof(char*)-1 = 7 bytes). */
				if (strcmp(Z_STRVAL_P(bind), allowed[i]) == 0) {
					ok = 1;
					break;
				}
			}
		}
		if (!ok) {
			zend_throw_exception_ex(NULL, 0,
				"Gene\\Orm\\Query::where() operator must be one of > >= < <= != =");
			RETURN_NULL();
		}
		{
			smart_str frag = {0};
			smart_str_appendl(&frag, Z_STRVAL_P(where), Z_STRLEN_P(where));
			smart_str_appendc(&frag, ' ');
			smart_str_appendl(&frag, Z_STRVAL_P(bind), Z_STRLEN_P(bind));
			smart_str_appends(&frag, " ?");
			smart_str_0(&frag);
			array_init_size(&op, 3);
			gene_orm_query_op_tag(&op, ZEND_STRL("where"));
			{
				zval t;
				ZVAL_STR(&t, frag.s);
				add_next_index_zval(&op, &t);
			}
			gene_orm_query_op_val(&op, val);
			gene_orm_query_push(self, &op);
		}
		RETURN_ZVAL(self, 1, 0);
	}
	array_init_size(&op, 3);
	gene_orm_query_op_tag(&op, ZEND_STRL("where"));
	gene_orm_query_op_val(&op, where);
	gene_orm_query_op_val(&op, bind);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

/*
 * {{{ in($col, array $ids) | in($sql, $bind)
 * Column form (identifier + array) expands to "col in(?)" for Db::in().
 * An EMPTY array sets the emptyResult latch: terminal methods then return
 * an empty result without SQL — never "IN ()", never a dropped condition
 * (a silent unconditional query here would be a data-leak bug).
 * The legacy marker form ("id in(?)", $bind) passes through unchanged. */
PHP_METHOD(gene_orm_query, in)
{
	zval *self = getThis(), *bind = NULL;
	zend_string *in = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|z", &in, &bind) == FAILURE) {
		return;
	}
	if (gene_orm_valid_ident(in)) {
		uint32_t n;
		if (!bind || Z_TYPE_P(bind) != IS_ARRAY) {
			zend_throw_exception_ex(NULL, 0,
				"Gene\\Orm\\Query::in(column) expects an array of values");
			RETURN_NULL();
		}
		n = zend_hash_num_elements(Z_ARRVAL_P(bind));
		if (n == 0) {
			zend_update_property_bool(gene_orm_query_ce, gene_strip_obj(self),
				ZEND_STRL(GENE_ORM_QUERY_EMPTY), 1);
			RETURN_ZVAL(self, 1, 0);
		}
		if (n > 1000) {
			php_error_docref(NULL, E_NOTICE,
				"Gene\\Orm\\Query::in() with %u placeholders; consider chunking "
				"(max_prepared_stmt_count / packet limits)", n);
		}
		/* column form: [tag, col, ids] */
		array_init_size(&op, 3);
		gene_orm_query_op_tag(&op, ZEND_STRL("in"));
		gene_orm_query_op_str(&op, in);
		gene_orm_query_op_val(&op, bind);
		gene_orm_query_push(self, &op);
		RETURN_ZVAL(self, 1, 0);
	}
	/* legacy raw marker form */
	array_init_size(&op, 3);
	gene_orm_query_op_tag(&op, ZEND_STRL("inraw"));
	gene_orm_query_op_str(&op, in);
	gene_orm_query_op_val(&op, bind);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, join)
{
	zval *self = getThis(), *on = NULL;
	zend_string *table = NULL, *type = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz|S", &table, &on, &type) == FAILURE) {
		return;
	}
	array_init_size(&op, 4);
	gene_orm_query_op_tag(&op, ZEND_STRL("join"));
	gene_orm_query_op_str(&op, table);
	gene_orm_query_op_val(&op, on);
	gene_orm_query_op_str(&op, type);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, group)
{
	zval *self = getThis();
	zend_string *group = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &group) == FAILURE) {
		return;
	}
	array_init_size(&op, 2);
	gene_orm_query_op_tag(&op, ZEND_STRL("group"));
	gene_orm_query_op_str(&op, group);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, having)
{
	zval *self = getThis();
	zend_string *having = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &having) == FAILURE) {
		return;
	}
	array_init_size(&op, 2);
	gene_orm_query_op_tag(&op, ZEND_STRL("having"));
	gene_orm_query_op_str(&op, having);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, order)
{
	zval *self = getThis();
	zend_string *order = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &order) == FAILURE) {
		return;
	}
	array_init_size(&op, 2);
	gene_orm_query_op_tag(&op, ZEND_STRL("order"));
	gene_orm_query_op_str(&op, order);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, limit)
{
	zval *self = getThis();
	zend_long num, limit = 0;
	zval op, t;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l", &num, &limit) == FAILURE) {
		return;
	}
	/* One arg: take num rows. Two args: (offset, count) like MySQL LIMIT a,b
	 * and Model::paginate — apply() routes via gene_orm_db_limit. */
	array_init_size(&op, 3);
	gene_orm_query_op_tag(&op, ZEND_STRL("limit"));
	ZVAL_LONG(&t, num);
	add_next_index_zval(&op, &t);
	if (ZEND_NUM_ARGS() > 1) {
		ZVAL_LONG(&t, limit);
	} else {
		ZVAL_NULL(&t);
	}
	add_next_index_zval(&op, &t);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

/* {{{ fields($fields) — per-query projection override (array|string) */
PHP_METHOD(gene_orm_query, fields)
{
	zval *self = getThis(), *fields = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &fields) == FAILURE) {
		return;
	}
	array_init_size(&op, 2);
	gene_orm_query_op_tag(&op, ZEND_STRL("fields"));
	gene_orm_query_op_val(&op, fields);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/* {{{ lockForUpdate() / sharedLock() — select mode only; the Db driver
 * decides the actual syntax (mysql/pgsql) or degrades (sqlite no-op with
 * E_NOTICE, mssql throws). Locks belong inside a transaction — the driver
 * emits E_NOTICE when inTransaction() is false. */
PHP_METHOD(gene_orm_query, lockForUpdate)
{
	zval *self = getThis();
	zval op, t;

	array_init_size(&op, 2);
	gene_orm_query_op_tag(&op, ZEND_STRL("lock"));
	ZVAL_STRING(&t, "update");
	add_next_index_zval(&op, &t);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, sharedLock)
{
	zval *self = getThis();
	zval op, t;

	array_init_size(&op, 2);
	gene_orm_query_op_tag(&op, ZEND_STRL("lock"));
	ZVAL_STRING(&t, "share");
	add_next_index_zval(&op, &t);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/* {{{ selectSub($sql, $alias) — correlated/scalar subquery as an extra
 * select column. $sql is developer-written (Db::sql() trust level); alias
 * must pass the identifier whitelist. */
PHP_METHOD(gene_orm_query, selectSub)
{
	zval *self = getThis();
	zend_string *sql = NULL, *alias = NULL;
	zval op;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &sql, &alias) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(sql) == 0 || !gene_orm_valid_ident(alias)) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Query::selectSub() expects non-empty sql and a plain alias");
		RETURN_NULL();
	}
	array_init_size(&op, 3);
	gene_orm_query_op_tag(&op, ZEND_STRL("selectsub"));
	gene_orm_query_op_str(&op, sql);
	gene_orm_query_op_str(&op, alias);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/* {{{ whereLike($col, $keyword) — LIKE with %/_/\ escaped and wrapped in
 * %...%. Use this instead of hand-escaping; if the business already
 * escaped the pattern (e.g. a custom escapeLikePattern), do NOT pass the
 * result here — that would double-escape. */
PHP_METHOD(gene_orm_query, whereLike)
{
	zval *self = getThis();
	zend_string *col = NULL, *keyword = NULL;
	smart_str frag = {0}, esc = {0};
	size_t i;
	zval op, t;
	zval *db;
	int kind;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "SS", &col, &keyword) == FAILURE) {
		return;
	}
	if (!gene_orm_valid_ident(col)) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Query::whereLike() expects a plain column name");
		RETURN_NULL();
	}
	for (i = 0; i < ZSTR_LEN(keyword); i++) {
		char c = ZSTR_VAL(keyword)[i];
		if (c == '\\' || c == '%' || c == '_') {
			smart_str_appendc(&esc, '\\');
		}
		smart_str_appendc(&esc, c);
	}
	smart_str_0(&esc);
	smart_str_appendl(&frag, ZSTR_VAL(col), ZSTR_LEN(col));
	smart_str_appends(&frag, " LIKE ? ESCAPE '");
	/* MySQL string literals interpret backslash escapes (needs '\\');
	 * sqlite/pgsql/mssql treat them verbatim (need '\'). */
	db = gene_orm_query_db(self);
	kind = db ? gene_orm_db_kind(db) : GENE_ORM_DB_UNKNOWN;
	if (!db) {
		smart_str_free(&esc);
		smart_str_free(&frag);
		RETURN_NULL();
	}
	smart_str_appends(&frag, kind == GENE_ORM_DB_MYSQL ? "\\\\'" : "\\'");
	smart_str_0(&frag);

	array_init_size(&op, 3);
	gene_orm_query_op_tag(&op, ZEND_STRL("where"));
	ZVAL_STR(&t, frag.s);
	add_next_index_zval(&op, &t);
	{
		zval v;
		smart_str vwrapped = {0};
		smart_str_appendc(&vwrapped, '%');
		if (esc.s) {
			smart_str_appendl(&vwrapped, ZSTR_VAL(esc.s), ZSTR_LEN(esc.s));
		}
		smart_str_appendc(&vwrapped, '%');
		smart_str_0(&vwrapped);
		ZVAL_STR(&v, vwrapped.s);
		add_next_index_zval(&op, &v);
	}
	smart_str_free(&esc);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/* {{{ terminals --------------------------------------------------------- */

PHP_METHOD(gene_orm_query, all)
{
	zval *self = getThis();
	zval *db, retval;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_NULL();
	}
	if (gene_orm_query_is_empty(self)) {
		gene_orm_query_finish(self, db);
		array_init(return_value);
		return;
	}
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_SELECT, NULL, 0, 0, 0) != SUCCESS) {
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
	if (gene_orm_query_is_empty(self)) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_SELECT, NULL, 0, 0, 0) != SUCCESS) {
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

/* {{{ first() — limit(1) + row(), without mutating the op list */
PHP_METHOD(gene_orm_query, first)
{
	zval *self = getThis();
	zval *db, retval;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_NULL();
	}
	if (gene_orm_query_is_empty(self)) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_SELECT, NULL, 1, 0, 1) != SUCCESS) {
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
/* }}} */

PHP_METHOD(gene_orm_query, cell)
{
	zval *self = getThis();
	zval *db, retval;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_NULL();
	}
	if (gene_orm_query_is_empty(self)) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_SELECT, NULL, 0, 0, 0) != SUCCESS) {
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
	if (gene_orm_query_is_empty(self)) {
		gene_orm_query_finish(self, db);
		RETURN_LONG(0);
	}
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_COUNT, NULL, 0, 0, 0) != SUCCESS) {
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

/* {{{ paginate($offset, $limit) → {count, list}
 * Single-table guarantee (plan 3.1): with JOINs the count phase replays the
 * same joins (count over joined rows). Callers needing a DISTINCT/main-table
 * count should keep the explicit count() + all() two-step. The order op is
 * inherited by the list phase only; count never emits ORDER BY. */
PHP_METHOD(gene_orm_query, paginate)
{
	zval *self = getThis();
	zend_long offset = 0, limit = 10;
	zval *db, retval, list_zv;
	zend_long count_val = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ll", &offset, &limit) == FAILURE) {
		return;
	}
	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_NULL();
	}
	if (gene_orm_query_is_empty(self)) {
		zval empty_list;
		gene_orm_query_finish(self, db);
		array_init(&empty_list);
		array_init(return_value);
		add_assoc_long_ex(return_value, ZEND_STRL("count"), 0);
		add_assoc_zval_ex(return_value, ZEND_STRL("list"), &empty_list);
		return;
	}

	/* count phase (no order/limit/lock) */
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_COUNT, NULL, 0, 0, 0) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	if (gene_orm_db_call(db, "cell", 0, NULL, &retval) == SUCCESS) {
		if (Z_TYPE(retval) == IS_LONG) {
			count_val = Z_LVAL(retval);
		} else if (Z_TYPE(retval) == IS_STRING) {
			count_val = zend_atol(Z_STRVAL(retval), Z_STRLEN(retval));
		} else if (Z_TYPE(retval) == IS_DOUBLE) {
			count_val = (zend_long)Z_DVAL(retval);
		} else {
			count_val = zval_get_long(&retval);
		}
		zval_ptr_dtor(&retval);
	}
	if (UNEXPECTED(gene_orm_has_exception())) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	gene_orm_query_finish(self, db);

	/* list phase (forced limit) */
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_SELECT, NULL, 1, offset, limit) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	/* [GENE_FIX:2026-08-19 P2-6] Fall back to an empty list whenever all()
	 * did not yield an ARRAY — a driver error path can return SUCCESS with
	 * false, which would break the {count:int, list:array} contract and
	 * fatal the caller's foreach. */
	if (gene_orm_db_call(db, "all", 0, NULL, &list_zv) != SUCCESS || Z_TYPE(list_zv) != IS_ARRAY) {
		if (!Z_ISUNDEF(list_zv)) {
			zval_ptr_dtor(&list_zv);
		}
		array_init(&list_zv);
	}
	gene_orm_query_finish(self, db);

	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL("count"), count_val);
	add_assoc_zval_ex(return_value, ZEND_STRL("list"), &list_zv);
}
/* }}} */

/* {{{ update(array $data) / delete() — immediate execution, symmetric with
 * Model::updateBy()/destroy(). Both REQUIRE at least one where/in condition
 * (a full-table write from a chainable builder is a foot-gun, not a
 * feature). join() is rejected during apply for these modes. */
PHP_METHOD(gene_orm_query, update)
{
	zval *self = getThis(), *data = NULL;
	zval *db, retval;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &data) == FAILURE) {
		return;
	}
	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_LONG(0);
	}
	if (gene_orm_query_is_empty(self)) {
		gene_orm_query_finish(self, db);
		RETURN_LONG(0);
	}
	/* The write guard lives INSIDE apply() (P0-2): it judges where_started
	 * after replay, i.e. the SQL that would actually run. */
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_UPDATE, data, 0, 0, 0) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_LONG(0);
	}
	if (gene_orm_db_call(db, "affectedRows", 0, NULL, &retval) == SUCCESS) {
		gene_orm_query_finish(self, db);
		if (Z_ISUNDEF(retval)) {
			RETURN_LONG(0);
		}
		RETURN_ZVAL(&retval, 0, 1);
	}
	gene_orm_query_finish(self, db);
	RETURN_LONG(0);
}

PHP_METHOD(gene_orm_query, delete)
{
	zval *self = getThis();
	zval *db, retval;

	db = gene_orm_query_db(self);
	if (!db) {
		RETURN_LONG(0);
	}
	if (gene_orm_query_is_empty(self)) {
		gene_orm_query_finish(self, db);
		RETURN_LONG(0);
	}
	/* Guard inside apply() — see update() / P0-2. */
	if (gene_orm_query_apply(self, db, GENE_ORM_Q_DELETE, NULL, 0, 0, 0) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_LONG(0);
	}
	if (gene_orm_db_call(db, "affectedRows", 0, NULL, &retval) == SUCCESS) {
		gene_orm_query_finish(self, db);
		if (Z_ISUNDEF(retval)) {
			RETURN_LONG(0);
		}
		RETURN_ZVAL(&retval, 0, 1);
	}
	gene_orm_query_finish(self, db);
	RETURN_LONG(0);
}
/* }}} */

/* }}} */

const zend_function_entry gene_orm_query_methods[] = {
	PHP_ME(gene_orm_query, __construct, gene_orm_query_void_arginfo, ZEND_ACC_PRIVATE)
	PHP_ME(gene_orm_query, __destruct, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, where, gene_orm_query_where_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, in, gene_orm_query_in_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, join, gene_orm_query_join_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, group, gene_orm_query_str_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, having, gene_orm_query_str_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, order, gene_orm_query_str_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, limit, gene_orm_query_limit_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, fields, gene_orm_query_fields_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, lockForUpdate, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, sharedLock, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, selectSub, gene_orm_query_selectsub_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, whereLike, gene_orm_query_wherelike_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, all, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, row, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, first, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, cell, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, count, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, paginate, gene_orm_query_paginate_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, update, gene_orm_query_update_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, delete, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
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
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_OPS), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_EMPTY), 0, ZEND_ACC_PROTECTED);
}
