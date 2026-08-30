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
#include <math.h>

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
#define GENE_ORM_Q_INCREMENT 4
#define GENE_ORM_Q_DECREMENT 5

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

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_arithmetic_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, column)
	ZEND_ARG_INFO(0, amount)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_query_union_arginfo, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, query, Gene\\Orm\\Query, 0)
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

static zend_bool gene_orm_query_ops_has(zval *ops, const char *name);

static zend_bool gene_orm_query_is_empty(zval *self)
{
	zval *e = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_EMPTY), 1, NULL);
	zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
	return e && zend_is_true(e) && !gene_orm_query_ops_has(ops, "union");
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
	case GENE_ORM_Q_DELETE:
	case GENE_ORM_Q_INCREMENT:
	case GENE_ORM_Q_DECREMENT: {
		ZVAL_STR_COPY(&args[0], Z_STR_P(table_zv));
		if (mode == GENE_ORM_Q_UPDATE) {
			ZVAL_COPY(&args[1], data);
			gene_orm_db_call(db, "update", 2, args, &retval);
			zval_ptr_dtor(&args[1]);
		} else if (mode == GENE_ORM_Q_DELETE) {
			gene_orm_db_call(db, "delete", 1, args, &retval);
		} else {
			zval *column = zend_hash_index_find(Z_ARRVAL_P(data), 0);
			zval *amount = zend_hash_index_find(Z_ARRVAL_P(data), 1);
			ZVAL_COPY(&args[1], column);
			ZVAL_COPY(&args[2], amount);
			gene_orm_db_call(db, mode == GENE_ORM_Q_INCREMENT ? "increment" : "decrement", 3, args, &retval);
			zval_ptr_dtor(&args[1]);
			zval_ptr_dtor(&args[2]);
		}
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&retval);
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
	if (gene_orm_query_ops_has(ops_zv, "union") && mode >= GENE_ORM_Q_UPDATE) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union cannot be combined with write operations");
		return FAILURE;
	}
	if (gene_orm_query_ops_has(ops_zv, "lock") && mode >= GENE_ORM_Q_UPDATE) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query locks cannot be combined with write operations");
		return FAILURE;
	}

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
	if (ops_zv && Z_TYPE_P(ops_zv) == IS_ARRAY) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops_zv), op) {
			zval *tag, *a1, *a2, *a3;
			uint32_t argc = 2;
			if (Z_TYPE_P(op) != IS_ARRAY) continue;
			tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
			if (!tag || Z_TYPE_P(tag) != IS_STRING ||
				(!zend_string_equals_literal(Z_STR_P(tag), "join") && !zend_string_equals_literal(Z_STR_P(tag), "joinon"))) continue;
			if (mode == GENE_ORM_Q_UPDATE || mode == GENE_ORM_Q_DELETE || mode == GENE_ORM_Q_INCREMENT || mode == GENE_ORM_Q_DECREMENT) {
				zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query: joins are not supported by write operations");
				goto out;
			}
			a1 = zend_hash_index_find(Z_ARRVAL_P(op), 1);
			a2 = zend_hash_index_find(Z_ARRVAL_P(op), 2);
			a3 = zend_hash_index_find(Z_ARRVAL_P(op), 3);
			ZVAL_COPY(&args[0], a1);
			ZVAL_COPY(&args[1], a2);
			if (a3 && Z_TYPE_P(a3) == IS_STRING && Z_STRLEN_P(a3) > 0) {
				ZVAL_COPY(&args[2], a3);
				argc = 3;
			}
			gene_orm_db_call(db, zend_string_equals_literal(Z_STR_P(tag), "joinon") ? "joinOn" : "join", argc, args, &retval);
			zval_ptr_dtor(&args[0]);
			zval_ptr_dtor(&args[1]);
			if (argc == 3) zval_ptr_dtor(&args[2]);
			zval_ptr_dtor(&retval);
			if (UNEXPECTED(gene_orm_has_exception())) goto out;
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
			} else if (strcmp(t, "join") == 0 || strcmp(t, "joinon") == 0) {
				continue;
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
	if ((mode == GENE_ORM_Q_UPDATE || mode == GENE_ORM_Q_DELETE || mode == GENE_ORM_Q_INCREMENT || mode == GENE_ORM_Q_DECREMENT) && !where_started) {
		const char *method = mode == GENE_ORM_Q_UPDATE ? "update" : mode == GENE_ORM_Q_DELETE ? "delete" : mode == GENE_ORM_Q_INCREMENT ? "increment" : "decrement";
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Query::%s() requires at least one effective where()/in() condition", method);
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

static zend_bool gene_orm_query_ops_has(zval *ops, const char *name)
{
	zval *op;
	if (!ops || Z_TYPE_P(ops) != IS_ARRAY) return 0;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops), op) {
		zval *tag;
		if (Z_TYPE_P(op) != IS_ARRAY) continue;
		tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
		if (tag && Z_TYPE_P(tag) == IS_STRING && strcmp(Z_STRVAL_P(tag), name) == 0) return 1;
	} ZEND_HASH_FOREACH_END();
	return 0;
}

