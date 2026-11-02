/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 | Author: Sasou  <zohocodes@outlook.com> web:www.1xm.net             |
 +----------------------------------------------------------------------+
 */
/* [GENE_SEC:2026-11-01 S1] Trusted-proxy aware client IP resolution. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include "zend_smart_str.h"
#include "ext/standard/php_string.h"
#include <string.h>

#include "../gene.h"
#include "../http/request.h"
#include "../http/ip.h"
#include "../exception/exception.h"

zend_class_entry *gene_ip_ce;

/* {{{ gene_ip_parse — parse "a.b.c.d" or IPv6 literal into 16 bytes.
 * v4 results are stored as ::ffff:a.b.c.d so a single matcher serves both
 * families. Returns 1 on success. */
int gene_ip_parse(const char *s, size_t len, unsigned char out[16]) {
	const char *p, *end;
	uint32_t parts[4];
	int i, is_v6 = 0;

	if (!s || len == 0 || len > 47) {
		return 0;
	}
	for (i = 0; (size_t)i < len; i++) {
		if (s[i] == ':') { is_v6 = 1; break; }
	}
	if (!is_v6) {
		unsigned long v;
		p = s; end = s + len;
		for (i = 0; i < 4; i++) {
			v = 0;
			if (p >= end || *p < '0' || *p > '9') return 0;
			/* reject leading zeros ("010"): octal in some parsers, never
			 * emitted by proxies — ambiguity is a matching hazard */
			if (*p == '0' && p + 1 < end && p[1] >= '0' && p[1] <= '9') return 0;
			while (p < end && *p >= '0' && *p <= '9') {
				v = v * 10 + (unsigned long)(*p - '0');
				if (v > 255) return 0;
				p++;
			}
			parts[i] = v;
			if (i < 3) {
				if (p >= end || *p != '.') return 0;
				p++;
			}
		}
		if (p != end) return 0;
		memset(out, 0, 10);
		out[10] = 0xff; out[11] = 0xff;
		out[12] = (unsigned char)parts[0]; out[13] = (unsigned char)parts[1];
		out[14] = (unsigned char)parts[2]; out[15] = (unsigned char)parts[3];
		return 1;
	}
	/* IPv6: :: expansion, optional embedded v4 tail, optional [ ] brackets.
	 * Strict: every byte must be consumed — trailing garbage, a lone leading
	 * or trailing ':', 5+ hex digits, >8 groups or a '::' standing for zero
	 * groups all reject (a lenient parser lets malformed XFF entries match
	 * CIDRs they should not). */
	{
		unsigned short grp[8];
		int head = 0, pre = -1, i2;
		const char *q = s, *e = s + len;
		if (s[0] == '[') {
			if (len < 3 || s[len - 1] != ']') return 0;
			q = s + 1; e = s + len - 1;
		}
		memset(grp, 0, sizeof(grp));
		if (e - q >= 2 && q[0] == ':' && q[1] == ':') {
			pre = 0;
			q += 2;
		} else if (q < e && q[0] == ':') {
			return 0;
		}
		while (q < e) {
			unsigned long val = 0;
			const char *c;
			if (head >= 8) return 0;
			for (c = q; c < e && *c != ':' && *c != '.'; c++);
			if (c < e && *c == '.') {
				/* embedded v4 tail must run to the end */
				unsigned char v4[16];
				if (head > 6 || !gene_ip_parse(q, (size_t)(e - q), v4)) return 0;
				grp[head++] = ((unsigned short)v4[12] << 8) | v4[13];
				grp[head++] = ((unsigned short)v4[14] << 8) | v4[15];
				q = e;
				break;
			}
			for (i2 = 0; q + i2 < e && i2 < 5; i2++) {
				char ch = q[i2];
				unsigned int d;
				if (ch >= '0' && ch <= '9') d = (unsigned int)(ch - '0');
				else if (ch >= 'a' && ch <= 'f') d = (unsigned int)(ch - 'a' + 10);
				else if (ch >= 'A' && ch <= 'F') d = (unsigned int)(ch - 'A' + 10);
				else break;
				val = (val << 4) | d;
			}
			if (i2 == 0 || i2 > 4) return 0;
			grp[head++] = (unsigned short)val;
			q += i2;
			if (q == e) break;
			if (*q != ':') return 0;
			q++;
			if (q < e && *q == ':') {
				if (pre >= 0) return 0; /* second :: */
				pre = head;
				q++;
			} else if (q == e) {
				return 0; /* trailing single ':' */
			}
		}
		if (pre < 0 ? head != 8 : head > 7) return 0;
		if (pre >= 0) {
			/* '::' expands between pre and the groups parsed after it:
			 * grp[pre..head-1] move to the right edge, zeros fill the middle. */
			int post = head - pre;
			memmove(&grp[8 - post], &grp[pre], post * sizeof(unsigned short));
			memset(&grp[pre], 0, (8 - pre - post) * sizeof(unsigned short));
		}
		for (i2 = 0; i2 < 8; i2++) {
			out[i2 * 2] = (unsigned char)(grp[i2] >> 8);
			out[i2 * 2 + 1] = (unsigned char)(grp[i2] & 0xff);
		}
		return 1;
	}
}
/* }}} */

