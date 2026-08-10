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
#include "../mvc/model.h"
#include "../di/di.h"
#include "../common/common.h"
#include "orm.h"

zend_class_entry *gene_orm_model_ce;

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_find_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, id)
	ZEND_ARG_INFO(0, asModel)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_findall_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, where)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_paginate_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, where)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, limit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_where_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, where)
	ZEND_ARG_INFO(0, bind)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_create_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, data, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_updateby_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, where)
	ZEND_ARG_ARRAY_INFO(0, data, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_destroy_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_destroyall_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, ids, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_fill_arginfo, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, data, 0)
	ZEND_ARG_INFO(0, hydrate)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_setexists_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, exists)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_orm_model_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

static zend_class_entry *gene_orm_called_ce(void)
{
	zend_class_entry *ce = zend_get_called_scope(EG(current_execute_data));
	if (!ce) {
		ce = gene_orm_model_ce;
	}
	return ce;
}

/* Non-empty primary key in an attribute/payload array (NULL/UNDEF/''/0 count
 * as empty, matching the fill() hydration rule). */
static zend_always_inline zval *gene_orm_pk_value(zval *attrs, zend_string *pk)
{
	zval *v;

	if (!pk || !attrs || Z_TYPE_P(attrs) != IS_ARRAY) {
		return NULL;
	}
	v = zend_hash_find(Z_ARRVAL_P(attrs), pk);
	if (!v || Z_TYPE_P(v) == IS_NULL || Z_TYPE_P(v) == IS_UNDEF ||
		(Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) == 0) ||
		(Z_TYPE_P(v) == IS_LONG && Z_LVAL_P(v) == 0)) {
		return NULL;
	}
	return v;
}

static int gene_orm_apply_where(zval *db, zval *where, gene_orm_meta_t *meta)
{
	zval args[2], retval, cond;
	uint32_t argc = 1;
	smart_str buf = {0};

	if (!where || Z_TYPE_P(where) == IS_NULL || Z_TYPE_P(where) == IS_UNDEF) {
		return SUCCESS;
	}
	if (Z_TYPE_P(where) == IS_ARRAY) {
		if (zend_hash_num_elements(Z_ARRVAL_P(where)) == 0) {
			return SUCCESS;
		}
		ZVAL_COPY(&args[0], where);
		gene_orm_db_call(db, "where", 1, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&retval);
		return SUCCESS;
	}

	/* Scalar → primaryKey=? */
	smart_str_appends(&buf, ZSTR_VAL(meta->primary_key));
	smart_str_appends(&buf, "=?");
	smart_str_0(&buf);
	ZVAL_STR(&cond, buf.s);
	ZVAL_COPY(&args[0], &cond);
	ZVAL_COPY(&args[1], where);
	gene_orm_db_call(db, "where", 2, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	zval_ptr_dtor(&retval);
	zval_ptr_dtor(&cond);
	return SUCCESS;
}

static int gene_orm_new_query(zval *retval)
{
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder;

	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		return FAILURE;
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		return FAILURE;
	}
	gene_orm_query_init(retval, db, meta.table, &meta.fields);
	zval_ptr_dtor(db); /* N1: query_init ZVAL_COPY'd the handle into a property */
	gene_orm_meta_release(&meta);
	return SUCCESS;
}

/*
 * {{{ public static Gene\Orm\Model::query()
 */
