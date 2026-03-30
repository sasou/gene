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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include "php.h"
#include "php_streams.h"
#include "string.h"
#include <ctype.h>
#include "../common/common.h"

char *str_init(const char *s)
{
	size_t s_l = strlen(s);
	char *p = (char *) emalloc(s_l + 1);
	strncpy(p, s, s_l);
	p[s_l] = 0;
	return p;
}

char *str_sub(char *s, size_t s_len)
{
	size_t s_l = strlen(s);
	if (s_len < s_l) {
		s_l = s_len;
	}
	char *p = (char *) emalloc(s_l + 1);
	strncpy(p, s, s_l);
	p[s_l] = 0;
	return p;
}

/* [GENE_AUDIT:2026-03-25] Changed len from int to size_t to prevent
 * negative length causing undersized allocation. */
char *str_sub_len(char *src, size_t start, size_t len) {
   size_t i;
   char *dest = (char *) emalloc(len + 1);
   for (i = 0; i < len && src[start + i] != '\0'; i++) {
      dest[i] = src[start + i];
   }
   dest[i] = 0;
   return dest;
}

char *str_append(char *s, const char*t)
{
	size_t size = 0;
    char *p = NULL;
	size =  strlen(s) + strlen(t) + 1;
	p = erealloc(s, size);
	strcat(p, t);
	p[size - 1] = 0;
	return p;
}

char *str_concat(char *s, const char *t)
{
	size_t s_l = strlen(s);
	size_t t_l = strlen(t);
    char *p = NULL,*tmp = NULL;
	p = (char *) emalloc(s_l + t_l +1);
    tmp = p;
    strncpy(tmp, s, s_l);
    tmp += s_l;
    strncpy(tmp, t, t_l);
	p[s_l+t_l] = 0;
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
 * {{{ void leftByChar(char *dst, char *src, char val)
 */
void leftByChar(char *dst, char *src, char val) {
	char *q = dst;
	while (*src != '\0') {
		if (*src == val)
			break;
		*(q++) = *(src++);
	}
	return;
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
char * insertAll(char * dest, char * src, char oldChar, char newChar) {
	size_t extra = 0;
	char *p, *head;
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
	char *sk = NULL, *newstr, *ptr;
	size_t size, newsize, num = 0, isFirst;
	num = findChildC(string, source);
	isFirst = charIfFirst(string, ':');
	if (num == 0)
		return NULL;
	newsize = strlen(string) + (strlen(destination) - 1) * num + 1;
	newstr = (char *) ecalloc(newsize, sizeof(char));
	if (newstr == NULL)
		return NULL;
	sk = php_strtok_r(string, ":", &ptr);
	size = 0;
	while (sk != NULL) {
		if (isFirst == 1) {
			if (size < num) {
				strcat(newstr, destination);
			}
			strcat(newstr, sk);
		} else {
			strcat(newstr, sk);
			if (size < num) {
				strcat(newstr, destination);
			}
		}
		sk = php_strtok_r(NULL, ":", &ptr);
		size++;
	}
	return newstr;

}
/* }}} */

/*
 * {{{ char * replace_string (char * string, const char * source, const char * destination )
 */
int ReplaceStr(char* sSrc, char* sMatchStr, char* sReplaceStr) {
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
        // copy the section until the occurence of the pattern
        strncpy(retptr, oriptr, skplen);
        retptr += skplen;
        // copy the replacement
        strncpy(retptr, replacement, replen);
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

/* [GENE_AUDIT:2026-03-25] Kept call_user_function wrappers instead of direct Zend
 * internal APIs (php_json_encode, php_json_decode, php_var_serialize, etc.).
 * Reason: these internal APIs change signatures across PHP minor versions (e.g.
 * php_json_encode added encoder parameter in 8.1), creating maintenance burden.
 * call_user_function is stable across PHP 8.x and these functions are NOT in the
 * hot request dispatch path, so the ~0.5us overhead per call is acceptable. */
void gene_json_encode(zval *value, zval *options, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "json_encode");
	zval params[] = { *value, *options };
    call_user_function(NULL, NULL, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_json_decode(zval *value, zval *options, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "json_decode");
	zval params[] = { *value, *options };
    call_user_function(NULL, NULL, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_func_get_args(zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "func_get_args");
    call_user_function(NULL, NULL, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_func_num_args(zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "func_num_args");
    call_user_function(NULL, NULL, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_serialize(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "serialize");
    zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_unserialize(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "unserialize");
    zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_igbinary_serialize(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "igbinary_serialize");
    zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_igbinary_unserialize(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "igbinary_unserialize");
    zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_md5(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "md5");
    zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_strip_tags(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "strip_tags");
	zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_microtime(zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "microtime");
	zval value;
	ZVAL_TRUE(&value);
	zval params[] = { value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
	zval_ptr_dtor(&value);
}/*}}}*/

void gene_time(zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "time");
    call_user_function(NULL, NULL, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_bcadd(zval *a, zval *b, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "bcadd");
	zval params[] = { *a,*b };
    call_user_function(NULL, NULL, &function_name, retval, 2, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_uniqid(zval *value, zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "uniqid");
	zval params[] = { *value };
    call_user_function(NULL, NULL, &function_name, retval, 1, params);
    zval_ptr_dtor(&function_name);
}/*}}}*/

void gene_class_name(zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "get_called_class");
    call_user_function(NULL, NULL, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

/* [GENE_AUDIT:2026-04-01] Fast path for getting the current class name.
 * Uses zend_get_executed_scope() instead of call_user_function("get_called_class").
 * Returns an interned zend_string* owned by the class entry — caller must NOT release it.
 * Returns NULL if called outside a class context. */
zend_string *gene_get_class_name_fast(void) /*{{{*/
{
    zend_class_entry *ce = zend_get_executed_scope();
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