static int gene_orm_query_snapshot(zval *query, zval *snapshot)
{
	zval *value, copy;
	array_init_size(snapshot, 5);
	value = zend_read_property(gene_orm_query_ce, gene_strip_obj(query), ZEND_STRL(GENE_ORM_QUERY_TABLE), 1, NULL);
	ZVAL_COPY(&copy, value); add_assoc_zval_ex(snapshot, ZEND_STRL("table"), &copy);
	value = zend_read_property(gene_orm_query_ce, gene_strip_obj(query), ZEND_STRL(GENE_ORM_QUERY_FIELDS), 1, NULL);
	ZVAL_DUP(&copy, value); add_assoc_zval_ex(snapshot, ZEND_STRL("fields"), &copy);
	value = zend_read_property(gene_orm_query_ce, gene_strip_obj(query), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
	ZVAL_DUP(&copy, value); add_assoc_zval_ex(snapshot, ZEND_STRL("ops"), &copy);
	value = zend_read_property(gene_orm_query_ce, gene_strip_obj(query), ZEND_STRL(GENE_ORM_QUERY_EMPTY), 1, NULL);
	add_assoc_bool_ex(snapshot, ZEND_STRL("empty"), value && zend_is_true(value));
	add_assoc_long_ex(snapshot, ZEND_STRL("origin"), (zend_long) Z_OBJ_HANDLE_P(query));
	return SUCCESS;
}

static zend_bool gene_orm_snapshot_contains(zval *snapshot, zend_long origin)
{
	zval *own, *ops, *op;
	own = zend_hash_str_find(Z_ARRVAL_P(snapshot), ZEND_STRL("origin"));
	if (own && Z_TYPE_P(own) == IS_LONG && Z_LVAL_P(own) == origin) return 1;
	ops = zend_hash_str_find(Z_ARRVAL_P(snapshot), ZEND_STRL("ops"));
	if (!ops || Z_TYPE_P(ops) != IS_ARRAY) return 0;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops), op) {
		zval *tag, *child;
		if (Z_TYPE_P(op) != IS_ARRAY) continue;
		tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
		child = zend_hash_index_find(Z_ARRVAL_P(op), 2);
		if (tag && Z_TYPE_P(tag) == IS_STRING && !strcmp(Z_STRVAL_P(tag), "union") && child && Z_TYPE_P(child) == IS_ARRAY && gene_orm_snapshot_contains(child, origin)) return 1;
	} ZEND_HASH_FOREACH_END();
	return 0;
}

static int gene_orm_snapshot_depth(zval *snapshot)
{
	zval *ops, *op;
	int max = 1;
	ops = zend_hash_str_find(Z_ARRVAL_P(snapshot), ZEND_STRL("ops"));
	if (!ops || Z_TYPE_P(ops) != IS_ARRAY) return max;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops), op) {
		zval *tag, *child;
		int depth;
		if (Z_TYPE_P(op) != IS_ARRAY) continue;
		tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
		child = zend_hash_index_find(Z_ARRVAL_P(op), 2);
		if (!tag || Z_TYPE_P(tag) != IS_STRING || strcmp(Z_STRVAL_P(tag), "union") || !child || Z_TYPE_P(child) != IS_ARRAY) continue;
		depth = 1 + gene_orm_snapshot_depth(child);
		if (depth > max) max = depth;
	} ZEND_HASH_FOREACH_END();
	return max;
}

static int gene_orm_clone_db(zval *db, zval *clone)
{
	zend_object *obj;
	if (!Z_OBJ_HT_P(db)->clone_obj) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query db handle cannot be cloned for read-only compilation");
		return FAILURE;
	}
	obj = Z_OBJ_HT_P(db)->clone_obj(Z_OBJ_P(db));
	if (!obj) return FAILURE;
	ZVAL_OBJ(clone, obj);
	zend_update_property_null(Z_OBJCE_P(clone), gene_strip_obj(clone), ZEND_STRL("pdo"));
	zend_update_property_null(Z_OBJCE_P(clone), gene_strip_obj(clone), ZEND_STRL("pool"));
	return SUCCESS;
}

static void gene_orm_params_append(zval *target, zval *source)
{
	zval *value;
	if (!source || Z_TYPE_P(source) != IS_ARRAY) return;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(source), value) {
		zval copy;
		ZVAL_COPY(&copy, value);
		add_next_index_zval(target, &copy);
	} ZEND_HASH_FOREACH_END();
}

