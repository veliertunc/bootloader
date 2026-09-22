ENTRY(ResetISR)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00008000
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00008000
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);

__StackTop   = _estack;
__StackLimit = ORIGIN(SRAM);

SECTIONS
{
    /*
     * Vector table
     */
    .isr_vector :
    {
        . = ALIGN(1024);

        KEEP(*(.isr_vector))

        . = ALIGN(4);
    } > FLASH

    /*
     * Program code
     */
    .text :
    {
        . = ALIGN(4);

        *(.text)
        *(.text.*)

        . = ALIGN(4);
    } > FLASH

    /*
     * Read-only data
     */
    .rodata :
    {
        . = ALIGN(4);

        *(.rodata)
        *(.rodata.*)

        . = ALIGN(4);
    } > FLASH

    /*
     * ARM exception tables
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
     * End of read-only Flash content.
     *
     * Used as the load address for .data.
     */
    __etext = .;

    /*
     * Initialized SRAM data.
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

    /*
     * Remove metadata which is not needed in the target.
     */
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
    }
}

/*
 * Safety checks.
 */
ASSERT(ORIGIN(FLASH) == 0x00000000,
       "Bootloader FLASH origin incorrect")

ASSERT(LENGTH(FLASH) == 0x00008000,
       "Bootloader FLASH size incorrect")

ASSERT((ADDR(.isr_vector) & 0x3FF) == 0,
       "Bootloader vector table is not 1024-byte aligned")

ASSERT(__StackTop <= 0x20008000,
       "Bootloader stack exceeds SRAM")

ASSERT(__bss_end__ <= 0x20008000,
       "Bootloader SRAM usage exceeds available SRAM")