/* {{{ gene_cidr_match */
int gene_cidr_match(const unsigned char ip[16], const gene_cidr_t *list, uint32_t n) {
	uint32_t i;
	for (i = 0; i < n; i++) {
		const gene_cidr_t *c = &list[i];
		uint8_t full = c->prefix >> 3, rem = c->prefix & 7;
		if (c->prefix == 0) {
			return 1;
		}
		if (full && memcmp(ip, c->addr, full) != 0) {
			continue;
		}
		if (rem) {
			unsigned char mask = (unsigned char)(0xff << (8 - rem));
			if ((ip[full] & mask) != (c->addr[full] & mask)) {
				continue;
			}
		}
		return 1;
	}
	return 0;
}
/* }}} */

int gene_ip_is_v4mapped(const unsigned char ip[16]) {
	static const unsigned char pfx[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
	return memcmp(ip, pfx, 12) == 0;
}

/* {{{ gene_ip_is_internal — RFC1918, loopback, link-local, CGNAT, ULA fc00::/7, fe80::/10, ::1 */
int gene_ip_is_internal(const unsigned char ip[16]) {
	/* ::1 */
	static const unsigned char loop6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
	if (memcmp(ip, loop6, 16) == 0) {
		return 1;
	}
	/* IPv6 ULA fc00::/7 and link-local fe80::/10 */
	if ((ip[0] & 0xfe) == 0xfc) return 1;
	if (ip[0] == 0xfe && (ip[1] & 0xc0) == 0x80) return 1;
	/* v4-mapped only below. The full ::ffff:0:0/96 prefix must match — a
	 * global v6 address whose interface id happens to carry ffff:0a00:xxxx
	 * (attacker-chosen within its own /64) must not read as 10.x. */
	if (!gene_ip_is_v4mapped(ip)) {
		return 0;
	}
	{
		unsigned char a = ip[12], b = ip[13];
		if (a == 127 || a == 10 || a == 0) return 1;
		if (a == 172 && b >= 16 && b <= 31) return 1;
		if (a == 192 && b == 168) return 1;
		if (a == 169 && b == 254) return 1;          /* link-local */
		if (a == 100 && (b & 0xc0) == 64) return 1;  /* CGNAT 100.64/10 */
	}
	return 0;
}
/* }}} */

int gene_ip_is_public_str(const char *s, size_t len) {
	unsigned char bin[16];
	if (!gene_ip_parse(s, len, bin)) {
		return 0;
	}
	return !gene_ip_is_internal(bin);
}

/* {{{ trust table compile/destroy */
static void gene_trust_free(void) {
	if (GENE_G(trust_cidrs)) {
		pefree(GENE_G(trust_cidrs), 1);
		GENE_G(trust_cidrs) = NULL;
	}
	GENE_G(trust_cidr_n) = 0;
	if (GENE_G(trust_hdrs)) {
		uint32_t i;
		for (i = 0; i < GENE_G(trust_hdr_n); i++) {
			zend_string_release(GENE_G(trust_hdrs)[i]);
		}
		pefree(GENE_G(trust_hdrs), 1);
		GENE_G(trust_hdrs) = NULL;
	}
	GENE_G(trust_hdr_n) = 0;
	GENE_G(trust_proto) = 0;
	GENE_G(trust_compiled) = 0;
}

void gene_trust_destroy(void) {
	gene_trust_free();
	if (GENE_G(trust_sig)) {
		zend_string_release(GENE_G(trust_sig));
		GENE_G(trust_sig) = NULL;
	}
}

/* Canonical signature of a trustProxies() config: entry scripts re-run the
 * whole bootstrap chain per request (FPM, and Swoole onRequest), so an
 * identical config must be a cheap no-op instead of a per-request rebuild —
 * and must not trip the post-workerReady guard. */
static void gene_trust_sig_append(smart_str *s, zval *v) {
	if (!v) {
		return;
	}
	if (Z_TYPE_P(v) == IS_STRING) {
		smart_str_append(s, Z_STR_P(v));
	} else if (Z_TYPE_P(v) == IS_ARRAY) {
		zval *item;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(v), item) {
			if (Z_TYPE_P(item) == IS_STRING) {
				smart_str_append(s, Z_STR_P(item));
				smart_str_appendc(s, ',');
			}
		} ZEND_HASH_FOREACH_END();
	}
}

