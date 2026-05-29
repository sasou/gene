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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "main/SAPI.h"
#include "ext/session/php_session.h"
#include "ext/standard/md5.h"
#include <time.h>

#ifdef PHP_WIN32
PHPAPI int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info);
#include <process.h>
#define gene_getpid _getpid
#else
#include <unistd.h>
#define gene_getpid getpid
#endif

#include "../gene.h"
#include "../session/session.h"
#include "../common/common.h"
#include "../di/di.h"
#include "../http/request.h"
#include "../http/response.h"

zend_class_entry * gene_session_ce;

/* [GENE_PERF:2026-04-27] Cached instance-property byte offsets for the cookie
 * hot path. Set in MINIT after all zend_declare_property_* calls so that
 * gene_cookie() / gene_set_cookie() can read 4–7 properties via OBJ_PROP
 * pointer arithmetic instead of zend_read_property hash lookups. */
static uint32_t gene_session_offset_name = 0;
static uint32_t gene_session_offset_cookie_id = 0;
static uint32_t gene_session_offset_lifetime = 0;
static uint32_t gene_session_offset_cookie_path = 0;
static uint32_t gene_session_offset_cookie_domain = 0;
static uint32_t gene_session_offset_secure = 0;
static uint32_t gene_session_offset_httponly = 0;

void gene_cookie(zval *self);
void gene_data_load(zval *obj);
void gene_data_save(zval *obj, zval *data);
void gene_data_clear(zval *obj);
static void gene_session_auto_cookie(zval *obj);
static zval * gene_session_set_val(zval *val, char *keyString, size_t keyString_len, zval *zvalue);
static zend_string *gene_session_method_get(void);
static zend_string *gene_session_method_set(void);
static zend_string *gene_session_method_delete(void);

 static zend_bool gene_session_call_method(zval *target, zend_string *method, uint32_t param_count, zval *params, zval *retval)
 {
	zend_function *func;
	zend_class_entry *called_scope;

	if (!target || Z_TYPE_P(target) != IS_OBJECT) {
		return 0;
	}

	called_scope = Z_OBJCE_P(target);
	/* [GENE_PERF:2026-05-04] Small (ce, method) -> zend_function* LRU cache.
	 * Session handler methods (get/set/delete/cookie) are interned zend_strings,
	 * so pointer equality is a valid identity match. 4 slots cover the typical
	 * single-handler production case with zero collisions. */
	{
		static struct { zend_class_entry *ce; zend_string *method; zend_function *fn; } gene_session_fn_cache[4] = {0};
		static unsigned gene_session_fn_cache_next = 0;
		unsigned i;
		func = NULL;
		for (i = 0; i < 4; i++) {
			if (gene_session_fn_cache[i].ce == called_scope && gene_session_fn_cache[i].method == method) {
				func = gene_session_fn_cache[i].fn;
				break;
			}
		}
		if (!func) {
			func = zend_hash_find_ptr(&called_scope->function_table, method);
			if (func) {
				unsigned slot = gene_session_fn_cache_next++ & 3;
				gene_session_fn_cache[slot].ce = called_scope;
				gene_session_fn_cache[slot].method = method;
				gene_session_fn_cache[slot].fn = func;
			}
		}
	}
	if (func) {
		/* [GENE_FIX:2026-04-27] Returning inside zend_catch skips zend_end_try,
		 * which leaves EG(bailout) pointing at our local jmp_buf for outer
		 * frames — a subsequent bailout would longjmp into freed stack.
		 * Use a flag, finish the try block, then return. */
		zend_bool bailed = 0;
		zend_try {
			zend_call_known_function(func, Z_OBJ_P(target), called_scope, retval, param_count, params, NULL);
		} zend_catch {
			bailed = 1;
		} zend_end_try();
		return bailed ? 0 : 1;
	}

	{
		zval function_name;
		ZVAL_STR(&function_name, zend_string_copy(method));
		call_user_function(NULL, target, &function_name, retval, param_count, params);
		zval_ptr_dtor(&function_name);
	}
	return 1;
 }

 static zval *gene_session_get_data(zval *obj)
 {
	zval *sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	if (sess && Z_TYPE_P(sess) == IS_NULL) {
		gene_data_load(obj);
		sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	}
	/* [GENE_PERF:2026-05-29] Pure reads (get/has) must NOT emit a Set-Cookie.
	 * Cookie emission belongs to the write lifecycle: set/del/setSessionId
	 * each call gene_session_auto_cookie() after gene_session_mark_dirty(),
	 * and the stime/uptime refresh in get() routes through gene_data_save().
	 * Keeping it here only burned 3 property reads per read and could plant a
	 * throwaway SSID on read-only access to a brand-new session. */
	return sess;
 }

 static zval *gene_session_walk_path(zval *root, zend_string *path, zend_bool create_missing, zval *leaf_value, zend_bool *changed)
 {
	const char *cur, *end, *seg_start;
	zval *tmp;

	if (!root || Z_TYPE_P(root) != IS_ARRAY) {
		return NULL;
	}

	if (!path || ZSTR_LEN(path) == 0) {
		return root;
	}

	tmp = root;
	cur = ZSTR_VAL(path);
	end = cur + ZSTR_LEN(path);
	seg_start = cur;

	while (1) {
		const char *seg_end = cur;
		size_t seg_len;
		zend_bool has_more;

		while (seg_end < end && *seg_end != '.') {
			seg_end++;
		}
		seg_len = (size_t)(seg_end - seg_start);
		has_more = (seg_end < end);

		if (seg_len == 0 || Z_TYPE_P(tmp) != IS_ARRAY) {
			return NULL;
		}

		if (create_missing) {
			if (has_more) {
				tmp = gene_session_set_val(tmp, (char *)seg_start, seg_len, NULL);
			} else {
				tmp = gene_session_set_val(tmp, (char *)seg_start, seg_len, leaf_value);
				if (changed) {
					*changed = 1;
				}
			}
			if (!tmp) {
				return NULL;
			}
		} else {
			if (has_more) {
				tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg_start, seg_len);
				if (!tmp) {
					return NULL;
				}
			} else {
				return zend_symtable_str_find(Z_ARRVAL_P(tmp), seg_start, seg_len);
			}
		}

		if (!has_more) {
			break;
		}
		cur = seg_end + 1;
		seg_start = cur;
	}

	return tmp;
 }

 static zend_bool gene_session_delete_path(zval *root, zend_string *path)
 {
	const char *cur, *end, *seg_start;
	zval *tmp;

	if (!root || Z_TYPE_P(root) != IS_ARRAY || !path || ZSTR_LEN(path) == 0) {
		return 0;
	}

	tmp = root;
	cur = ZSTR_VAL(path);
	end = cur + ZSTR_LEN(path);
	seg_start = cur;

	while (1) {
		const char *seg_end = cur;
		size_t seg_len;
		zend_bool has_more;

		while (seg_end < end && *seg_end != '.') {
			seg_end++;
		}
		seg_len = (size_t)(seg_end - seg_start);
		has_more = (seg_end < end);

		if (seg_len == 0 || Z_TYPE_P(tmp) != IS_ARRAY) {
			return 0;
		}

		if (has_more) {
			tmp = zend_symtable_str_find(Z_ARRVAL_P(tmp), seg_start, seg_len);
			if (!tmp) {
				return 0;
			}
		} else {
			return zend_symtable_str_del(Z_ARRVAL_P(tmp), seg_start, seg_len) == 0 ? 1 : 0;
		}

		cur = seg_end + 1;
		seg_start = cur;
	}
 }

