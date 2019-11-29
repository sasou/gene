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
  | Author: Sasou  <admin@caophp.com>                                    |
  +----------------------------------------------------------------------+
*/

#ifndef GENE_FACTORY_H
#define GENE_FACTORY_H


extern zend_class_entry *gene_factory_ce;


GENE_MINIT_FUNCTION(factory);


zend_bool gene_factory(char *className, int tmp_len, zval *params, zval *classObject);
void gene_factory_call(zval *object, char *action, zval *param, zval *retval);
void gene_factory_call_1(zval *object, char *action, zval *param, zval *retval);
void gene_factory_call_2(char *method, zval *key, zval *retval);
void gene_factory_function_call(char *action, zval *param_key, zval *param_arr, zval *retval);
void gene_factory_function_call_1(zval *function_name, zval *param_key, zval *param_arr, zval *retval);
zend_bool gene_factory_load_class(char *className, int tmp_len, zval *classObject);

#endif
