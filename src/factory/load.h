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

#ifndef GENE_LOAD_H
#define GENE_LOAD_H
#define GENE_LOAD_PROPERTY_INSTANCE "_instance"
#define GENE_SPL_AUTOLOAD_REGISTER_NAME "spl_autoload_register"
#define GENE_AUTOLOAD_FUNC_NAME 		"Gene_Load::autoload"
#define GENE_AUTOLOAD_FUNC_NAME_NS 		"Gene\\Load::autoload"

extern zend_class_entry *gene_load_ce;

int gene_load_import(char *path, zval *obj, zend_array *symbol_table);
void gene_load_file_by_class_name (char *className);
zval *gene_load_instance(zval *this_ptr);
int gene_loader_register();

GENE_MINIT_FUNCTION (load);

#endif
