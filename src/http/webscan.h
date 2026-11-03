/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 */
#ifndef GENE_WEBSCAN_H
#define GENE_WEBSCAN_H

extern zend_class_entry *gene_webscan_ce;

int gene_webscan_scan(zval *white_dir, zval *white_urls, int scan_get_on, int scan_post_on,
    int scan_cookie_on, int scan_referer_on, int fail_closed);

GENE_MINIT_FUNCTION (webscan);

#endif
