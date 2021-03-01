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
 | Author: Sasou  <admin@php-gene.com> web:www.php-gene.com             |
 +----------------------------------------------------------------------+
 */

#ifndef GENE_DB_PDO_H
#define GENE_DB_PDO_H

#include "zend_smart_str_public.h"

void array_to_string(zval *array, char **result);
void mssql_array_to_string(zval *array, char **result);
void gene_pdo_construct(zval *pdo_object, zval *dsn, zval *user, zval *pass, zval *options);
void gene_pdo_begin_transaction(zval *pdo_object, zval *retval);
void gene_pdo_commit(zval *pdo_object, zval *retval);
void gene_pdo_exec(zval *pdo_object, char *sql, zval *retval);
void gene_pdo_in_transaction(zval *pdo_object, zval *retval);
void gene_pdo_last_insert_id(zval *pdo_object, char *name, zval *retval);
void gene_pdo_error_code(zval *pdo_object, zval *retval);
void gene_pdo_error_info(zval *pdo_object, zval *retval);
zend_bool show_sql_errors(zval *pdo_object);
void gene_pdo_prepare(zval *pdo_object, char *sql, zval *retval);
void gene_pdo_rollback(zval *pdo_object, zval *retval);
void gene_pdo_statement_execute(zval *pdostatement_obj, zval *bind_parameters, zval *retval);
void gene_pdo_statement_fetch(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_fetch_all(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_fetch_column(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_fetch_object(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_row_count(zval *pdostatement_obj, zval *retval);
void gene_pdo_statement_set_fetch_mode(zval *pdostatement_obj, int fetch_style, zval *retval);
void jsonEncode(zval *data, zval *param);

void gene_insert_field_value (zval *fields, smart_str *field_str, smart_str *value_str,zval *field_value);
void gene_insert_field_value_batch(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value);
void gene_insert_field_value_batch_other(zval *fields, smart_str *value_str, zval *field_value);
void gene_update_field_value(zval *fields, smart_str *field_str, zval *field_value);

void gene_mysql_insert_field_value (zval *fields, smart_str *field_str, smart_str *value_str,zval *field_value);
void gene_mysql_insert_field_value_batch(zval *fields, smart_str *field_str, smart_str *value_str, zval *field_value);
void gene_mysql_update_field_value(zval *fields, smart_str *field_str, zval *field_value);

void makeWhere(zval *self, smart_str *where_str, zval *where, zval *field_value);
zend_bool checkPdoError(zend_object *ex);

#endif
