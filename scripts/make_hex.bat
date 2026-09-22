@echo off
setlocal

set BUILD_DIR=%~1

if "%BUILD_DIR%"=="" (
    echo Usage:
    echo   make_hex.bat build
    exit /b 1
)

set OBJCOPY=arm-none-eabi-objcopy

echo.
echo Generating application HEX...

%OBJCOPY% ^
    -I binary ^
    -O ihex ^
    --change-addresses 0x00008000 ^
    "%BUILD_DIR%\application_image.bin" ^
    "%BUILD_DIR%\application_image.hex"

if errorlevel 1 (
    echo ERROR: Application HEX generation failed.
    exit /b 1
)

echo.
echo Generating full Flash HEX...

%OBJCOPY% ^
    -I binary ^
    -O ihex ^
    --change-addresses 0x00000000 ^
    "%BUILD_DIR%\tm4c123_full_image.bin" ^
    "%BUILD_DIR%\tm4c123_full_image.hex"

if errorlevel 1 (
    echo ERROR: Full HEX generation failed.
    exit /b 1
)

echo.
echo HEX generation complete.

endlocal