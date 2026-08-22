/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */

#ifndef GENE_CONTEXT_H
#define GENE_CONTEXT_H

extern zend_class_entry *gene_context_ce;

/* Lazy request-bag accessor. Returns &ctx->user_bag (IS_ARRAY). */
zval *gene_context_bag(void);

GENE_MINIT_FUNCTION(context);

#endif
