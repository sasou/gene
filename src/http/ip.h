/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01 S1] Trusted-proxy aware client IP resolution.
 * One canonical source for directIp()/ip()/isInternal()/inCidr so apps stop
 * shipping divergent Ext\Ip copies (XFF left-most vs right-most). */

#ifndef GENE_IP_H
#define GENE_IP_H

/* gene_cidr_t is declared in ../gene.h (needed by zend_gene_globals). */

extern zend_class_entry *gene_ip_ce;

int  gene_ip_parse(const char *s, size_t len, unsigned char out[16]);
int  gene_cidr_match(const unsigned char ip[16], const gene_cidr_t *list, uint32_t n);
int  gene_ip_is_internal(const unsigned char ip[16]);
int  gene_ip_is_v4mapped(const unsigned char ip[16]);
int  gene_ip_is_public_str(const char *s, size_t len);

/* Worker-lifetime trusted-proxy config (pemalloc persistent; compile once,
 * rebuild only before workerReady / outside runtime_type>=2). */
void gene_trust_compile(zval *cfg);
void gene_trust_compile_ini(void);
void gene_trust_destroy(void);
zend_string *gene_trust_signature(zval *cfg);
int  gene_trust_same(zend_string *sig);
void gene_trust_set_sig(zend_string *sig);

zend_string *gene_request_direct_ip(void);
zend_string *gene_request_client_ip(void);
zend_bool gene_request_ip_is_internal(void);
int  gene_request_is_secure(void);

GENE_MINIT_FUNCTION (ip);

#endif
