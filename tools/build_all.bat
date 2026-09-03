@echo off
setlocal enabledelayedexpansion

if not defined PHP_SDK_ROOT set "PHP_SDK_ROOT=F:\php-sdk-2.6.0"
if not exist "%PHP_SDK_ROOT%" if exist "F:\php-sdk-2.3.0" set "PHP_SDK_ROOT=F:\php-sdk-2.3.0"
if not defined PHP_SRC_ROOT set "PHP_SRC_ROOT=F:\php_src"
set "SDK=%PHP_SDK_ROOT%"
set "TASK_X64=%~dp0task_build_x64.bat"
set "TASK_X86=%~dp0task_build_x86.bat"
set "ARCH=%~1"
set "VERSION=%~2"
set /a FAILED=0, SUCCEEDED=0, SKIPPED=0

if not defined ARCH set "ARCH=all"
if not defined VERSION set "VERSION=all"
if /i "%ARCH%"=="--help" goto :usage
if /i "%ARCH%"=="-h" goto :usage
if /i not "%ARCH%"=="all" if /i not "%ARCH%"=="x64" if /i not "%ARCH%"=="x86" (
    echo Invalid architecture: %ARCH%
    goto :usage_error
)
if not "%~3"=="" (
    echo Too many arguments.
    goto :usage_error
)
if /i not "%VERSION%"=="all" (
    set "VALID_VERSION="
    for %%V in (8.1 8.2 8.3 8.4 8.5) do if /i "%VERSION%"=="%%V" set "VALID_VERSION=1"
    if not defined VALID_VERSION (
        echo Invalid PHP version: %VERSION%
        goto :usage_error
    )
)
if not exist "%SDK%" (
    echo PHP SDK not found: %SDK%
    exit /b 2
)
if not exist "%TASK_X64%" (
    echo Build task not found: %TASK_X64%
    exit /b 2
)
if not exist "%TASK_X86%" (
    echo Build task not found: %TASK_X86%
    exit /b 2
)

echo ========================================
echo Phase 1: X64 NTS Builds
echo ========================================

rem 8.1 - vs16 (restore NTS Makefile first)
call :build x64 8.1 8.1.30 vs16
rem 8.2 - vs16
call :build x64 8.2 8.2.33 vs16
rem 8.3 - vs16
call :build x64 8.3 8.3.33 vs16
rem 8.4 - vs17
call :build x64 8.4 8.4.25 vs17
rem 8.5 - vs17
call :build x64 8.5 8.5.10 vs17

echo.
echo ========================================
echo Phase 2: X86 NTS Builds
echo ========================================

rem 8.1 - vs16
call :build x86 8.1 8.1.30 vs16
rem 8.2 - vs16
call :build x86 8.2 8.2.33 vs16
rem 8.3 - vs16
call :build x86 8.3 8.3.33 vs16
rem 8.4 - vs17
call :build x86 8.4 8.4.25 vs17
rem 8.5 - vs17
call :build x86 8.5 8.5.10 vs17

echo.
echo ========================================
echo Results: !SUCCEEDED! succeeded, !FAILED! failed, !SKIPPED! skipped
if !FAILED! equ 0 (echo All selected builds completed successfully) else (echo One or more selected builds failed)
echo ========================================
endlocal & exit /b %FAILED%

:build
if /i not "%ARCH%"=="all" if /i not "%ARCH%"=="%~1" (
    set /a SKIPPED+=1
    exit /b 0
)
if /i not "%VERSION%"=="all" if /i not "%VERSION%"=="%~2" if /i not "%VERSION%"=="%~3" (
    set /a SKIPPED+=1
    exit /b 0
)
set "PHP_SOURCE=%PHP_SRC_ROOT%\php-%~3-src"
set "SDK_LAUNCHER=%SDK%\phpsdk-%~4-%~1.bat"
if /i "%~1"=="x64" (set "BUILD_TASK=%TASK_X64%") else (set "BUILD_TASK=%TASK_X86%")
echo.
echo [%~1] Building php-%~3-src (%~4)...
if not exist "!PHP_SOURCE!\buildconf.bat" (
    echo [%~1] FAILED: source not found at "!PHP_SOURCE!"
    set /a FAILED+=1
    exit /b 0
)
if not exist "!SDK_LAUNCHER!" (
    echo [%~1] FAILED: SDK launcher not found at "!SDK_LAUNCHER!"
    set /a FAILED+=1
    exit /b 0
)
call "!SDK_LAUNCHER!" -t "!BUILD_TASK!" --task-args "!PHP_SOURCE!"
if errorlevel 1 (
    echo [%~1] FAILED: %~2
    set /a FAILED+=1
) else (
    echo [%~1] OK: %~2
    set /a SUCCEEDED+=1
)
exit /b 0

:usage
echo Usage: %~nx0 [all^|x64^|x86] [all^|8.1^|8.2^|8.3^|8.4^|8.5]
echo Environment: PHP_SDK_ROOT, PHP_SRC_ROOT
exit /b 0

:usage_error
call :usage
exit /b 64
