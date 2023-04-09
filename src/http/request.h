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

#ifndef GENE_REQUEST_H
#define GENE_REQUEST_H
#define GENE_REQUEST_PROPERTY_ATTR "_attr"

extern zend_class_entry *gene_request_ce;
zval * request_query(zend_ulong type, char * name, size_t len);
void setVal(zend_ulong type, zval *value);
zval *getVal(zend_ulong type, char *name, size_t len);

#define GENE_REQUEST_IS_METHOD(ce, x) \
PHP_METHOD(ce, is##x) {\
	zend_string *me;\
	if (!GENE_G(method)) { \
		RETURN_FALSE; \
	} \
	me = zend_string_init(GENE_G(method), strlen(GENE_G(method))+1, 0);\
	if (strncasecmp(#x, ZSTR_VAL(me), ZSTR_LEN(me)) == 0) { \
		zend_string_release(me);\
		RETURN_TRUE; \
	} \
	zend_string_release(me);\
	RETURN_FALSE; \
}

#define GENE_REQUEST_METHOD(ce, x, type) \
PHP_METHOD(ce, x) { \
	char *name = NULL; \
	size_t name_len = 0;  \
    zval *ret = NULL, *def = NULL; \
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|sz", &name, &name_len, &def) == FAILURE) { \
		return; \
	} \
	ret = getVal(type, name, name_len); \
	if (ret == NULL && def != NULL) { \
		RETURN_ZVAL(def, 1, 0); \
	} \
	if (ret) { \
		RETURN_ZVAL(ret, 1, 0); \
	} \
	RETURN_NULL();\
}

GENE_MINIT_FUNCTION (request);

#endif