PHP_METHOD(gene_orm_model, query)
{
	if (gene_orm_new_query(return_value) != SUCCESS) {
		RETURN_NULL();
	}
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::where($where, $bind = null)
 */
PHP_METHOD(gene_orm_model, where)
{
	zval *where = NULL, *bind = NULL;
	zval query, args[2], retval;
	uint32_t argc = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|z", &where, &bind) == FAILURE) {
		return;
	}
	if (gene_orm_new_query(&query) != SUCCESS) {
		RETURN_NULL();
	}
	ZVAL_COPY(&args[0], where);
	if (bind) {
		ZVAL_COPY(&args[1], bind);
		argc = 2;
	}
	gene_orm_db_call(&query, "where", argc, args, &retval);
	zval_ptr_dtor(&args[0]);
	if (argc == 2) {
		zval_ptr_dtor(&args[1]);
	}
	zval_ptr_dtor(&retval);
	RETURN_ZVAL(&query, 0, 1);
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::find($id, $asModel = false)
 */
PHP_METHOD(gene_orm_model, find)
{
	zval *id = NULL;
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder, args[2], retval, lim;
	smart_str buf = {0};
	zend_bool as_model = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|b", &id, &as_model) == FAILURE) {
		return;
	}
	if (Z_TYPE_P(id) == IS_ARRAY || Z_TYPE_P(id) == IS_OBJECT ||
		Z_TYPE_P(id) == IS_RESOURCE) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Model::find() expects a scalar primary key");
		RETURN_NULL();
	}
	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_NULL();
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_NULL();
	}

	gene_orm_db_select(db, meta.table, &meta.fields);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	smart_str_appends(&buf, ZSTR_VAL(meta.primary_key));
	smart_str_appends(&buf, "=?");
	smart_str_0(&buf);
	ZVAL_STR(&args[0], buf.s);
	ZVAL_COPY(&args[1], id);
	gene_orm_db_call(db, "where", 2, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	ZVAL_LONG(&lim, 1);
	gene_orm_db_call(db, "limit", 1, &lim, &retval);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	if (gene_orm_db_call(db, "row", 0, NULL, return_value) != SUCCESS) {
		ZVAL_NULL(return_value);
	} else if (Z_TYPE_P(return_value) == IS_FALSE || Z_TYPE_P(return_value) == IS_NULL ||
		Z_TYPE_P(return_value) == IS_UNDEF) {
		zval_ptr_dtor(return_value);
		ZVAL_NULL(return_value);
	}

	/* [GENE_FIX:2026-08-09 M1] find($id, true) hydrates a model instance
	 * (exists=1) so the natural (new U)->fill(U::find($id))->save() pattern —
	 * or simply find($id, true)->save() — takes the UPDATE branch instead of
	 * a duplicate INSERT with an explicit primary key. */
	if (as_model && Z_TYPE_P(return_value) == IS_ARRAY) {
		zval model, attrs;
		object_init_ex(&model, ce);
		ZVAL_COPY(&attrs, return_value);
		zend_update_property(gene_orm_model_ce, gene_strip_obj(&model),
			ZEND_STRL(GENE_ORM_ATTRS), &attrs);
		zval_ptr_dtor(&attrs);
		zend_update_property_bool(gene_orm_model_ce, gene_strip_obj(&model),
			ZEND_STRL(GENE_ORM_EXISTS), 1);
		/* [GENE_FIX:2026-08-10 N3] Run the (inherited or subclass) constructor
		 * when it takes no required arguments so a hydrated instance behaves
		 * like `new U()` (default values, injected deps). Constructors with
		 * mandatory parameters are skipped — hydration cannot supply them.
		 * Runs AFTER attributes/exists are set: __set() merges into the
		 * existing attributes, so constructor writes survive (setting the
		 * properties after the constructor would wipe them). A throwing
		 * constructor aborts hydration and surfaces the exception. */
		/* [GENE_FIX:2026-08-10 R1] Only PUBLIC constructors are invoked:
		 * zend_call_known_function() bypasses visibility checks, so without
		 * this guard hydration would call private/protected constructors
		 * (factory/singleton models) from outside the class — something
		 * `new T()` at the same location would reject with a fatal Error.
		 * Non-public constructors are treated as "hydration skips the
		 * constructor" (Laravel semantics). */
		if (ce->constructor && ce->constructor->common.required_num_args == 0 &&
			(ce->constructor->common.fn_flags & ZEND_ACC_PUBLIC)) {
			zval ctor_ret;
			zend_call_known_function(ce->constructor, Z_OBJ(model), ce,
				&ctor_ret, 0, NULL, NULL);
			zval_ptr_dtor(&ctor_ret);
			if (UNEXPECTED(gene_orm_has_exception())) {
				zval_ptr_dtor(&model);
				zval_ptr_dtor(return_value);
				ZVAL_NULL(return_value);
				goto cleanup;
			}
		}
		zval_ptr_dtor(return_value);
		ZVAL_COPY_VALUE(return_value, &model);
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::findAll($where = [])
 */
PHP_METHOD(gene_orm_model, findAll)
{
	zval *where = NULL;
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &where) == FAILURE) {
		return;
	}
	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_NULL();
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_NULL();
	}

	gene_orm_db_select(db, meta.table, &meta.fields);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;
	gene_orm_apply_where(db, where, &meta);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	if (gene_orm_db_call(db, "all", 0, NULL, return_value) != SUCCESS) {
		array_init(return_value);
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::paginate($where, $offset, $limit)
 */
PHP_METHOD(gene_orm_model, paginate)
{
	zval *where = NULL;
	zend_long offset = 0, limit = 10;
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder, args[2], retval, count_zv, list_zv;
	zend_long count_val = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zll", &where, &offset, &limit) == FAILURE) {
		return;
	}
	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_NULL();
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_NULL();
	}

	/* count */
	ZVAL_STR_COPY(&args[0], meta.table);
	gene_orm_db_call(db, "count", 1, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;
	gene_orm_apply_where(db, where, &meta);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;
	if (gene_orm_db_call(db, "cell", 0, NULL, &count_zv) == SUCCESS) {
		if (Z_TYPE(count_zv) == IS_LONG) {
			count_val = Z_LVAL(count_zv);
		} else if (Z_TYPE(count_zv) == IS_STRING) {
			count_val = zend_atol(Z_STRVAL(count_zv), Z_STRLEN(count_zv));
		} else if (Z_TYPE(count_zv) == IS_DOUBLE) {
			count_val = (zend_long)Z_DVAL(count_zv);
		}
		zval_ptr_dtor(&count_zv);
	}
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;
	gene_orm_db_reset(db);

	/* list */
	gene_orm_db_select(db, meta.table, &meta.fields);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;
	gene_orm_apply_where(db, where, &meta);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;
	gene_orm_db_limit(db, offset, limit);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	if (gene_orm_db_call(db, "all", 0, NULL, &list_zv) != SUCCESS) {
		array_init(&list_zv);
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	gene_orm_meta_release(&meta);

	if (UNEXPECTED(gene_orm_has_exception())) {
		return;
	}
	array_init(return_value);
	add_assoc_long_ex(return_value, ZEND_STRL("count"), count_val);
	add_assoc_zval_ex(return_value, ZEND_STRL("list"), &list_zv);
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::create(array $data)
 */
PHP_METHOD(gene_orm_model, create)
{
	zval *data = NULL;
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder, args[2], retval, data_copy;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &data) == FAILURE) {
		return;
	}
	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_LONG(0);
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}

	ZVAL_COPY(&data_copy, data);
	SEPARATE_ARRAY(&data_copy);
	if (meta.timestamps) {
		gene_orm_apply_timestamps(&data_copy, 1);
	}

	ZVAL_STR_COPY(&args[0], meta.table);
	ZVAL_COPY(&args[1], &data_copy);
	gene_orm_db_call(db, "insert", 2, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	{
		zval *pk_in = gene_orm_pk_value(&data_copy, meta.primary_key);
		if (pk_in) {
			/* [GENE_FIX:2026-08-10 N3] Payload carries the (client-generated)
			 * primary key: lastInsertId() is the driver rowid, not the pk —
			 * return the payload pk verbatim. normalize_id() stays reserved for
			 * the auto-increment scenario (payload without pk) only.
			 * insert() is lazy: still issue a read call (lastId) to execute it,
			 * discarding the rowid. */
			zval lid;
			ZVAL_COPY(return_value, pk_in);
			if (gene_orm_db_call(db, "lastId", 0, NULL, &lid) == SUCCESS) {
				zval_ptr_dtor(&lid);
			}
		} else if (gene_orm_db_call(db, "lastId", 0, NULL, return_value) != SUCCESS) {
			RETVAL_LONG(0);
		} else {
			gene_orm_normalize_id(return_value); /* M2: string id → int (auto-increment only) */
		}
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	zval_ptr_dtor(&data_copy);
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::updateBy($where, array $data)
 */
PHP_METHOD(gene_orm_model, updateBy)
{
	zval *where = NULL, *data = NULL;
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder, args[2], retval, data_copy;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "za", &where, &data) == FAILURE) {
		return;
	}
	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_LONG(0);
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}

	ZVAL_COPY(&data_copy, data);
	SEPARATE_ARRAY(&data_copy);
	if (meta.timestamps) {
		gene_orm_apply_timestamps(&data_copy, 0);
	}

	ZVAL_STR_COPY(&args[0], meta.table);
	ZVAL_COPY(&args[1], &data_copy);
	gene_orm_db_call(db, "update", 2, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	gene_orm_apply_where(db, where, &meta);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	if (gene_orm_db_call(db, "affectedRows", 0, NULL, return_value) != SUCCESS) {
		RETVAL_LONG(0);
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	zval_ptr_dtor(&data_copy);
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::destroy($id)
 */
PHP_METHOD(gene_orm_model, destroy)
{
	zval *id = NULL;
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder, args[2], retval;
	smart_str buf = {0};

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &id) == FAILURE) {
		return;
	}
	if (Z_TYPE_P(id) == IS_ARRAY || Z_TYPE_P(id) == IS_OBJECT ||
		Z_TYPE_P(id) == IS_RESOURCE) {
		zend_throw_exception_ex(NULL, 0,
			"Gene\\Orm\\Model::destroy() expects a scalar primary key");
		RETURN_LONG(0);
	}
	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_LONG(0);
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}

	ZVAL_STR_COPY(&args[0], meta.table);
	gene_orm_db_call(db, "delete", 1, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	smart_str_appends(&buf, ZSTR_VAL(meta.primary_key));
	smart_str_appends(&buf, "=?");
	smart_str_0(&buf);
	ZVAL_STR(&args[0], buf.s);
	ZVAL_COPY(&args[1], id);
	gene_orm_db_call(db, "where", 2, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	if (gene_orm_db_call(db, "affectedRows", 0, NULL, return_value) != SUCCESS) {
		RETVAL_LONG(0);
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public static Gene\Orm\Model::destroyAll(array $ids)
 */
PHP_METHOD(gene_orm_model, destroyAll)
{
	zval *ids = NULL;
	gene_orm_meta_t meta;
	zend_class_entry *ce = gene_orm_called_ce();
	zval db_holder, *db = &db_holder, args[2], retval;
	smart_str buf = {0};

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &ids) == FAILURE) {
		return;
	}
	if (zend_hash_num_elements(Z_ARRVAL_P(ids)) == 0) {
		RETURN_LONG(0);
	}
	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_LONG(0);
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}

	ZVAL_STR_COPY(&args[0], meta.table);
	gene_orm_db_call(db, "delete", 1, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	smart_str_appends(&buf, ZSTR_VAL(meta.primary_key));
	smart_str_appends(&buf, " in(?)");
	smart_str_0(&buf);
	ZVAL_STR(&args[0], buf.s);
	ZVAL_COPY(&args[1], ids);
	gene_orm_db_call(db, "in", 2, args, &retval);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&args[1]);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

	if (gene_orm_db_call(db, "affectedRows", 0, NULL, return_value) != SUCCESS) {
		RETVAL_LONG(0);
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::fill(array $data, bool $hydrate = true)
 */
PHP_METHOD(gene_orm_model, fill)
{
	zval *self = getThis(), *data = NULL;
	zval attrs, *existing, *val;
	zend_string *key;
	zend_ulong idx;
	gene_orm_meta_t meta;
	zend_bool hydrate = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|b", &data, &hydrate) == FAILURE) {
		return;
	}
	if (gene_orm_meta_load(Z_OBJCE_P(self), &meta) != SUCCESS) {
		RETURN_NULL();
	}

	existing = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
	if (existing && Z_TYPE_P(existing) == IS_ARRAY) {
		ZVAL_COPY(&attrs, existing);
		SEPARATE_ARRAY(&attrs);
	} else {
		array_init(&attrs);
	}

	ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(data), idx, key, val) {
		zval tmp;
		ZVAL_COPY(&tmp, val);
		if (key) {
			zend_hash_update(Z_ARRVAL(attrs), key, &tmp);
		} else {
			zend_hash_index_update(Z_ARRVAL(attrs), idx, &tmp);
		}
	} ZEND_HASH_FOREACH_END();

	zend_update_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), &attrs);

	/* [GENE_FIX:2026-08-09 M1] Hydration: when the merged attributes carry a
	 * non-empty primary key (e.g. fill(Model::find($id))), mark the instance as
	 * existing so save() takes the UPDATE branch instead of issuing a duplicate
	 * INSERT with an explicit primary key (UNIQUE conflict / duplicated rows).
	 * [GENE_FIX:2026-08-10 N2] $hydrate=false opts out of the auto-marking —
	 * the escape hatch for client-generated primary keys (UUID / natural key /
	 * data import), where the payload pk means "new row", not "persisted". */
	if (hydrate && gene_orm_pk_value(&attrs, meta.primary_key)) {
		zend_update_property_bool(gene_orm_model_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_EXISTS), 1);
	}

	zval_ptr_dtor(&attrs);
	gene_orm_meta_release(&meta);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::setExists(bool $exists) — explicit exists flag
 */
