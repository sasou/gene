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

#ifndef GENE_VIEW_H
#define GENE_VIEW_H
#define GENE_VIEW_VIEW	"Views"
#define GENE_VIEW_EXT ".php"
#define PARSER_NUMS 28

extern zend_class_entry *gene_view_ce;

int gene_view_display(char *file TSRMLS_DC);
static int check_folder_exists(char *fullpath);
int gene_view_display_ext(char *file ,zend_bool isCompile TSRMLS_DC);
static int parser_templates(php_stream **stream, char *compile_path);
static int check_folder_exists(char *fullpath);

GENE_MINIT_FUNCTION (view);

#endif
