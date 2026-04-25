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

#ifndef GENE_MEMORY_H
#define GENE_MEMORY_H
#define GENE_MEMORY_SAFE	"safe"
#define PHP_GENE_URL_PARAMS ":gene_url"
/* Skip read locks when worker_ready is set: after workerReady() the
 * persistent cache is read-only, so read locks only add contention
 * across Swoole coroutines without providing safety.  In FPM mode
 * worker_ready is always 0, so locks are always taken (correct). */
#define GENE_CACHE_RDLOCK()    do { if (!GENE_G(worker_ready)) gene_rwlock_rdlock(&GENE_G(cache_lock)); } while(0)
#define GENE_CACHE_RDUNLOCK()  do { if (!GENE_G(worker_ready)) gene_rwlock_rdunlock(&GENE_G(cache_lock)); } while(0)
#define GENE_CACHE_WRLOCK()    gene_rwlock_wrlock(&GENE_G(cache_lock))
#define GENE_CACHE_WRUNLOCK()  gene_rwlock_wrunlock(&GENE_G(cache_lock))

/* Internal: cache.c only — bracket each gene_memory_set/del from Cache methods.
 * Do not span gene_cache_call() or other userland; see gene_memory_write_allowed. */
#define GENE_CACHE_LAYER_MEMORY_WRITE_ENTER() (GENE_G(cache_layer_memory_write_depth)++)
#define GENE_CACHE_LAYER_MEMORY_WRITE_LEAVE() do { \
	if (GENE_G(cache_layer_memory_write_depth) > 0) { \
		GENE_G(cache_layer_memory_write_depth)--; \
	} \
} while (0)

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

void gene_memory_init();
int gene_memory_write_allowed(const char *op);
void gene_hash_destroy(HashTable *ht);
void gene_memory_set(char *keyString, size_t keyString_len, zval *zvalue, int validity);
zval * gene_memory_get(char *keyString, size_t keyString_len);
/* [GENE_PERF:2026-04-19] gene_memory_get_quick collapsed to macro — it was an alias
 * of gene_memory_get. Macro expansion eliminates one function call per lookup on
 * the hottest router dispatch path (router_tree/router_event/router_conf lookups
 * per request). Signature preserved for source-level compatibility. */
#define gene_memory_get_quick(k, l) gene_memory_get((k), (l))
/* [GENE_PERF:2026-04-24 v5.5.8] Batched single-lock lookup for the 3 consecutive
 * persistent-cache reads that happen on every router dispatch (tree/event/conf).
 * Reduces 3× RDLOCK/RDUNLOCK atomics to 1× under ZTS and under non-ZTS prior to
 * workerReady(). Any out1/out2/out3 may be NULL if not needed. */
void gene_memory_get_triple(
	const char *k1, size_t k1_len, zval **out1,
	const char *k2, size_t k2_len, zval **out2,
	const char *k3, size_t k3_len, zval **out3);
zval * gene_memory_get_by_config(char *keyString, size_t keyString_len,char *path);
void gene_memory_set_by_router(char *keyString, size_t keyString_len, char *path, zval *zvalue, int validity);
zend_long gene_memory_getTime(char *keyString, size_t keyString_len);
int gene_memory_exists(char *keyString, size_t keyString_len);
int gene_memory_del(char *keyString, size_t keyString_len);
void file_cache_set_val(char *val, size_t keyString_len, zend_long times, int validity);
filenode * file_cache_get_easy(char *keyString, size_t keyString_len);

void gene_memory_hash_copy_local(HashTable *target, HashTable *source);
zval * gene_memory_zval_local(zval *dst, zval *source);

GENE_MINIT_FUNCTION (memory);

#endif
