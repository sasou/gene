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

#include <stdio.h>
#include <stdlib.h>
#include "php.h"
#include "php_streams.h"
#include <stdint.h>
#include "ext/standard/md5.h"
#include "string.h"
#include <ctype.h>
#include "../common/common.h"
#include "../gene.h"

char *str_init(const char *s)
{
	size_t s_l;
	char *p;
	if (s == NULL) {
		return NULL;
	}
	s_l = strlen(s);
	p = (char *) emalloc(s_l + 1);
	memcpy(p, s, s_l + 1);
	return p;
}

char *str_sub(char *s, size_t s_len)
{
	size_t s_l = strlen(s);
	if (s_len < s_l) {
		s_l = s_len;
	}
	char *p = (char *) emalloc(s_l + 1);
	memcpy(p, s, s_l);
	p[s_l] = 0;
	return p;
}

/* [GENE_AUDIT:2026-03-25] Changed len from int to size_t to prevent
 * negative length causing undersized allocation. */
char *str_sub_len(char *src, size_t start, size_t len) {
   size_t i, src_len;
   char *dest;
   if (src == NULL) {
      return NULL;
   }
   src_len = strlen(src);
   if (start > src_len) {
      return NULL;
   }
   if (len > src_len - start) {
      len = src_len - start;
   }
   dest = (char *) emalloc(len + 1);
   for (i = 0; i < len; i++) {
      dest[i] = src[start + i];
   }
   dest[i] = 0;
   return dest;
}

char *str_append(char *s, const char*t)
{
	size_t s_len, t_len;
    char *p;
	if (s == NULL || t == NULL) {
		return NULL;
	}
	s_len = strlen(s);
	t_len = strlen(t);
	p = erealloc(s, s_len + t_len + 1);
	memcpy(p + s_len, t, t_len + 1);
	return p;
}

char *str_concat(char *s, const char *t)
{
	size_t s_l = strlen(s);
	size_t t_l = strlen(t);
    char *p = NULL;
	p = (char *) emalloc(s_l + t_l + 1);
    memcpy(p, s, s_l);
    memcpy(p + s_l, t, t_l);
	p[s_l + t_l] = 0;
	return p;
}

/*
 * {{{ char *strupr(char *str)
 */
char *gene_strtoupper(char *str) {
	char *orign = str;
	while (*str != '\0') {
		*str = toupper(*str);
		str++;
	}
	return orign;
}
/* }}} */

/*
 * {{{ char *strupr(char *str)
 */
char *gene_strtolower(char *str) {
	char *orign = str;
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return orign;
}
/* }}} */

/*
 * {{{ char *firstToUpper(char *str)
 */
char *firstToUpper(char *str) {
	char *orign = str;
	if (*str != '\0') {
		*str = toupper(*str);
	}
	return orign;
}
/* }}} */

/*
 * {{{ char *firstToUpper(char *str)
 */
char *firstToLower(char *str) {
	char *orign = str;
	if (*str != '\0') {
		*str = tolower(*str);
	}
	return orign;
}
/* }}} */

/*
 * {{{ left(char *dst,char *src, int n)
 */
void left(char *dst, char *src, size_t n) {
	char *p = src;
	char *q = dst;
	size_t len = strlen(src);
	if (n > len)
		n = len;
	/*p += (len-n);*/
	while (n--)
		*(q++) = *(p++);
	*(q++) = '\0';
	return;
}
/* }}} */

/*
 * {{{ size_t leftByChar(char *dst, char *src, char val)
 * [GENE_PERF:2026-04-20] Returns the number of bytes copied (length of dst,
 * excluding the null terminator). Callers can use this to populate a
 * pre-known length cache without a subsequent strlen() pass.
 * Always writes a null terminator even when src is empty or starts with val.
 */
size_t leftByChar(char *dst, char *src, char val) {
	char *q = dst;
	while (*src != '\0') {
		if (*src == val)
			break;
		*(q++) = *(src++);
	}
	*q = '\0';
	return (size_t)(q - dst);
}
/* }}} */

/*
 * {{{ mid(char *dst, char *src, int n,int m) n为长度，m为位置
 */
void mid(char *dst, char *src, size_t n, size_t m) {
	char *p = src;
	char *q = dst;
	size_t len = strlen(src);
	if (n > len)
		n = len - m;
	/* m is size_t (unsigned), no need to check < 0 */
	//if(m>len) return NULL;
	p += m;
	while (n--)
		*(q++) = *(p++);
	*(q++) = '\0';
	return;
}
/* }}} */

/*
 * {{{ right(char *dst,char *src, int n)
 */
void right(char *dst, char *src, size_t n) {
	char *p = src;
	char *q = dst;
	size_t len = strlen(src);
	if (n > len)
		n = len;
	p += (len - n);
	while (*(q++) = *(p++)) // @suppress("Assignment in condition")
		;
	return;
}
/* }}} */

/*
 * {{{ right(char *dst, char *src, int n)
 */
char * replaceAll(char * src, const char oldChar, const char newChar) {
	char *head = src;
	while (*src != '\0') {
		if (*src == oldChar)
			*src = newChar;
		src++;
	}
	return head;
}
/* }}} */

/*
 * {{{ insertAll(char *dst, char *src, int n)
 */
char * insertAll(char * src, char oldChar, char newChar) {
	size_t extra = 0;
	char *p, *head, *dest;
	if (src == NULL) {
		return NULL;
	}
	for (p = src; *p != '\0'; p++) {
		if (*p == oldChar) extra++;
	}
	dest = (char *) emalloc(strlen(src) + extra + 1);
	head = dest;
	while (*src != '\0') {
		if (*src == oldChar) {
			*dest = *src;
			dest++;
			*dest = newChar;
		} else {
			*dest = *src;
		}
		src++;
		dest++;
	}
	*dest = '\0';
	return head;
}
/* }}} */

/*
 * {{{ trim(char* s, char c)
 */
void trim(char* s, const char c) {
	char *t = s;
	while (*s == c) {
		s++;
	};
	if (*s) {
		char* t1 = s;
		while (*s) {
			s++;
		};
		s--;
		while (*s == c) {
			s--;
		};
		while (t1 <= s) {
			*(t++) = *(t1++);
		}
	}
	*t = 0;
}
/* }}} */

