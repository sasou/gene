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

#ifndef GENE_ROUTER_H
#define GENE_ROUTER_H
#define GENE_ROUTER_SAFE	"safe"
#define GENE_ROUTER_GROUP	"group"
#define GENE_ROUTER_PREFIX	"prefix"
#define GENE_ROUTER_LANG	"lang"
#define GENE_ROUTER_LANGS	"langs"
#define GENE_ROUTER_ROUTER_EVENT ":re"
#define GENE_ROUTER_ROUTER_TREE ":rt"
#define GENE_ROUTER_ROUTER_CONF ":cf"
#define GENE_ROUTER_LEAF_RUN "%s/leaf/run"
#define GENE_ROUTER_LEAF_HOOK "%s/leaf/hook"
#define GENE_ROUTER_LEAF_KEY "%s/leaf/key"
#define GENE_ROUTER_LEAF_RUN_L "%s/%s/leaf/run"
#define GENE_ROUTER_LEAF_HOOK_L "%s/%s/leaf/hook"
#define GENE_ROUTER_LEAF_KEY_L "%s/%s/leaf/key"
#define GENE_ROUTER_CHIRD "chird/"
#define GENE_ROUTER_CONTENT_B  "$gene_b=new %s;$gene_h=$gene_b->%s();if(isset($gene_h)&&($gene_h==0))return;"
#define GENE_ROUTER_CONTENT_A  "$gene_a=new %s;$gene_a->%s($gene_mp);"
#define GENE_ROUTER_CONTENT_M  "$gene_mp=\\Gene\\Router::dispatch(\"%s\",\"%s\",\\Gene\\Router::params());"
#define GENE_ROUTER_CONTENT_H  "$gene_h=new %s;$gene_h=$gene_h->%s();if(isset($gene_h)&&($gene_h==0))return;"
#define GENE_ROUTER_CONTENT_FB  "$funb=%s;$gene_h=$funb();if(isset($gene_h)&&($gene_h==0))return;"
#define GENE_ROUTER_CONTENT_FA  "$funa=%s;$funa($gene_mp);"
#define GENE_ROUTER_CONTENT_FM  "$funm=%s;$gene_mp=$funm(\\Gene\\Router::params());"
#define GENE_ROUTER_CONTENT_FH  "$funh=%s;$gene_h=$funh();if(isset($gene_h)&&($gene_h==0))return;"

extern zend_class_entry *gene_router_ce;

void init();
void get_router_content_run(char *methodin,char *pathin,zval *safe);

GENE_MINIT_FUNCTION (router);

#endif
