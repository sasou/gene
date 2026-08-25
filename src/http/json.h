/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifndef GENE_JSON_H
#define GENE_JSON_H

extern zend_class_entry *gene_json_ce;

#ifndef JSON_UNESCAPED_UNICODE
#define JSON_UNESCAPED_UNICODE 256
#endif
#ifndef JSON_UNESCAPED_SLASHES
#define JSON_UNESCAPED_SLASHES 64
#endif
#ifndef JSON_THROW_ON_ERROR
#define JSON_THROW_ON_ERROR (1 << 22)
#endif

#define GENE_JSON_ENCODE_FLAGS (JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES)

/* Encode with UNESCAPED_UNICODE|UNESCAPED_SLASHES. Throws on failure.
 * Returns SUCCESS and sets *retval (IS_STRING), or FAILURE with exception. */
int gene_json_encode_throw(zval *value, zval *retval);
/* Decode as associative array/value. Empty string is a syntax error (throw).
 * Returns SUCCESS and sets *retval, or FAILURE with exception. */
int gene_json_decode_throw(zend_string *str, zval *retval);

GENE_MINIT_FUNCTION(json);

#endif