static int gene_orm_query_compile_snapshot(zval *snapshot, zval *db, zend_bool strip_outer, zend_bool force_limit, zend_long offset, zend_long limit, zval *compiled)
{
	zval *table, *fields, *ops, *op, filtered, q, clone, printed;
	zval sql_zv, params_zv;
	zend_bool has_union, outer_stage;
	smart_str compound = {0};
	int status = FAILURE;
	ZVAL_UNDEF(&printed);
	ZVAL_UNDEF(&params_zv);
	table = zend_hash_str_find(Z_ARRVAL_P(snapshot), ZEND_STRL("table"));
	fields = zend_hash_str_find(Z_ARRVAL_P(snapshot), ZEND_STRL("fields"));
	ops = zend_hash_str_find(Z_ARRVAL_P(snapshot), ZEND_STRL("ops"));
	if (!table || Z_TYPE_P(table) != IS_STRING || !ops || Z_TYPE_P(ops) != IS_ARRAY) return FAILURE;
	has_union = gene_orm_query_ops_has(ops, "union");
	outer_stage = has_union || strip_outer || force_limit;
	array_init(&filtered);
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops), op) {
		zval *tag, copy;
		if (Z_TYPE_P(op) != IS_ARRAY) continue;
		tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
		if (!tag || Z_TYPE_P(tag) != IS_STRING) continue;
		if (!strcmp(Z_STRVAL_P(tag), "union")) continue;
		if (outer_stage && (!strcmp(Z_STRVAL_P(tag), "order") || !strcmp(Z_STRVAL_P(tag), "limit") || !strcmp(Z_STRVAL_P(tag), "lock"))) continue;
		ZVAL_DUP(&copy, op);
		add_next_index_zval(&filtered, &copy);
	} ZEND_HASH_FOREACH_END();
	{
		zval *empty = zend_hash_str_find(Z_ARRVAL_P(snapshot), ZEND_STRL("empty"));
		if (empty && zend_is_true(empty)) {
			zval empty_op, value;
			array_init_size(&empty_op, 3);
			gene_orm_query_op_tag(&empty_op, ZEND_STRL("where"));
			ZVAL_STRING(&value, "1=0"); add_next_index_zval(&empty_op, &value);
			ZVAL_NULL(&value); add_next_index_zval(&empty_op, &value);
			add_next_index_zval(&filtered, &empty_op);
		}
	}
	if (gene_orm_clone_db(db, &clone) != SUCCESS) goto out;
	gene_orm_query_init(&q, &clone, Z_STR_P(table), fields, NULL);
	zend_update_property(gene_orm_query_ce, gene_strip_obj(&q), ZEND_STRL(GENE_ORM_QUERY_OPS), &filtered);
	if (gene_orm_query_apply(&q, &clone, GENE_ORM_Q_SELECT, NULL, 0, 0, 0) != SUCCESS) goto compiled_out;
	ZVAL_UNDEF(&printed);
	if (gene_orm_db_call(&clone, "print", 0, NULL, &printed) != SUCCESS || Z_TYPE(printed) != IS_ARRAY) goto compiled_out;
	{
		zval *base_sql = zend_hash_str_find(Z_ARRVAL(printed), ZEND_STRL("sql"));
		zval *base_params = zend_hash_str_find(Z_ARRVAL(printed), ZEND_STRL("param"));
		if (!base_sql || Z_TYPE_P(base_sql) != IS_STRING) goto compiled_out;
		smart_str_appendl(&compound, Z_STRVAL_P(base_sql), Z_STRLEN_P(base_sql));
		array_init(&params_zv);
		gene_orm_params_append(&params_zv, base_params);
	}
	if (has_union) {
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops), op) {
			zval *tag, *all, *child, child_compiled, *child_sql, *child_params;
			if (Z_TYPE_P(op) != IS_ARRAY) continue;
			tag = zend_hash_index_find(Z_ARRVAL_P(op), 0);
			if (!tag || Z_TYPE_P(tag) != IS_STRING || strcmp(Z_STRVAL_P(tag), "union")) continue;
			all = zend_hash_index_find(Z_ARRVAL_P(op), 1);
			child = zend_hash_index_find(Z_ARRVAL_P(op), 2);
			if (!child || Z_TYPE_P(child) != IS_ARRAY) goto compiled_out;
			ZVAL_UNDEF(&child_compiled);
			if (gene_orm_query_compile_snapshot(child, db, 0, 0, 0, 0, &child_compiled) != SUCCESS) goto compiled_out;
			child_sql = zend_hash_str_find(Z_ARRVAL(child_compiled), ZEND_STRL("sql"));
			child_params = zend_hash_str_find(Z_ARRVAL(child_compiled), ZEND_STRL("param"));
			smart_str_appends(&compound, all && zend_is_true(all) ? " UNION ALL " : " UNION ");
			{
				zval *child_ops = zend_hash_str_find(Z_ARRVAL_P(child), ZEND_STRL("ops"));
				if (gene_orm_query_ops_has(child_ops, "union")) smart_str_appends(&compound, "SELECT * FROM (");
				smart_str_appendl(&compound, Z_STRVAL_P(child_sql), Z_STRLEN_P(child_sql));
				if (gene_orm_query_ops_has(child_ops, "union")) smart_str_appends(&compound, ") gene_union_branch");
			}
			gene_orm_params_append(&params_zv, child_params);
			zval_ptr_dtor(&child_compiled);
		} ZEND_HASH_FOREACH_END();
	}
	smart_str_0(&compound);
	if (outer_stage) {
		zval clone2, args[2], rv;
		if (gene_orm_clone_db(db, &clone2) != SUCCESS) goto compiled_out;
		ZVAL_STR_COPY(&args[0], compound.s);
		ZVAL_COPY(&args[1], &params_zv);
		gene_orm_db_call(&clone2, "sql", 2, args, &rv);
		zval_ptr_dtor(&args[0]); zval_ptr_dtor(&args[1]); zval_ptr_dtor(&rv);
		if (gene_orm_has_exception()) { zval_ptr_dtor(&clone2); goto compiled_out; }
		if (!strip_outer) {
			smart_str orders = {0};
			zend_bool has_limit = 0; zend_long la = 0, lb = -1; int lock_mode = 0;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops), op) {
				zval *tag, *a1, *a2;
				if (Z_TYPE_P(op) != IS_ARRAY) continue;
				tag = zend_hash_index_find(Z_ARRVAL_P(op), 0); a1 = zend_hash_index_find(Z_ARRVAL_P(op), 1); a2 = zend_hash_index_find(Z_ARRVAL_P(op), 2);
				if (!tag || Z_TYPE_P(tag) != IS_STRING) continue;
				if (!strcmp(Z_STRVAL_P(tag), "order") && a1 && Z_TYPE_P(a1) == IS_STRING) { if (orders.s && ZSTR_LEN(orders.s)) smart_str_appends(&orders, ", "); smart_str_appendl(&orders, Z_STRVAL_P(a1), Z_STRLEN_P(a1)); }
				else if (!strcmp(Z_STRVAL_P(tag), "limit")) { la = a1 && Z_TYPE_P(a1) == IS_LONG ? Z_LVAL_P(a1) : 0; lb = a2 && Z_TYPE_P(a2) == IS_LONG ? Z_LVAL_P(a2) : -1; has_limit = 1; }
				else if (!strcmp(Z_STRVAL_P(tag), "lock")) { lock_mode = a1 && Z_TYPE_P(a1) == IS_STRING && zend_string_equals_literal(Z_STR_P(a1), "share") ? 2 : 1; }
			} ZEND_HASH_FOREACH_END();
			if (orders.s && ZSTR_LEN(orders.s)) { smart_str_0(&orders); ZVAL_STR_COPY(&args[0], orders.s); gene_orm_db_call(&clone2, "order", 1, args, &rv); zval_ptr_dtor(&args[0]); zval_ptr_dtor(&rv); }
			smart_str_free(&orders);
			if (force_limit) gene_orm_db_limit(&clone2, offset, limit);
			else if (has_limit) { if (lb >= 0) gene_orm_db_limit(&clone2, la, lb); else { ZVAL_LONG(&args[0], la); gene_orm_db_call(&clone2, "limit", 1, args, &rv); zval_ptr_dtor(&rv); } }
			if (lock_mode) { gene_orm_db_call(&clone2, lock_mode == 1 ? "lockForUpdate" : "sharedLock", 0, NULL, &rv); zval_ptr_dtor(&rv); }
		}
		if (gene_orm_has_exception()) { zval_ptr_dtor(&clone2); goto compiled_out; }
		ZVAL_UNDEF(&sql_zv);
		if (gene_orm_db_call(&clone2, "print", 0, NULL, &sql_zv) != SUCCESS || Z_TYPE(sql_zv) != IS_ARRAY) { if (!Z_ISUNDEF(sql_zv)) zval_ptr_dtor(&sql_zv); zval_ptr_dtor(&clone2); goto compiled_out; }
		array_init(compiled);
		{
			zval *final_sql = zend_hash_str_find(Z_ARRVAL(sql_zv), ZEND_STRL("sql"));
			zval copy;
			ZVAL_COPY(&copy, final_sql); add_assoc_zval_ex(compiled, ZEND_STRL("sql"), &copy);
			ZVAL_COPY(&copy, &params_zv); add_assoc_zval_ex(compiled, ZEND_STRL("param"), &copy);
		}
		zval_ptr_dtor(&sql_zv);
		zval_ptr_dtor(&clone2);
	} else {
		array_init(compiled);
		ZVAL_STR_COPY(&sql_zv, compound.s); add_assoc_zval_ex(compiled, ZEND_STRL("sql"), &sql_zv);
		ZVAL_COPY(&sql_zv, &params_zv); add_assoc_zval_ex(compiled, ZEND_STRL("param"), &sql_zv);
	}
	status = SUCCESS;