PHP_METHOD(gene_orm_model, setExists)
{
	zval *self = getThis();
	zend_bool exists = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &exists) == FAILURE) {
		return;
	}
	/* [GENE_FIX:2026-08-10 N2] Escape hatch for the fill() hydration rule:
	 * on client-generated-primary-key tables, fill($data) marks exists=1 and
	 * save() would silently UPDATE 0 rows. setExists(false) (or
	 * fill($data, false)) forces the INSERT branch. */
	zend_update_property_bool(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_EXISTS), exists);
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::toArray()
 */
PHP_METHOD(gene_orm_model, toArray)
{
	zval *self = getThis();
	zval *attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
	if (attrs && Z_TYPE_P(attrs) == IS_ARRAY) {
		RETURN_ZVAL(attrs, 1, 0);
	}
	array_init(return_value);
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::save()
 */
PHP_METHOD(gene_orm_model, save)
{
	zval *self = getThis();
	gene_orm_meta_t meta;
	zend_class_entry *ce = Z_OBJCE_P(self);
	zval db_holder, *db = &db_holder, *attrs, *exists, *pk_val, args[2], retval, data_copy;
	smart_str buf = {0};

	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_LONG(0);
	}
	attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
	if (!attrs || Z_TYPE_P(attrs) != IS_ARRAY) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}
	if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}

	exists = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_EXISTS), 1, NULL);
	pk_val = zend_hash_find(Z_ARRVAL_P(attrs), meta.primary_key);

	ZVAL_COPY(&data_copy, attrs);
	SEPARATE_ARRAY(&data_copy);

	if (exists && zend_is_true(exists) && pk_val &&
		Z_TYPE_P(pk_val) != IS_NULL && Z_TYPE_P(pk_val) != IS_UNDEF) {
		/* update — copy pk first, then drop from payload (avoids UAF on shared HT) */
		zval pk_copy;
		ZVAL_COPY(&pk_copy, pk_val);
		zend_hash_del(Z_ARRVAL(data_copy), meta.primary_key);
		if (meta.timestamps) {
			gene_orm_apply_timestamps(&data_copy, 0);
		}
		ZVAL_STR_COPY(&args[0], meta.table);
		ZVAL_COPY(&args[1], &data_copy);
		gene_orm_db_call(db, "update", 2, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&args[1]);
		zval_ptr_dtor(&retval);
		if (UNEXPECTED(gene_orm_has_exception())) {
			zval_ptr_dtor(&pk_copy);
			goto cleanup;
		}

		smart_str_appends(&buf, ZSTR_VAL(meta.primary_key));
		smart_str_appends(&buf, "=?");
		smart_str_0(&buf);
		ZVAL_STR(&args[0], buf.s);
		ZVAL_COPY(&args[1], &pk_copy);
		gene_orm_db_call(db, "where", 2, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&args[1]);
		zval_ptr_dtor(&retval);
		zval_ptr_dtor(&pk_copy);
		if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

		if (gene_orm_db_call(db, "affectedRows", 0, NULL, return_value) != SUCCESS) {
			RETVAL_LONG(0);
		} else if ((Z_TYPE_P(return_value) == IS_LONG && Z_LVAL_P(return_value) == 0) ||
			(Z_TYPE_P(return_value) == IS_STRING &&
				zend_atol(Z_STRVAL_P(return_value), Z_STRLEN_P(return_value)) == 0)) {
			/* [GENE_FIX:2026-08-10 N2] A hydrated model whose row is missing
			 * (typical: client-generated primary key inserted via fill()+save())
			 * used to fail silently — 0 rows affected, return 0, no error. */
			php_error_docref(NULL, E_NOTICE,
				"UPDATE affected 0 rows on \"%s\" -- the row may not exist; "
				"to insert with a client-generated primary key use fill($data, false) + save(), setExists(false), or Model::create()",
				ZSTR_VAL(meta.table));
		}
	} else {
		if (meta.timestamps) {
			gene_orm_apply_timestamps(&data_copy, 1);
		}
		ZVAL_STR_COPY(&args[0], meta.table);
		ZVAL_COPY(&args[1], &data_copy);
		gene_orm_db_call(db, "insert", 2, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&args[1]);
		zval_ptr_dtor(&retval);
		if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

		{
			zval *pk_in = gene_orm_pk_value(&data_copy, meta.primary_key);
			if (pk_in) {
				/* [GENE_FIX:2026-08-10 N3] The payload already carries a non-empty
				 * (client-generated) primary key: lastInsertId() would be the driver
				 * rowid, not the pk. Return the payload pk verbatim and skip
				 * normalize_id() — that normalization is for the auto-increment
				 * scenario only (a zero-padded string key like '007' must not be
				 * mangled to int 7). insert() is lazy, so a read call (lastId)
				 * is still needed to actually execute it; its rowid is discarded. */
				zval lid;
				ZVAL_COPY(return_value, pk_in);
				if (gene_orm_db_call(db, "lastId", 0, NULL, &lid) == SUCCESS) {
					zval_ptr_dtor(&lid);
				}
				zend_update_property_bool(gene_orm_model_ce, gene_strip_obj(self),
					ZEND_STRL(GENE_ORM_EXISTS), 1);
			} else if (gene_orm_db_call(db, "lastId", 0, NULL, return_value) == SUCCESS) {
				gene_orm_normalize_id(return_value); /* M2: string id → int (auto-increment only) */
				if (Z_TYPE_P(return_value) != IS_NULL &&
					!(Z_TYPE_P(return_value) == IS_LONG && Z_LVAL_P(return_value) == 0)) {
					zval tmp, *attrs_w;
					ZVAL_COPY(&tmp, return_value);
					attrs_w = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
						ZEND_STRL(GENE_ORM_ATTRS), 0, NULL);
					if (attrs_w && Z_TYPE_P(attrs_w) == IS_ARRAY) {
						SEPARATE_ARRAY(attrs_w);
						zend_hash_update(Z_ARRVAL_P(attrs_w), meta.primary_key, &tmp);
					} else {
						zval_ptr_dtor(&tmp);
					}
					zend_update_property_bool(gene_orm_model_ce, gene_strip_obj(self),
						ZEND_STRL(GENE_ORM_EXISTS), 1);
				}
			} else {
				RETVAL_LONG(0);
			}
		}
	}

