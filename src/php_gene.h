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

#ifndef PHP_GENE_H
#define PHP_GENE_H

extern zend_module_entry gene_module_entry;
#define phpext_gene_ptr &gene_module_entry

#define PHP_GENE_VERSION "1.22"

#ifdef PHP_WIN32
#	define PHP_GENE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_GENE_API __attribute__ ((visibility("default")))
#else
#	define PHP_GENE_API
#endif


#ifdef ZTS
#include "TSRM.h"
#endif

#ifdef ZTS
#define GENE_G(v) TSRMG(gene_globals_id, zend_gene_globals *, v)
#else
#define GENE_G(v) (gene_globals.v)
#endif

#define GENE_INIT_CLASS_ENTRY(ce, common_name, namespace_name, methods) \
	if(GENE_G(use_namespace)) { \
		INIT_CLASS_ENTRY(ce, namespace_name, methods); \
	} else { \
		INIT_CLASS_ENTRY(ce, common_name, methods); \
	}
#define GENE_MINIT_FUNCTION(module)   	    ZEND_MINIT_FUNCTION(gene_##module)
#define GENE_MSHUTDOWN_FUNCTION(module)   	ZEND_MSHUTDOWN_FUNCTION(gene_##module)
#define GENE_RINIT_FUNCTION(module)   	    ZEND_RINIT_FUNCTION(gene_##module)
#define GENE_RSHUTDOWN_FUNCTION(module)   	ZEND_RSHUTDOWN_FUNCTION(gene_##module)
#define GENE_MINFO_FUNCTION(module)   	PHP_MINFO_FUNCTION(module)
#define GENE_STARTUP(module)	 		ZEND_MODULE_STARTUP_N(gene_##module)(INIT_FUNC_ARGS_PASSTHRU)


PHP_MINIT_FUNCTION(gene);
PHP_MSHUTDOWN_FUNCTION(gene);
PHP_RINIT_FUNCTION(gene);
PHP_RSHUTDOWN_FUNCTION(gene);
PHP_MINFO_FUNCTION(gene);



ZEND_BEGIN_MODULE_GLOBALS(gene)
	char 		*directory;
    char        *app_root;
    char        *app_view;
    char        *app_ext;
	char		*method;
	char 		*path;
	char 		*router_path;
	char 		*app_key;
	char 		*auto_load_fun;
	char 		*child_views;
    zend_bool   gene_error;
    zend_bool   gene_exception;
    zend_bool   run_environment;
	zend_bool   use_namespace;
    zend_bool   view_compile;
	HashTable	*cache;
	HashTable	*cache_easy;
ZEND_END_MODULE_GLOBALS(gene)

extern ZEND_DECLARE_MODULE_GLOBALS(gene);


#endif	/* PHP_GENE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