compiled_out:
	if (!Z_ISUNDEF(printed)) zval_ptr_dtor(&printed);
	if (compound.s) smart_str_free(&compound);
	if (Z_TYPE(params_zv) == IS_ARRAY) zval_ptr_dtor(&params_zv);
	zval_ptr_dtor(&q);
	zval_ptr_dtor(&clone);
out:
	zval_ptr_dtor(&filtered);
	return status;
}

static int gene_orm_query_compile(zval *self, zval *db, zend_bool strip_outer, zend_bool force_limit, zend_long offset, zend_long limit, zval *compiled)
{
	zval snapshot;
	int status;
	gene_orm_query_snapshot(self, &snapshot);
	status = gene_orm_query_compile_snapshot(&snapshot, db, strip_outer, force_limit, offset, limit, compiled);
	zval_ptr_dtor(&snapshot);
	return status;
}

static int gene_orm_query_execute_compiled(zval *db, zval *compiled, const char *terminal, zval *retval)
{
	zval *sql = zend_hash_str_find(Z_ARRVAL_P(compiled), ZEND_STRL("sql"));
	zval *params = zend_hash_str_find(Z_ARRVAL_P(compiled), ZEND_STRL("param"));
	zval args[2], rv;
	ZVAL_COPY(&args[0], sql); ZVAL_COPY(&args[1], params);
	gene_orm_db_call(db, "sql", 2, args, &rv);
	zval_ptr_dtor(&args[0]); zval_ptr_dtor(&args[1]); zval_ptr_dtor(&rv);
	if (gene_orm_has_exception()) return FAILURE;
	if (gene_orm_db_call(db, terminal, 0, NULL, retval) != SUCCESS || gene_orm_has_exception()) {
		if (!Z_ISUNDEF_P(retval)) { zval_ptr_dtor(retval); ZVAL_UNDEF(retval); }
		return FAILURE;
	}
	return SUCCESS;
}

