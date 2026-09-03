@echo off
setlocal
set "PHP_SRC=%~1"
if not defined PHP_SRC (
    echo Missing PHP source directory.
    exit /b 2
)
if not exist "%PHP_SRC%\buildconf.bat" (
    echo Invalid PHP source directory: "%PHP_SRC%".
    exit /b 2
)
pushd "%PHP_SRC%" || exit /b 2
call buildconf.bat --force
if errorlevel 1 goto :failed
call configure.bat --disable-all --enable-cli --enable-cgi --disable-zts --enable-pdo --enable-gene=shared
if errorlevel 1 goto :failed
nmake php_gene.dll
if errorlevel 1 goto :failed
popd
exit /b 0

:failed
set "BUILD_ERROR=%ERRORLEVEL%"
popd
exit /b %BUILD_ERROR%