/*
 * {{{ replace(char originalString[], char key[], char swap[], size_t buf_size)
 * [GENE_AUDIT:2026-03-25] Added buf_size parameter to prevent heap buffer overflow
 * when swap is longer than key and result exceeds originalString's allocation.
 * If buf_size is 0, legacy behavior (no bounds check) is used for backward compat.
 */
void replace(char originalString[], char key[], char swap[], size_t buf_size) {
	size_t lengthOfOriginalString, lengthOfKey, lengthOfSwap, i, j, flag;
	char *tmp = NULL;
	size_t tmp_size = 0;

	lengthOfOriginalString = strlen(originalString);
	lengthOfKey = strlen(key);
	lengthOfSwap = strlen(swap);

	if (lengthOfKey == 0) return;

	for (i = 0; i <= lengthOfOriginalString - lengthOfKey; i++) {
		flag = 1;
		for (j = 0; j < lengthOfKey; j++) {
			if (originalString[i + j] != key[j]) {
				flag = 0;
				break;
			}
		}
		if (flag) {
			size_t new_len = lengthOfOriginalString - lengthOfKey + lengthOfSwap;
			if (buf_size > 0 && new_len + 1 > buf_size) {
				if (tmp) efree(tmp);
				return;
			}
			if (new_len + 1 > tmp_size) {
				tmp_size = new_len + 1;
				if (tmp) efree(tmp);
				tmp = (char *) emalloc(tmp_size);
			}
			memcpy(tmp, originalString, i);
			memcpy(tmp + i, swap, lengthOfSwap);
			memcpy(tmp + i + lengthOfSwap, originalString + i + lengthOfKey, lengthOfOriginalString - i - lengthOfKey + 1);
			memcpy(originalString, tmp, new_len + 1);
			i += lengthOfSwap - 1;
			lengthOfOriginalString = new_len;
		}
	}
	if (tmp) efree(tmp);
}
/* }}} */

/*
 * {{{ int findChildCnt(char* str1, char* str2)
 */
int findChildCnt(char* str1, const char* str2) {
	size_t len = strlen(str2);
	int cnt = 0;
	while (str1 = strstr(str1, str2)) { // @suppress("Assignment in condition")
		cnt++;
		str1 += len;
	}
	return cnt;
}
/* }}} */

/*
 * {{{ int findChildC(char* src, char* car)
 */
int findChildC(char* src, const char car) {
	char *h = src;
	int num = 0;
	while (*h != '\0') {
		if (*h == car)
			num++;
		h++;
	}
	return num;
}
/* }}} */

/*
 * {{{ char * charIfFirst(char * src,const char oldChar)
 */
int charIfFirst(char *src, const char car) {
	char *h = src;
	if (h[0] == car) {
		return 1;
	}
	return 0;
}
/* }}} */

/*
 * {{{ char * replace_string (char * string, const char * source, const char * destination )
 */
char * replace_string(char * string, char source, const char * destination) {
	char *newstr, *out;
	const char *in;
	size_t destination_len, newsize, num;
	if (string == NULL || destination == NULL) {
		return NULL;
	}
	num = findChildC(string, source);
	if (num == 0) {
		return NULL;
	}
	destination_len = strlen(destination);
	newsize = strlen(string) + num * destination_len - num + 1;
	newstr = (char *) emalloc(newsize);
	out = newstr;
	for (in = string; *in != '\0'; in++) {
		if (*in == source) {
			memcpy(out, destination, destination_len);
			out += destination_len;
		} else {
			*out++ = *in;
		}
	}
	*out = '\0';
	return newstr;
}
/* }}} */

/*
 * {{{ char * replace_string (char * string, const char * source, const char * destination )
 * [GENE_AUDIT:2026-07-03 P3] Added buf_size parameter to prevent heap buffer
 * overflow when the replacement string is longer than the match string. The
 * previous in-place memcpy back into sSrc had no capacity check — safe only as
 * long as replace_len <= match_len (true for the sole existing caller
 * "in(?)"->"$"). Now refuses (returns -2) if a replacement would exceed
 * buf_size, neutralizing the landmine for any future caller. */
int ReplaceStr(char* sSrc, size_t buf_size, char* sMatchStr, char* sReplaceStr) {
	size_t StringLen, src_len, match_len, replace_len, new_len;
	char *caNewString = NULL;
	char* FindPos;

	if ((!sMatchStr) || (!*sMatchStr))
		return -1;

	FindPos = (char *) strstr(sSrc, sMatchStr);
	if (!FindPos)
		return -1;

	match_len = strlen(sMatchStr);
	replace_len = strlen(sReplaceStr);

	while (FindPos) {
		src_len = strlen(sSrc);
		new_len = src_len - match_len + replace_len;
		if (new_len + 1 > buf_size) {
			/* Replacement would overflow the caller's buffer — abort rather
			 * than corrupt the heap. The input string is left in a possibly
			 * partially-replaced state, but no out-of-bounds write occurs. */
			php_error_docref(NULL, E_WARNING, "ReplaceStr: replacement would overflow buffer (need %zu, have %zu)", new_len + 1, buf_size);
			return -2;
		}
		caNewString = (char *) emalloc(new_len + 1);
		StringLen = FindPos - sSrc;
		memcpy(caNewString, sSrc, StringLen);
		memcpy(caNewString + StringLen, sReplaceStr, replace_len);
		memcpy(caNewString + StringLen + replace_len, FindPos + match_len, src_len - StringLen - match_len + 1);
		memcpy(sSrc, caNewString, new_len + 1);
		efree(caNewString);

		FindPos = (char *) strstr(sSrc, sMatchStr);
	}
	return 0;
}
/* }}} */

/*
 * {{{ char * insert_string (char * string, const char * source, const char * destination )
 */
