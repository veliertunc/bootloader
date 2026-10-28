cmake_minimum_required(VERSION 3.24)

set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake"
    CACHE FILEPATH "ARM GNU toolchain file")

project(tm4c123_bootloader LANGUAGES C)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(TIVAWARE_DIR "D:/TI/TivaWare_C_Series" CACHE PATH
    "TivaWare C Series installation directory")
set(BL_TRANSPORT "RS232" CACHE STRING "Bootloader transport")
set_property(CACHE BL_TRANSPORT PROPERTY STRINGS RS232 RS485 SPI I2C ETHERNET)

string(TOUPPER "${BL_TRANSPORT}" BL_TRANSPORT_UPPER)
set(BL_TRANSPORTS RS232 RS485 SPI I2C ETHERNET)
list(FIND BL_TRANSPORTS "${BL_TRANSPORT_UPPER}" BL_TRANSPORT_INDEX)
if(BL_TRANSPORT_INDEX EQUAL -1)
    message(FATAL_ERROR
        "Invalid BL_TRANSPORT='${BL_TRANSPORT}'. Choose RS232, RS485, SPI, I2C, or ETHERNET.")
endif()

if(NOT EXISTS "${TIVAWARE_DIR}/driverlib/gpio.c")
    message(FATAL_ERROR
        "TivaWare was not found at '${TIVAWARE_DIR}'. Set -DTIVAWARE_DIR=<path>.")
endif()

set(BL_TRANSPORT_VALUE ${BL_TRANSPORT_INDEX})

set(BL_SOURCES
    bootloader/main.c
    bootloader/startup_gcc.c
    platform/board.c
    config/bl_version.c
    boot/bl_boot.c
    boot/bl_jump.c
    boot/bl_time.c
    core/bl_context.c
    core/bl_crc32.c
    core/bl_memory.c
    core/bl_packet.c
    core/bl_protocol.c
    core/bl_state.c
    flash/bl_flash.c
    image/bl_image.c
    transport/bl_fifo.c
    transport/bl_transport.c
    transport/bl_transport_packetizer.c
    ${TIVAWARE_DIR}/driverlib/cpu.c
    ${TIVAWARE_DIR}/driverlib/flash.c
    ${TIVAWARE_DIR}/driverlib/gpio.c
    ${TIVAWARE_DIR}/driverlib/sysctl.c
    ${TIVAWARE_DIR}/driverlib/systick.c
    ${TIVAWARE_DIR}/driverlib/interrupt.c
)

if(BL_TRANSPORT_UPPER STREQUAL "SPI" OR BL_TRANSPORT_UPPER STREQUAL "ETHERNET")
    list(APPEND BL_SOURCES ${TIVAWARE_DIR}/driverlib/ssi.c)
endif()
if(BL_TRANSPORT_UPPER STREQUAL "SPI")
    list(APPEND BL_SOURCES transport/bl_spi.c)
endif()
if(BL_TRANSPORT_UPPER STREQUAL "ETHERNET")
    list(APPEND BL_SOURCES transport/bl_ethernet.c)
endif()
if(BL_TRANSPORT_UPPER STREQUAL "I2C")
    list(APPEND BL_SOURCES transport/bl_i2c.c
        ${TIVAWARE_DIR}/driverlib/i2c.c)
endif()
if(BL_TRANSPORT_UPPER STREQUAL "RS232" OR BL_TRANSPORT_UPPER STREQUAL "RS485")
    list(APPEND BL_SOURCES transport/bl_uart.c
        ${TIVAWARE_DIR}/driverlib/uart.c)
endif()

add_executable(bootloader ${BL_SOURCES})
set_target_properties(bootloader PROPERTIES SUFFIX ".elf")
target_compile_definitions(bootloader PRIVATE
    PART_TM4C123GH6PM
    TARGET_IS_TM4C123_RA1
    BL_TRANSPORT=${BL_TRANSPORT_VALUE})
