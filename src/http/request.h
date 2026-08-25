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
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifndef GENE_REQUEST_H
#define GENE_REQUEST_H
extern zend_class_entry *gene_request_ce;
#define GENE_REQUEST_STACK_MAX 8
#define GENE_INVOKE_DEPTH_MAX 8
#define GENE_REQUEST_ATTR_RAW 8

zval * request_query(zend_ulong type, char * name, size_t len);
void gene_merge_query_into_get(const char *qs, size_t qs_len);
void setVal(zend_ulong type, zval *value);
zval *getVal(zend_ulong type, char *name, size_t len);
int gene_request_snapshot(zend_long *depth_out);
int gene_request_snapshot_ctx(gene_request_context *ctx, zend_long *depth_out);
int gene_request_restore(void);
int gene_request_restore_ctx(gene_request_context *ctx);
void gene_request_stack_drain(gene_request_context *ctx);
void gene_request_scope(zval *get, zval *post, zval *files, zval *request);

/* [GENE_PERF:2026-04-19 #2] Cache ctx once per is-method call — previously issued
 * 2 ctx lookups (one for NULL check, one for strcasecmp). Expanded across
 * isGet/isPost/isPut/isDelete/isHead/isOptions/isPatch for both request &
 * controller classes, so 14 methods all benefit. */
#define GENE_REQUEST_IS_METHOD(ce, x) \
PHP_METHOD(ce, is##x) {\
	gene_request_context *ctx = gene_request_ctx(); \
	if (!ctx->method) { \
		RETURN_FALSE; \
	} \
	if (strcasecmp(#x, ctx->method) == 0) { \
		RETURN_TRUE; \
	} \
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
