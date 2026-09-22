@echo off

setlocal

echo ==========================================
echo TM4C123 Bootloader Flash
echo ==========================================

set PROJECT_ROOT=%~dp0..
set IMAGE=%PROJECT_ROOT%\build\bootloader.bin

set LMFLASH=D:\ti\LMFlash\LMFlash.exe

if not exist "%IMAGE%" (
    echo ERROR:
    echo Bootloader image does not exist:
    echo   %IMAGE%
    echo.
    echo Build the project first.
    exit /b 1
)

if not exist "%LMFLASH%" (
    echo ERROR:
    echo LMFlash was not found:
    echo   %LMFLASH%
    echo.
    echo Update the LMFLASH path in this script.
    exit /b 1
)

echo.
echo Image:
echo   %IMAGE%

echo.
echo Programming device...

"%LMFLASH%" ^
    -r "%IMAGE%"

if errorlevel 1 (
    echo.
    echo Flashing FAILED.
    exit /b 1
)

echo.
echo ==========================================
echo FLASH SUCCESSFUL
echo ==========================================

endlocal