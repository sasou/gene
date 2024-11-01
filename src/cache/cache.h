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

#ifndef GENE_CACHE_H
#define GENE_CACHE_H
#define GENE_CACHE_CONFIG "config"
#define GENE_CACHE_TMP "tmp:"

extern zend_class_entry *gene_cache_ce;

void makeArgsArr(zval *arr, smart_str *tmp_s);
void makeArgsKey(zend_ulong indexs, zend_string *id, zval *element, smart_str *tmp_s);
GENE_MINIT_FUNCTION(cache);

#endif
