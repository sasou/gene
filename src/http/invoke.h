#ifndef GENE_INVOKE_H
#define GENE_INVOKE_H

extern zend_class_entry *gene_invoke_ce;

int gene_invoke_local(const char *class_name, size_t class_len,
	const char *action, size_t action_len, zval *params, zval *files, zval *retval);

GENE_MINIT_FUNCTION(invoke);

#endif
