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
#include "string.h"
#include <ctype.h>
#include "../common/common.h"

char *str_init(char *s)
{
	int s_l = strlen(s);
	char *p = (char *) emalloc(s_l + 1);
	strncpy(p, s, s_l);
	p[s_l] = 0;
	return p;
}


char *str_append(char *s, const char*t)
{
    int size = 0;
    char *p = NULL;
	size =  strlen(s) + strlen(t) + 1;
	p = erealloc(s, size);
	strcat(p, t);
	p[size - 1] = 0;
	return p;
}

char *str_concat(char *s, const char *t)
{
	int s_l = strlen(s);
	int t_l = strlen(t);
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
char *strtoupper(char *str) {
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
char *strtolower(char *str) {
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
void left(char *dst, char *src, int n) {
	char *p = src;
	char *q = dst;
	int len = strlen(src);
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
void mid(char *dst, char *src, int n, int m) {
	char *p = src;
	char *q = dst;
	int len = strlen(src);
	if (n > len)
		n = len - m;
	if (m < 0)
		m = 0;
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
void right(char *dst, char *src, int n) {
	char *p = src;
	char *q = dst;
	int len = strlen(src);
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
	int size = 50;
	dest = (char *) emalloc(strlen(src) + size + 1);
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
	return dest;
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
 * {{{ replace(char originalString[], char key[], char swap[])
 */
void replace(char originalString[], char key[], char swap[]) {
	int lengthOfOriginalString, lengthOfKey, lengthOfSwap, i, j, flag;
	char tmp[1000];

	lengthOfOriginalString = strlen(originalString);
	lengthOfKey = strlen(key);
	lengthOfSwap = strlen(swap);

	for (i = 0; i <= lengthOfOriginalString - lengthOfKey; i++) {
		flag = 1;
		for (j = 0; j < lengthOfKey; j++) {
			if (originalString[i + j] != key[j]) {
				flag = 0;
				break;
			}
		}
		if (flag) {
			strcpy(tmp, originalString);
			strcpy(&tmp[i], swap);
			strcpy(&tmp[i + lengthOfSwap], &originalString[i + lengthOfKey]);
			strcpy(originalString, tmp);
			i += lengthOfSwap - 1;
			lengthOfOriginalString = strlen(originalString);
		}
	}
}
/* }}} */

/*
 * {{{ int findChildCnt(char* str1, char* str2)
 */
int findChildCnt(char* str1, const char* str2) {
	int len = strlen(str2);
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
	int size, newsize, num = 0, isFirst;
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
	int StringLen;
	char caNewString[500];
	char* FindPos;
	FindPos = (char *) strstr(sSrc, sMatchStr);
	if ((!FindPos) || (!sMatchStr))
		return -1;

	while (FindPos) {
		memset(caNewString, 0, sizeof(caNewString));
		StringLen = FindPos - sSrc;
		strncpy(caNewString, sSrc, StringLen);
		strcat(caNewString, sReplaceStr);
		strcat(caNewString, FindPos + strlen(sMatchStr));
		strcpy(sSrc, caNewString);

		FindPos = (char *) strstr(sSrc, sMatchStr);
	}
	free(FindPos);
	return 0;
}
/* }}} */

/*
 * {{{ char * insert_string (char * string, const char * source, const char * destination )
 */
char * insert_string(char * string, const char * source,
		const char * destination) {
	char *sk, *newstr, *retstr;
	int size;
	int pos = 0;
	sk = strstr(string, source);
	if (sk == NULL)
		return NULL;
	size = strlen(string) + strlen(destination) + 1;
	newstr = (char*) emalloc(size);
	if (newstr == NULL)
		return NULL;

	retstr = (char*) emalloc(size);
	if (retstr == NULL) {
		free(newstr);
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

	free(newstr);
	return retstr;

}
/* }}} */

int fullToHalf(char *sFullStr, char *sHalfStr) {
	int iFullStrIndex = 0;
	int iHalfStrIndex = 0;

	int iFullStrLen = strlen(sFullStr);

	printf("sFullStr[%s]\n", sFullStr);

	for (; iFullStrIndex < iFullStrLen; iFullStrIndex++, iHalfStrIndex++) {

		if (sFullStr[iFullStrIndex] == 0xA3) {
			sHalfStr[iHalfStrIndex] = sFullStr[++iFullStrIndex] - 0x80;

		} else {
			sHalfStr[iHalfStrIndex] = sFullStr[iFullStrIndex];

		}

	}

	printf("sHalfStr:[%s]\n", sHalfStr);

	return 0;
}

int halfToFull(char *sFullStr, char *sHalfStr) {
	int iFullStrIndex = 0;
	int iHalfStrIndex = 0;

	int iHalfStrlen = strlen(sHalfStr);

	printf("sHalfStr[%s]\n", sHalfStr);

	for (; iHalfStrIndex < iHalfStrlen; iHalfStrIndex++, iFullStrIndex++) {

		if (sHalfStr[iHalfStrIndex] < 0x80) {
			sFullStr[iFullStrIndex] = 0xA3;
			sFullStr[++iFullStrIndex] = sHalfStr[iHalfStrIndex] + 0x80;

		} else {
			sFullStr[iFullStrIndex] = sHalfStr[iHalfStrIndex];

		}

	}

	printf("sFullStr:[%s]\n", sFullStr);

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
	int file_size = 0;
	FILE *fp = NULL;
	fp = fopen(file, "rb");
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		tmp = (char*) ecalloc(file_size + 1, sizeof(char));
		fread(tmp, file_size, sizeof(char), fp);
		fclose(fp);
	}
	return tmp;
}

char *strreplace(char *original, char *pattern, char *replacement)
{
  int replen = strlen(replacement);
  int patlen = strlen(pattern);
  int orilen = strlen(original);

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
	int const retlen = orilen + patcnt * (replen - patlen);
    char *returned = (char *) ecalloc(retlen + 1,  sizeof(char));

    if (returned != NULL)
    {
      // copy the original string,
      // replacing all the instances of the pattern
      char * retptr = returned;
      for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
      {
    	int skplen = patloc - oriptr;
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
   int size    = strlen(src) + 1;
   int fromlen = strlen(from);
   int tolen   = strlen(to);
   char *value = ecalloc(size, sizeof(char));
   char *dst = value;
   if ( value != NULL )
   {
      for ( ;; )
      {
         const char *match = strstr(src, from);
         if ( match != NULL )
         {
        	int count = match - src;
            char *temp;
            size += tolen - fromlen;
            temp = erealloc(value, size);
            if ( temp == NULL )
            {
               free(value);
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

void gene_class_name(zval *retval) /*{{{*/
{
    zval function_name;
    ZVAL_STRING(&function_name, "get_called_class");
    call_user_function(NULL, NULL, &function_name, retval, 0, NULL);
    zval_ptr_dtor(&function_name);
}/*}}}*/

int is_json(zval *str) {
	int length = ZSTR_LEN(Z_STR_P(str));
	if (length > 1) {
		char *ch = ZSTR_VAL(Z_STR_P(str));
		if ((ch[0] == '{' && ch[length -1] == '}') || (ch[0] == '[' && ch[length -1] == ']')) {
			return 1;
		}
	}
	return 0;
}

int is_serialize(zval *str) {
	int length = ZSTR_LEN(Z_STR_P(str));
	if (length > 1) {
		char *ch = ZSTR_VAL(Z_STR_P(str));
		if (ch[1] == ':' && ch[length -1] == '}') {
			return 1;
		}
	}
	return 0;
}

int is_igbinary(zval *str) {
	int length = ZSTR_LEN(Z_STR_P(str));
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
	long type = Z_LVAL_P(serializer_handler);
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
		long type = Z_LVAL_P(serializer_handler);
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