zend_string *gene_trust_signature(zval *cfg) {
	smart_str s = {0};
	smart_str_appends(&s, "t1|");
	if (cfg && Z_TYPE_P(cfg) == IS_ARRAY) {
		zval *proto = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("proto"));
		gene_trust_sig_append(&s, zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("proxies")));
		smart_str_appendc(&s, '|');
		gene_trust_sig_append(&s, zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("headers")));
		smart_str_appendc(&s, '|');
		smart_str_appendc(&s, (!proto || zend_is_true(proto)) ? '1' : '0');
	}
	smart_str_0(&s);
	return s.s;
}

int gene_trust_same(zend_string *sig) {
	return GENE_G(trust_compiled) && GENE_G(trust_sig) && zend_string_equals(GENE_G(trust_sig), sig);
}

void gene_trust_set_sig(zend_string *sig) {
	if (GENE_G(trust_sig)) {
		zend_string_release(GENE_G(trust_sig));
	}
	GENE_G(trust_sig) = zend_string_init(ZSTR_VAL(sig), ZSTR_LEN(sig), 1);
}

/* Parse a comma-separated "ip[/len]" list into persistent gene_cidr_t[]. */
static uint32_t gene_cidr_list_parse(const char *s, size_t len, gene_cidr_t **out) {
	gene_cidr_t *list;
	uint32_t n = 0, cap = 8;
	const char *p = s, *end = s + len;
	list = (gene_cidr_t *)pemalloc(cap * sizeof(gene_cidr_t), 1);
	while (p < end) {
		const char *comma = memchr(p, ',', (size_t)(end - p));
		const char *item_end = comma ? comma : end;
		const char *a = p, *b = item_end;
		const char *slash;
		unsigned char bin[16];
		long prefix = -1;
		int bad = 0;
		while (a < b && (*a == ' ' || *a == '\t')) a++;
		while (b > a && (b[-1] == ' ' || b[-1] == '\t')) b--;
		if (b == a) { /* empty item (",," / trailing comma) */
			p = item_end + 1;
			continue;
		}
		slash = memchr(a, '/', (size_t)(b - a));
		if (slash) {
			/* 1..3 digits required: "10.0.0.1/" must not become /0 (= trust all) */
			const char *q = slash + 1;
			long v = 0;
			int nd = 0;
			while (q < b && *q >= '0' && *q <= '9' && nd < 4) { v = v * 10 + (*q - '0'); q++; nd++; }
			if (q == b && nd >= 1 && nd <= 3) {
				prefix = v;
			} else {
				bad = 1;
			}
			b = slash;
		}
		if (!bad && b > a && gene_ip_parse(a, (size_t)(b - a), bin)) {
			/* textual family decides the prefix space: v4 "/n" (n<=32) maps to
			 * 96+n in ::ffff:0:0/96; a v6 literal keeps its prefix verbatim */
			zend_bool v4text = memchr(a, ':', (size_t)(b - a)) == NULL;
			long eff = prefix < 0 ? 128 : (v4text ? 96 + prefix : prefix);
			if ((v4text && prefix > 32) || eff > 128) {
				bad = 1;
			} else {
				if (n >= cap) {
					cap *= 2;
					list = (gene_cidr_t *)perealloc(list, cap * sizeof(gene_cidr_t), 1);
				}
				memcpy(list[n].addr, bin, 16);
				list[n].prefix = (uint8_t)eff;
				n++;
			}
		} else {
			bad = 1;
		}
		if (bad) {
			php_error_docref(NULL, E_WARNING, "Gene: ignoring invalid CIDR entry '%.*s'", (int)(item_end - a), a);
		}
		p = item_end + 1;
	}
	*out = list;
	return n;
}

