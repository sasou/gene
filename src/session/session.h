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

#ifndef GENE_SESSION_H
#define GENE_SESSION_H
#define GENE_SESSION_DRIVER "driver"
#define GENE_SESSION_DATA  "data"
#define GENE_SESSION_ID  "session_id"
#define GENE_SESSION_NAME  "session_name"
#define GENE_SESSION_PREFIX  "session_prefix"
#define GENE_SESSION_COOKIE_ID  "cookie_id"
#define GENE_SESSION_COOKIE_LIFTTIME  "cookie_lifetime"
#define GENE_SESSION_COOKIE_UPTIME  "cookie_uptime"
#define GENE_SESSION_COOKIE_DOMAIN  "cookie_domain"
#define GENE_SESSION_COOKIE_PATH  "cookie_path"
#define GENE_SESSION_SECURE  "secure"
#define GENE_SESSION_HTTPONLY  "httponly"
#define GENE_SESSION_SAMESITE  "samesite"
#define GENE_SESSION_HASH_MODE  "hash_mode"
#define GENE_SESSION_HANDLER  "_handler"
#define GENE_SESSION_DIRTY  "_dirty"
#define GENE_SESSION_COOKIE_SENT  "_cookie_sent"
#define GENE_SESSION_COOKIE_NEW  "_cookie_new"

extern zend_class_entry *gene_session_ce;


GENE_MINIT_FUNCTION (session);

#endif