static void gene_orm_query_finish(zval *self, zval *db)
{
	gene_orm_db_reset(db);
	gene_orm_query_mark_dirty(self, 0);
}

int gene_orm_query_init(zval *query, zval *db, zend_string *table, zval *fields, zend_string *primary_key)
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
	if (primary_key && ZSTR_LEN(primary_key) > 0) {
		zend_update_property_str(gene_orm_query_ce, gene_strip_obj(query),
			ZEND_STRL(GENE_ORM_QUERY_PK), primary_key);
	} else {
		zend_update_property_null(gene_orm_query_ce, gene_strip_obj(query),
			ZEND_STRL(GENE_ORM_QUERY_PK));
	}
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
	/* [GENE_FIX:2026-08-20] Scalar where (e.g. where(34) or where("34"))
	 * must not be silently dropped nor treated as a raw SQL fragment —
	 * apply() pass 1 only handles arrays, pass 2 splices strings verbatim
	 * into WHERE (so where("4") becomes "WHERE 4" = constant true).
	 * Convert scalar / numeric-string to "pk=?" with bind, matching
	 * Model::find() semantics. */
	if (where && Z_TYPE_P(where) != IS_ARRAY
		&& Z_TYPE_P(where) != IS_NULL && Z_TYPE_P(where) != IS_UNDEF) {
		zend_bool is_scalar_id = (Z_TYPE_P(where) != IS_STRING);
		if (!is_scalar_id) {
			zend_long l;
			double d;
			is_scalar_id = (is_numeric_string(Z_STRVAL_P(where),
				Z_STRLEN_P(where), &l, &d, 0) != 0);
		}
		if (is_scalar_id) {
			zval *pk_zv = zend_read_property(gene_orm_query_ce,
				gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_PK), 1, NULL);
			if (pk_zv && Z_TYPE_P(pk_zv) == IS_STRING && Z_STRLEN_P(pk_zv) > 0) {
				smart_str frag = {0};
				smart_str_appendl(&frag, Z_STRVAL_P(pk_zv), Z_STRLEN_P(pk_zv));
				smart_str_appends(&frag, "=?");
				smart_str_0(&frag);
				zval t;
				ZVAL_STR(&t, frag.s);
				add_next_index_zval(&op, &t);
				gene_orm_query_op_val(&op, where);
				gene_orm_query_push(self, &op);
				RETURN_ZVAL(self, 1, 0);
			}
		}
	}
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

PHP_METHOD(gene_orm_query, joinOn)
{
	zval *self = getThis(), *predicates = NULL;
	zend_string *table = NULL, *type = NULL;
	zval op;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sa|S", &table, &predicates, &type) == FAILURE) return;
	if (zend_hash_num_elements(Z_ARRVAL_P(predicates)) == 0) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query::joinOn() predicates must not be empty");
		RETURN_NULL();
	}
	array_init_size(&op, 4);
	gene_orm_query_op_tag(&op, ZEND_STRL("joinon"));
	gene_orm_query_op_str(&op, table);
	gene_orm_query_op_val(&op, predicates);
	gene_orm_query_op_str(&op, type);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