/* spl_ce_Countable is globally available since PHP 7.2+ */

/* {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(gene_session_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_get_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_has_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_del_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_sets_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_session_cookie_arginfo, 0, 0, 3)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
	ZEND_ARG_INFO(0, time)
ZEND_END_ARG_INFO()
/* }}} */

 static void gene_session_update_ids(zval *obj, zend_string *cookie_id_str)
 {
	zval *prefix = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_PREFIX), 1, NULL);
	size_t prefix_len = (prefix && Z_TYPE_P(prefix) == IS_STRING) ? Z_STRLEN_P(prefix) : 0;
	size_t cookie_len = ZSTR_LEN(cookie_id_str);
	size_t session_len = prefix_len + cookie_len;
	char session_buf[256];
	char *session_id = session_buf;
	int session_heap = 0;

	if (session_len >= sizeof(session_buf)) {
		session_id = emalloc(session_len + 1);
		session_heap = 1;
	}
	if (prefix_len > 0) {
		memcpy(session_id, Z_STRVAL_P(prefix), prefix_len);
	}
	memcpy(session_id + prefix_len, ZSTR_VAL(cookie_id_str), cookie_len);
	session_id[session_len] = '\0';

	zend_update_property_str(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_ID), cookie_id_str);
	zend_update_property_stringl(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), session_id, session_len);

	if (session_heap) {
		efree(session_id);
	}
 }

 static zend_string *gene_session_sanitize_cookie_id(zend_string *cookie_id_str)
 {
	size_t i, len = ZSTR_LEN(cookie_id_str);
	char *src = ZSTR_VAL(cookie_id_str);
	zend_string *out = zend_string_alloc(len, 0);
	char *dst = ZSTR_VAL(out);
	size_t out_len = 0;

	for (i = 0; i < len; i++) {
		unsigned char c = (unsigned char)src[i];
		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_') {
			dst[out_len++] = (char)c;
		}
	}
	if (out_len == 0) {
		zend_string_release(out);
		return NULL;
	}
	ZSTR_VAL(out)[out_len] = '\0';
	ZSTR_LEN(out) = out_len;
	return out;
 }

