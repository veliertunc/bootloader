cmake_minimum_required(VERSION 3.24)

project(tm4c123_bootloader
    LANGUAGES C
)


# ============================================================================
# Project configuration
# ============================================================================

set(TIVAWARE_DIR
    "D:/TI/TivaWare_C_Series"
    CACHE PATH
    "TivaWare C Series installation directory"
)


set(BL_TRANSPORT "UART" CACHE STRING "Bootloader transport")

set_property(
    CACHE BL_TRANSPORT
    PROPERTY STRINGS
        UART
        CAN
        SPI
        I2C
        USB
)

if(BL_TRANSPORT STREQUAL "UART")
    set(BL_TRANSPORT_VALUE 0)
elseif(BL_TRANSPORT STREQUAL "CAN")
    set(BL_TRANSPORT_VALUE 1)
elseif(BL_TRANSPORT STREQUAL "SPI")
    set(BL_TRANSPORT_VALUE 2)
elseif(BL_TRANSPORT STREQUAL "I2C")
    set(BL_TRANSPORT_VALUE 3)
elseif(BL_TRANSPORT STREQUAL "USB")
    set(BL_TRANSPORT_VALUE 4)
else()
    message(FATAL_ERROR
        "Invalid BL_TRANSPORT='${BL_TRANSPORT}'")
endif()

target_compile_definitions(
    bootloader
    PRIVATE
        BL_TRANSPORT=${BL_TRANSPORT_VALUE}
)


set_property(
    CACHE BL_TRANSPORT
    PROPERTY STRINGS
    UART CAN SPI I2C USB
)


string(TOUPPER
    "${BL_TRANSPORT}"
    BL_TRANSPORT_UPPER
)


# ============================================================================
# ARM GNU toolchain
# ============================================================================

set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake"
    CACHE FILEPATH
    "ARM GNU toolchain file"
)


# ============================================================================
# Transport selection
# ============================================================================

set(BL_TRANSPORT_SOURCES)


if(BL_TRANSPORT_UPPER STREQUAL "UART")

    set(BL_TRANSPORT_VALUE 0)

    list(APPEND
        BL_TRANSPORT_SOURCES

        ${CMAKE_CURRENT_SOURCE_DIR}/src/transport/bl_uart.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "CAN")

    set(BL_TRANSPORT_VALUE 1)

    list(APPEND
        BL_TRANSPORT_SOURCES

        ${CMAKE_CURRENT_SOURCE_DIR}/src/transport/bl_can.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "SPI")

    set(BL_TRANSPORT_VALUE 2)

    list(APPEND
        BL_TRANSPORT_SOURCES

        ${CMAKE_CURRENT_SOURCE_DIR}/src/transport/bl_spi.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "I2C")

    set(BL_TRANSPORT_VALUE 3)

    list(APPEND
        BL_TRANSPORT_SOURCES

        ${CMAKE_CURRENT_SOURCE_DIR}/src/transport/bl_i2c.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "USB")

    set(BL_TRANSPORT_VALUE 4)

    list(APPEND
        BL_TRANSPORT_SOURCES

        ${CMAKE_CURRENT_SOURCE_DIR}/src/transport/bl_usb.c
    )


else()

    message(FATAL_ERROR
        "Unsupported BL_TRANSPORT='${BL_TRANSPORT}'. "
        "Valid values are UART, CAN, SPI, I2C, USB."
    )

endif()


# ============================================================================
# DriverLib
# ============================================================================

set(TIVAWARE_DRIVERLIB_SOURCES

    ${TIVAWARE_DIR}/driverlib/cpu.c
    ${TIVAWARE_DIR}/driverlib/flash.c
    ${TIVAWARE_DIR}/driverlib/gpio.c
    ${TIVAWARE_DIR}/driverlib/sysctl.c
    ${TIVAWARE_DIR}/driverlib/systick.c
    ${TIVAWARE_DIR}/driverlib/interrupt.c
)


if(BL_TRANSPORT_UPPER STREQUAL "UART")

    list(APPEND
        TIVAWARE_DRIVERLIB_SOURCES

        ${TIVAWARE_DIR}/driverlib/uart.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "CAN")

    list(APPEND
        TIVAWARE_DRIVERLIB_SOURCES

        ${TIVAWARE_DIR}/driverlib/can.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "SPI")

    list(APPEND
        TIVAWARE_DRIVERLIB_SOURCES

        ${TIVAWARE_DIR}/driverlib/ssi.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "I2C")

    list(APPEND
        TIVAWARE_DRIVERLIB_SOURCES

        ${TIVAWARE_DIR}/driverlib/i2c.c
    )


elseif(BL_TRANSPORT_UPPER STREQUAL "USB")

    list(APPEND
        TIVAWARE_DRIVERLIB_SOURCES

        ${TIVAWARE_DIR}/driverlib/usb.c
    )

endif()


# ============================================================================
# USBLib
# ============================================================================

set(TIVAWARE_USBLIB_SOURCES)


