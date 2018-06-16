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

#ifndef GENE_DI_H
#define GENE_DI_H
#define GENE_DI_PROPERTY_INSTANCE "_instance"
#define GENE_DI_PROPERTY_REG "_reg"

extern zend_class_entry *gene_di_ce;

GENE_MINIT_FUNCTION (di);

zval *gene_di_instance();

zval *gene_di_get(zval *props, zend_string *name, zval *classObject);
zval *gene_di_get_easy(zend_string *name);

#endif