char * insert_string(char * string, const char * source,
		const char * destination) {
	char *sk, *newstr, *retstr;
	size_t size,pos = 0;
	sk = strstr(string, source);
	if (sk == NULL)
		return NULL;
	size = strlen(string) + strlen(destination) + 1;
	newstr = (char*) emalloc(size);
	if (newstr == NULL)
		return NULL;

	retstr = (char*) emalloc(size);
	if (retstr == NULL) {
		efree(newstr);
		return NULL;
	}

	snprintf(newstr, size - 1, "%s", string);

	sk = strstr(newstr, source);

	pos = 0;
	sk += strlen(source);

	memcpy(retstr + pos, newstr, sk - newstr);
	pos += sk - newstr;

	memcpy(retstr + pos, destination, strlen(destination));
	pos += strlen(destination);
	memcpy(retstr + pos, sk, strlen(sk));
	pos += strlen(sk);
	retstr[pos] = '\0';

	efree(newstr);
	return retstr;

}
/* }}} */

int fullToHalf(char *sFullStr, char *sHalfStr) {
	size_t iFullStrIndex = 0;
	size_t iHalfStrIndex = 0;

	size_t iFullStrLen = strlen(sFullStr);

	for (; iFullStrIndex < iFullStrLen; iFullStrIndex++, iHalfStrIndex++) {

		if (sFullStr[iFullStrIndex] == 0xA3) {
			sHalfStr[iHalfStrIndex] = sFullStr[++iFullStrIndex] - 0x80;

		} else {
			sHalfStr[iHalfStrIndex] = sFullStr[iFullStrIndex];

		}

	}

	return 0;
}

int halfToFull(char *sFullStr, char *sHalfStr) {
	size_t iFullStrIndex = 0;
	size_t iHalfStrIndex = 0;

	size_t iHalfStrlen = strlen(sHalfStr);

	for (; iHalfStrIndex < iHalfStrlen; iHalfStrIndex++, iFullStrIndex++) {

		if (sHalfStr[iHalfStrIndex] < 0x80) {
			sFullStr[iFullStrIndex] = 0xA3;
			sFullStr[++iFullStrIndex] = sHalfStr[iHalfStrIndex] + 0x80;

		} else {
			sFullStr[iFullStrIndex] = sHalfStr[iHalfStrIndex];

		}

	}

	return 0;
}

void remove_extra_space(char *str) {
	char *sp = str;
	char *prev = 0;

	while (*str && (*str == ' ' || *str == '\n' || *str == '\t'))
		++str;

	for (; *str; ++str) {
		switch (*str) {
		case '\t':
			*str = ' ';
			/* no break */
		case ' ':
			if (*prev == '\n' || *prev == ' ')
				continue;
			break;

		case '\n':
			if (*prev == '\n')
				continue;
			else if (*prev == ' ') {
				*prev = '\n';
				continue;
			}
			break;
		}

		prev = sp;
		*sp++ = *str;
	}

	if (prev && *prev && (*prev == ' ' || *prev == '\n'))
		--sp;
	*sp = 0;
}

char *readfilecontent(char *file) {
	char *tmp = NULL;
	php_stream *stream = NULL;
	zend_string *contents = NULL;
	if (strstr(file, "..") != NULL) {
		php_error_docref(NULL, E_WARNING, "Path traversal detected in file path: %s", file);
		return NULL;
	}
	stream = php_stream_open_wrapper(file, "rb", REPORT_ERRORS, NULL);
	if (stream) {
		contents = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
		php_stream_close(stream);
		if (contents) {
			tmp = estrndup(ZSTR_VAL(contents), ZSTR_LEN(contents));
			zend_string_release(contents);
		}
	}
	return tmp;
}

char *strreplace(char *original, char *pattern, char *replacement)
{
   size_t replen = strlen(replacement);
   size_t patlen = strlen(pattern);
   size_t orilen = strlen(original);

  int patcnt = 0;
  char * oriptr;
  char * patloc;

  // find how many times the pattern occurs in the original string
  for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
  {
    patcnt++;
  }

  {
    // allocate memory for the new string
	size_t const retlen = orilen + patcnt * replen - patcnt * patlen;
    char *returned = (char *) ecalloc(retlen + 1,  sizeof(char));

    if (returned != NULL)
    {
      // copy the original string,
      // replacing all the instances of the pattern
      char * retptr = returned;
      for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
      {
    	size_t skplen = patloc - oriptr;
        // [GENE_PERF:2026-04-27] memcpy: dest is ecalloc'd (zeroed); skip strncpy zero-fill.
        memcpy(retptr, oriptr, skplen);
        retptr += skplen;
        memcpy(retptr, replacement, replen);
        retptr += replen;
      }
      // copy the rest of the string.
      strcpy(retptr, oriptr);
    }
    efree(original);
    return returned;
  }
}

char *strreplace2(char *src, char *from, char *to)
{
   size_t size    = strlen(src) + 1;
   size_t fromlen = strlen(from);
   size_t tolen   = strlen(to);
   char *value = ecalloc(size, sizeof(char));
   char *dst = value;
   if ( value != NULL )
   {
      for ( ;; )
      {
         const char *match = strstr(src, from);
         if ( match != NULL )
         {
        	size_t count = match - src;
            char *temp;
            size += tolen - fromlen;
            temp = erealloc(value, size);
            if ( temp == NULL )
            {
               efree(value);
               return NULL;
            }
            dst = temp + (dst - value);
            value = temp;
            memmove(dst, src, count);
            src += count;
            dst += count;
            memmove(dst, to, tolen);
            src += fromlen;
            dst += tolen;
         }
         else /* No match found. */
         {
            strcpy(dst, src);
            break;
         }
      }
   }
   return value;
}

char *gene_strreplace_fast(const char *src, size_t src_len, const char *from, size_t from_len, const char *to, size_t to_len, size_t *out_len)
{
	const char *p, *match;
	char *dst, *d;
	size_t count = 0, tail_len, new_len;

	if (!src || !from || !to || from_len == 0) {
		return NULL;
	}

	p = src;
	while ((match = strstr(p, from)) != NULL) {
		count++;
		p = match + from_len;
	}

	if (count == 0) {
		return NULL;
	}

	if (to_len >= from_len) {
		new_len = src_len + count * (to_len - from_len);
	} else {
		new_len = src_len - count * (from_len - to_len);
	}

	dst = emalloc(new_len + 1);
	d = dst;
	p = src;
	while ((match = strstr(p, from)) != NULL) {
		tail_len = (size_t)(match - p);
		if (tail_len > 0) {
			memcpy(d, p, tail_len);
			d += tail_len;
		}
		if (to_len > 0) {
			memcpy(d, to, to_len);
			d += to_len;
		}
		p = match + from_len;
	}
	tail_len = src_len - (size_t)(p - src);
	if (tail_len > 0) {
		memcpy(d, p, tail_len);
		d += tail_len;
	}
	*d = '\0';
	if (out_len) {
		*out_len = new_len;
	}
	return dst;
}