/* hash_mode: 0=MD5 (default), 1=fast FNV-1a, 2=xxHash64, 3=FarmHash64, 4=MurmurHash3, 5=TurboHash32 */
 static void gene_session_generate_cookie_id(zval *retval, int hash_mode)
 {
	char seed[96];
	size_t seed_len;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	seed_len = (size_t)snprintf(seed, sizeof(seed), "%ld:%ld:%p:%u", (long)tv.tv_sec, (long)tv.tv_usec, (void *)EG(current_execute_data), (unsigned int)gene_getpid());
	if (seed_len >= sizeof(seed)) {
		seed_len = sizeof(seed) - 1;
	}
	
	switch (hash_mode) {
	case 1:
		gene_hash_fast_buf(seed, seed_len, retval);
		break;
	case 2:
		gene_hash_xxhash64_buf(seed, seed_len, retval);
		break;
	case 3:
		gene_hash_farmhash64_buf(seed, seed_len, retval);
		break;
	case 4:
		gene_hash_murmur3_32_buf(seed, seed_len, retval);
		break;
	case 5:
		gene_hash_turbo_hash32_buf(seed, seed_len, retval);
		break;
	default: {
		PHP_MD5_CTX ctx;
		unsigned char digest[16];
		PHP_MD5Init(&ctx);
		PHP_MD5Update(&ctx, (unsigned char *)seed, seed_len);
		PHP_MD5Final(digest, &ctx);
		zend_string *out = zend_string_alloc(32, 0);
		make_digest_ex(ZSTR_VAL(out), digest, 16);
		ZSTR_VAL(out)[32] = '\0';
		ZVAL_STR(retval, out);
		break;
	}
	}
 }

 static zend_long gene_session_now(void)
 {
	return (zend_long)time(NULL);
 }

 static zend_string *gene_session_method_get(void)
 {
	/* [GENE_FIX:2026-05-24] gene_interned_str_persistent avoids the unsafe
	 * static zend_string* + zend_string_init_interned(...,1) pattern that
	 * dangles across requests under opcache.file_cache_only=1. */
	static zend_string *method_slot = NULL;
	return gene_interned_str_persistent(&method_slot, "get", sizeof("get") - 1);
 }

 static zend_string *gene_session_method_set(void)
 {
	/* [GENE_FIX:2026-05-24] gene_interned_str_persistent avoids the unsafe
	 * static zend_string* + zend_string_init_interned(...,1) pattern that
	 * dangles across requests under opcache.file_cache_only=1. */
	static zend_string *method_slot = NULL;
	return gene_interned_str_persistent(&method_slot, "set", sizeof("set") - 1);
 }

 static zend_string *gene_session_method_delete(void)
 {
	/* [GENE_FIX:2026-05-24] gene_interned_str_persistent avoids the unsafe
	 * static zend_string* + zend_string_init_interned(...,1) pattern that
	 * dangles across requests under opcache.file_cache_only=1. */
	static zend_string *method_slot = NULL;
	return gene_interned_str_persistent(&method_slot, "delete", sizeof("delete") - 1);
 }

 static zval *gene_session_get_handler(zval *obj)
 {
	zval *handler = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_HANDLER), 1, NULL);
	if (handler && Z_TYPE_P(handler) == IS_OBJECT) {
		return handler;
	}
	{
		zval *driver = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DRIVER), 1, NULL);
		zend_string *class_name = NULL;
		zval *hook = NULL;

		if (!driver || Z_TYPE_P(driver) != IS_STRING) {
			return NULL;
		}
		class_name = gene_get_class_name_fast();
		if (class_name) {
			hook = gene_di_get_class(class_name, Z_STR_P(driver));
		}
		if (hook && Z_TYPE_P(hook) == IS_OBJECT) {
			zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_HANDLER), hook);
			handler = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_HANDLER), 1, NULL);
			if (handler && Z_TYPE_P(handler) == IS_OBJECT) {
				return handler;
			}
		}
	}
	return NULL;
 }

 static zend_bool gene_session_is_dirty(zval *obj)
 {
	zval *dirty = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DIRTY), 1, NULL);
	return (dirty && zend_is_true(dirty)) ? 1 : 0;
 }

 static void gene_session_mark_dirty(zval *obj)
 {
	zend_update_property_bool(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DIRTY), 1);
	zend_update_property_bool(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_SENT), 0);
 }

 static void gene_session_mark_clean(zval *obj)
 {
	zend_update_property_bool(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DIRTY), 0);
 }

 static void gene_session_mark_cookie_sent(zval *obj)
 {
	zend_update_property_bool(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_SENT), 1);
 }

 static zend_bool gene_session_cookie_sent(zval *obj)
 {
	zval *sent = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_SENT), 1, NULL);
	return (sent && zend_is_true(sent)) ? 1 : 0;
 }

 static zend_bool gene_session_is_new(zval *obj)
 {
	zval *is_new = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_NEW), 1, NULL);
	return (is_new && zend_is_true(is_new)) ? 1 : 0;
 }

 static void gene_session_mark_new(zval *obj, zend_bool is_new)
 {
	zend_update_property_bool(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_NEW), is_new ? 1 : 0);
 }

 static void gene_data_save_ex(zval *obj, zval *data, zend_bool send_cookie)
 {
	zval *session_id = NULL, *hook = NULL;

	session_id = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), 1, NULL);
	if (session_id && Z_TYPE_P(session_id) == IS_STRING) {
		if (data == NULL) {
			data = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
		}
		if (data && Z_TYPE_P(data) == IS_ARRAY) {
			zval time;
			ZVAL_LONG(&time, gene_session_now());
			zend_hash_str_update(Z_ARRVAL_P(data), ZEND_STRL("stime"), &time);
		}

		hook = gene_session_get_handler(obj);
		if (hook) {
			zval params[] = { *session_id,*data };
			zval ret;
			ZVAL_UNDEF(&ret);
			gene_session_call_method(hook, gene_session_method_set(), 2, params, &ret);
			zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), data);
			if (!Z_ISUNDEF(ret)) {
				zval_ptr_dtor(&ret);
			}
			gene_session_mark_clean(obj);
			if (send_cookie && !gene_session_cookie_sent(obj)) {
				gene_cookie(obj);
			}
		} else {
			php_error_docref(NULL, E_WARNING, "Configure or inject the session storage plug-in");
		}
	}
 }

