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

#ifndef GENE_DB_POOL_H
#define GENE_DB_POOL_H

#define GENE_POOL_PROPERTY_CHANNEL   "channel"
#define GENE_POOL_PROPERTY_CONFIG    "config"
#define GENE_POOL_PROPERTY_MIN       "min"
#define GENE_POOL_PROPERTY_MAX       "max"
#define GENE_POOL_PROPERTY_IDLE_TIME "idleTimeout"
#define GENE_POOL_PROPERTY_WAIT_TIME "waitTimeout"
#define GENE_POOL_PROPERTY_COUNT     "currentCount"
#define GENE_POOL_PROPERTY_TIMER_ID  "timerId"
#define GENE_POOL_PROPERTY_CLOSED    "closed"
#define GENE_POOL_PROPERTY_INSTANCES "instances"

extern zend_class_entry *gene_pool_ce;

GENE_MINIT_FUNCTION(pool);

/* Shared helper functions for all DB classes to use pool */
bool gene_pool_get_pdo(zend_class_entry *db_ce, zval *self, zval *config, const char *pool_key, size_t pool_key_len, const char *pdo_key, size_t pdo_key_len);
void gene_pool_return_pdo(zend_class_entry *db_ce, zval *self, const char *pool_key, size_t pool_key_len, const char *pdo_key, size_t pdo_key_len);
void gene_pool_notify_remove(zend_class_entry *db_ce, zval *self, const char *pool_key, size_t pool_key_len);

#endif