/* [GENE_AUDIT:2026-03-25] Kept call_user_function wrappers instead of direct Zend
 * internal APIs (php_json_encode, php_json_decode, php_var_serialize, etc.).
 * Reason: these internal APIs change signatures across PHP minor versions (e.g.
 * php_json_encode added encoder parameter in 8.1), creating maintenance burden.
 * call_user_function is stable across PHP 8.x and these functions are NOT in the
 * hot request dispatch path, so the ~0.5us overhead per call is acceptable. */
void gene_json_encode(zval *value, zval *options, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "json_encode");
	zval params[] = { *value, *options };
	zend_call_known_function(fn, NULL, NULL, retval, 2, params, NULL);
}/*}}}*/

void gene_json_decode(zval *value, zval *options, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "json_decode");
	zval params[] = { *value, *options };
	zend_call_known_function(fn, NULL, NULL, retval, 2, params, NULL);
}/*}}}*/

void gene_func_get_args(zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "func_get_args");
	zend_call_known_function(fn, NULL, NULL, retval, 0, NULL, NULL);
}/*}}}*/

void gene_func_num_args(zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "func_num_args");
	zend_call_known_function(fn, NULL, NULL, retval, 0, NULL, NULL);
}/*}}}*/

void gene_serialize(zval *value, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "serialize");
	zval params[] = { *value };
	zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_unserialize(zval *value, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "unserialize");
	zval params[] = { *value };
	zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_igbinary_serialize(zval *value, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "igbinary_serialize");
	if (UNEXPECTED(!fn)) { ZVAL_FALSE(retval); return; }
	zval params[] = { *value };
	zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_igbinary_unserialize(zval *value, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "igbinary_unserialize");
	if (UNEXPECTED(!fn)) { ZVAL_FALSE(retval); return; }
	zval params[] = { *value };
	zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_md5_buf(const char *data, size_t len, zval *retval) /*{{{*/
{
	PHP_MD5_CTX ctx;
	unsigned char digest[16];
	zend_string *out;

	PHP_MD5Init(&ctx);
	if (len != 0) {
		PHP_MD5Update(&ctx, data, len);
	}
	PHP_MD5Final(digest, &ctx);
	out = zend_string_alloc(32, 0);
	make_digest_ex(ZSTR_VAL(out), digest, 16);
	ZVAL_STR(retval, out);
}/*}}}*/

void gene_md5(zval *value, zval *retval) /*{{{*/
{
	zval tmp;
	zend_string *str;

	if (Z_TYPE_P(value) == IS_STRING) {
		str = Z_STR_P(value);
		gene_md5_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
		return;
	}
	ZVAL_COPY(&tmp, value);
	convert_to_string(&tmp);
	str = Z_STR(tmp);
	gene_md5_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
	zval_ptr_dtor(&tmp);
}/*}}}*/

/* FNV-1a 64-bit hash constants */
#define FNV_64_INIT ((uint64_t)0xcbf29ce484222325ULL)
#define FNV_64_PRIME ((uint64_t)0x100000001b3ULL)

/* Fast FNV-1a 64-bit hash - 8x unrolled, same output as byte-by-byte */
uint64_t gene_fnv1a_64(const char *data, size_t len) /*{{{*/
{
	uint64_t hash = FNV_64_INIT;
	const unsigned char *p = (const unsigned char *)data;
	const unsigned char *end = p + len;

	/* 8x unroll: reduces loop overhead, enables CPU multiply pipelining */
	while (p + 8 <= end) {
		hash ^= p[0]; hash *= FNV_64_PRIME;
		hash ^= p[1]; hash *= FNV_64_PRIME;
		hash ^= p[2]; hash *= FNV_64_PRIME;
		hash ^= p[3]; hash *= FNV_64_PRIME;
		hash ^= p[4]; hash *= FNV_64_PRIME;
		hash ^= p[5]; hash *= FNV_64_PRIME;
		hash ^= p[6]; hash *= FNV_64_PRIME;
		hash ^= p[7]; hash *= FNV_64_PRIME;
		p += 8;
	}
	/* Remaining 0-7 bytes with Duff's device */
	switch (end - p) {
		case 7: hash ^= p[6]; hash *= FNV_64_PRIME; /* fallthrough */
		case 6: hash ^= p[5]; hash *= FNV_64_PRIME; /* fallthrough */
		case 5: hash ^= p[4]; hash *= FNV_64_PRIME; /* fallthrough */
		case 4: hash ^= p[3]; hash *= FNV_64_PRIME; /* fallthrough */
		case 3: hash ^= p[2]; hash *= FNV_64_PRIME; /* fallthrough */
		case 2: hash ^= p[1]; hash *= FNV_64_PRIME; /* fallthrough */
		case 1: hash ^= p[0]; hash *= FNV_64_PRIME;
		case 0: break;
	}
	return hash;
}/*}}}*/

void gene_hash_fast_buf(const char *data, size_t len, zval *retval) /*{{{*/
{
	uint64_t hash = gene_fnv1a_64(data, len);
	gene_set_u64_hex_result(hash, retval);
}/*}}}*/

void gene_hash_fast(zval *value, zval *retval) /*{{{*/
{
	zval tmp;
	zend_string *str;

	if (Z_TYPE_P(value) == IS_STRING) {
		str = Z_STR_P(value);
		gene_hash_fast_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
		return;
	}
	ZVAL_COPY(&tmp, value);
	convert_to_string(&tmp);
	str = Z_STR(tmp);
	gene_hash_fast_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
	zval_ptr_dtor(&tmp);
}/*}}}*/

static zend_always_inline uint32_t gene_read32le(const void *ptr)
{
	uint32_t v;
	memcpy(&v, ptr, sizeof(v));
	return v;
}

static zend_always_inline uint64_t gene_read64le(const void *ptr)
{
	uint64_t v;
	memcpy(&v, ptr, sizeof(v));
	return v;
}

