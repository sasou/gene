/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01 S7] Atomic throttling over generic cache stores —
 * see throttle.c. */

#ifndef GENE_THROTTLE_H
#define GENE_THROTTLE_H

extern zend_class_entry *gene_throttle_ce;

GENE_MINIT_FUNCTION (throttle);

#endif