static void gene_orm_query_union(INTERNAL_FUNCTION_PARAMETERS, zend_bool all)
{
	zval *self = getThis(), *query = NULL, *db, *child_db, snapshot, op, flag;
	zval *child_ops, *child_op;
	zend_long origin = (zend_long) Z_OBJ_HANDLE_P(self);
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &query, gene_orm_query_ce) == FAILURE) return;
	if (Z_OBJ_P(query) == Z_OBJ_P(self)) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query cannot union itself");
		RETURN_NULL();
	}
	db = gene_orm_query_db(self);
	child_db = gene_orm_query_db(query);
	if (!db || !child_db) RETURN_NULL();
	if (Z_OBJ_P(db) != Z_OBJ_P(child_db)) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union branches must use the same db handle");
		RETURN_NULL();
	}
	child_ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(query), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
	{
		zval *parent_ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (gene_orm_query_ops_has(parent_ops, "lock")) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union cannot be combined with locks");
			RETURN_NULL();
		}
	}
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(child_ops), child_op) {
		zval *tag;
		if (Z_TYPE_P(child_op) != IS_ARRAY) continue;
		tag = zend_hash_index_find(Z_ARRVAL_P(child_op), 0);
		if (tag && Z_TYPE_P(tag) == IS_STRING && (!strcmp(Z_STRVAL_P(tag), "order") || !strcmp(Z_STRVAL_P(tag), "limit") || !strcmp(Z_STRVAL_P(tag), "lock"))) {
			zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union branches cannot contain order()/limit()/lock");
			RETURN_NULL();
		}
	} ZEND_HASH_FOREACH_END();
	gene_orm_query_snapshot(query, &snapshot);
	if (gene_orm_snapshot_contains(&snapshot, origin)) {
		zval_ptr_dtor(&snapshot);
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union cycle detected");
		RETURN_NULL();
	}
	if (gene_orm_snapshot_depth(&snapshot) >= 8) {
		zval_ptr_dtor(&snapshot);
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union depth exceeds 8");
		RETURN_NULL();
	}
	array_init_size(&op, 3);
	gene_orm_query_op_tag(&op, ZEND_STRL("union"));
	ZVAL_BOOL(&flag, all);
	add_next_index_zval(&op, &flag);
	add_next_index_zval(&op, &snapshot);
	gene_orm_query_push(self, &op);
	RETURN_ZVAL(self, 1, 0);
}