static zend_always_inline uint64_t gene_rotl64(uint64_t value, int count)
{
	return (value << count) | (value >> (64 - count));
}

static zend_always_inline uint32_t gene_rotl32(uint32_t value, int count)
{
	return (value << count) | (value >> (32 - count));
}

static zend_always_inline uint64_t gene_hash_avalanche64(uint64_t h)
{
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccdULL;
	h ^= h >> 33;
	h *= 0xc4ceb9fe1a85ec53ULL;
	h ^= h >> 33;
	return h;
}

static zend_always_inline uint64_t gene_hash_len16_mix(uint64_t u, uint64_t v, uint64_t mul)
{
	uint64_t a = (u ^ v) * mul;
	a ^= a >> 47;
	uint64_t b = (v ^ a) * mul;
	b ^= b >> 47;
	b *= mul;
	return b;
}

/* xxHash64 - extremely fast non-cryptographic hash */
#define XXH_PRIME64_1 11400714785074694791ULL
#define XXH_PRIME64_2 14029467366897019727ULL
#define XXH_PRIME64_3 1609587929392839161ULL
#define XXH_PRIME64_4 9650029242287828579ULL
#define XXH_PRIME64_5 2870177450012600261ULL

static zend_always_inline uint64_t xxh64_round(uint64_t acc, uint64_t input)
{
	acc += input * XXH_PRIME64_2;
	acc = gene_rotl64(acc, 31);
	acc *= XXH_PRIME64_1;
	return acc;
}

static zend_always_inline uint64_t xxh64_merge_round(uint64_t acc, uint64_t val)
{
	val = xxh64_round(0, val);
	acc ^= val;
	acc = acc * XXH_PRIME64_1 + XXH_PRIME64_4;
	return acc;
}

