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

#ifndef GENE_DB_H
#define GENE_DB_H
#define GENE_DB_CONFIG "config"
#define GENE_DB_PDO "pdo"
#define GENE_DB_TYPE "type"
#define GENE_DB_SQL "sql"
#define GENE_DB_WHERE "where"
#define GENE_DB_GROUP "group"
#define GENE_DB_HAVING "having"
#define GENE_DB_ORDER "order"
#define GENE_DB_LIMIT "limit"
#define GENE_DB_DATA "data"
#define GENE_DB_HISTORY "history"


extern zend_class_entry *gene_db_ce;


GENE_MINIT_FUNCTION(db);

#endif
