@echo off
setlocal

set PROJECT_ROOT=%~dp0..
set BUILD_DIR=%PROJECT_ROOT%\build
set TIVAWARE_DIR=D:\TI\TivaWare_C_Series

cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G Ninja ^
    "-DTIVAWARE_DIR=%TIVAWARE_DIR%" ^
    -DBL_TRANSPORT=RS232
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 exit /b 1

echo Build complete. Firmware, application, and full flash images are in:
echo   %BUILD_DIR%
endlocal