uint64_t gene_xxhash64(const char *data, size_t len) /*{{{*/
{
	const uint8_t *p = (const uint8_t *)data;
	const uint8_t *end = p + len;
	uint64_t h64;

	/* Fast path for short keys (most common: cache keys, route keys) */
	if (EXPECTED(len < 32)) {
		h64 = XXH_PRIME64_5 + (uint64_t)len;
		/* Unrolled tail: handle 8/4/1 byte chunks without loops */
		if (len >= 16) {
			uint64_t k1 = xxh64_round(0, gene_read64le(p));
			h64 ^= k1;
			h64 = gene_rotl64(h64, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
			k1 = xxh64_round(0, gene_read64le(p + 8));
			h64 ^= k1;
			h64 = gene_rotl64(h64, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
			p += 16;
		} else if (len >= 8) {
			uint64_t k1 = xxh64_round(0, gene_read64le(p));
			h64 ^= k1;
			h64 = gene_rotl64(h64, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
			p += 8;
		}
		if (p + 4 <= end) {
			h64 ^= (uint64_t)gene_read32le(p) * XXH_PRIME64_1;
			h64 = gene_rotl64(h64, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
			p += 4;
		}
		while (p < end) {
			h64 ^= ((uint64_t)*p++) * XXH_PRIME64_5;
			h64 = gene_rotl64(h64, 11) * XXH_PRIME64_1;
		}
	} else {
		/* Large input: 4-lane accumulator */
		const uint8_t *limit = end - 32;
		uint64_t v1 = XXH_PRIME64_1 + XXH_PRIME64_2;
		uint64_t v2 = XXH_PRIME64_2;
		uint64_t v3 = 0;
		uint64_t v4 = 0 - XXH_PRIME64_1;
		do {
			v1 = xxh64_round(v1, gene_read64le(p)); p += 8;
			v2 = xxh64_round(v2, gene_read64le(p)); p += 8;
			v3 = xxh64_round(v3, gene_read64le(p)); p += 8;
			v4 = xxh64_round(v4, gene_read64le(p)); p += 8;
		} while (p <= limit);
		h64 = gene_rotl64(v1, 1) + gene_rotl64(v2, 7) + gene_rotl64(v3, 12) + gene_rotl64(v4, 18);
		h64 = xxh64_merge_round(h64, v1);
		h64 = xxh64_merge_round(h64, v2);
		h64 = xxh64_merge_round(h64, v3);
		h64 = xxh64_merge_round(h64, v4);
		h64 += (uint64_t)len;
		/* Tail processing for remaining bytes after main loop */
		while (p + 8 <= end) {
			uint64_t k1 = xxh64_round(0, gene_read64le(p));
			h64 ^= k1;
			h64 = gene_rotl64(h64, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
			p += 8;
		}
		if (p + 4 <= end) {
			h64 ^= (uint64_t)gene_read32le(p) * XXH_PRIME64_1;
			h64 = gene_rotl64(h64, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
			p += 4;
		}
		while (p < end) {
			h64 ^= ((uint64_t)*p++) * XXH_PRIME64_5;
			h64 = gene_rotl64(h64, 11) * XXH_PRIME64_1;
		}
	}
	h64 ^= h64 >> 33;
	h64 *= XXH_PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= XXH_PRIME64_3;
	h64 ^= h64 >> 32;
	return h64;
}/*}}}*/

void gene_hash_xxhash64_buf(const char *data, size_t len, zval *retval) /*{{{*/
{
	uint64_t hash = gene_xxhash64(data, len);
	gene_set_u64_hex_result(hash, retval);
}/*}}}*/

void gene_hash_xxhash64(zval *value, zval *retval) /*{{{*/
{
	zval tmp;
	zend_string *str;

	if (Z_TYPE_P(value) == IS_STRING) {
		str = Z_STR_P(value);
		gene_hash_xxhash64_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
		return;
	}
	ZVAL_COPY(&tmp, value);
	convert_to_string(&tmp);
	str = Z_STR(tmp);
	gene_hash_xxhash64_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
	zval_ptr_dtor(&tmp);
}/*}}}*/

/* MurmurHash3 32-bit - fast non-cryptographic hash */
#define MURMUR3_C1 0xcc9e2d51
#define MURMUR3_C2 0x1b873593
#define MURMUR3_R1 15
#define MURMUR3_R2 13
#define MURMUR3_M 5
#define MURMUR3_N 0xe6546b64

uint32_t gene_murmur3_32(const char *data, size_t len) /*{{{*/
{
	const uint8_t *p = (const uint8_t *)data;
	const uint8_t *end = p + (len & ~(size_t)3);
	uint32_t h1 = 0;
	uint32_t k1;

	/* 2-block unroll: process 8 bytes per iteration, halves loop overhead */
	while (p + 8 <= end) {
		k1 = gene_read32le(p);
		k1 *= MURMUR3_C1;
		k1 = gene_rotl32(k1, MURMUR3_R1);
		k1 *= MURMUR3_C2;
		h1 ^= k1;
		h1 = gene_rotl32(h1, MURMUR3_R2);
		h1 = h1 * MURMUR3_M + MURMUR3_N;

		k1 = gene_read32le(p + 4);
		k1 *= MURMUR3_C1;
		k1 = gene_rotl32(k1, MURMUR3_R1);
		k1 *= MURMUR3_C2;
		h1 ^= k1;
		h1 = gene_rotl32(h1, MURMUR3_R2);
		h1 = h1 * MURMUR3_M + MURMUR3_N;
		p += 8;
	}
	/* Handle remaining 4-byte block */
	if (p < end) {
		k1 = gene_read32le(p);
		k1 *= MURMUR3_C1;
		k1 = gene_rotl32(k1, MURMUR3_R1);
		k1 *= MURMUR3_C2;
		h1 ^= k1;
		h1 = gene_rotl32(h1, MURMUR3_R2);
		h1 = h1 * MURMUR3_M + MURMUR3_N;
		p += 4;
	}

	/* Tail: 1-3 remaining bytes (p now points to end) */
	k1 = 0;
	switch (len & 3) {
		case 3: k1 ^= ((uint32_t)p[2]) << 16; /* fallthrough */
		case 2: k1 ^= ((uint32_t)p[1]) << 8;  /* fallthrough */
		case 1: k1 ^= ((uint32_t)p[0]);
			k1 *= MURMUR3_C1;
			k1 = gene_rotl32(k1, MURMUR3_R1);
			k1 *= MURMUR3_C2;
			h1 ^= k1;
	}

	h1 ^= (uint32_t)len;
	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;
	return h1;
}/*}}}*/

void gene_hash_murmur3_32_buf(const char *data, size_t len, zval *retval) /*{{{*/
{
	uint32_t hash = gene_murmur3_32(data, len);
	gene_set_u32_hex_result(hash, retval);
}/*}}}*/

void gene_hash_murmur3_32(zval *value, zval *retval) /*{{{*/
{
	zval tmp;
	zend_string *str;

	if (Z_TYPE_P(value) == IS_STRING) {
		str = Z_STR_P(value);
		gene_hash_murmur3_32_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
		return;
	}
	ZVAL_COPY(&tmp, value);
	convert_to_string(&tmp);
	str = Z_STR(tmp);
	gene_hash_murmur3_32_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
	zval_ptr_dtor(&tmp);
}/*}}}*/

 /* FarmHash64 - Google's fast hash */
 #define FARMHASH64_INIT ((uint64_t)0x9E3779B97F4A7C15ULL)
 #define FARMHASH64_PRIME ((uint64_t)0x9E3779B97F4A7C15ULL)
 
 static zend_always_inline uint64_t farmhash_mix(uint64_t u, uint64_t v) {
 	return gene_hash_len16_mix(u, v, 0x9ddfea08eb382d69ULL);
 }
 
 static zend_always_inline uint64_t farmhash_shift_mix(uint64_t val) {
 	return val ^ (val >> 47);
 }
 
 uint64_t gene_farmhash64(const char *data, size_t len) /*{{{*/
 {
 	const uint8_t *s = (const uint8_t *)data;
 	const uint64_t k0 = 0xc3a5c85c97cb3127ULL;
 	const uint64_t k1 = 0xb492b66fbe98f273ULL;
 	const uint64_t k2 = 0x9ae16a3b2f90404fULL;
 	if (EXPECTED(len <= 16)) {
 		if (len >= 8) {
 			uint64_t mul = k2 + len * 2;
 			uint64_t a = gene_read64le(s) + k2;
 			uint64_t b = gene_read64le(s + len - 8);
 			uint64_t c = gene_rotl64(b, 37) * mul + a;
 			uint64_t d = (gene_rotl64(a, 25) + b) * mul;
 			return gene_hash_len16_mix(c, d, mul);
 		}
 		if (len >= 4) {
 			uint64_t mul = k2 + len * 2;
 			uint64_t a = gene_read32le(s);
 			return gene_hash_len16_mix(len + (a << 3), gene_read32le(s + len - 4), mul);
 		}
 		if (len > 0) {
 			uint64_t a = s[0];
 			uint64_t b = s[len >> 1];
 			uint64_t c = s[len - 1];
 			uint64_t y = a + (b << 8);
 			uint64_t z = len + (c << 2);
 			return farmhash_shift_mix(y * k2 ^ z * k0) * k2;
 		}
 		return k2;
 	}
 	if (EXPECTED(len <= 32)) {
 		uint64_t mul = k2 + len * 2;
 		uint64_t a = gene_read64le(s) * k1;
 		uint64_t b = gene_read64le(s + 8);
 		uint64_t c = gene_read64le(s + len - 8) * mul;
 		uint64_t d = gene_read64le(s + len - 16) * k2;
 		return gene_hash_len16_mix(gene_rotl64(a + b, 43) + gene_rotl64(c, 30) + d,
 				a + gene_rotl64(b + k2, 18) + c, mul);
 	}
 	if (EXPECTED(len <= 64)) {
 		uint64_t mul = k2 + len * 2;
 		uint64_t a = gene_read64le(s) * k2;
 		uint64_t b = gene_read64le(s + 8);
 		uint64_t c = gene_read64le(s + len - 8) * mul;
 		uint64_t d = gene_read64le(s + len - 16) * k2;
 		uint64_t y = gene_rotl64(a + b, 43) + gene_rotl64(c, 30) + d;
 		uint64_t z = gene_hash_len16_mix(y, a + gene_rotl64(b + k2, 18) + c, mul);
 		uint64_t e = gene_read64le(s + 16) * mul;
 		uint64_t f = gene_read64le(s + 24);
 		uint64_t g = (y + gene_read64le(s + len - 32)) * mul;
 		uint64_t h = (z + gene_read64le(s + len - 24)) * mul;
 		return gene_hash_len16_mix(gene_rotl64(e + f, 43) + gene_rotl64(g, 30) + h,
 				e + gene_rotl64(f + a, 18) + g, mul);
 	}
 	{
 		const uint8_t *p = s;
 		const uint8_t *end = s + len;
 		uint64_t x = gene_read64le(end - 40);
 		uint64_t y = gene_read64le(end - 16) + gene_read64le(end - 56);
 		uint64_t z = gene_hash_len16_mix(gene_read64le(end - 48) + len, gene_read64le(end - 24), k2);
 		uint64_t v_low = gene_hash_len16_mix(gene_read64le(end - 64) + len, x, k2);
 		uint64_t v_high = gene_hash_len16_mix(y + k1, gene_read64le(end - 32), k2);
 		uint64_t w_low = gene_hash_len16_mix(gene_read64le(end - 32) + z, gene_read64le(end - 8), k2);
 		uint64_t w_high = gene_hash_len16_mix(x + gene_read64le(end - 24), y + gene_read64le(end - 40), k2);
 		x = x * k1 + gene_read64le(p);
 		len = (len - 1) & ~(size_t)63;
 		do {
 			x = gene_rotl64(x + y + v_low + gene_read64le(p + 8), 37) * k1;
 			y = gene_rotl64(y + v_high + gene_read64le(p + 48), 42) * k1;
 			x ^= w_high;
 			y += v_low + gene_read64le(p + 40);
 			z = gene_rotl64(z + w_low, 33) * k1;
 			v_low = gene_hash_len16_mix(gene_read64le(p), v_high + z, k1);
 			v_high = gene_hash_len16_mix(gene_read64le(p + 32) + y, x + gene_read64le(p + 16), k1);
 			w_low = gene_hash_len16_mix(gene_read64le(p + 32) + w_high, z + gene_read64le(p + 48), k1);
 			w_high = gene_hash_len16_mix(x + gene_read64le(p + 24), y + gene_read64le(p + 56), k1);
 			{
 				uint64_t tmp = x;
 				x = z;
 				z = tmp;
 			}
 			p += 64;
 			len -= 64;
 		} while (len != 0);
 		return gene_hash_len16_mix(gene_hash_len16_mix(v_low, w_low, k1) + farmhash_shift_mix(y) * k1 + z,
 				gene_hash_len16_mix(v_high, w_high, k1) + x, k1);
 	}
 }/*}}}*/

void gene_hash_farmhash64_buf(const char *data, size_t len, zval *retval) /*{{{*/
{
	uint64_t hash = gene_farmhash64(data, len);
	gene_set_u64_hex_result(hash, retval);
}/*}}}*/

void gene_hash_farmhash64(zval *value, zval *retval) /*{{{*/
{
	zval tmp;
	zend_string *str;

	if (Z_TYPE_P(value) == IS_STRING) {
		str = Z_STR_P(value);
		gene_hash_farmhash64_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
		return;
	}
	ZVAL_COPY(&tmp, value);
	convert_to_string(&tmp);
	str = Z_STR(tmp);
	gene_hash_farmhash64_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
	zval_ptr_dtor(&tmp);
}/*}}}*/

 /* TurboHash32 - fast 32-bit hash with dual-lane interleaving.
  * 32-bit multiplies are faster than 64-bit on all platforms.
  * Uses well-separated primes for low collision rate. */
#define TURBO32_C1 0xcc9e2d51U
#define TURBO32_C2 0x1b873593U
#define TURBO32_C3 0x85ebca6bU
#define TURBO32_C4 0xc2b2ae35U
#define TURBO32_C5 0xe6546b64U

static zend_always_inline uint32_t turbo_mix32(uint32_t h, uint32_t k) {
    k *= TURBO32_C1;
    k = gene_rotl32(k, 15);
    k *= TURBO32_C2;
    h ^= k;
    h = gene_rotl32(h, 13);
    h = h * 5 + TURBO32_C5;
    return h;
}

static zend_always_inline uint32_t turbo_avalanche32(uint32_t h) {
    h ^= h >> 16;
    h *= TURBO32_C3;
    h ^= h >> 13;
    h *= TURBO32_C4;
    h ^= h >> 16;
    return h;
}

uint32_t gene_turbo_hash32(const char *data, size_t len) /*{{{*/
{
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + len;
    uint32_t h1 = 0x9368e53cU ^ (uint32_t)len;
    uint32_t h2 = 0x586dcd20U + (uint32_t)(len << 1);

    /* Short-key fast path: most cache/route keys < 32 bytes */
    if (EXPECTED(len < 32)) {
        uint32_t h = h1 ^ (h2 * TURBO32_C1);
        /* 8-byte unrolled: 2 × 4-byte blocks per iteration */
        while (p + 8 <= end) {
            h = turbo_mix32(h, gene_read32le(p));
            h = turbo_mix32(h, gene_read32le(p + 4));
            p += 8;
        }
        if (p + 4 <= end) {
            h = turbo_mix32(h, gene_read32le(p));
            p += 4;
        }
        /* Tail: 1-3 bytes */
        if (UNEXPECTED(p < end)) {
            uint32_t k = 0;
            switch (end - p) {
                case 3: k ^= ((uint32_t)p[2]) << 16; /* fallthrough */
                case 2: k ^= ((uint32_t)p[1]) << 8;  /* fallthrough */
                case 1: k ^= ((uint32_t)p[0]);
                    k *= TURBO32_C1;
                    k = gene_rotl32(k, 15);
                    k *= TURBO32_C2;
                    h ^= k;
            }
        }
        h ^= (uint32_t)len;
        return turbo_avalanche32(h);
    }

    /* Large input: dual-lane 32-byte blocks */
    while (p + 32 <= end) {
        h1 = turbo_mix32(h1, gene_read32le(p));
        h2 = turbo_mix32(h2, gene_read32le(p + 4));
        h1 = turbo_mix32(h1, gene_read32le(p + 8));
        h2 = turbo_mix32(h2, gene_read32le(p + 12));
        h1 = turbo_mix32(h1, gene_read32le(p + 16));
        h2 = turbo_mix32(h2, gene_read32le(p + 20));
        h1 = turbo_mix32(h1, gene_read32le(p + 24));
        h2 = turbo_mix32(h2, gene_read32le(p + 28));
        p += 32;
    }
    uint32_t h = h1 ^ (gene_rotl32(h2, 7) * TURBO32_C3);
    /* Remaining 4-byte blocks */
    while (p + 4 <= end) {
        h = turbo_mix32(h, gene_read32le(p));
        p += 4;
    }
    /* Tail: 1-3 bytes */
    if (p < end) {
        uint32_t k = 0;
        switch (end - p) {
            case 3: k ^= ((uint32_t)p[2]) << 16; /* fallthrough */
            case 2: k ^= ((uint32_t)p[1]) << 8;  /* fallthrough */
            case 1: k ^= ((uint32_t)p[0]);
                k *= TURBO32_C1;
                k = gene_rotl32(k, 15);
                k *= TURBO32_C2;
                h ^= k;
        }
    }
    h ^= (uint32_t)len;
    return turbo_avalanche32(h);
}/*}}}*/

void gene_hash_turbo_hash32_buf(const char *data, size_t len, zval *retval) /*{{{*/
{
	uint32_t hash = gene_turbo_hash32(data, len);
	gene_set_u32_hex_result(hash, retval);
}/*}}}*/

void gene_hash_turbo_hash32(zval *value, zval *retval) /*{{{*/
{
	zval tmp;
	zend_string *str;

	if (Z_TYPE_P(value) == IS_STRING) {
		str = Z_STR_P(value);
		gene_hash_turbo_hash32_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
		return;
	}
	ZVAL_COPY(&tmp, value);
	convert_to_string(&tmp);
	str = Z_STR(tmp);
	gene_hash_turbo_hash32_buf(ZSTR_VAL(str), ZSTR_LEN(str), retval);
	zval_ptr_dtor(&tmp);
}/*}}}*/

void gene_strip_tags(zval *value, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "strip_tags");
	zval params[] = { *value };
	zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_microtime(zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "microtime");
	zval value;
	ZVAL_TRUE(&value);
	zval params[] = { value };
	zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_time(zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "time");
	zend_call_known_function(fn, NULL, NULL, retval, 0, NULL, NULL);
}/*}}}*/

void gene_bcadd(zval *a, zval *b, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "bcadd");
	if (UNEXPECTED(!fn)) { ZVAL_FALSE(retval); return; }
	zval params[] = { *a, *b };
	zend_call_known_function(fn, NULL, NULL, retval, 2, params, NULL);
}/*}}}*/

void gene_uniqid(zval *value, zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "uniqid");
	zval params[] = { *value };
	zend_call_known_function(fn, NULL, NULL, retval, 1, params, NULL);
}/*}}}*/

void gene_class_name(zval *retval) /*{{{*/
{
	GENE_CG_FN_DECL(fn);
	fn = GENE_CG_FN_LOOKUP(fn, "get_called_class");
	zend_call_known_function(fn, NULL, NULL, retval, 0, NULL, NULL);
}/*}}}*/

/* [GENE_AUDIT:2026-04-01] Fast path for getting the current class name.
 * Uses zend_get_called_scope() instead of call_user_function("get_called_class").
 * zend_get_called_scope respects late static binding (returns the actual subclass,
 * not the class where __get/__set is defined), matching get_called_class() semantics.
 * Returns an interned zend_string* owned by the class entry — caller must NOT release it.
 * Returns NULL if called outside a class context. */
zend_string *gene_get_class_name_fast(void) /*{{{*/
{
    zend_class_entry *ce = zend_get_called_scope(EG(current_execute_data));
    if (ce) {
        return ce->name;
    }
    return NULL;
}/*}}}*/

int is_json(zval *str) {
	size_t length = ZSTR_LEN(Z_STR_P(str));
	if (length > 1) {
		char *ch = ZSTR_VAL(Z_STR_P(str));
		if ((ch[0] == '{' && ch[length -1] == '}') || (ch[0] == '[' && ch[length -1] == ']')) {
			return 1;
		}
	}
	return 0;
}

int is_serialize(zval *str) {
	size_t length = ZSTR_LEN(Z_STR_P(str));
	if (length > 1) {
		char *ch = ZSTR_VAL(Z_STR_P(str));
		if (ch[1] == ':' && ch[length -1] == '}') {
			return 1;
		}
	}
	return 0;
}

int is_igbinary(zval *str) {
	size_t length = ZSTR_LEN(Z_STR_P(str));
	if (length >= 4) {
		char *ch = ZSTR_VAL(Z_STR_P(str));
		if (strncmp(ch, "\x00\x00\x00\x02", 4) == 0 || strncmp(ch, "\x00\x00\x00\x01", 4) == 0) {
			return 1;
		}
	}
	return 0;
}

int serialize(zval *arr, zval *string, zval *serializer_handler) {
	zval options;
	zend_long type = Z_LVAL_P(serializer_handler);
	switch(type) {
	case 1:
		ZVAL_LONG(&options, 256);
		gene_json_encode(arr, &options, string);
		zval_ptr_dtor(&options);
		break;
	case 2:
		gene_igbinary_serialize(arr, string);
		break;
	default:
		gene_serialize(arr, string);
		break;
	}

	if (Z_TYPE_P(string) != IS_STRING) {
		zval_ptr_dtor(string);
		return 0;
	}
	return 1;
}


int unserialize(zval *string, zval *arr, zval *serializer_handler) {
	zval assoc;
	if (Z_TYPE_P(string) == IS_STRING) {
		zend_long type = Z_LVAL_P(serializer_handler);
		switch(type) {
		case 1:
			if(is_json(string) == 0) {
				return 0;
			}
			ZVAL_BOOL(&assoc, 1);
			gene_json_decode(string, &assoc, arr);
			zval_ptr_dtor(&assoc);
			break;
		case 2:
			if(is_igbinary(string) == 0) {
				return 0;
			}
			gene_igbinary_unserialize(string, arr);
			break;
		default:
			if(is_serialize(string) == 0) {
				return 0;
			}
			gene_unserialize(string, arr);
			break;
		}
		if (Z_TYPE_P(arr) == IS_NULL) {
			zval_ptr_dtor(arr);
			return 0;
		}
		return 1;
	}
	return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
