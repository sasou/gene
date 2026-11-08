/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01] Security policy engine: same-origin/CSRF policy,
 * internal REST guard, security response headers, JSONP validation and
 * dynamic-route GET readonly guard. Configs live as Application static
 * properties (same pattern as requestIdConfig); enforcement runs in the
 * single run() choke point before webscan so route order cannot bypass it. */

#ifndef GENE_POLICY_H
#define GENE_POLICY_H

#define GENE_APPLICATION_CSRF_CONFIG "csrfConfig"
#define GENE_APPLICATION_REST_CONFIG "restGuardConfig"
#define GENE_APPLICATION_HEADERS_CONFIG "secureHeadersConfig"
#define GENE_APPLICATION_READONLY_CONFIG "readonlyConfig"
#define GENE_APPLICATION_THROTTLE_CONFIG "throttleConfig"

extern zend_class_entry *gene_csrf_ce;

/* Apply configured default security headers at dispatch start (userland
 * header() calls later win via SAPI_HEADER_REPLACE / Swoole overwrite). */
void gene_policy_headers_apply(void);

/* Run the request policy (csrf + restGuard + readonly). Returns 1 when the
 * request was answered/denied and dispatch must be skipped (webscan-style). */
int gene_policy_check(const char *method, size_t mlen, const char *path, size_t plen);

/* JSONP callback whitelist validator (strict app variant). */
int gene_jsonp_valid(const char *s, size_t n);

/* Length of a leading router->lang() segment ("/zh" in "/zh/admin/x"), 0 when
 * the first segment is not a registered language. */
size_t gene_policy_lang_skip(const char *path, size_t plen);

/* Simple case-insensitive glob: '*' any run, '?' single char. */
int gene_path_glob(const char *pat, size_t plen, const char *s, size_t slen);

/* Shared by Crypto::signRequest / Rest client signing / restGuard verify. */
zend_string *gene_policy_sign_request(zend_string *secret, const char *method, size_t mlen,
	const char *path, size_t plen, zend_long ts, zend_string *nonce, zend_string *body);
zend_bool gene_policy_nonce_check(zend_string *component, zend_string *key, zend_long ttl);
/* Atomic single-use marker: 1 first use, 0 seen, -1 store error. */
int gene_store_once(zval *store, zend_string *key, zend_long ttl);
zend_string *policy_b64url(const char *data, size_t len);
zend_string *policy_random_bytes(zend_long n);

/* Session-backed CSRF token helpers (Gene\Csrf + csrf() token mode). */
zend_string *gene_csrf_session_token(zval *cfg);
zend_bool gene_csrf_check_token(zval *cfg, zend_string *given);

GENE_MINIT_FUNCTION (policy);

#endif
