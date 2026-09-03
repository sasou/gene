@echo off
if "%GENE_PHP_SRC%"=="" set "GENE_PHP_SRC=F:\php_src\php-8.1.30-src"
cd /d "%GENE_PHP_SRC%" || exit /b 1
if defined GENE_DEPS_BRANCH call phpsdk_deps --update --branch %GENE_DEPS_BRANCH% || exit /b 1
if not defined GENE_SKIP_CONFIG call buildconf.bat || exit /b 1
if "%GENE_CONFIGURE_ARGS%"=="" set "GENE_CONFIGURE_ARGS=--disable-all --enable-cli --enable-pdo --enable-gene=shared"
if not defined GENE_SKIP_CONFIG call configure.bat %GENE_CONFIGURE_ARGS% || exit /b 1
if "%GENE_BUILD_TARGETS%"=="" set "GENE_BUILD_TARGETS=php.exe php_gene.dll"
nmake %GENE_BUILD_TARGETS%
