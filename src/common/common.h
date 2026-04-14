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

char *str_init(const char *s);
char *str_sub(char *s, size_t s_len);
char *str_sub_len(char *src, size_t start, size_t len);
char *str_append(char* s, const char* t);
char *firstToUpper(char *str);
char *gene_strtoupper(char *str);
char *gene_strtolower(char *str);
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
void replace(char originalString[], char key[], char swap[], size_t buf_size);
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
void gene_md5_buf(const char *data, size_t len, zval *retval);
void gene_md5(zval *value, zval *retval);
uint64_t gene_fnv1a_64(const char *data, size_t len);
void gene_hash_fast_buf(const char *data, size_t len, zval *retval);
void gene_hash_fast(zval *value, zval *retval);
uint64_t gene_xxhash64(const char *data, size_t len);
void gene_hash_xxhash64_buf(const char *data, size_t len, zval *retval);
void gene_hash_xxhash64(zval *value, zval *retval);
uint64_t gene_farmhash64(const char *data, size_t len);
void gene_hash_farmhash64_buf(const char *data, size_t len, zval *retval);
void gene_hash_farmhash64(zval *value, zval *retval);
uint32_t gene_murmur3_32(const char *data, size_t len);
void gene_hash_murmur3_32_buf(const char *data, size_t len, zval *retval);
void gene_hash_murmur3_32(zval *value, zval *retval);
uint32_t gene_turbo_hash32(const char *data, size_t len);
void gene_hash_turbo_hash32_buf(const char *data, size_t len, zval *retval);
void gene_hash_turbo_hash32(zval *value, zval *retval);
void gene_class_name(zval *retval);

/* Paired hex lookup: memcpy(dst, gene_hex_pair[byte], 2) compiles to a single
 * 16-bit load+store at -O2. 256×2 = 512 bytes, fits in L1 cache. Endian-safe. */
static const char gene_hex_pair[256][2] = {
	{'0','0'},{'0','1'},{'0','2'},{'0','3'},{'0','4'},{'0','5'},{'0','6'},{'0','7'},
	{'0','8'},{'0','9'},{'0','a'},{'0','b'},{'0','c'},{'0','d'},{'0','e'},{'0','f'},
	{'1','0'},{'1','1'},{'1','2'},{'1','3'},{'1','4'},{'1','5'},{'1','6'},{'1','7'},
	{'1','8'},{'1','9'},{'1','a'},{'1','b'},{'1','c'},{'1','d'},{'1','e'},{'1','f'},
	{'2','0'},{'2','1'},{'2','2'},{'2','3'},{'2','4'},{'2','5'},{'2','6'},{'2','7'},
	{'2','8'},{'2','9'},{'2','a'},{'2','b'},{'2','c'},{'2','d'},{'2','e'},{'2','f'},
	{'3','0'},{'3','1'},{'3','2'},{'3','3'},{'3','4'},{'3','5'},{'3','6'},{'3','7'},
	{'3','8'},{'3','9'},{'3','a'},{'3','b'},{'3','c'},{'3','d'},{'3','e'},{'3','f'},
	{'4','0'},{'4','1'},{'4','2'},{'4','3'},{'4','4'},{'4','5'},{'4','6'},{'4','7'},
	{'4','8'},{'4','9'},{'4','a'},{'4','b'},{'4','c'},{'4','d'},{'4','e'},{'4','f'},
	{'5','0'},{'5','1'},{'5','2'},{'5','3'},{'5','4'},{'5','5'},{'5','6'},{'5','7'},
	{'5','8'},{'5','9'},{'5','a'},{'5','b'},{'5','c'},{'5','d'},{'5','e'},{'5','f'},
	{'6','0'},{'6','1'},{'6','2'},{'6','3'},{'6','4'},{'6','5'},{'6','6'},{'6','7'},
	{'6','8'},{'6','9'},{'6','a'},{'6','b'},{'6','c'},{'6','d'},{'6','e'},{'6','f'},
	{'7','0'},{'7','1'},{'7','2'},{'7','3'},{'7','4'},{'7','5'},{'7','6'},{'7','7'},
	{'7','8'},{'7','9'},{'7','a'},{'7','b'},{'7','c'},{'7','d'},{'7','e'},{'7','f'},
	{'8','0'},{'8','1'},{'8','2'},{'8','3'},{'8','4'},{'8','5'},{'8','6'},{'8','7'},
	{'8','8'},{'8','9'},{'8','a'},{'8','b'},{'8','c'},{'8','d'},{'8','e'},{'8','f'},
	{'9','0'},{'9','1'},{'9','2'},{'9','3'},{'9','4'},{'9','5'},{'9','6'},{'9','7'},
	{'9','8'},{'9','9'},{'9','a'},{'9','b'},{'9','c'},{'9','d'},{'9','e'},{'9','f'},
	{'a','0'},{'a','1'},{'a','2'},{'a','3'},{'a','4'},{'a','5'},{'a','6'},{'a','7'},
	{'a','8'},{'a','9'},{'a','a'},{'a','b'},{'a','c'},{'a','d'},{'a','e'},{'a','f'},
	{'b','0'},{'b','1'},{'b','2'},{'b','3'},{'b','4'},{'b','5'},{'b','6'},{'b','7'},
	{'b','8'},{'b','9'},{'b','a'},{'b','b'},{'b','c'},{'b','d'},{'b','e'},{'b','f'},
	{'c','0'},{'c','1'},{'c','2'},{'c','3'},{'c','4'},{'c','5'},{'c','6'},{'c','7'},
	{'c','8'},{'c','9'},{'c','a'},{'c','b'},{'c','c'},{'c','d'},{'c','e'},{'c','f'},
	{'d','0'},{'d','1'},{'d','2'},{'d','3'},{'d','4'},{'d','5'},{'d','6'},{'d','7'},
	{'d','8'},{'d','9'},{'d','a'},{'d','b'},{'d','c'},{'d','d'},{'d','e'},{'d','f'},
	{'e','0'},{'e','1'},{'e','2'},{'e','3'},{'e','4'},{'e','5'},{'e','6'},{'e','7'},
	{'e','8'},{'e','9'},{'e','a'},{'e','b'},{'e','c'},{'e','d'},{'e','e'},{'e','f'},
	{'f','0'},{'f','1'},{'f','2'},{'f','3'},{'f','4'},{'f','5'},{'f','6'},{'f','7'},
	{'f','8'},{'f','9'},{'f','a'},{'f','b'},{'f','c'},{'f','d'},{'f','e'},{'f','f'}
};

