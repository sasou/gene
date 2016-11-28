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
#include "gene_common.h"
#include <ctype.h>

/*
 * {{{ char *strupr(char *str)
 */
char *strtoupper(char *str)
{
	char *orign=str;
	while(*str!='\0'){
		*str = toupper(*str);
		str++;
	}
	return orign;
}
/* }}} */

/*
 * {{{ char *strupr(char *str)
 */
char *strtolower(char *str)
{
	char *orign=str;
	while(*str!='\0'){
		*str = tolower(*str);
		str++;
	}
	return orign;
}
/* }}} */

/*
 * {{{ left(char *dst,char *src, int n)
 */
void left(char *dst,char *src, int n)
{
    char *p = src;
    char *q = dst;
    int len = strlen(src);
    if(n>len) n = len;
    /*p += (len-n);*/
    while(n--) *(q++) = *(p++);
    *(q++)='\0';
    return;
}
/* }}} */

/*
 * {{{ void leftByChar(char *dst,char *src, char val)
 */
void leftByChar(char *dst,char *src, char val)
{
	char *q=dst;
	while(*src!='\0'){
		if(*src==val) break;
		*(q++) = *(src++);
	}
	return;
}
/* }}} */

/*
 * {{{ mid(char *dst,char *src, int n,int m) n为长度，m为位置
 */
void mid(char *dst,char *src, int n,int m)
{
    char *p = src;
    char *q = dst;
    int len = strlen(src);
    if(n>len) n = len-m;
    if(m<0) m=0;
    //if(m>len) return NULL;
    p += m;
    while(n--) *(q++) = *(p++);
    *(q++)='\0';
    return;
}
/* }}} */

/*
 * {{{ right(char *dst,char *src, int n)
 */
void right(char *dst,char *src, int n)
{
    char *p = src;
    char *q = dst;
    int len = strlen(src);
    if(n>len) n = len;
    p += (len-n);
    while(*(q++) = *(p++));
    return;
}
/* }}} */

/*
 * {{{ right(char *dst,char *src, int n)
 */
char * replaceAll(char * src,const char oldChar,const char newChar)
{
	char *head=src;
	while(*src!='\0'){
		if(*src==oldChar) *src=newChar;
		src++;
	}
	return head;
}
/* }}} */

/*
 * {{{ insertAll(char *dst,char *src, int n)
 */
