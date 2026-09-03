@echo off
setlocal enabledelayedexpansion

set "SDK=F:\php-sdk-2.6.0"
set "TASK_X64=F:\gene\tools\task_build_simple.bat"
set "TASK_X86=F:\gene\tools\task_build_x86.bat"

echo ========================================
echo Phase 1: X64 NTS Builds
echo ========================================

rem 8.1 - vs16 (restore NTS Makefile first)
echo.
echo [X64] Building php-8.1.30-src (vs16)...
call "%SDK%\phpsdk-vs16-x64.bat" -t "%TASK_X64%" --task-args "F:\php_src\php-8.1.30-src"
if errorlevel 1 (echo [X64] FAILED: 8.1) else (echo [X64] OK: 8.1)

rem 8.2 - vs16
echo.
echo [X64] Building php-8.2.33-src (vs16)...
call "%SDK%\phpsdk-vs16-x64.bat" -t "%TASK_X64%" --task-args "F:\php_src\php-8.2.33-src"
if errorlevel 1 (echo [X64] FAILED: 8.2) else (echo [X64] OK: 8.2)

rem 8.3 - vs16
echo.
echo [X64] Building php-8.3.33-src (vs16)...
call "%SDK%\phpsdk-vs16-x64.bat" -t "%TASK_X64%" --task-args "F:\php_src\php-8.3.33-src"
if errorlevel 1 (echo [X64] FAILED: 8.3) else (echo [X64] OK: 8.3)

rem 8.4 - vs17
echo.
echo [X64] Building php-8.4.25-src (vs17)...
call "%SDK%\phpsdk-vs17-x64.bat" -t "%TASK_X64%" --task-args "F:\php_src\php-8.4.25-src"
if errorlevel 1 (echo [X64] FAILED: 8.4) else (echo [X64] OK: 8.4)

rem 8.5 - vs17
echo.
echo [X64] Building php-8.5.10-src (vs17)...
call "%SDK%\phpsdk-vs17-x64.bat" -t "%TASK_X64%" --task-args "F:\php_src\php-8.5.10-src"
if errorlevel 1 (echo [X64] FAILED: 8.5) else (echo [X64] OK: 8.5)

echo.
echo ========================================
echo Phase 2: X86 NTS Builds
echo ========================================

rem 8.1 - vs16
echo.
echo [X86] Building php-8.1.30-src (vs16)...
call "%SDK%\phpsdk-vs16-x86.bat" -t "%TASK_X86%" --task-args "F:\php_src\php-8.1.30-src"
if errorlevel 1 (echo [X86] FAILED: 8.1) else (echo [X86] OK: 8.1)

rem 8.2 - vs16
echo.
echo [X86] Building php-8.2.33-src (vs16)...
call "%SDK%\phpsdk-vs16-x86.bat" -t "%TASK_X86%" --task-args "F:\php_src\php-8.2.33-src"
if errorlevel 1 (echo [X86] FAILED: 8.2) else (echo [X86] OK: 8.2)

rem 8.3 - vs16
echo.
echo [X86] Building php-8.3.33-src (vs16)...
call "%SDK%\phpsdk-vs16-x86.bat" -t "%TASK_X86%" --task-args "F:\php_src\php-8.3.33-src"
if errorlevel 1 (echo [X86] FAILED: 8.3) else (echo [X86] OK: 8.3)

rem 8.4 - vs17
echo.
echo [X86] Building php-8.4.25-src (vs17)...
call "%SDK%\phpsdk-vs17-x86.bat" -t "%TASK_X86%" --task-args "F:\php_src\php-8.4.25-src"
if errorlevel 1 (echo [X86] FAILED: 8.4) else (echo [X86] OK: 8.4)

rem 8.5 - vs17
echo.
echo [X86] Building php-8.5.10-src (vs17)...
call "%SDK%\phpsdk-vs17-x86.bat" -t "%TASK_X86%" --task-args "F:\php_src\php-8.5.10-src"
if errorlevel 1 (echo [X86] FAILED: 8.5) else (echo [X86] OK: 8.5)

echo.
echo ========================================
echo All builds complete
echo ========================================
endlocal