/* Fast 64-bit to hex: 8 × memcpy-2 → compiler emits 8 × 16-bit load+store */
static zend_always_inline void gene_u64_to_hex(uint64_t val, char *dst) {
	const uint8_t *p = (const uint8_t *)&val;
#ifdef WORDS_BIGENDIAN
	memcpy(dst,    gene_hex_pair[p[7]], 2); memcpy(dst+2,  gene_hex_pair[p[6]], 2);
	memcpy(dst+4,  gene_hex_pair[p[5]], 2); memcpy(dst+6,  gene_hex_pair[p[4]], 2);
	memcpy(dst+8,  gene_hex_pair[p[3]], 2); memcpy(dst+10, gene_hex_pair[p[2]], 2);
	memcpy(dst+12, gene_hex_pair[p[1]], 2); memcpy(dst+14, gene_hex_pair[p[0]], 2);
#else
	memcpy(dst,    gene_hex_pair[p[0]], 2); memcpy(dst+2,  gene_hex_pair[p[1]], 2);
	memcpy(dst+4,  gene_hex_pair[p[2]], 2); memcpy(dst+6,  gene_hex_pair[p[3]], 2);
	memcpy(dst+8,  gene_hex_pair[p[4]], 2); memcpy(dst+10, gene_hex_pair[p[5]], 2);
	memcpy(dst+12, gene_hex_pair[p[6]], 2); memcpy(dst+14, gene_hex_pair[p[7]], 2);
#endif
}

/* Fast 32-bit to hex: 4 × memcpy-2 → compiler emits 4 × 16-bit load+store */
static zend_always_inline void gene_u32_to_hex(uint32_t val, char *dst) {
	const uint8_t *p = (const uint8_t *)&val;
#ifdef WORDS_BIGENDIAN
	memcpy(dst,   gene_hex_pair[p[3]], 2); memcpy(dst+2, gene_hex_pair[p[2]], 2);
	memcpy(dst+4, gene_hex_pair[p[1]], 2); memcpy(dst+6, gene_hex_pair[p[0]], 2);
#else
	memcpy(dst,   gene_hex_pair[p[0]], 2); memcpy(dst+2, gene_hex_pair[p[1]], 2);
	memcpy(dst+4, gene_hex_pair[p[2]], 2); memcpy(dst+6, gene_hex_pair[p[3]], 2);
#endif
}

/* Helper to set 64-bit hash as 16-char hex string in zval */
static zend_always_inline void gene_set_u64_hex_result(uint64_t hash, zval *retval)
{
	zend_string *out = zend_string_alloc(16, 0);
	gene_u64_to_hex(hash, ZSTR_VAL(out));
	ZSTR_VAL(out)[16] = '\0';
	ZVAL_STR(retval, out);
}

/* Helper to set 32-bit hash as 8-char hex string in zval */
static zend_always_inline void gene_set_u32_hex_result(uint32_t hash, zval *retval)
{
	zend_string *out = zend_string_alloc(8, 0);
	gene_u32_to_hex(hash, ZSTR_VAL(out));
	ZSTR_VAL(out)[8] = '\0';
	ZVAL_STR(retval, out);
}

zend_string *gene_get_class_name_fast(void);
int is_json(zval *str);
int is_serialize(zval *str);
int is_igbinary(zval *str);
int serialize(zval *arr, zval *string, zval *serializer_handler);
int unserialize(zval *string, zval *arr, zval *serializer_handler);
void gene_microtime(zval *retval);
void gene_strip_tags(zval *value, zval *retval);
void gene_uniqid(zval *value, zval *retval);
void gene_time(zval *retval);

#endif