/* Parse a comma-separated header-name list into persistent lowercase zend_string*[]. */
static uint32_t gene_header_list_parse(const char *s, size_t len, zend_string ***out) {
	zend_string **list;
	uint32_t n = 0, cap = 4;
	const char *p = s, *end = s + len;
	list = (zend_string **)pemalloc(cap * sizeof(zend_string *), 1);
	while (p < end) {
		const char *comma = memchr(p, ',', (size_t)(end - p));
		const char *item_end = comma ? comma : end;
		const char *a = p, *b = item_end;
		while (a < b && (*a == ' ' || *a == '\t')) a++;
		while (b > a && (b[-1] == ' ' || b[-1] == '\t')) b--;
		if (b > a) {
			char *buf = estrndup(a, (size_t)(b - a)); /* request-scope temp */
			size_t i;
			for (i = 0; i < (size_t)(b - a); i++) {
				unsigned char c = (unsigned char)buf[i];
				buf[i] = (c == '_') ? '-' : ((c >= 'A' && c <= 'Z') ? (char)(c | 0x20) : (char)c);
			}
			if (n >= cap) {
				cap *= 2;
				list = (zend_string **)perealloc(list, cap * sizeof(zend_string *), 1);
			}
			list[n++] = zend_string_init(buf, (size_t)(b - a), 1); /* persistent */
			efree(buf);
		}
		p = item_end + 1;
	}
	*out = list;
	return n;
}

/* Compile trusted-proxy table from a userland config array:
 * ['proxies'=>[cidrs...], 'headers'=>[names...], 'proto'=>bool] */
void gene_trust_compile(zval *cfg) {
	gene_cidr_t *cidrs = NULL;
	zend_string **hdrs = NULL;
	uint32_t cn = 0, hn = 0;
	zend_bool proto = 0;

	if (cfg && Z_TYPE_P(cfg) == IS_ARRAY) {
		zval *v;
		v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("proto"));
		proto = v ? zend_is_true(v) : 1;
		v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("proxies"));
		if (v && Z_TYPE_P(v) == IS_ARRAY) {
			smart_str joined = {0};
			zval *item;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(v), item) {
				if (Z_TYPE_P(item) == IS_STRING && Z_STRLEN_P(item) > 0) {
					if (joined.s) smart_str_appendc(&joined, ',');
					smart_str_append(&joined, Z_STR_P(item));
				}
			} ZEND_HASH_FOREACH_END();
			if (joined.s) {
				smart_str_0(&joined);
				cn = gene_cidr_list_parse(ZSTR_VAL(joined.s), ZSTR_LEN(joined.s), &cidrs);
				smart_str_free(&joined);
			}
		} else if (v && Z_TYPE_P(v) == IS_STRING) {
			cn = gene_cidr_list_parse(Z_STRVAL_P(v), Z_STRLEN_P(v), &cidrs);
		}
		v = zend_hash_str_find(Z_ARRVAL_P(cfg), ZEND_STRL("headers"));
		if (v && Z_TYPE_P(v) == IS_ARRAY) {
			smart_str joined = {0};
			zval *item;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(v), item) {
				if (Z_TYPE_P(item) == IS_STRING && Z_STRLEN_P(item) > 0) {
					if (joined.s) smart_str_appendc(&joined, ',');
					smart_str_append(&joined, Z_STR_P(item));
				}
			} ZEND_HASH_FOREACH_END();
			if (joined.s) {
				smart_str_0(&joined);
				hn = gene_header_list_parse(ZSTR_VAL(joined.s), ZSTR_LEN(joined.s), &hdrs);
				smart_str_free(&joined);
			}
		} else if (v && Z_TYPE_P(v) == IS_STRING) {
			hn = gene_header_list_parse(Z_STRVAL_P(v), Z_STRLEN_P(v), &hdrs);
		}
	}
	/* default header when proxies are set but headers omitted */
	if (cn > 0 && hn == 0) {
		hdrs = (zend_string **)pemalloc(sizeof(zend_string *), 1);
		hdrs[0] = zend_string_init(ZEND_STRL("x-forwarded-for"), 1);
		hn = 1;
	}
	gene_trust_free();
	GENE_G(trust_cidrs) = cidrs;
	GENE_G(trust_cidr_n) = cn;
	GENE_G(trust_hdrs) = hdrs;
	GENE_G(trust_hdr_n) = hn;
	GENE_G(trust_proto) = proto;
	GENE_G(trust_compiled) = 1;
}

