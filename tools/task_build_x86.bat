@echo off
set "PHPDIR=%~1"
cd /d "%PHPDIR%" || exit /b 1
call buildconf.bat --force || exit /b 1
call configure.bat --disable-all --enable-cli --enable-cgi --disable-zts --enable-pdo --enable-gene=shared || exit /b 1
nmake php_gene.dll || exit /b 1
exit /b 0