/* [GENE_FIX:2026-05-29] Session cookie emission is a best-effort side effect
 * triggered from get/set/save/__destruct. Routing it through gene_response_cookie()
 * lost the zend_try/zend_catch protection the old gene_session_call_method /
 * gene_session_call_setcookie wrappers provided. In Swoole, Response::cookie()
 * throws when the HTTP response has already been ended; an uncaught exception
 * escaping a destructor or post-send path aborts the worker process. Catch
 * engine bailouts and swallow pending exceptions so a stale/ended Response can
 * never crash the worker. Returns 1 if the cookie was accepted, 0 otherwise. */
static zend_bool gene_session_emit_cookie(zval *name, zval *value, zval *expires, zval *path, zval *domain, zval *secure, zval *httponly)
{
	zval ret;
	zend_bool sent = 1;
	zend_bool bailed = 0;
	ZVAL_UNDEF(&ret);
	zend_try {
		gene_response_cookie(name, value, expires, path, domain, secure, httponly, &ret);
	} zend_catch {
		bailed = 1;
	} zend_end_try();
	if (UNEXPECTED(EG(exception))) {
		zend_clear_exception();
		sent = 0;
	}
	if (bailed || Z_TYPE(ret) == IS_FALSE) {
		sent = 0;
	}
	if (!Z_ISUNDEF(ret)) {
		zval_ptr_dtor(&ret);
	}
	return sent;
}

void gene_cookie(zval *self) /*{{{*/
{
	zend_object *obj = Z_OBJ_P(self);
	zval *name      = OBJ_PROP(obj, gene_session_offset_name);
	zval *cookie_id = OBJ_PROP(obj, gene_session_offset_cookie_id);
	zval *lifetime  = OBJ_PROP(obj, gene_session_offset_lifetime);
	zval *path      = OBJ_PROP(obj, gene_session_offset_cookie_path);
	zval *domain    = OBJ_PROP(obj, gene_session_offset_cookie_domain);
	zval *secure    = OBJ_PROP(obj, gene_session_offset_secure);
	zval *httponly  = OBJ_PROP(obj, gene_session_offset_httponly);
	if (!name || Z_TYPE_P(name) != IS_STRING || !cookie_id || Z_TYPE_P(cookie_id) != IS_STRING || !path || Z_TYPE_P(path) != IS_STRING || !domain || Z_TYPE_P(domain) != IS_STRING) {
		return;
	}
	if (!secure || (Z_TYPE_P(secure) != IS_TRUE && Z_TYPE_P(secure) != IS_FALSE)) {
		return;
	}
	if (!httponly || (Z_TYPE_P(httponly) != IS_TRUE && Z_TYPE_P(httponly) != IS_FALSE)) {
		return;
	}
	/* [GENE_FIX:2026-05-29] FPM/CGI: skip setcookie after response headers were sent
	 * (e.g. __destruct fallback). Early get/set paths should have flushed already;
	 * silently no-op avoids "headers already sent" warnings. Swoole buffers via
	 * Response::cookie() and is unaffected by SG(headers_sent). */
	if (GENE_G(runtime_type) < 2 && SG(headers_sent)) {
		return;
	}

	zval times;
	zend_long now = gene_session_now();
	zend_long jg;
	if (Z_TYPE_P(lifetime) == IS_LONG) {
		jg = now + Z_LVAL_P(lifetime);
	} else {
		jg = now + 7200;
	}
	ZVAL_LONG(&times, jg);

	/* [GENE_FIX:2026-05-29] Route cookie output through gene_response_cookie()
	 * (Swoole Response::cookie or FPM setcookie). Without a bound Response in
	 * Swoole mode, skip silently — do not fall back to setcookie() or mark sent. */
	if (GENE_G(runtime_type) >= 2 && !gene_response_context_obj()) {
		return;
	}
	if (gene_session_emit_cookie(name, cookie_id, &times, path, domain, secure, httponly)) {
		gene_session_mark_cookie_sent(self);
	}
}/*}}}*/

 static void gene_session_auto_cookie(zval *obj)
 {
	if (gene_session_cookie_sent(obj)) {
		return;
	}
	if (gene_session_is_new(obj) || gene_session_is_dirty(obj)) {
		gene_cookie(obj);
	}
 }

void gene_set_cookie(zval *self, zval *name, zval *value, zval *time) /*{{{*/
{
	zend_object *obj = Z_OBJ_P(self);
	zval *path     = OBJ_PROP(obj, gene_session_offset_cookie_path);
	zval *domain   = OBJ_PROP(obj, gene_session_offset_cookie_domain);
	zval *secure   = OBJ_PROP(obj, gene_session_offset_secure);
	zval *httponly = OBJ_PROP(obj, gene_session_offset_httponly);

	if (GENE_G(runtime_type) < 2 && SG(headers_sent)) {
		return;
	}

	if (GENE_G(runtime_type) >= 2 && !gene_response_context_obj()) {
		return;
	}
	if (gene_session_emit_cookie(name, value, time, path, domain, secure, httponly)) {
		gene_session_mark_cookie_sent(self);
	}
}/*}}}*/


