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

#ifndef GENE_COMMON_H
#define GENE_COMMON_H

char *str_init(char *s);
char *str_sub(char *s, size_t s_len);
char *str_sub_len(char *src, size_t start, int len);
char *str_append(char* s, const char* t);
char *firstToUpper(char *str);
char *strtoupper(char *str);
char *strtolower(char *str);
void left(char *dst, char *src, size_t n);
void leftByChar(char *dst, char *src, char val);
void mid(char *dst, char *src, size_t n, size_t m);
void right(char *dst, char *src, size_t n);
char *replaceAll(char * src, const char oldChar, const char newChar);
void trim(char* s, const char c);
char *insert_string(char * string, const char * source, const char * destination);
char *replace_string(char * string, char source, const char * destination);
int ReplaceStr(char* sSrc, char* sMatchStr, char* sReplaceStr);
char *insertAll(char * dest, char * src, char oldChar, char newChar);
void replace(char originalString[], char key[], char swap[]);
int findChildCnt(char* str1, const char* str2);
int fullToHalf(char *sFullStr, char *sHalfStr);
void remove_extra_space(char *str);
char *readfilecontent(char *file);
char *strreplace(char *original, char *pattern, char *replacement);
char *strreplace2(char *src, char *from, char *to);
char *str_concat(char *s, const char *t);
void gene_json_encode(zval *value, zval *options, zval *retval);
void gene_json_decode(zval *value, zval *options, zval *retval);
void gene_func_get_args(zval *retval);
void gene_func_num_args(zval *retval);
void gene_serialize(zval *value, zval *retval);
void gene_unserialize(zval *value, zval *retval);
void gene_igbinary_serialize(zval *value, zval *retval);
void gene_igbinary_unserialize(zval *value, zval *retval);
void gene_md5(zval *value, zval *retval);
void gene_class_name(zval *retval);
int is_json(zval *str);
int is_serialize(zval *str);
int is_igbinary(zval *str);
int serialize(zval *arr, zval *string, zval *serializer_handler);
int unserialize(zval *string, zval *arr, zval *serializer_handler);

#endif