cleanup:
	gene_orm_db_reset(db);
	zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	zval_ptr_dtor(&data_copy);
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::delete()
 */
PHP_METHOD(gene_orm_model, delete)
{
	zval *self = getThis();
	gene_orm_meta_t meta;
	zend_class_entry *ce = Z_OBJCE_P(self);
	zval *attrs, *pk_val;

	if (gene_orm_meta_load(ce, &meta) != SUCCESS) {
		RETURN_LONG(0);
	}
	attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
	if (!attrs || Z_TYPE_P(attrs) != IS_ARRAY) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}
	pk_val = zend_hash_find(Z_ARRVAL_P(attrs), meta.primary_key);
	if (!pk_val) {
		gene_orm_meta_release(&meta);
		RETURN_LONG(0);
	}

	/* Reuse destroy logic via static call pattern */
	{
		zval db_holder, *db = &db_holder, args[2], retval, pk_copy;
		smart_str buf = {0};

		ZVAL_COPY(&pk_copy, pk_val);

		if (gene_orm_get_db(meta.connection, db) != SUCCESS) {
			zval_ptr_dtor(&pk_copy);
			gene_orm_meta_release(&meta);
			RETURN_LONG(0);
		}
		ZVAL_STR_COPY(&args[0], meta.table);
		gene_orm_db_call(db, "delete", 1, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&retval);
		if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

		smart_str_appends(&buf, ZSTR_VAL(meta.primary_key));
		smart_str_appends(&buf, "=?");
		smart_str_0(&buf);
		ZVAL_STR(&args[0], buf.s);
		ZVAL_COPY(&args[1], &pk_copy);
		gene_orm_db_call(db, "where", 2, args, &retval);
		zval_ptr_dtor(&args[0]);
		zval_ptr_dtor(&args[1]);
		zval_ptr_dtor(&retval);
		zval_ptr_dtor(&pk_copy);
		ZVAL_UNDEF(&pk_copy);
		if (UNEXPECTED(gene_orm_has_exception())) goto cleanup;

		if (gene_orm_db_call(db, "affectedRows", 0, NULL, return_value) != SUCCESS) {
			RETVAL_LONG(0);
		}
		zend_update_property_bool(gene_orm_model_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_EXISTS), 0);
		/* Drop pk so a subsequent save() cannot revive the deleted id */
		{
			zval *attrs_w = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
				ZEND_STRL(GENE_ORM_ATTRS), 0, NULL);
			if (attrs_w && Z_TYPE_P(attrs_w) == IS_ARRAY) {
				SEPARATE_ARRAY(attrs_w);
				zend_hash_del(Z_ARRVAL_P(attrs_w), meta.primary_key);
			}
		}