/** {{{ void gene_init_ssid(zval *obj)
 */
void gene_init_ssid(zval *obj) {
	zval *cookie_id = NULL;
	zval *name = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_NAME), 1, NULL);
	zval *hash_mode_zv = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_HASH_MODE), 1, NULL);
	zend_long hash_mode = 0;
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}

	/* [GENE_FIX:2026-04-27] Defensive type check: user code may have unset
	 * or overwritten the name property with a non-string before construct. */
	if (!name || Z_TYPE_P(name) != IS_STRING || Z_STRLEN_P(name) == 0) {
		zval hash_val;
		gene_session_generate_cookie_id(&hash_val, (int)hash_mode);
		gene_session_update_ids(obj, Z_STR(hash_val));
		zval_ptr_dtor(&hash_val);
		gene_session_mark_new(obj, 1);
		return;
	}

	cookie_id = getVal(TRACK_VARS_COOKIE, Z_STRVAL_P(name), Z_STRLEN_P(name));
	if (cookie_id && Z_TYPE_P(cookie_id) == IS_STRING) {
		zend_string *filtered = gene_session_sanitize_cookie_id(Z_STR_P(cookie_id));
		if (filtered) {
			gene_session_update_ids(obj, filtered);
			zend_string_release(filtered);
			gene_session_mark_new(obj, 0);
		} else {
			zval hash_val;
			gene_session_generate_cookie_id(&hash_val, (int)hash_mode);
			gene_session_update_ids(obj, Z_STR(hash_val));
			zval_ptr_dtor(&hash_val);
			gene_session_mark_new(obj, 1);
		}
	} else {
		zval hash_val;
		gene_session_generate_cookie_id(&hash_val, (int)hash_mode);
		gene_session_update_ids(obj, Z_STR(hash_val));
		zval_ptr_dtor(&hash_val);
		gene_session_mark_new(obj, 1);
	}
}
/* }}} */

/** {{{ void gene_update_ssid(zval *obj)
 */
void gene_update_ssid(zval *obj) {
	zval *hash_mode_zv = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_HASH_MODE), 1, NULL);
	zend_long hash_mode = 0;
	if (hash_mode_zv && Z_TYPE_P(hash_mode_zv) == IS_LONG) {
		hash_mode = Z_LVAL_P(hash_mode_zv);
	}
	
	zval hash_val;
	gene_session_generate_cookie_id(&hash_val, (int)hash_mode);
	gene_session_update_ids(obj, Z_STR(hash_val));
	zval_ptr_dtor(&hash_val);
	gene_session_mark_new(obj, 1);
}
/* }}} */

void gene_data_load(zval *obj) { /*{{{*/
	zval *session_id = NULL, *hook = NULL;

	session_id = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), 1, NULL);
	if (session_id && Z_TYPE_P(session_id) == IS_STRING) {
		hook = gene_session_get_handler(obj);
		if (hook) {
			zval params[] = { *session_id };
			zval ret;
			ZVAL_UNDEF(&ret);
			gene_session_call_method(hook, gene_session_method_get(), 1, params, &ret);
			if (Z_TYPE(ret) == IS_ARRAY) {
				zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), &ret);
			} else {
				zval karr;
				array_init(&karr);
				zend_update_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), &karr);
				zval_ptr_dtor(&karr);
			}
			if (!Z_ISUNDEF(ret)) {
				zval_ptr_dtor(&ret);
			}
			gene_session_mark_clean(obj);
		} else {
			php_error_docref(NULL, E_WARNING, "Configure or inject the session storage plug-in");
		}
	}
}/*}}}*/

void gene_data_save(zval *obj, zval *data) { /*{{{*/
	gene_data_save_ex(obj, data, 1);
}/*}}}*/

/** {{{ void gene_data_clear(zval *obj)
 */
void gene_data_clear(zval *obj) {
	zval *session_id = NULL, *hook = NULL;

	session_id = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_ID), 1, NULL);
	if (session_id && Z_TYPE_P(session_id) == IS_STRING) {
		hook = gene_session_get_handler(obj);
		if (hook) {
			zval params[] = { *session_id };
			zval ret;
			ZVAL_UNDEF(&ret);
			gene_session_call_method(hook, gene_session_method_delete(), 1, params, &ret);
			if (!Z_ISUNDEF(ret)) {
				zval_ptr_dtor(&ret);
			}
			zend_update_property_null(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA));
			gene_update_ssid(obj);
			gene_cookie(obj);
			gene_session_mark_clean(obj);
		} else {
			php_error_docref(NULL, E_WARNING, "Configure or inject the session storage plug-in");
		}
	}

}/*}}}*/

/** {{{ zval * gene_session_get_by_path(char *path)
 */
