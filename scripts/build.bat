@echo off
setlocal EnableExtensions

set PROJECT_ROOT=%~dp0..
set BUILD_DIR=%PROJECT_ROOT%\build

set TIVAWARE_DIR=D:/TI/TivaWare_C_Series

echo.
echo ==========================================
echo TM4C123 Bootloader Build
echo ==========================================
echo.

if not exist "%PROJECT_ROOT%\CMakeLists.txt" (
    echo ERROR: CMakeLists.txt not found.
    exit /b 1
)

if not exist "%TIVAWARE_DIR%" (
    echo ERROR: TivaWare directory not found:
    echo   %TIVAWARE_DIR%
    echo.
    echo Edit scripts\build.bat and set TIVAWARE_DIR.
    exit /b 1
)

echo [1/3] Configuring CMake...

cmake ^
    -S "%PROJECT_ROOT%" ^
    -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE="%PROJECT_ROOT%\cmake\arm-none-eabi.cmake" ^
    -DTIVAWARE_DIR="%TIVAWARE_DIR%" ^
    -DBL_TRANSPORT=RS232

if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

echo.
echo [2/3] Building...

cmake --build "%BUILD_DIR%" --parallel

if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

echo.
echo [3/3] Generating HEX files...

call "%PROJECT_ROOT%\scripts\make_hex.bat" "%BUILD_DIR%"

if errorlevel 1 (
    echo ERROR: HEX generation failed.
    exit /b 1
)

echo.
echo ==========================================
echo BUILD SUCCESSFUL
echo ==========================================
echo.
echo Output:
echo.
echo   %BUILD_DIR%\bootloader.elf
echo   %BUILD_DIR%\bootloader.bin
echo.
echo   %BUILD_DIR%\application.elf
echo   %BUILD_DIR%\application.bin
echo.
echo   %BUILD_DIR%\image_header.bin
echo   %BUILD_DIR%\application_image.bin
echo   %BUILD_DIR%\application_image.hex
echo.
echo   %BUILD_DIR%\tm4c123_full_image.bin
echo   %BUILD_DIR%\tm4c123_full_image.hex
echo.

endlocal