char * insertAll(char * dest,char * src,char oldChar,char newChar)
{
	int size = 50;
	dest = (char *) emalloc(strlen(src)+size+1);
	while(*src!='\0'){
		if(*src==oldChar){
			*dest = *src;
			dest++;
			*dest=newChar;
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
void trim(char* s,const char c)
{
    char *t  = s;
    while (*s == c){s++;};
    if (*s)
    {
        char* t1 = s;
        while (*s){s++;};
        s--;
        while (*s == c){s--;};
        while (t1 <= s)
        {
            *(t++) = *(t1++);
        }
    }
    *t = 0;
}
/* }}} */

/*
 * {{{ char *replace(char *st, char *orig, char *repl)
 */
void replace(char originalString[], char key[], char swap[])
{
	int lengthOfOriginalString, lengthOfKey, lengthOfSwap, i, j , flag;
	char tmp[1000];

	//获取各个字符串的长度
	lengthOfOriginalString = strlen(originalString);
	lengthOfKey = strlen(key);
	lengthOfSwap = strlen(swap);

	for( i = 0; i <= lengthOfOriginalString - lengthOfKey; i++){
		flag = 1;
		//搜索key
		for(j  = 0; j < lengthOfKey; j ++){
			if(originalString[i + j] != key[j]){
				flag = 0;
				break;
			}
		}
		//如果搜索成功，则进行替换
		if(flag){
			strcpy(tmp, originalString);
			strcpy(&tmp[i], swap);
			strcpy(&tmp[i + lengthOfSwap], &originalString[i  + lengthOfKey]);
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
int findChildCnt(char* str1,const char* str2)
{
    int len = strlen(str2);
    int cnt = 0;
    while (str1 = strstr(str1, str2))
    {
        cnt++;
        str1 += len;
    }
    return cnt;
}
/* }}} */

/*
 * {{{ int findChildC(char* src, char* car)
 */
int findChildC(char* src,const char car)
{
	char *h = src;
	int num = 0;
	while(*h!='\0'){
		if(*h==car) num++;
		h++;
	}
	return num;
}
/* }}} */

/*
 * {{{ char * charIfFirst(char * src,const char oldChar)
 */
int charIfFirst(char *src,const char car)
{
	char *h = src;
	if(h[0] == car) {
		return 1;
	}
	return 0;
}
/* }}} */

/*
 * {{{ char * replace_string (char * string, const char * source, const char * destination )
 */
char * replace_string (char * string, char source, const char * destination )
{
	char *sk = NULL,*newstr,*ptr;
	int size,newsize,num = 0,isFirst;
	num = findChildC(string, source);
	isFirst = charIfFirst(string, ':');
	if (num == 0) return NULL;
	newsize = strlen(string)+(strlen(destination) - 1 )*num+1;
	newstr = (char *) ecalloc(newsize,sizeof(char));
	if (newstr == NULL) return NULL;
	sk = php_strtok_r(string, ":", &ptr);
	size = 0;
	while (sk != NULL)
	{
		if(isFirst == 1) {
			if (size<num) {
				strcat(newstr,destination);
			}
			strcat(newstr,sk);
		} else {
			strcat(newstr,sk);
			if (size<num) {
				strcat(newstr,destination);
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
int ReplaceStr(char* sSrc, char* sMatchStr, char* sReplaceStr)
{
	int StringLen;
	char caNewString[500];
	char* FindPos;
	FindPos =(char *)strstr(sSrc, sMatchStr);
	if( (!FindPos) || (!sMatchStr) )
			return -1;

	while( FindPos )
	{
			memset(caNewString, 0, sizeof(caNewString));
			StringLen = FindPos - sSrc;
			strncpy(caNewString, sSrc, StringLen);
			strcat(caNewString, sReplaceStr);
			strcat(caNewString, FindPos + strlen(sMatchStr));
			strcpy(sSrc, caNewString);

			FindPos =(char *)strstr(sSrc, sMatchStr);
	}
	free(FindPos);
	return 0;
}
/* }}} */

/*
 * {{{ char * insert_string (char * string, const char * source, const char * destination )
 */
char * insert_string (char * string, const char * source, const char * destination )
{
	char *sk,*newstr,*retstr;
	int size;
	int pos = 0;
	sk = strstr (string, source);
	if (sk == NULL) return NULL;
	size = strlen(string)+strlen(destination)+1;
	newstr = (char*)emalloc (size);
	if (newstr == NULL) return NULL;

	retstr = (char*)emalloc (size);
	if (retstr == NULL)
	{
		free (newstr);
		return NULL;
	}

	snprintf (newstr, size-1, "%s", string);

	sk = strstr (newstr, source);

	pos = 0;
	sk += strlen(source);

	memcpy (retstr+pos, newstr, sk - newstr);
	pos += sk - newstr;

	memcpy (retstr+pos, destination, strlen(destination));
	pos += strlen(destination);
	memcpy (retstr+pos, sk, strlen(sk));

	free (newstr);
	return retstr;

}
/* }}} */

int fullToHalf(char *sFullStr, char *sHalfStr)
{
    int iFullStrIndex = 0;
    int iHalfStrIndex = 0;

    int iFullStrLen = strlen(sFullStr);

    printf("sFullStr[%s]\n", sFullStr);

    for(; iFullStrIndex < iFullStrLen; iFullStrIndex++, iHalfStrIndex++){

        if(sFullStr[iFullStrIndex] == 0xA3){
            sHalfStr[iHalfStrIndex] = sFullStr[++iFullStrIndex]- 0x80;

        }else{
            sHalfStr[iHalfStrIndex] = sFullStr[iFullStrIndex];

        }

    }

    printf("sHalfStr:[%s]\n", sHalfStr);

    return 0;
}


int halfToFull(char *sFullStr, char *sHalfStr)
{
    int iFullStrIndex = 0;
    int iHalfStrIndex = 0;

    int iHalfStrlen = strlen(sHalfStr);

    printf("sHalfStr[%s]\n", sHalfStr);

    for(; iHalfStrIndex < iHalfStrlen; iHalfStrIndex++, iFullStrIndex++){

        if(sHalfStr[iHalfStrIndex] < 0x80){
            sFullStr[iFullStrIndex] = 0xA3;
            sFullStr[++iFullStrIndex] = sHalfStr[iHalfStrIndex] + 0x80;

        }else{
            sFullStr[iFullStrIndex] = sHalfStr[iHalfStrIndex];

        }

    }

    printf("sFullStr:[%s]\n", sFullStr);

    return 0;
}

void remove_extra_space(char *str)
{
    char *sp = str;
    char *prev = 0;

    while(*str && (*str == ' ' || *str == '\n' || *str =='\t'))
        ++str;

    for(; *str; ++str)
    {
        switch (*str)
        {
			case '\t':
				*str = ' ';
			case ' ' :
				if (*prev == '\n' || *prev ==' ')
					continue;
				break;

			case '\n':
				if (*prev == '\n')
					continue;
				else if (*prev == ' ')
				{
					*prev = '\n';
					continue;
				}
				break;
        }

        prev = sp;
        *sp++ = *str;
    }

    if(prev && *prev && (*prev == ' ' || *prev == '\n'))
        --sp;
    *sp = 0;
}

char *readfilecontent(char *file)
{
	char *tmp = NULL;
	int file_size = 0;
	FILE *fp = NULL;
    fp = fopen(file, "rb");
    if (fp != NULL) {
        fseek(fp , 0 , SEEK_END);
        file_size = ftell(fp);
        fseek(fp , 0 , SEEK_SET);
        tmp = (char*) ecalloc(file_size + 1, sizeof(char));
        fread(tmp , file_size , sizeof(char) , fp);
        fclose(fp);
    }
    return tmp;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