zval * gene_session_get_by_path(zval *obj, char *path) {
	zval *tmp = NULL;
	zval *sess = NULL, *stime = NULL, *uptime = NULL;

	sess = gene_session_get_data(obj);

	if (sess && Z_TYPE_P(sess) == IS_ARRAY) {
		tmp = sess;
		if (path != NULL) {
			zend_string *path_str = zend_string_init(path, strlen(path), 0);
			tmp = gene_session_walk_path(sess, path_str, 0, NULL, NULL);
			zend_string_release(path_str);
			if (tmp == NULL) {
				return NULL;
			}
		}
		stime = zend_symtable_str_find(Z_ARRVAL_P(sess), ZEND_STRL("stime"));
		if (stime && Z_TYPE_P(stime) == IS_LONG) {
			uptime = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_COOKIE_UPTIME), 1, NULL);
			/* [GENE_FIX:2026-04-27] Type-check uptime: zend_read_property of an
			 * unset property returns &EG(uninitialized_zval) (IS_NULL), and
			 * Z_LVAL_P would read garbage from the union. */
			if (uptime && Z_TYPE_P(uptime) == IS_LONG) {
				zend_long jg = gene_session_now() - Z_LVAL_P(stime);
				if (jg > Z_LVAL_P(uptime)) {
					gene_data_save(obj, NULL);
				}
			}
		}
		return tmp;
	}
	return NULL;
}
/* }}} */


/** {{{ static zval * gene_session_set_val(char *keyString, size_t keyString_len)
 */
static zval * gene_session_set_val(zval *val, char *keyString, size_t keyString_len, zval *zvalue) {
	zval tmp, *copyval;
	if (val == NULL) {
		return NULL;
	}

	if (zvalue == NULL) {
		copyval = zend_symtable_str_find(Z_ARRVAL_P(val), keyString, keyString_len);
		if (copyval == NULL) {
			array_init(&tmp);
			copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
		} else {
			if (Z_TYPE_P(copyval) != IS_ARRAY) {
				array_init(&tmp);
				copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, &tmp);
			}
		}
	} else {
		Z_TRY_ADDREF_P(zvalue);
		copyval = zend_symtable_str_update(Z_ARRVAL_P(val), keyString, keyString_len, zvalue);
	}
	return copyval;
}
/* }}} */

/** {{{ void gene_session_set_by_path(char *path, zval *zvalue)
 */
void gene_session_set_by_path(zval *obj, char *path, zval *zvalue) {
	zval *sess = NULL,*tmp = NULL;
	zend_string *path_str;
	zend_bool changed = 0;
	sess = gene_session_get_data(obj);

	if (Z_TYPE_P(sess) == IS_ARRAY) {
		path_str = zend_string_init(path, strlen(path), 0);
		tmp = gene_session_walk_path(sess, path_str, 1, zvalue, &changed);
		zend_string_release(path_str);
	}

	if (tmp && changed) {
		gene_session_mark_dirty(obj);
		gene_session_auto_cookie(obj);
	}
	return;
}
/* }}} */

/** {{{ bool gene_session_del_by_path(zval *obj, char *path)
 */
bool gene_session_del_by_path(zval *obj, char *path) {
	zval *sess = NULL;
	zend_string *path_str;
	zend_bool deleted;

	sess = zend_read_property(gene_session_ce, gene_strip_obj(obj), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	if (sess && Z_TYPE_P(sess) == IS_NULL) {
		return 0;
	}

	if (Z_TYPE_P(sess) == IS_ARRAY) {
		if (path != NULL) {
			path_str = zend_string_init(path, strlen(path), 0);
			deleted = gene_session_delete_path(sess, path_str);
			zend_string_release(path_str);
			if (deleted) {
				gene_session_mark_dirty(obj);
				gene_session_auto_cookie(obj);
				return 1;
			}
		}
		return 0;
	}
	return 0;
}
/* }}} */

/*
 * {{{ gene_session
 */
PHP_METHOD(gene_session, __construct) {
	zval *config = NULL, *self = getThis(), *val = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(),"|z", &config) == FAILURE) {
        RETURN_NULL();
    }

    if (config && Z_TYPE_P(config) == IS_ARRAY) {
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("driver"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_DRIVER), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("prefix"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_PREFIX), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("name"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_NAME), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("domain"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("path"));
		if (val && Z_TYPE_P(val) == IS_STRING) {
			zend_update_property_string(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_PATH), Z_STRVAL_P(val));
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("secure"));
		if (val) {
			if (Z_TYPE_P(val) == IS_TRUE) {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_SECURE), 1);
			} else {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_SECURE), 0);
			}
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("httponly"));
		if (val) {
			if (Z_TYPE_P(val) == IS_TRUE) {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HTTPONLY), 1);
			} else {
				zend_update_property_bool(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HTTPONLY), 0);
			}
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("ttl"));
		if (val && Z_TYPE_P(val) == IS_LONG) {
			zend_update_property_long(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), Z_LVAL_P(val));
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("uttl"));
		if (val && Z_TYPE_P(val) == IS_LONG) {
			zend_update_property_long(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_UPTIME), Z_LVAL_P(val));
		}
		val = zend_hash_str_find(Z_ARRVAL_P(config), ZEND_STRL("hash_mode"));
		if (val && Z_TYPE_P(val) == IS_LONG) {
			zend_update_property_long(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_HASH_MODE), Z_LVAL_P(val));
		}
	}

	gene_init_ssid(self);
    RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::load()
 */
