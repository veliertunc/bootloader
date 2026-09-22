ENTRY(ResetISR)

MEMORY
{
    /*
     * Application begins at 0x8400.
     *
     * 0x8000 - 0x801F : image header
     * 0x8020 - 0x83FF : reserved
     */
    FLASH (rx)  : ORIGIN = 0x00008400, LENGTH = 0x00037C00
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00008000
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);

__StackTop   = _estack;
__StackLimit = ORIGIN(SRAM);

SECTIONS
{
    /*
     * Application vector table.
     */
    .isr_vector :
    {
        . = ALIGN(1024);

        KEEP(*(.isr_vector))

        . = ALIGN(4);
    } > FLASH

    /*
     * Application code.
     */
    .text :
    {
        . = ALIGN(4);

        *(.text)
        *(.text.*)

        . = ALIGN(4);
    } > FLASH

    /*
     * Read-only data.
     */
    .rodata :
    {
        . = ALIGN(4);

        *(.rodata)
        *(.rodata.*)

        . = ALIGN(4);
    } > FLASH

    /*
     * ARM exception information.
     */
    .ARM.extab :
    {
        . = ALIGN(4);

        *(.ARM.extab*)

        . = ALIGN(4);
    } > FLASH

    .ARM :
    {
        . = ALIGN(4);

        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;

        . = ALIGN(4);
    } > FLASH

    /*
     * Flash load address of initialized SRAM data.
     */
    __etext = .;

    /*
     * Initialized data.
     */
    .data : AT(__etext)
    {
        . = ALIGN(4);

        __data_start__ = .;

        *(.data)
        *(.data.*)

        . = ALIGN(4);

        __data_end__ = .;
    } > SRAM

    __data_load__ = LOADADDR(.data);

    /*
     * Zero-initialized data.
     */
    .bss :
    {
        . = ALIGN(4);

        __bss_start__ = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4);

        __bss_end__ = .;
    } > SRAM

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
    }
}

ASSERT(ORIGIN(FLASH) == 0x00008400,
       "Application FLASH origin incorrect")

ASSERT(LENGTH(FLASH) == 0x00037C00,
       "Application FLASH size incorrect")

ASSERT((ADDR(.isr_vector) & 0x3FF) == 0,
       "Application vector table is not 1024-byte aligned")

ASSERT((ADDR(.isr_vector) == 0x00008400),
       "Application vector table is not at 0x8400")

ASSERT(__StackTop <= 0x20008000,
       "Application stack exceeds SRAM")

ASSERT(__bss_end__ <= 0x20008000,
       "Application SRAM usage exceeds available SRAM")