PHP_METHOD(gene_orm_query, union) { gene_orm_query_union(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0); }
PHP_METHOD(gene_orm_query, unionAll) { gene_orm_query_union(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1); }

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
	zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
	if (gene_orm_query_ops_has(ops, "union")) { zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union cannot be combined with locks"); RETURN_NULL(); }

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
	zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
	if (gene_orm_query_ops_has(ops, "union")) { zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query union cannot be combined with locks"); RETURN_NULL(); }

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

/* {{{ print() — build SQL without executing, return Db::print() result.
 * Applies the op list to the db handle (same as row()/all()), calls
 * Db::print() to inspect the generated SQL + bind params, then resets the
 * handle so the Query can still be used for a real terminal call. */
PHP_METHOD(gene_orm_query, print)
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
		zval z_sql;
		ZVAL_STRING(&z_sql, "");
		add_assoc_zval_ex(return_value, ZEND_STRL("sql"), &z_sql);
		zval z_param;
		array_init(&z_param);
		add_assoc_zval_ex(return_value, ZEND_STRL("param"), &z_param);
		return;
	}
	if (gene_orm_query_compile(self, db, 0, 0, 0, 0, &retval) != SUCCESS) {
		gene_orm_query_finish(self, db);
		RETURN_NULL();
	}
	gene_orm_query_finish(self, db);
	RETURN_ZVAL(&retval, 0, 1);
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
	{
		zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (gene_orm_query_ops_has(ops, "union")) {
			zval compiled;
			if (gene_orm_query_compile(self, db, 0, 0, 0, 0, &compiled) != SUCCESS) { gene_orm_query_finish(self, db); RETURN_NULL(); }
			if (gene_orm_query_execute_compiled(db, &compiled, "all", &retval) != SUCCESS) { zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_NULL(); }
			zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_ZVAL(&retval, 0, 1);
		}
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
	{
		zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (gene_orm_query_ops_has(ops, "union")) {
			zval compiled;
			if (gene_orm_query_compile(self, db, 0, 0, 0, 0, &compiled) != SUCCESS) { gene_orm_query_finish(self, db); RETURN_NULL(); }
			if (gene_orm_query_execute_compiled(db, &compiled, "row", &retval) != SUCCESS) { zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_NULL(); }
			zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_ZVAL(&retval, 0, 1);
		}
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
	{
		zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (gene_orm_query_ops_has(ops, "union")) {
			zval compiled;
			if (gene_orm_query_compile(self, db, 0, 1, 0, 1, &compiled) != SUCCESS) { gene_orm_query_finish(self, db); RETURN_NULL(); }
			if (gene_orm_query_execute_compiled(db, &compiled, "row", &retval) != SUCCESS) { zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_NULL(); }
			zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_ZVAL(&retval, 0, 1);
		}
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
	{
		zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (gene_orm_query_ops_has(ops, "union")) {
			zval compiled;
			if (gene_orm_query_compile(self, db, 0, 0, 0, 0, &compiled) != SUCCESS) { gene_orm_query_finish(self, db); RETURN_NULL(); }
			if (gene_orm_query_execute_compiled(db, &compiled, "cell", &retval) != SUCCESS) { zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_NULL(); }
			zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_ZVAL(&retval, 0, 1);
		}
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
	{
		zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (gene_orm_query_ops_has(ops, "union")) {
			zval compiled, count_compiled, *sql, *params;
			smart_str wrapped = {0};
			if (gene_orm_query_compile(self, db, 1, 0, 0, 0, &compiled) != SUCCESS) { gene_orm_query_finish(self, db); RETURN_LONG(0); }
			sql = zend_hash_str_find(Z_ARRVAL(compiled), ZEND_STRL("sql")); params = zend_hash_str_find(Z_ARRVAL(compiled), ZEND_STRL("param"));
			smart_str_appends(&wrapped, "SELECT COUNT(*) FROM ("); smart_str_appendl(&wrapped, Z_STRVAL_P(sql), Z_STRLEN_P(sql)); smart_str_appends(&wrapped, ") gene_union_count"); smart_str_0(&wrapped);
			array_init(&count_compiled); add_assoc_str_ex(&count_compiled, ZEND_STRL("sql"), wrapped.s); { zval copy; ZVAL_COPY(&copy, params); add_assoc_zval_ex(&count_compiled, ZEND_STRL("param"), &copy); }
			if (gene_orm_query_execute_compiled(db, &count_compiled, "cell", &retval) != SUCCESS) { zval_ptr_dtor(&count_compiled); zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_LONG(0); }
			n = zval_get_long(&retval); zval_ptr_dtor(&retval); zval_ptr_dtor(&count_compiled); zval_ptr_dtor(&compiled); gene_orm_query_finish(self, db); RETURN_LONG(n);
		}
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
	{
		zval *ops = zend_read_property(gene_orm_query_ce, gene_strip_obj(self), ZEND_STRL(GENE_ORM_QUERY_OPS), 1, NULL);
		if (gene_orm_query_ops_has(ops, "union")) {
			zval args[2];
			ZVAL_LONG(&args[0], offset); ZVAL_LONG(&args[1], limit);
			if (gene_orm_db_call(self, "paginateResult", 2, args, &retval) == SUCCESS) RETURN_ZVAL(&retval, 0, 1);
			RETURN_NULL();
		}
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

PHP_METHOD(gene_orm_query, paginateResult)
{
	zval *self = getThis(), *db, snapshot, count_compiled, list_compiled, wrapped_compiled, retval, list;
	zval *sql, *params;
	zend_long offset, limit, count = 0;
	smart_str wrapped = {0};
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ll", &offset, &limit) == FAILURE) return;
	db = gene_orm_query_db(self);
	if (!db) RETURN_NULL();
	if (gene_orm_query_is_empty(self)) { array_init(return_value); add_assoc_long_ex(return_value, ZEND_STRL("count"), 0); array_init(&list); add_assoc_zval_ex(return_value, ZEND_STRL("list"), &list); return; }
	gene_orm_query_snapshot(self, &snapshot);
	ZVAL_UNDEF(&count_compiled); ZVAL_UNDEF(&list_compiled); ZVAL_UNDEF(&list);
	if (gene_orm_query_compile_snapshot(&snapshot, db, 1, 0, 0, 0, &count_compiled) != SUCCESS) goto fail;
	sql = zend_hash_str_find(Z_ARRVAL(count_compiled), ZEND_STRL("sql")); params = zend_hash_str_find(Z_ARRVAL(count_compiled), ZEND_STRL("param"));
	smart_str_appends(&wrapped, "SELECT COUNT(*) FROM ("); smart_str_appendl(&wrapped, Z_STRVAL_P(sql), Z_STRLEN_P(sql)); smart_str_appends(&wrapped, ") gene_result_count"); smart_str_0(&wrapped);
	array_init(&wrapped_compiled); add_assoc_str_ex(&wrapped_compiled, ZEND_STRL("sql"), wrapped.s); { zval copy; ZVAL_COPY(&copy, params); add_assoc_zval_ex(&wrapped_compiled, ZEND_STRL("param"), &copy); }
	if (gene_orm_query_execute_compiled(db, &wrapped_compiled, "cell", &retval) != SUCCESS) { zval_ptr_dtor(&wrapped_compiled); goto fail; }
	count = zval_get_long(&retval); zval_ptr_dtor(&retval); zval_ptr_dtor(&wrapped_compiled); gene_orm_db_reset(db);
	if (gene_orm_query_compile_snapshot(&snapshot, db, 0, 1, offset, limit, &list_compiled) != SUCCESS) goto fail;
	if (gene_orm_query_execute_compiled(db, &list_compiled, "all", &list) != SUCCESS || Z_TYPE(list) != IS_ARRAY) {
		if (!Z_ISUNDEF(list)) zval_ptr_dtor(&list);
		array_init(&list);
	}
	gene_orm_query_finish(self, db);
	zval_ptr_dtor(&snapshot); zval_ptr_dtor(&count_compiled); zval_ptr_dtor(&list_compiled);
	array_init(return_value); add_assoc_long_ex(return_value, ZEND_STRL("count"), count); add_assoc_zval_ex(return_value, ZEND_STRL("list"), &list);
	return;
fail:
	gene_orm_query_finish(self, db);
	if (!Z_ISUNDEF(count_compiled)) zval_ptr_dtor(&count_compiled);
	if (!Z_ISUNDEF(list_compiled)) zval_ptr_dtor(&list_compiled);
	zval_ptr_dtor(&snapshot);
	RETURN_NULL();
}

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

static void gene_orm_query_arithmetic(INTERNAL_FUNCTION_PARAMETERS, zend_bool increment)
{
	zval *self = getThis(), *amount = NULL, payload, value, *db, retval;
	zend_string *column = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|z", &column, &amount) == FAILURE) return;
	if (!gene_orm_valid_ident(column) || (amount && Z_TYPE_P(amount) != IS_LONG && Z_TYPE_P(amount) != IS_DOUBLE) ||
		(amount && Z_TYPE_P(amount) == IS_LONG && Z_LVAL_P(amount) <= 0) ||
		(amount && Z_TYPE_P(amount) == IS_DOUBLE && (!isfinite(Z_DVAL_P(amount)) || Z_DVAL_P(amount) <= 0))) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Orm\\Query::increment()/decrement() expects a valid column and finite positive amount");
		RETURN_LONG(0);
	}
	db = gene_orm_query_db(self);
	if (!db) RETURN_LONG(0);
	if (gene_orm_query_is_empty(self)) { gene_orm_query_finish(self, db); RETURN_LONG(0); }
	array_init_size(&payload, 2);
	ZVAL_STR_COPY(&value, column); add_next_index_zval(&payload, &value);
	if (amount) ZVAL_COPY(&value, amount); else ZVAL_LONG(&value, 1);
	add_next_index_zval(&payload, &value);
	if (gene_orm_query_apply(self, db, increment ? GENE_ORM_Q_INCREMENT : GENE_ORM_Q_DECREMENT, &payload, 0, 0, 0) != SUCCESS) { zval_ptr_dtor(&payload); gene_orm_query_finish(self, db); RETURN_LONG(0); }
	zval_ptr_dtor(&payload);
	if (gene_orm_db_call(db, "affectedRows", 0, NULL, &retval) == SUCCESS) {
		gene_orm_query_finish(self, db);
		if (Z_ISUNDEF(retval)) RETURN_LONG(0);
		RETURN_ZVAL(&retval, 0, 1);
	}
	gene_orm_query_finish(self, db); RETURN_LONG(0);
}

PHP_METHOD(gene_orm_query, increment) { gene_orm_query_arithmetic(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1); }
PHP_METHOD(gene_orm_query, decrement) { gene_orm_query_arithmetic(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0); }

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
	PHP_ME(gene_orm_query, joinOn, gene_orm_query_join_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, union, gene_orm_query_union_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, unionAll, gene_orm_query_union_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, group, gene_orm_query_str_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, having, gene_orm_query_str_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, order, gene_orm_query_str_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, limit, gene_orm_query_limit_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, fields, gene_orm_query_fields_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, lockForUpdate, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, sharedLock, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, selectSub, gene_orm_query_selectsub_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, whereLike, gene_orm_query_wherelike_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, print, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, all, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, row, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, first, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, cell, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, count, gene_orm_query_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, paginate, gene_orm_query_paginate_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, paginateResult, gene_orm_query_paginate_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, update, gene_orm_query_update_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, increment, gene_orm_query_arithmetic_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_query, decrement, gene_orm_query_arithmetic_arginfo, ZEND_ACC_PUBLIC)
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
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_PK), ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_FIELDS), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_DIRTY), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_null(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_OPS), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_orm_query_ce, ZEND_STRL(GENE_ORM_QUERY_EMPTY), 0, ZEND_ACC_PROTECTED);
}
