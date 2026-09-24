@echo off
setlocal
set "PHP_SRC=%~1"
if not defined PHP_SRC (
    echo Missing PHP source directory.
    exit /b 2
)
if not exist "%PHP_SRC%\config.nice.bat" (
    echo Missing config.nice.bat in "%PHP_SRC%".
    exit /b 2
)
pushd "%PHP_SRC%" || exit /b 2
call config.nice.bat
if errorlevel 1 goto :failed
rem .dep does not track ext/gene headers reliably; stale objects built
rem against an older gene.h struct layout link silently and corrupt
rem zend_gene_globals field offsets at runtime. Force a clean gene build.
if exist "x64\Release\ext\gene" del /s /q "x64\Release\ext\gene\*.obj" >nul 2>&1
nmake php_gene.dll
if errorlevel 1 goto :failed
popd
exit /b 0

:failed
set "BUILD_ERROR=%ERRORLEVEL%"
popd
exit /b %BUILD_ERROR%
