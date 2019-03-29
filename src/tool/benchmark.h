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

#ifndef GENE_BENCHMARK_H
#define GENE_BENCHMARK_H
#define GENE_BENCHMARK_T "time"
#define GENE_BENCHMARK_M "memory"


extern zend_class_entry *gene_benchmark_ce;


GENE_MINIT_FUNCTION (benchmark);

void markStart(struct timeval *start, long *memory_start);
void markEnd(struct timeval *end, long *memory_end);
void getBenchTime(struct timeval *start, struct timeval *end, char **ret, zend_bool type);
void getBenchMemory(long *memory_start, long *memory_end, char **ret, zend_bool type);

#endif