cleanup:
		if (!Z_ISUNDEF(pk_copy)) {
			zval_ptr_dtor(&pk_copy);
		}
		gene_orm_db_reset(db);
		zval_ptr_dtor(db); /* N1: owned copy from gene_orm_get_db() */
	}
	gene_orm_meta_release(&meta);
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::__get($name) — attributes first, then DI (parent)
 */
PHP_METHOD(gene_orm_model, __get)
{
	zval *self = getThis();
	zend_string *name = NULL;
	zval *attrs, *val, *exists;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}
	if (!name) {
		RETURN_NULL();
	}

	if (zend_string_equals_literal(name, GENE_ORM_EXISTS)) {
		exists = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_EXISTS), 1, NULL);
		RETURN_BOOL(exists && zend_is_true(exists));
	}
	if (zend_string_equals_literal(name, GENE_ORM_ATTRS)) {
		attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
			ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
		if (attrs && Z_TYPE_P(attrs) == IS_ARRAY) {
			RETURN_ZVAL(attrs, 1, 0);
		}
		array_init(return_value);
		return;
	}

	attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
	if (attrs && Z_TYPE_P(attrs) == IS_ARRAY) {
		val = zend_hash_find(Z_ARRVAL_P(attrs), name);
		if (val) {
			RETURN_ZVAL(val, 1, 0);
		}
	}

	/* Fall through to Gene\Model DI lookup ($this->db, etc.) */
	{
		zend_string *class_name = gene_get_class_name_fast();
		zval *pzval;
		if (!class_name) {
			RETURN_NULL();
		}
		pzval = gene_di_get_class(class_name, name);
		if (pzval) {
			RETURN_ZVAL(pzval, 1, 0);
		}
	}
	RETURN_NULL();
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::__set($name, $value) — write attributes (not DI)
 */
