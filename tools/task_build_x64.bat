@echo off
set "PHP_SRC=%~1"
cd /d "%PHP_SRC%" || exit /b 1
call config.nice.bat || exit /b 1
nmake php_gene.dll || exit /b 1