if(BL_TRANSPORT_UPPER STREQUAL "USB")

    set(TIVAWARE_USBLIB_SOURCES

        # --------------------------------------------------------------------
        # Generic USB library
        # --------------------------------------------------------------------
        ${TIVAWARE_DIR}/usblib/usbbuffer.c
        ${TIVAWARE_DIR}/usblib/usbdesc.c
        ${TIVAWARE_DIR}/usblib/usbmode.c
        ${TIVAWARE_DIR}/usblib/usbringbuf.c
        ${TIVAWARE_DIR}/usblib/usbtick.c

        # --------------------------------------------------------------------
        # USB device layer
        # --------------------------------------------------------------------

        ${TIVAWARE_DIR}/usblib/device/usbdcdesc.c
        ${TIVAWARE_DIR}/usblib/device/usbdconfig.c
        ${TIVAWARE_DIR}/usblib/device/usbdcdc.c
        ${TIVAWARE_DIR}/usblib/device/usbdenum.c
        ${TIVAWARE_DIR}/usblib/device/usbdhandler.c
    )

endif()


# ============================================================================
# Common bootloader sources
# ============================================================================

set(BL_COMMON_SOURCES

    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/main.c

    ${CMAKE_CURRENT_SOURCE_DIR}/platform/board.c

    ${CMAKE_CURRENT_SOURCE_DIR}/boot/bl_boot.c
    ${CMAKE_CURRENT_SOURCE_DIR}/boot/bl_time.c

    ${CMAKE_CURRENT_SOURCE_DIR}/core/bl_context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/core/bl_state.c

    ${CMAKE_CURRENT_SOURCE_DIR}/core/bl_memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/flash/bl_flash.c

    ${CMAKE_CURRENT_SOURCE_DIR}/core/bl_crc32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/core/bl_packet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/core/bl_protocol.c

    ${CMAKE_CURRENT_SOURCE_DIR}/image/bl_image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/boot/bl_jump.c

    ${CMAKE_CURRENT_SOURCE_DIR}/transport/bl_transport.c
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/bl_transport_packetizer.c

    ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/startup_gcc.c
)


# ============================================================================
# Executable
# ============================================================================

add_executable(
    bootloader

    ${BL_COMMON_SOURCES}
    ${BL_TRANSPORT_SOURCES}
    ${TIVAWARE_DRIVERLIB_SOURCES}
    ${TIVAWARE_USBLIB_SOURCES}
)


# ============================================================================
# Preprocessor definitions
# ============================================================================

target_compile_definitions(
    bootloader
    PRIVATE

    PART_TM4C123GH6PM
    TARGET_IS_TM4C123_RA1

    BL_TRANSPORT=${BL_TRANSPORT_VALUE}
)


# ============================================================================
# Include directories
# ============================================================================

target_include_directories(
    bootloader
    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/transport

    ${TIVAWARE_DIR}

    ${TIVAWARE_DIR}/driverlib
    ${TIVAWARE_DIR}/inc
)


if(BL_TRANSPORT_UPPER STREQUAL "USB")

    target_include_directories(
        bootloader
        PRIVATE

        ${TIVAWARE_DIR}/usblib
        ${TIVAWARE_DIR}/usblib/device
    )

endif()


# ============================================================================
# Compiler options
# ============================================================================

target_compile_options(
    bootloader
    PRIVATE

    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=softfp

    -ffunction-sections
    -fdata-sections

    -Wall
    -Wextra
    -Wpedantic

    -Wshadow
    -Wconversion
    -Wsign-conversion

    -fno-common
)


# ============================================================================
# Linker options
# ============================================================================

target_link_options(
    bootloader
    PRIVATE

    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=softfp

    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/bootloader.ld

    -Wl,--gc-sections

    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/bootloader.map

    --specs=nano.specs
    --specs=nosys.specs
)


# ============================================================================
# Binary / HEX / size output
# ============================================================================

add_custom_command(
    TARGET bootloader
    POST_BUILD

    COMMAND
        ${ARM_OBJCOPY}
        -O binary
        $<TARGET_FILE:bootloader>
        ${CMAKE_CURRENT_BINARY_DIR}/bootloader.bin

    COMMAND
        ${ARM_OBJCOPY}
        -O ihex
        $<TARGET_FILE:bootloader>
        ${CMAKE_CURRENT_BINARY_DIR}/bootloader.hex

    COMMAND
        ${ARM_SIZE}
        $<TARGET_FILE:bootloader>

    COMMENT
        "Generating bootloader binary, HEX and size information"
)


# ============================================================================
# Build information
# ============================================================================

message(STATUS "")
message(STATUS "==============================================")
message(STATUS "TM4C123 Bootloader")
message(STATUS "==============================================")
message(STATUS "TivaWare       : ${TIVAWARE_DIR}")
message(STATUS "Transport      : ${BL_TRANSPORT_UPPER}")
message(STATUS "Transport ID   : ${BL_TRANSPORT_VALUE}")
message(STATUS "==============================================")
message(STATUS "")