PHP_METHOD(gene_session, load) {
	zval *self = getThis();
	if (gene_session_is_dirty(self)) {
		gene_data_save(self, NULL);
	}

	gene_data_load(self);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::save()
 */
PHP_METHOD(gene_session, save) {
	zval *self = getThis();

	if (gene_session_is_dirty(self)) {
		gene_data_save(self, NULL);
	} else if (!gene_session_cookie_sent(self)) {
		gene_cookie(self);
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public gene_session::__destruct()
 */
PHP_METHOD(gene_session, __destruct) {
	zval *self = getThis();
	zval *handler;

	if (!self) {
		return;
	}
	/* [GENE_FIX:2026-05-29] Destructors run during request shutdown, after the
	 * HTTP response is typically already flushed/ended (especially under Swoole).
	 * Emitting cookies here is unsafe (ended-response exceptions / "headers
	 * already sent" warnings) and pointless — the cookie is sent eagerly during
	 * the normal get/set/save lifecycle via gene_session_auto_cookie(). So here
	 * we only flush dirty data to the storage handler (send_cookie = 0) to avoid
	 * losing writes that happened without an explicit save(), and never touch
	 * the cookie. */
	if (!gene_session_is_dirty(self)) {
		return;
	}
	handler = gene_session_get_handler(self);
	if (!handler) {
		return;
	}
	gene_data_save_ex(self, NULL, 0);
	/* [GENE_FIX:2026-05-29] A storage handler's set() may throw. An exception
	 * left pending when a destructor returns triggers a fatal "Exception thrown
	 * without a stack frame" and, under Swoole, kills the worker. Swallow it —
	 * persistence at shutdown is best-effort and must not crash the process. */
	if (UNEXPECTED(EG(exception))) {
		zend_clear_exception();
	}
}
/* }}} */

/** {{{ public static gene_session::get($name)
 */
PHP_METHOD(gene_session, get) {
	zend_string *name = NULL;
	zval *self = getThis(),*data = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|S", &name) == FAILURE) {
		RETURN_NULL();
	}
	if (name) {
		zval *sess = gene_session_get_data(self);
		if (sess && Z_TYPE_P(sess) == IS_ARRAY) {
			data = gene_session_walk_path(sess, name, 0, NULL, NULL);
			if (data) {
				zval *stime = zend_symtable_str_find(Z_ARRVAL_P(sess), ZEND_STRL("stime"));
				if (stime && Z_TYPE_P(stime) == IS_LONG) {
					zval *uptime = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_UPTIME), 1, NULL);
					/* [GENE_FIX:2026-04-27] See gene_session_get_by_path. */
					if (uptime && Z_TYPE_P(uptime) == IS_LONG) {
						zend_long jg = gene_session_now() - Z_LVAL_P(stime);
						if (jg > Z_LVAL_P(uptime)) {
							gene_data_save(self, NULL);
						}
					}
				}
			}
		}
	} else {
		data = gene_session_get_by_path(self, NULL);
	}
	if (data) {
		RETURN_ZVAL(data, 1, 0);
	}

	RETURN_NULL();
}
/* }}} */

/** {{{ public static gene_session::set($name, $value)
 */
PHP_METHOD(gene_session, set) {
	zval *self = getThis(),*value = NULL;
	zend_string *name;
	zval *sess;
	zend_bool changed = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "Sz", &name, &value) == FAILURE) {
		RETURN_NULL();
	}

	sess = gene_session_get_data(self);
	if (sess && Z_TYPE_P(sess) == IS_ARRAY) {
		gene_session_walk_path(sess, name, 1, value, &changed);
		if (changed) {
			gene_session_mark_dirty(self);
			gene_session_auto_cookie(self);
		}
	}

	RETURN_TRUE;
}
/* }}} */

/** {{{ public static gene_session::del($name)
 */
PHP_METHOD(gene_session, del) {
	zval *self = getThis();
	zend_string *name;
	bool ret = 0;
	zval *sess;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}

	sess = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_DATA), 1, NULL);
	if (sess && Z_TYPE_P(sess) == IS_ARRAY) {
		ret = gene_session_delete_path(sess, name);
		if (ret) {
			gene_session_mark_dirty(self);
			gene_session_auto_cookie(self);
		}
	} else {
		ret = gene_session_del_by_path(self, ZSTR_VAL(name));
	}

	RETURN_BOOL(ret);
}
/* }}} */

/** {{{ public static gene_session::destroy()
 */
PHP_METHOD(gene_session, destroy) {
	zval *self = getThis();

	gene_data_clear(self);

	RETURN_TRUE;
}
/* }}} */

/** {{{ public gene_session::has($name)
 */
PHP_METHOD(gene_session, has) {
	zend_string *name;
	zval *self = getThis(), *data = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &name) == FAILURE) {
		RETURN_NULL();
	}

	if (name) {
		zval *sess = gene_session_get_data(self);
		if (sess && Z_TYPE_P(sess) == IS_ARRAY) {
			data = gene_session_walk_path(sess, name, 0, NULL, NULL);
		}
	}
	if (data) {
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ public gene_session::cookie($name, $value, $time)
 */
PHP_METHOD(gene_session, cookie) {
	zval *self = getThis(), *name = NULL, *value = NULL, *time = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zzz", &name, &value, &time) == FAILURE) {
		RETURN_FALSE;
	}

	gene_set_cookie(self, name, value, time);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::getSessionId()
 */
PHP_METHOD(gene_session, getSessionId) {
	zval *self = getThis(),*data = NULL;

	data = zend_read_property(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_ID), 1, NULL);
	if (Z_TYPE_P(data) == IS_STRING) {
		RETURN_ZVAL(data, 1, 0);
	}

	RETURN_NULL();
}
/* }}} */

