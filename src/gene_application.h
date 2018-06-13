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

#ifndef GENE_APPLICATION_H
#define GENE_APPLICATION_H
#define GENE_APPLICATION_INSTANCE "instance"
#define GENE_APPLICATION_SAFE "safe"
#define GENE_APPLICATION_ATTR "attr"

extern zend_class_entry *gene_application_ce;

void load_file(char *key, int key_len,char *php_script, int validity TSRMLS_DC);
int gene_file_modified(char *file, long ctime TSRMLS_DC);
void gene_ini_router();
void gene_router_set_uri(zval **leaf TSRMLS_DC);

GENE_MINIT_FUNCTION (application);

#endif
