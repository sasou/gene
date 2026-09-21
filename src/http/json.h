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

#include "zend_smart_str_public.h"

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

/* [GENE_PERF:2026-09-20 V3-2.7] Encode straight into a smart_str, skipping
 * the zend_call_known_function frame and the returned-zval/string round
 * trip. Mirrors json_encode()'s contract exactly: SUCCESS means buf->s
 * holds the encoded text (NULL-safe = empty output); FAILURE means
 * json_encode() would have returned false, and a pending EG(exception)
 * (JsonSerialize callback, or JSON_THROW_ON_ERROR) still propagates.
 * When options carries JSON_THROW_ON_ERROR the VM path is used internally
 * so JsonException construction stays byte-identical. */
int gene_json_encode_buf(smart_str *buf, zval *value, zend_long options);

GENE_MINIT_FUNCTION(json);

#endif
