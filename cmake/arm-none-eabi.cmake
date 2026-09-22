set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(ARM_GNU_PREFIX arm-none-eabi)

find_program(
    ARM_GCC
    ${ARM_GNU_PREFIX}-gcc
    REQUIRED
)

find_program(
    ARM_GXX
    ${ARM_GNU_PREFIX}-g++
    REQUIRED
)

find_program(
    ARM_OBJCOPY
    ${ARM_GNU_PREFIX}-objcopy
    REQUIRED
)

find_program(
    ARM_SIZE
    ${ARM_GNU_PREFIX}-size
    REQUIRED
)

set(CMAKE_C_COMPILER "${ARM_GCC}")
set(CMAKE_CXX_COMPILER "${ARM_GXX}")
set(CMAKE_OBJCOPY "${ARM_OBJCOPY}")
set(CMAKE_SIZE "${ARM_SIZE}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCPU_FLAGS
    "-mcpu=cortex-m4"
    "-mthumb"
    "-mfpu=fpv4-sp-d16"
    "-mfloat-abi=softfp"
)

string(JOIN " " MCPU_FLAGS_STRING ${MCPU_FLAGS})

set(CMAKE_C_FLAGS_INIT
    "${MCPU_FLAGS_STRING}"
)

set(CMAKE_CXX_FLAGS_INIT
    "${MCPU_FLAGS_STRING}"
)

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${MCPU_FLAGS_STRING}"
)