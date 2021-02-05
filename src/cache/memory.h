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

#ifndef GENE_MEMORY_H
#define GENE_MEMORY_H
#define GENE_MEMORY_SAFE	"safe"
#define PHP_GENE_URL_PARAMS ":gene_url"
#define PALLOC_HASHTABLE(ht)   do {                                                       \
	(ht) = (HashTable*) pemalloc(sizeof(HashTable), 1);                                    \
	if ((ht) == NULL) {                                                                   \
		zend_error(E_ERROR, "Cannot allocate persistent HashTable, out enough memory?");  \
	}                                                                                     \
} while(0)

struct _filenode {
	zend_long stime;
	zend_long ftime;
	int validity;
	int status;
};

typedef struct _filenode filenode;

extern zend_class_entry *gene_memory_ce;

void gene_memory_init(TSRMLS_D);
void gene_hash_destroy(HashTable *ht);
void gene_memory_set(char *keyString, size_t keyString_len, zval *zvalue, int validity);
zval * gene_memory_get(char *keyString, size_t keyString_len);
zval * gene_memory_get_quick(char *keyString, size_t keyString_len);
zval * gene_memory_get_by_config(char *keyString, size_t keyString_len,char *path);
void gene_memory_set_by_router(char *keyString, size_t keyString_len, char *path, zval *zvalue, int validity);
zend_long gene_memory_getTime(char *keyString, size_t keyString_len);
int gene_memory_exists(char *keyString, size_t keyString_len);
int gene_memory_del(char *keyString, size_t keyString_len);
void file_cache_set_val(char *val, size_t keyString_len, zend_long times, int validity);
filenode * file_cache_get_easy(char *keyString, size_t keyString_len);
static zval * gene_memory_set_val(zval *val, char *keyString, size_t keyString_len, zval *zvalue);
void gene_memory_hash_copy_local(HashTable *target, HashTable *source);
zval * gene_memory_zval_local(zval *dst, zval *source);

GENE_MINIT_FUNCTION (memory);

#endif
