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

#ifndef GENE_LOG_H
#define GENE_LOG_H

#define GENE_LOG_LEVEL_DEBUG   1
#define GENE_LOG_LEVEL_INFO    2
#define GENE_LOG_LEVEL_NOTICE  3
#define GENE_LOG_LEVEL_WARNING 4
#define GENE_LOG_LEVEL_ERROR   5

extern zend_class_entry *gene_log_ce;

GENE_MINIT_FUNCTION(log);

#endif
