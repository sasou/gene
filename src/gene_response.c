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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"


#include "php_gene.h"
#include "gene_response.h"

zend_class_entry * gene_response_ce;

/** {{{ int gene_response_set_redirect(zval *response, char *url, long code TSRMLS_DC)
*/
int gene_response_set_redirect(char *url, long code TSRMLS_DC) {
	sapi_header_line ctr = {0};

	ctr.line_len = spprintf(&(ctr.line), 0, "%s %s", "Location:", url);
	ctr.response_code = code;
	if (sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC) == SUCCESS) {
		efree(ctr.line);
		return 1;
	}
	efree(ctr.line);
	return 0;
}
/* }}} */


/*
 * {{{ gene_response
 */
PHP_METHOD(gene_response, __construct)
{
	long debug = 0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &debug) == FAILURE)
    {
        RETURN_NULL();
    }
}
/* }}} */

/** {{{ proto public gene_response::redirect(string $url)
*/
PHP_METHOD(gene_response, redirect) {
  char  *url;
  int  url_len;
  long  code = 302;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &url, &url_len, &code) == FAILURE) {
    return;
  }

  if (!url_len) {
    RETURN_FALSE;
  }

  RETURN_BOOL(gene_response_set_redirect(url, code TSRMLS_CC));
}
/* }}} */


/** {{{ proto public gene_response::alert(string $text, string $url = NULL)
*/
PHP_METHOD(gene_response, alert) {
  char  *text,*url;
  int  text_len=0,url_len=0;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &text, &text_len, &url, &url_len) == FAILURE) {
	  return;
  }
  php_printf("\n<script type=\"text/javascript\">\nalert(\"%s\");\n" , text);
  if (url_len) {
	  php_printf("window.location.href=\"%s\";\n" , url);
  }
  php_printf("</script>\n" , text);
  RETURN_TRUE;
}
/* }}} */

/*
 * {{{ gene_response_methods
 */
zend_function_entry gene_response_methods[] = {
		PHP_ME(gene_response, redirect, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_response, alert, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
		PHP_ME(gene_response, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		{NULL, NULL, NULL}
};
/* }}} */


/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(response)
{
    zend_class_entry gene_response;
    GENE_INIT_CLASS_ENTRY(gene_response, "gene_response",  "gene\\response", gene_response_methods);
    gene_response_ce = zend_register_internal_class(&gene_response TSRMLS_CC);

	//debug
    //zend_declare_property_null(gene_application_ce, GENE_EXECUTE_DEBUG, strlen(GENE_EXECUTE_DEBUG), ZEND_ACC_PUBLIC TSRMLS_CC);
    //
	return SUCCESS;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
