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
/* [GENE_FEATURE:2026-08-07] bail(): stop validation at the first failure. */
#define GENE_VALIDATE_BAIL  "bail"
/* [GENE_FEATURE:2026-08-07] sometimes(): per-field conditional callback.
 * Stores an array<field, callable($data):bool> on the instance; validCheck()
 * skips a field's rules when its callback returns false. */
#define GENE_VALIDATE_SOMETIMES  "sometimes"
#define GENE_VALIDATE_MOBILE "/^(13[0-9]|14[01456879]|15[0-35-9]|16[2567]|17[0-8]|18[0-9]|19[0-35-9])\\d{8}$/"
#define GENE_VALIDATE_DATE "/^\\d{4}[\\/-]\\d{1,2}[\\/-]\\d{1,2}$/"

extern zend_class_entry *gene_validate_ce;


GENE_MINIT_FUNCTION(validate);

#endif
