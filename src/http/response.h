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

#ifndef GENE_RESPONSE_H
#define GENE_RESPONSE_H
#define GENE_RESPONSE_CODE "code"
#define GENE_RESPONSE_MSG  "msg"
#define GENE_RESPONSE_DATA  "data"
#define GENE_RESPONSE_COUNT  "count"

extern zend_class_entry *gene_response_ce;

void gene_response_set_redirect(char *url, zend_long code);

GENE_MINIT_FUNCTION (response);

#endif