PHP_METHOD(gene_orm_model, __set)
{
	zval *self = getThis();
	zend_string *name;
	zval *value, *attrs, attrs_copy;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		return;
	}
	if (zend_string_equals_literal(name, GENE_ORM_EXISTS) ||
		zend_string_equals_literal(name, GENE_ORM_ATTRS)) {
		RETURN_FALSE;
	}

	attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
	if (attrs && Z_TYPE_P(attrs) == IS_ARRAY) {
		ZVAL_COPY(&attrs_copy, attrs);
		SEPARATE_ARRAY(&attrs_copy);
	} else {
		array_init(&attrs_copy);
	}
	{
		zval tmp;
		ZVAL_COPY(&tmp, value);
		zend_hash_update(Z_ARRVAL(attrs_copy), name, &tmp);
	}
	zend_update_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), &attrs_copy);
	zval_ptr_dtor(&attrs_copy);
	RETURN_TRUE;
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::__isset($name) — attributes-aware isset()
 */
PHP_METHOD(gene_orm_model, __isset)
{
	zval *self = getThis();
	zend_string *name = NULL;
	zval *attrs, *val;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}
	if (!name) {
		RETURN_FALSE;
	}
	/* [GENE_FIX:2026-08-09 M7] isset($m->name) / empty() / ?? used to always
	 * report "missing" because __isset was never implemented. */
	if (zend_string_equals_literal(name, GENE_ORM_EXISTS) ||
		zend_string_equals_literal(name, GENE_ORM_ATTRS)) {
		RETURN_TRUE;
	}
	attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 1, NULL);
	if (attrs && Z_TYPE_P(attrs) == IS_ARRAY) {
		val = zend_hash_find(Z_ARRVAL_P(attrs), name);
		RETURN_BOOL(val && Z_TYPE_P(val) != IS_NULL);
	}
	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ public Gene\Orm\Model::__unset($name) — remove an attribute
 */
