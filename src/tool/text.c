/*
 +----------------------------------------------------------------------+
 | gene                                                                 |
 +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "Zend/zend_API.h"
#include "zend_exceptions.h"
#include "zend_smart_str.h"
#include <string.h>

#include "../gene.h"
#include "../http/json.h"
#include "text.h"

zend_class_entry *gene_text_ce;

#define GENE_TEXT_CHUNK_MAX_CHARS 8192
#define GENE_TEXT_CHUNK_MAX_COUNT 4096
#define GENE_TEXT_MAX_INPUT (16 * 1024 * 1024)

static int gene_text_php_call(const char *name, size_t name_len, uint32_t argc, zval *params, zval *retval) {
	zend_function *fn;
	ZVAL_UNDEF(retval);
	fn = zend_hash_str_find_ptr(CG(function_table), name, name_len);
	if (UNEXPECTED(!fn)) {
		return FAILURE;
	}
	zend_call_known_function(fn, NULL, NULL, retval, argc, params, NULL);
	return EG(exception) ? FAILURE : SUCCESS;
}

static size_t gene_text_utf8_seq_len(const unsigned char *p, const unsigned char *end) {
	unsigned char c = *p;
	size_t need = 1;

	if (c <= 0x7F) {
		return 1;
	}
	if ((c & 0xE0) == 0xC0) {
		need = 2;
	} else if ((c & 0xF0) == 0xE0) {
		need = 3;
	} else if ((c & 0xF8) == 0xF0) {
		need = 4;
	}
	if ((size_t)(end - p) >= need) {
		size_t i;
		for (i = 1; i < need; i++) {
			if ((p[i] & 0xC0) != 0x80) {
				return 1;
			}
		}
		return need;
	}
	return 1;
}

static zend_long gene_text_utf8_cp_len(zend_string *s) {
	const unsigned char *p = (const unsigned char *)ZSTR_VAL(s);
	const unsigned char *end = p + ZSTR_LEN(s);
	zend_long n = 0;

	while (p < end) {
		size_t step = gene_text_utf8_seq_len(p, end);
		n++;
		p += step;
	}
	return n;
}

static zend_string *gene_text_utf8_cp_sub(zend_string *s, zend_long off, zend_long len) {
	const unsigned char *p = (const unsigned char *)ZSTR_VAL(s);
	const unsigned char *end = p + ZSTR_LEN(s);
	const unsigned char *out_start;

	if (off < 0) {
		off = 0;
	}
	if (len < 0) {
		len = 0;
	}
	while (p < end && off > 0) {
		p += gene_text_utf8_seq_len(p, end);
		off--;
	}
	out_start = p;
	while (p < end && len > 0) {
		p += gene_text_utf8_seq_len(p, end);
		len--;
	}
	return zend_string_init((char *)out_start, (size_t)(p - out_start), 0);
}

static zend_long gene_text_mb_len(zend_string *s) {
	zval params[2], out;
	ZVAL_STR(&params[0], s);
	ZVAL_STRING(&params[1], "UTF-8");
	if (gene_text_php_call("mb_strlen", sizeof("mb_strlen") - 1, 2, params, &out) != SUCCESS
		|| Z_TYPE(out) != IS_LONG) {
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&out);
		return gene_text_utf8_cp_len(s);
	}
	{
		zend_long n = Z_LVAL(out);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&out);
		return n;
	}
}

static zend_string *gene_text_mb_sub(zend_string *s, zend_long off, zend_long len) {
	zval params[4], out;
	ZVAL_STR(&params[0], s);
	ZVAL_LONG(&params[1], off);
	ZVAL_LONG(&params[2], len);
	ZVAL_STRING(&params[3], "UTF-8");
	if (gene_text_php_call("mb_substr", sizeof("mb_substr") - 1, 4, params, &out) != SUCCESS
		|| Z_TYPE(out) != IS_STRING) {
		zval_ptr_dtor(&params[3]);
		zval_ptr_dtor(&out);
		return gene_text_utf8_cp_sub(s, off, len);
	}
	zval_ptr_dtor(&params[3]);
	return Z_STR(out);
}

static zend_string *gene_text_sanitize_utf8(zend_string *in) {
	const unsigned char *p = (const unsigned char *)ZSTR_VAL(in);
	const unsigned char *end = p + ZSTR_LEN(in);
	smart_str out = {0};
	while (p < end) {
		unsigned char c = *p;
		size_t need = 1;
		if (c == 0) {
			p++;
			continue;
		}
		if (c <= 0x7F) {
			smart_str_appendc(&out, (char)c);
			p++;
			continue;
		}
		if ((c & 0xE0) == 0xC0) need = 2;
		else if ((c & 0xF0) == 0xE0) need = 3;
		else if ((c & 0xF8) == 0xF0) need = 4;
		else need = 1;
		if ((size_t)(end - p) >= need) {
			size_t i;
			int ok = 1;
			for (i = 1; i < need; i++) {
				if ((p[i] & 0xC0) != 0x80) {
					ok = 0;
					break;
				}
			}
			if (ok) {
				smart_str_appendl(&out, (const char *)p, need);
				p += need;
				continue;
			}
		}
		smart_str_appendl(&out, "\xEF\xBF\xBD", 3);
		p++;
	}
	smart_str_0(&out);
	if (!out.s) {
		return ZSTR_EMPTY_ALLOC();
	}
	return out.s;
}

ZEND_BEGIN_ARG_INFO_EX(gene_text_str_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, s)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(gene_text_chunk_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, s)
	ZEND_ARG_INFO(0, maxChars)
	ZEND_ARG_INFO(0, overlap)
ZEND_END_ARG_INFO()

PHP_METHOD(gene_text, utf8Len) {
	zend_string *s, *clean;
	zend_long n;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &s) == FAILURE) {
		return;
	}
	clean = gene_text_sanitize_utf8(s);
	n = gene_text_mb_len(clean);
	zend_string_release(clean);
	RETURN_LONG(n);
}

PHP_METHOD(gene_text, sanitizeMb4) {
	zend_string *s, *clean;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &s) == FAILURE) {
		return;
	}
	clean = gene_text_sanitize_utf8(s);
	RETURN_STR(clean);
}

static void gene_text_hard_split(zval *out, zend_string *s, zend_long max_chars, zend_long overlap, zend_long *count) {
	zend_long len = gene_text_mb_len(s);
	zend_long off = 0;
	zend_long step = max_chars - overlap;
	if (step < 1) step = 1;
	while (off < len && *count < GENE_TEXT_CHUNK_MAX_COUNT) {
		zend_string *part = gene_text_mb_sub(s, off, max_chars);
		if (ZSTR_LEN(part) > 0) {
			add_next_index_str(out, part);
			(*count)++;
		} else {
			zend_string_release(part);
		}
		off += step;
	}
}

static void gene_text_add_chunk(zval *out, zend_string *chunk, zend_long *count) {
	zend_string *clean;
	if (*count >= GENE_TEXT_CHUNK_MAX_COUNT) {
		return;
	}
	clean = gene_text_sanitize_utf8(chunk);
	if (ZSTR_LEN(clean) == 0) {
		zend_string_release(clean);
		return;
	}
	add_next_index_str(out, clean);
	(*count)++;
}

PHP_METHOD(gene_text, chunk) {
	zend_string *s, *norm;
	zend_long max_chars = 1200, overlap = 80, count = 0;
	char *buf;
	size_t blen, i;
	zval parts;
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|ll", &s, &max_chars, &overlap) == FAILURE) {
		return;
	}
	if (ZSTR_LEN(s) > GENE_TEXT_MAX_INPUT) {
		zend_throw_exception_ex(NULL, 0, "Gene\\Text::chunk input exceeds maximum size");
		RETURN_THROWS();
	}
	if (max_chars < 1) max_chars = 1;
	if (max_chars > GENE_TEXT_CHUNK_MAX_CHARS) max_chars = GENE_TEXT_CHUNK_MAX_CHARS;
	if (overlap < 0) overlap = 0;
	if (overlap >= max_chars) overlap = max_chars - 1;

	norm = gene_text_sanitize_utf8(s);
	buf = estrndup(ZSTR_VAL(norm), ZSTR_LEN(norm));
	blen = ZSTR_LEN(norm);
	zend_string_release(norm);
	for (i = 0; i < blen; i++) {
		if (buf[i] == '\r') buf[i] = '\n';
	}
	{
		size_t w = 0;
		for (i = 0; i < blen; i++) {
			if (buf[i] == ' ' || buf[i] == '\t') {
				if (w > 0 && buf[w - 1] != ' ') {
					buf[w++] = ' ';
				}
			} else if (buf[i] == '\n' && w >= 2 && buf[w - 1] == '\n' && buf[w - 2] == '\n') {
				continue;
			} else {
				buf[w++] = buf[i];
			}
		}
		blen = w;
		buf[blen] = '\0';
	}

	array_init(return_value);
	array_init(&parts);
	{
		char *p = buf, *seg;
		while ((seg = strstr(p, "\n\n")) != NULL) {
			*seg = '\0';
			if (p[0]) add_next_index_stringl(&parts, p, strlen(p));
			p = seg + 2;
		}
		if (p[0]) add_next_index_stringl(&parts, p, strlen(p));
	}

	{
		zend_ulong idx;
		zval *part;
		zend_string *buf_str = ZSTR_EMPTY_ALLOC();
		ZEND_HASH_FOREACH_NUM_KEY_VAL(Z_ARRVAL(parts), idx, part) {
			zend_string *ps, *combined;
			zend_long buf_len, part_len;
			(void)idx;
			if (Z_TYPE_P(part) != IS_STRING) continue;
			ps = Z_STR_P(part);
			part_len = gene_text_mb_len(ps);
			buf_len = gene_text_mb_len(buf_str);
			if (buf_len == 0) {
				zend_string_release(buf_str);
				buf_str = zend_string_copy(ps);
			} else if (buf_len + 2 + part_len <= max_chars) {
				combined = zend_string_alloc(ZSTR_LEN(buf_str) + 2 + ZSTR_LEN(ps), 0);
				memcpy(ZSTR_VAL(combined), ZSTR_VAL(buf_str), ZSTR_LEN(buf_str));
				memcpy(ZSTR_VAL(combined) + ZSTR_LEN(buf_str), "\n\n", 2);
				memcpy(ZSTR_VAL(combined) + ZSTR_LEN(buf_str) + 2, ZSTR_VAL(ps), ZSTR_LEN(ps));
				ZSTR_VAL(combined)[ZSTR_LEN(buf_str) + 2 + ZSTR_LEN(ps)] = '\0';
				zend_string_release(buf_str);
				buf_str = combined;
			} else {
				if (gene_text_mb_len(buf_str) <= max_chars) {
					gene_text_add_chunk(return_value, buf_str, &count);
				} else {
					gene_text_hard_split(return_value, buf_str, max_chars, overlap, &count);
				}
				zend_string_release(buf_str);
				buf_str = zend_string_copy(ps);
			}
		} ZEND_HASH_FOREACH_END();
		if (ZSTR_LEN(buf_str) > 0) {
			if (gene_text_mb_len(buf_str) <= max_chars) {
				gene_text_add_chunk(return_value, buf_str, &count);
			} else {
				gene_text_hard_split(return_value, buf_str, max_chars, overlap, &count);
			}
		}
		zend_string_release(buf_str);
	}
	zval_ptr_dtor(&parts);
	efree(buf);
}

const zend_function_entry gene_text_methods[] = {
	PHP_ME(gene_text, utf8Len, gene_text_str_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_text, sanitizeMb4, gene_text_str_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(gene_text, chunk, gene_text_chunk_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	{NULL, NULL, NULL}
};

GENE_MINIT_FUNCTION(text) {
	zend_class_entry ce;
	GENE_INIT_CLASS_ENTRY(ce, "Gene_Text", "Gene\\Text", gene_text_methods);
	gene_text_ce = zend_register_internal_class(&ce);
	gene_text_ce->ce_flags |= ZEND_ACC_FINAL;
	return SUCCESS;
}
