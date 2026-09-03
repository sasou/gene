@echo off
setlocal
if not "%~1"=="" set "GENE_PHP_SRC=%~1"
if "%GENE_PHP_SRC%"=="" set "GENE_PHP_SRC=F:\php_src\php-8.1.30-src"
if not exist "%GENE_PHP_SRC%\buildconf.bat" (
    echo Invalid PHP source directory: "%GENE_PHP_SRC%".
    exit /b 2
)
pushd "%GENE_PHP_SRC%" || exit /b 2
if defined GENE_DEPS_BRANCH (
    call phpsdk_deps --update --branch %GENE_DEPS_BRANCH%
    if errorlevel 1 goto :failed
)
if not defined GENE_SKIP_CONFIG (
    call buildconf.bat
    if errorlevel 1 goto :failed
)
if "%GENE_CONFIGURE_ARGS%"=="" set "GENE_CONFIGURE_ARGS=--disable-all --enable-cli --enable-pdo --enable-gene=shared"
if not defined GENE_SKIP_CONFIG (
    call configure.bat %GENE_CONFIGURE_ARGS%
    if errorlevel 1 goto :failed
)
if "%GENE_BUILD_TARGETS%"=="" set "GENE_BUILD_TARGETS=php.exe php_gene.dll"
nmake %GENE_BUILD_TARGETS%
if errorlevel 1 goto :failed
popd
exit /b 0

:failed
set "BUILD_ERROR=%ERRORLEVEL%"
popd
exit /b %BUILD_ERROR%
