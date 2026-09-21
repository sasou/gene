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

#ifndef GENE_DB_PDO_H
#define GENE_DB_PDO_H

#include "zend_smart_str_public.h"
#include "Zend/zend_API.h"

void array_to_string(zval *array, char **result, char oq, char cq);
void mssql_array_to_string(zval *array, char **result, char oq, char cq);
void gene_quote_identifier(smart_str *dest, const char *name, size_t len, char oq, char cq);
char *gene_quote_table(const char *name, char oq, char cq);
char *gene_quote_columns(const char *name, char oq, char cq);
char *gene_quote_order(const char *name, char oq, char cq);
void gene_pdo_construct(zval *pdo_object, zval *dsn, zval *user, zval *pass, zval *options);
void gene_pdo_begin_transaction(zval *pdo_object, zval *retval);
void gene_pdo_commit(zval *pdo_object, zval *retval);
/* Callback transaction: own begin/commit when not already in a PDO tx;
 * nested calls only run $fn. Alias in userland: transact(). */
void gene_pdo_run_transaction(zval *pdo_object, zend_fcall_info *fci, zend_fcall_info_cache *fcc, zval *retval);
void gene_pdo_exec(zval *pdo_object, char *sql, zval *retval);
void gene_pdo_in_transaction(zval *pdo_object, zval *retval);
void gene_pdo_last_insert_id(zval *pdo_object, char *name, zval *retval);
void gene_pdo_quote(zval *pdo_object, zend_string *str, zend_long param_type, zval *retval);
void gene_pdo_error_code(zval *pdo_object, zval *retval);
void gene_pdo_error_info(zval *pdo_object, zval *retval);
bool show_sql_errors(zval *pdo_object);
void gene_pdo_prepare(zval *pdo_object, char *sql, zval *retval);
void gene_pdo_rollback(zval *pdo_object, zval *retval);
void gene_pdo_get_attribute(zval *pdo_object, zend_long attr, zval *retval);
void gene_pdo_set_attribute(zval *pdo_object, zend_long attr, zend_long value);
void gene_pdo_statement_execute(zval *pdostatement_obj, zval *bind_parameters, zval *retval);
void gene_pdo_statement_fetch(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_fetch_all(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_fetch_column(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_fetch_object(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_row_count(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_set_fetch_mode(zval *pdostatement_obj, int fetch_style, zval *retval);
void jsonEncode(zval *data, zval *param);

void gene_insert_field_value (zval *fields, smart_str *field_str, smart_str *value_str,zval *field_value, char oq, char cq);
void gene_insert_field_value_batch(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value, char oq, char cq);
void gene_insert_field_value_batch_other(zval *fields, smart_str *value_str, zval *field_value);
void gene_update_field_value(zval *fields, smart_str *field_str, zval *field_value, char oq, char cq);
zend_bool gene_db_valid_identifier(zend_string *name);
int gene_db_build_join_on(zval *self, zend_class_entry *ce, const char *join_prop, size_t join_prop_len, const char *data_prop, size_t data_prop_len, zend_string *table, zval *predicates, zend_string *type, char oq, char cq);
int gene_db_build_arithmetic(zval *self, zend_class_entry *ce, const char *sql_prop, size_t sql_prop_len, const char *data_prop, size_t data_prop_len, zend_string *table, zend_string *column, double amount, zend_bool amount_is_long, zend_long amount_long, zend_bool increment, char oq, char cq);
void makeWhere(zval *self, smart_str *where_str, zval *where, zval *field_value);
bool checkPdoError(zend_object *ex);

/* [GENE_FIX:2026-08-19 N2] Discard ONLY the currently-pending exception.
 * zend_clear_exception() also releases EG(prev_exception), which is exactly
 * where zend_exception_save() stashes an in-flight business exception, so
 * calling it inside a save/restore window silently destroys the business
 * exception whenever e.g. PDO::rollBack() throws while the request unwinds.
 * Cleanup paths (tx hygiene, best-effort reset) must discard only the NEW
 * exception raised by the cleanup call itself. */
static zend_always_inline void gene_discard_current_exception(void)
{
	if (EG(exception)) {
		zend_object *ex = EG(exception);
		EG(exception) = NULL;
		OBJ_RELEASE(ex);
		if (EG(current_execute_data)) {
			EG(current_execute_data)->opline = EG(opline_before_exception);
		}
	}
}

/* [GENE_FIX:2026-08-19 N3] Shared transaction hygiene for every path that
 * releases a PDO handle: the DI registry scan, gene_pool_return_pdo(), and
 * the 4 drivers' free()/__destruct no-pool branches. Hardening rules
 * (P1-4 + N2 + N6 + N8): park any pending exception in a LOCAL variable
 * (reentrant, never touches EG(prev_exception)), roll back BEFORE warning,
 * force PDO::ERRMODE_SILENT around rollBack() so the cleanup path cannot
 * throw at all (frameless RSHUTDOWN would escalate the exception to E_ERROR
 * + bailout), keep gene_discard_current_exception() as second insurance,
 * and emit the E_WARNING with the user error handler bypassed under
 * zend_try so it is always restored. `who` completes the warning sentence
 * "... with an open transaction". */
void gene_db_tx_hygiene(zval *pdo_object, const char *who);

/* [GENE_PERF:2026-09-21 V3-3.2] Declared-property slot access for the four Db
 * drivers. Every driver declares the same 13 untyped properties in MINIT, so
 * their object-slot offsets are process constants; resolving them once removes
 * a properties_info hash lookup + visibility check from each of the ~80 reads
 * and writes a single SQL build performs. The offsets live in a per-driver
 * array indexed by gene_db_prop_id; ZTS is safe because every thread runs the
 * same MINIT declaration order and therefore gets identical offsets. */
typedef enum {
	GENE_DB_PROP_CONFIG = 0,
	GENE_DB_PROP_PDO,
	GENE_DB_PROP_SQL,
	GENE_DB_PROP_JOIN,
	GENE_DB_PROP_WHERE,
	GENE_DB_PROP_GROUP,
	GENE_DB_PROP_HAVING,
	GENE_DB_PROP_UNION,
	GENE_DB_PROP_ORDER,
	GENE_DB_PROP_LIMIT,
	GENE_DB_PROP_LOCK,
	GENE_DB_PROP_DATA,
	GENE_DB_PROP_POOL,
	GENE_DB_PROP_N
} gene_db_prop_id;

/* Fills offsets[GENE_DB_PROP_N] from ce->properties_info. Must be called at the
 * end of each driver MINIT, after zend_declare_property_null(). */
void gene_db_prop_offsets_init(zend_class_entry *ce, uint32_t *offsets);

/* Reads a declared slot. An unset() leaves IS_UNDEF behind; mirror
 * zend_read_property(silent=1) by reporting NULL for it. References are
 * dereferenced exactly like the by-name read did. */
static zend_always_inline zval *gene_db_prop_get(zend_object *obj, uint32_t offset)
{
	zval *slot = OBJ_PROP(obj, offset);
	if (UNEXPECTED(Z_TYPE_P(slot) == IS_UNDEF)) {
		return &EG(uninitialized_zval);
	}
	ZVAL_DEREF(slot);
	return slot;
}

/* Writes a declared slot with zend_update_property() semantics: assign through
 * an existing reference, addref the value, and drop the previous value only
 * after the new one is installed (self-assignment safe). */
static zend_always_inline void gene_db_prop_assign(zend_object *obj, uint32_t offset, zval *value)
{
	zval *slot = OBJ_PROP(obj, offset);
	zval garbage;
	if (UNEXPECTED(Z_ISREF_P(slot))) {
		slot = Z_REFVAL_P(slot);
	}
	ZVAL_COPY_VALUE(&garbage, slot);
	ZVAL_COPY(slot, value);
	if (Z_TYPE(garbage) != IS_UNDEF) {
		zval_ptr_dtor(&garbage);
	}
}

static zend_always_inline void gene_db_prop_set(zend_object *obj, uint32_t offset, zval *value)
{
	gene_db_prop_assign(obj, offset, value);
}

static zend_always_inline void gene_db_prop_set_null(zend_object *obj, uint32_t offset)
{
	zval tmp;
	ZVAL_NULL(&tmp);
	gene_db_prop_assign(obj, offset, &tmp);
}

static zend_always_inline void gene_db_prop_set_str(zend_object *obj, uint32_t offset, zend_string *value)
{
	zval tmp;
	ZVAL_STR(&tmp, value);
	gene_db_prop_assign(obj, offset, &tmp);
}

static zend_always_inline void gene_db_prop_set_stringl(zend_object *obj, uint32_t offset, const char *value, size_t len)
{
	zval tmp;
	ZVAL_STRINGL(&tmp, value, len);
	gene_db_prop_assign(obj, offset, &tmp);
	zval_ptr_dtor_str(&tmp);
}

static zend_always_inline void gene_db_prop_set_string(zend_object *obj, uint32_t offset, const char *value)
{
	gene_db_prop_set_stringl(obj, offset, value, strlen(value));
}

#endif
