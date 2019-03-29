PHP_ARG_ENABLE(gene, whether to enable gene support,
[  --enable-gene           Enable gene support], yes)

AC_ARG_ENABLE(gene-debug,
[  --enable-gene-debug     Enable gene debug mode default=no],
[PHP_GENE_DEBUG=$enableval],
[PHP_GENE_DEBUG="no"])  

if test "$PHP_GENE" != "no"; then

  if test "$PHP_YAF_DEBUG" = "yes"; then
    AC_DEFINE(PHP_GENE_DEBUG, 1, [define to 1 if you want to change the POST/GET by php script])
  else
    AC_DEFINE(PHP_GENE_DEBUG, 0, [define to 1 if you want to change the POST/GET by php script])
  fi

  AC_MSG_CHECKING([PHP version])

  tmp_version=$PHP_VERSION
  if test -z "$tmp_version"; then
    if test -z "$PHP_CONFIG"; then
      AC_MSG_ERROR([php-config not found])
    fi
    php_version=`$PHP_CONFIG --version 2>/dev/null|head -n 1|sed -e 's#\([0-9]\.[0-9]*\.[0-9]*\)\(.*\)#\1#'`
  else
    php_version=`echo "$tmp_version"|sed -e 's#\([0-9]\.[0-9]*\.[0-9]*\)\(.*\)#\1#'`
  fi

  if test -z "$php_version"; then
    AC_MSG_ERROR([failed to detect PHP version, please report])
  fi

  ac_IFS=$IFS
  IFS="."
  set $php_version
  IFS=$ac_IFS
  gene_php_version=`expr [$]1 \* 1000000 + [$]2 \* 1000 + [$]3`

  if test "$gene_php_version" -le "5002000"; then
    AC_MSG_ERROR([You need at least PHP 5.2.0 to be able to use this version of Gene. PHP $php_version found])
  else
    AC_MSG_RESULT([$php_version, ok])
  fi
  
  PHP_NEW_EXTENSION(gene, 
  gene.c \
  app/application.c \
  factory/load.c \
  di/di.c \
  router/router.c \
  tool/execute.c \
  cache/memory.c \ 
  common/common.c \
  config/configs.c \
  mvc/controller.c \
  session/session.c \
  http/request.c \
  http/response.c \
  mvc/view.c \
  exception/exception.c \
  tool/benchmark.c \
  db/db.c \
  mvc/model.c \
  service/service.c \
  factory/factory.c \
  cache/redis.c \
  cache/memcached.c \
  cache/cache.c, 
  $ext_shared, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1, ,yes)
  PHP_ADD_BUILD_DIR([$ext_builddir/app], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/cache], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/common], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/config], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/db], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/di], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/exception], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/factory], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/http], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/mvc], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/router], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/service], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/session], 1)
  PHP_ADD_BUILD_DIR([$ext_builddir/tool], 1)

fi