/** {{{ public static gene_session::setSessionId()
 */
PHP_METHOD(gene_session, setSessionId) {
	zval *self = getThis(),*cookie = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &cookie) == FAILURE) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(cookie) == IS_STRING && Z_STRLEN_P(cookie) > 0) {
		zend_string *filtered = gene_session_sanitize_cookie_id(Z_STR_P(cookie));
		if (filtered) {
			gene_session_update_ids(self, filtered);
			zend_string_release(filtered);
			gene_session_mark_new(self, 0);
			gene_session_mark_dirty(self);
			gene_session_auto_cookie(self);
		}
	}

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ public static gene_session::setLifeTime()
 */
PHP_METHOD(gene_session, setLifeTime) {
	zval *self = getThis(),*time = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &time) == FAILURE) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(time) == IS_LONG) {
		zend_update_property_long(gene_session_ce, gene_strip_obj(self), ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), Z_LVAL_P(time));
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/*
 * {{{ gene_session_methods
 */
const zend_function_entry gene_session_methods[] = {
	PHP_ME(gene_session, __construct, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, __destruct, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, load, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, save, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, setSessionId, gene_session_sets_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, getSessionId, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, setLifeTime, gene_session_sets_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, destroy, gene_session_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(gene_session, cookie, gene_session_cookie_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __get, get, gene_session_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __isset, has, gene_session_has_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __set, set, gene_session_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(gene_session, __unset, del, gene_session_del_arginfo, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};
/* }}} */

/*
 * {{{ GENE_MINIT_FUNCTION
 */
GENE_MINIT_FUNCTION(session) {
    zend_class_entry gene_session;
    GENE_INIT_CLASS_ENTRY(gene_session, "Gene_Session", "Gene\\Session", gene_session_methods);
    gene_session_ce = zend_register_internal_class(&gene_session);
#if PHP_VERSION_ID >= 80200
    gene_session_ce->ce_flags |= ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES;
#endif


    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_DRIVER), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_DATA), ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_ID), ZEND_ACC_PUBLIC);
    zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_NAME), "SSID", ZEND_ACC_PUBLIC);
    zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_PREFIX), "", ZEND_ACC_PUBLIC);
    zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_ID), ZEND_ACC_PUBLIC);
    zend_declare_property_long(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME), 86400, ZEND_ACC_PUBLIC);
	zend_declare_property_long(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_UPTIME), 1800, ZEND_ACC_PUBLIC);
	zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN), "", ZEND_ACC_PUBLIC);
	zend_declare_property_string(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_PATH), "/", ZEND_ACC_PUBLIC);
	zend_declare_property_bool(gene_session_ce, ZEND_STRL(GENE_SESSION_SECURE), 0, ZEND_ACC_PUBLIC);
	zend_declare_property_bool(gene_session_ce, ZEND_STRL(GENE_SESSION_HTTPONLY), 1, ZEND_ACC_PUBLIC);
	zend_declare_property_null(gene_session_ce, ZEND_STRL(GENE_SESSION_HANDLER), ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_session_ce, ZEND_STRL(GENE_SESSION_DIRTY), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_SENT), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_bool(gene_session_ce, ZEND_STRL(GENE_SESSION_COOKIE_NEW), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_long(gene_session_ce, ZEND_STRL(GENE_SESSION_HASH_MODE), 0, ZEND_ACC_PROTECTED);

	/* Cache instance-property offsets used in the cookie hot path */
	{
		zend_property_info *pi;
		pi = zend_hash_str_find_ptr(&gene_session_ce->properties_info, ZEND_STRL(GENE_SESSION_NAME));
		gene_session_offset_name = pi->offset;
		pi = zend_hash_str_find_ptr(&gene_session_ce->properties_info, ZEND_STRL(GENE_SESSION_COOKIE_ID));
		gene_session_offset_cookie_id = pi->offset;
		pi = zend_hash_str_find_ptr(&gene_session_ce->properties_info, ZEND_STRL(GENE_SESSION_COOKIE_LIFTTIME));
		gene_session_offset_lifetime = pi->offset;
		pi = zend_hash_str_find_ptr(&gene_session_ce->properties_info, ZEND_STRL(GENE_SESSION_COOKIE_PATH));
		gene_session_offset_cookie_path = pi->offset;
		pi = zend_hash_str_find_ptr(&gene_session_ce->properties_info, ZEND_STRL(GENE_SESSION_COOKIE_DOMAIN));
		gene_session_offset_cookie_domain = pi->offset;
		pi = zend_hash_str_find_ptr(&gene_session_ce->properties_info, ZEND_STRL(GENE_SESSION_SECURE));
		gene_session_offset_secure = pi->offset;
		pi = zend_hash_str_find_ptr(&gene_session_ce->properties_info, ZEND_STRL(GENE_SESSION_HTTPONLY));
		gene_session_offset_httponly = pi->offset;
	}

	return SUCCESS; // @suppress("Symbol is not resolved")
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