/* Lazy compile from INI when Application::trustProxies() was never called. */
void gene_trust_compile_ini(void) {
	zval cfg;
	if (GENE_G(trust_compiled)) {
		return;
	}
	GENE_G(trust_proto) = GENE_G(trust_proto_ini);
	if (GENE_G(trusted_proxies) && GENE_G(trusted_proxies)[0] != '\0') {
		array_init(&cfg);
		add_assoc_bool_ex(&cfg, ZEND_STRL("proto"), GENE_G(trust_proto_ini));
		if (GENE_G(trusted_proxies)) {
			add_assoc_string_ex(&cfg, ZEND_STRL("proxies"), GENE_G(trusted_proxies));
		}
		if (GENE_G(trusted_headers)) {
			add_assoc_string_ex(&cfg, ZEND_STRL("headers"), GENE_G(trusted_headers));
		}
		gene_trust_compile(&cfg);
		zval_ptr_dtor(&cfg);
		return;
	}
	GENE_G(trust_compiled) = 1; /* compiled-empty: trusted proxies disabled */
}
/* }}} */

/* {{{ direct/client ip resolution */
static zval *gene_server_val(const char *name, size_t len) {
	return getVal(TRACK_VARS_SERVER, (char *)name, len);
}

zend_string *gene_request_direct_ip(void) {
	zval *v = gene_server_val(ZEND_STRL("REMOTE_ADDR"));
	const char *s;
	size_t n;
	if (!v || Z_TYPE_P(v) != IS_STRING || Z_STRLEN_P(v) == 0) {
		return NULL;
	}
	s = Z_STRVAL_P(v);
	n = Z_STRLEN_P(v);
	while (n && s[0] == ' ') { s++; n--; }
	while (n && s[n - 1] == ' ') { n--; }
	return zend_string_init(s, n, 0);
}

/* Allocation-free: parse the TCP peer straight out of the server bag.
 * Hot path (isSecure / secureHeaders / csrf / restGuard each consult it). */
static int gene_direct_bin(unsigned char bin[16]) {
	zval *v = gene_server_val(ZEND_STRL("REMOTE_ADDR"));
	const char *s;
	size_t n;
	if (!v || Z_TYPE_P(v) != IS_STRING || Z_STRLEN_P(v) == 0) {
		return 0;
	}
	s = Z_STRVAL_P(v);
	n = Z_STRLEN_P(v);
	while (n && s[0] == ' ') { s++; n--; }
	while (n && s[n - 1] == ' ') { n--; }
	return gene_ip_parse(s, n, bin);
}

static zend_bool gene_direct_is_trusted(void) {
	unsigned char bin[16];
	gene_trust_compile_ini();
	if (GENE_G(trust_cidr_n) == 0 || !gene_direct_bin(bin)) {
		return 0;
	}
	return gene_cidr_match(bin, GENE_G(trust_cidrs), GENE_G(trust_cidr_n));
}

