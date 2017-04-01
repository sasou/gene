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

#ifndef GENE_REQUEST_H
#define GENE_REQUEST_H

extern zend_class_entry *gene_request_ce;
zval * request_query(int type, char * name, int len TSRMLS_DC);


#define GENE_REQUEST_IS_METHOD(ce, x) \
PHP_METHOD(ce, is##x) {\
	zend_string *me;\
	if (!GENE_G(method)) { \
		RETURN_FALSE; \
	} \
	me = zend_string_init(GENE_G(method), strlen(GENE_G(method))+1, 0);\
	if (strncasecmp(#x, ZSTR_VAL(me), ZSTR_LEN(me)) == 0) { \
		zend_string_free(me);\
		RETURN_TRUE; \
	} \
	zend_string_free(me);\
	RETURN_FALSE; \
}

#define GENE_REQUEST_METHOD(ce, x, type) \
PHP_METHOD(ce, x) { \
	zend_string *name; \
    zval *ret; \
	zval *def = NULL; \
	if (ZEND_NUM_ARGS() == 0) { \
		ret = request_query(type, NULL, 0 TSRMLS_CC); \
	} else if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S|z", &name, &def) == FAILURE) { \
		return; \
	} else { \
		ret = request_query(type, ZSTR_VAL(name), ZSTR_LEN(name) TSRMLS_CC); \
		if (ret == NULL) { \
			if (def != NULL) { \
				RETURN_ZVAL(def, 1, 0); \
			} \
		} \
	} \
	if (ret) { \
	    RETURN_ZVAL(ret, 1, 0); \
	} else { \
		RETURN_NULL(); \
	} \
}

GENE_MINIT_FUNCTION(request);

#endif