PHP_METHOD(gene_orm_model, __unset)
{
	zval *self = getThis();
	zend_string *name = NULL;
	zval *attrs;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		return;
	}
	if (!name) {
		RETURN_FALSE;
	}
	if (zend_string_equals_literal(name, GENE_ORM_EXISTS) ||
		zend_string_equals_literal(name, GENE_ORM_ATTRS)) {
		RETURN_FALSE;
	}
	attrs = zend_read_property(gene_orm_model_ce, gene_strip_obj(self),
		ZEND_STRL(GENE_ORM_ATTRS), 0, NULL);
	if (attrs && Z_TYPE_P(attrs) == IS_ARRAY) {
		SEPARATE_ARRAY(attrs);
		zend_hash_del(Z_ARRVAL_P(attrs), name);
	}
	RETURN_TRUE;
}
/* }}} */

const zend_function_entry gene_orm_model_methods[] = {
	PHP_ME(gene_orm_model, query, gene_orm_model_void_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, where, gene_orm_model_where_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, find, gene_orm_model_find_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, findAll, gene_orm_model_findall_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, paginate, gene_orm_model_paginate_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, create, gene_orm_model_create_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, updateBy, gene_orm_model_updateby_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, destroy, gene_orm_model_destroy_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, destroyAll, gene_orm_model_destroyall_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_orm_model, fill, gene_orm_model_fill_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, setExists, gene_orm_model_setexists_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, save, gene_orm_model_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, delete, gene_orm_model_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, toArray, gene_orm_model_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, __get, gene_orm_model_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, __set, gene_orm_model_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, __isset, gene_orm_model_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_orm_model, __unset, gene_orm_model_get_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};

/*
 * {{{ GENE_MINIT_FUNCTION(orm)
 */
GENE_MINIT_FUNCTION(orm)
{
	zend_class_entry ce;

	gene_orm_query_register();

	GENE_INIT_CLASS_ENTRY(ce, "Gene_Orm_Model", "Gene\\Orm\\Model", gene_orm_model_methods);
	gene_orm_model_ce = zend_register_internal_class_ex(&ce, gene_model_ce);
#if PHP_VERSION_ID >= 80200
	gene_orm_model_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif

	/* Static metadata defaults (overridden by PHP subclasses) */
	zend_declare_property_string(gene_orm_model_ce, ZEND_STRL(GENE_ORM_TABLE), "",
		ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_string(gene_orm_model_ce, ZEND_STRL(GENE_ORM_PK), "id",
		ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_null(gene_orm_model_ce, ZEND_STRL(GENE_ORM_FIELDS),
		ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_bool(gene_orm_model_ce, ZEND_STRL(GENE_ORM_TIMESTAMPS), 0,
		ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);
	zend_declare_property_string(gene_orm_model_ce, ZEND_STRL(GENE_ORM_CONNECTION), "db",
		ZEND_ACC_PROTECTED | ZEND_ACC_STATIC);

	/* Instance state */
	zend_declare_property_null(gene_orm_model_ce, ZEND_STRL(GENE_ORM_ATTRS), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_orm_model_ce, ZEND_STRL(GENE_ORM_EXISTS), 0, ZEND_ACC_PROTECTED);

	return SUCCESS;
}
/* }}} */