/* Header lookup goes through the HEADER bag (lowercase dash keys): Swoole
 * fills it from $request->header, FPM materializes it from HTTP_* server
 * keys — one code path for both runtimes. */
static zval *gene_header_lookup(const char *name, size_t len) {
	zval *v = getVal(GENE_REQUEST_ATTR_HEADER, (char *)name, len);
	if (v && Z_TYPE_P(v) == IS_STRING) {
		return v;
	}
	return NULL;
}

zend_string *gene_request_client_ip(void) {
	unsigned char bin[16];
	uint32_t h;
	if (!gene_direct_is_trusted()) {
		return gene_request_direct_ip();
	}
	for (h = 0; h < GENE_G(trust_hdr_n); h++) {
		zend_string *hn = GENE_G(trust_hdrs)[h];
		zval *xff = gene_header_lookup(ZSTR_VAL(hn), ZSTR_LEN(hn));
		const char *s, *e, *p;
		if (!xff || Z_TYPE_P(xff) != IS_STRING) {
			continue;
		}
		s = Z_STRVAL_P(xff);
		e = s + Z_STRLEN_P(xff);
		p = e;
		/* right-most non-trusted entry wins (the node the trusted proxy appended) */
		while (p > s) {
			const char *q = p, *a, *b;
			while (q > s && q[-1] != ',') q--;
			a = q; b = p;
			while (a < b && (*a == ' ' || *a == '\t')) a++;
			while (b > a && (b[-1] == ' ' || b[-1] == '\t')) b--;
			if (b > a && gene_ip_parse(a, (size_t)(b - a), bin)) {
				if (!gene_cidr_match(bin, GENE_G(trust_cidrs), GENE_G(trust_cidr_n))) {
					return zend_string_init(a, (size_t)(b - a), 0);
				}
			} else {
				break; /* malformed segment: stop, fall back to direct ip */
			}
			p = (q > s) ? q - 1 : s;
		}
	}
	return gene_request_direct_ip();
}

zend_bool gene_request_direct_trusted(void) {
	return gene_direct_is_trusted();
}

/* Behind a trusted proxy the TCP peer is the proxy itself (typically
 * 127.0.0.1 / RFC1918), so judging it would admit every public client.
 * Judge the resolved client instead; untrusted peers keep the direct-peer
 * rule (XFF is never consulted for them). */
zend_bool gene_request_ip_is_internal(void) {
	unsigned char bin[16];
	if (gene_direct_is_trusted()) {
		zend_string *ip = gene_request_client_ip();
		zend_bool ok = ip && gene_ip_parse(ZSTR_VAL(ip), ZSTR_LEN(ip), bin) && gene_ip_is_internal(bin);
		if (ip) {
			zend_string_release(ip);
		}
		return ok;
	}
	return gene_direct_bin(bin) && gene_ip_is_internal(bin);
}

/* Secure detection honoring trust_proto: when trusted proxies are compiled
 * the X-Forwarded-Proto header is only believed from a trusted peer. */
int gene_request_is_secure(void) {
	zval *https = gene_server_val(ZEND_STRL("HTTPS"));
	if (https && Z_TYPE_P(https) == IS_STRING && Z_STRLEN_P(https) > 0
		&& strcasecmp("off", Z_STRVAL_P(https)) != 0) {
		return 1;
	}
	gene_trust_compile_ini();
	{
		/* X-Forwarded-Proto is client-controlled input: believe it only when
		 * trusted proxies are configured AND the direct peer is trusted.
		 * Without a trust table the header must not escalate isSecure(). */
		if (GENE_G(trust_proto) && gene_direct_is_trusted()) {
			/* first token: a proxy chain may send "https, http" — the left-most
			 * entry is what the edge proxy saw from the client */
			zval *proto = gene_header_lookup(ZEND_STRL("x-forwarded-proto"));
			if (proto) {
				const char *a = Z_STRVAL_P(proto), *e = a + Z_STRLEN_P(proto), *b;
				while (a < e && (*a == ' ' || *a == '\t')) a++;
				b = a;
				while (b < e && *b != ',' && *b != ' ' && *b != '\t') b++;
				if (b - a == 5 && strncasecmp("https", a, 5) == 0) {
					return 1;
				}
			}
		} else if (GENE_G(trust_cidr_n) == 0 && !GENE_G(trust_xfp_warned)
				&& gene_header_lookup(ZEND_STRL("x-forwarded-proto"))) {
			/* behavior change vs <=2026-10: one diag per process so deployments
			 * behind a TLS-terminating proxy notice the missing trust table */
			GENE_G(trust_xfp_warned) = 1;
			gene_log_diag(E_NOTICE, "Gene: X-Forwarded-Proto ignored because no trusted proxies are configured; "
				"call Application::trustProxies() or set gene.trusted_proxies");
		}
	}
	{
		zval *port = gene_server_val(ZEND_STRL("SERVER_PORT"));
		if (port) {
			if (Z_TYPE_P(port) == IS_LONG && Z_LVAL_P(port) == 443) {
				return 1;
			}
			if (Z_TYPE_P(port) == IS_STRING && Z_STRLEN_P(port) == 3
				&& memcmp("443", Z_STRVAL_P(port), 3) == 0) {
				return 1;
			}
		}
	}
	return 0;
}
/* }}} */

