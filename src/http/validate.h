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

#ifndef GENE_VALIDATE_H
#define GENE_VALIDATE_H
#define GENE_VALIDATE_DATA "data"
#define GENE_VALIDATE_KEY  "key"
#define GENE_VALIDATE_FIELD  "field"
#define GENE_VALIDATE_METHOD  "method"
#define GENE_VALIDATE_CONFIG  "config"
#define GENE_VALIDATE_VALUE  "value"
#define GENE_VALIDATE_ERROR  "error"
#define GENE_VALIDATE_CLOSURE  "closure"

extern zend_class_entry *gene_validate_ce;


GENE_MINIT_FUNCTION(validate);

#endif