target_include_directories(bootloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/boot
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/flash
    ${CMAKE_CURRENT_SOURCE_DIR}/image
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/transport
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TIVAWARE_DIR}
    ${TIVAWARE_DIR}/driverlib
    ${TIVAWARE_DIR}/inc)
target_compile_options(bootloader PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp
    -ffunction-sections -fdata-sections
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -fno-common)
target_link_options(bootloader PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/bootloader.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/bootloader.map
    --specs=nano.specs --specs=nosys.specs)

add_executable(application
    application/main.c
    application/app.c
    application/startup_gcc.c
    platform/board.c
    ${TIVAWARE_DIR}/driverlib/gpio.c
    ${TIVAWARE_DIR}/driverlib/sysctl.c)
set_target_properties(application PROPERTIES SUFFIX ".elf")
target_compile_definitions(application PRIVATE
    PART_TM4C123GH6PM TARGET_IS_TM4C123_RA1)
target_include_directories(application PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/application
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${TIVAWARE_DIR} ${TIVAWARE_DIR}/driverlib ${TIVAWARE_DIR}/inc)
target_compile_options(application PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp
    -ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion -fno-common)
target_link_options(application PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/application.ld
    -Wl,--gc-sections -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/application.map
    --specs=nano.specs --specs=nosys.specs)
add_custom_command(TARGET application POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:application>
        ${CMAKE_CURRENT_BINARY_DIR}/application.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:application>
    COMMENT "Generating application binary")

set(BL_IMAGE_HEADER ${CMAKE_CURRENT_BINARY_DIR}/image_header.bin)
set(BL_APPLICATION_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/application_image.bin)
set(BL_APPLICATION_IMAGE_HEX ${CMAKE_CURRENT_BINARY_DIR}/application_image.hex)
add_custom_command(OUTPUT ${BL_IMAGE_HEADER} ${BL_APPLICATION_IMAGE}
        ${BL_APPLICATION_IMAGE_HEX}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_image.py
        --input ${CMAKE_CURRENT_BINARY_DIR}/application.bin
        --header ${BL_IMAGE_HEADER}
        --output ${BL_APPLICATION_IMAGE}
    COMMAND ${CMAKE_OBJCOPY} -I binary -O ihex --change-addresses 0x00008000
        ${BL_APPLICATION_IMAGE} ${BL_APPLICATION_IMAGE_HEX}
    DEPENDS application ${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_image.py
    VERBATIM
    COMMENT "Packaging application header and image")
add_custom_target(application_image ALL
    DEPENDS ${BL_IMAGE_HEADER} ${BL_APPLICATION_IMAGE} ${BL_APPLICATION_IMAGE_HEX})

set(BL_FULL_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/tm4c123_full_image.bin)
set(BL_FULL_IMAGE_HEX ${CMAKE_CURRENT_BINARY_DIR}/tm4c123_full_image.hex)
add_custom_command(OUTPUT ${BL_FULL_IMAGE} ${BL_FULL_IMAGE_HEX}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_full_image.py
        --bootloader ${CMAKE_CURRENT_BINARY_DIR}/bootloader.bin
        --application ${BL_APPLICATION_IMAGE}
        --output ${BL_FULL_IMAGE}
    COMMAND ${CMAKE_OBJCOPY} -I binary -O ihex --change-addresses 0
        ${BL_FULL_IMAGE} ${BL_FULL_IMAGE_HEX}
    DEPENDS bootloader application_image ${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_full_image.py
    VERBATIM
    COMMENT "Packaging full flash image")
add_custom_target(full_image ALL
    DEPENDS ${BL_FULL_IMAGE} ${BL_FULL_IMAGE_HEX})

add_custom_command(TARGET bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader>
        ${CMAKE_CURRENT_BINARY_DIR}/bootloader.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:bootloader>
        ${CMAKE_CURRENT_BINARY_DIR}/bootloader.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:bootloader>
    COMMENT "Generating bootloader binary, HEX, and size information")

message(STATUS "TM4C123 bootloader transport: ${BL_TRANSPORT_UPPER}")
message(STATUS "TivaWare: ${TIVAWARE_DIR}")