/* {{{ proto static bool Gene\Ip::inCidr(string $ip, string|array $cidrs) */
PHP_METHOD(gene_ip, inCidr) {
	zend_string *ip;
	zval *cidrs;
	unsigned char bin[16];
	gene_cidr_t *list = NULL;
	uint32_t n;
	int ok;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &ip, &cidrs) == FAILURE) {
		return;
	}
	if (!gene_ip_parse(ZSTR_VAL(ip), ZSTR_LEN(ip), bin)) {
		RETURN_FALSE;
	}
	if (Z_TYPE_P(cidrs) == IS_STRING) {
		n = gene_cidr_list_parse(Z_STRVAL_P(cidrs), Z_STRLEN_P(cidrs), &list);
	} else if (Z_TYPE_P(cidrs) == IS_ARRAY) {
		smart_str joined = {0};
		zval *item;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(cidrs), item) {
			if (Z_TYPE_P(item) == IS_STRING && Z_STRLEN_P(item) > 0) {
				if (joined.s) smart_str_appendc(&joined, ',');
				smart_str_append(&joined, Z_STR_P(item));
			}
		} ZEND_HASH_FOREACH_END();
		if (!joined.s) {
			RETURN_FALSE;
		}
		smart_str_0(&joined);
		n = gene_cidr_list_parse(ZSTR_VAL(joined.s), ZSTR_LEN(joined.s), &list);
		smart_str_free(&joined);
	} else {
		RETURN_FALSE;
	}
	ok = gene_cidr_match(bin, list, n);
	if (list) pefree(list, 1);
	RETURN_BOOL(ok);
}
/* }}} */

/* {{{ proto static bool Gene\Ip::isInternal(string $ip) */
PHP_METHOD(gene_ip, isInternal) {
	zend_string *ip;
	unsigned char bin[16];
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &ip) == FAILURE) {
		return;
	}
	RETURN_BOOL(gene_ip_parse(ZSTR_VAL(ip), ZSTR_LEN(ip), bin) && gene_ip_is_internal(bin));
}
/* }}} */

/* {{{ proto static bool Gene\Ip::isValid(string $ip) */
PHP_METHOD(gene_ip, isValid) {
	zend_string *ip;
	unsigned char bin[16];
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &ip) == FAILURE) {
		return;
	}
	RETURN_BOOL(gene_ip_parse(ZSTR_VAL(ip), ZSTR_LEN(ip), bin));
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(gene_ip_incidr_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, ip)
	ZEND_ARG_INFO(0, cidrs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_ip_str_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, ip)
ZEND_END_ARG_INFO()

const zend_function_entry gene_ip_methods[] = {
	PHP_ME(gene_ip, inCidr, gene_ip_incidr_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_ip, isInternal, gene_ip_str_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_ip, isValid, gene_ip_str_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{ NULL, NULL, NULL }
};

GENE_MINIT_FUNCTION(ip) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Ip", "Gene\\Ip", gene_ip_methods);
	gene_ip_ce = zend_register_internal_class(&ce);
	return SUCCESS;
}
