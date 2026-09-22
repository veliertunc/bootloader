@echo off

setlocal

echo ==========================================
echo TM4C123 Bootloader Build
echo ==========================================

set PROJECT_ROOT=%~dp0..

set TIVAWARE_ROOT=D:\ti\TivaWare_C_Series
set ARM_GNU_TOOLCHAIN=D:\Program Files\Arm\arm-none-eabi

set BUILD_DIR=%PROJECT_ROOT%\build

echo.
echo Project:
echo   %PROJECT_ROOT%

echo.
echo TivaWare:
echo   %TIVAWARE_ROOT%

echo.
echo ARM GNU:
echo   %ARM_GNU_TOOLCHAIN%

echo.
echo [1/2] Configuring...

cmake ^
    -S "%PROJECT_ROOT%" ^
    -B "%BUILD_DIR%" ^
    -G Ninja ^
    "-DTIVAWARE_ROOT=%TIVAWARE_ROOT%" ^
    "-DARM_GNU_TOOLCHAIN=%ARM_GNU_TOOLCHAIN%"

if errorlevel 1 (
    echo.
    echo CMake configuration FAILED.
    exit /b 1
)

echo.
echo [2/2] Building...

cmake ^
    --build "%BUILD_DIR%" ^
    --parallel

if errorlevel 1 (
    echo.
    echo Build FAILED.
    exit /b 1
)

echo.
echo ==========================================
echo BUILD SUCCESSFUL
echo ==========================================

echo.
echo Output:
echo   %BUILD_DIR%\bootloader.elf
echo   %BUILD_DIR%\bootloader.bin
echo   %BUILD_DIR%\bootloader.hex
echo   %BUILD_DIR%\bootloader.map

endlocal