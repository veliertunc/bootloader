#include "bl_jump.h"

#include <stdint.h>

#include "bl_image.h"
#include "bl_memory.h"

#define BL_SCB_VTOR_ADDRESS           (0xE000ED08UL)

#define BL_SYST_CSR_ADDRESS           (0xE000E010UL)
#define BL_SYST_RVR_ADDRESS           (0xE000E014UL)
#define BL_SYST_CVR_ADDRESS           (0xE000E018UL)

#define BL_NVIC_ICER_BASE             (0xE000E180UL)
#define BL_NVIC_ICPR_BASE             (0xE000E280UL)

#define BL_NVIC_REGISTER_COUNT        (5UL)

typedef void (*BL_ApplicationEntry_t)(void);

static void BL_SetMainStackPointer(
    uint32_t value)
{
    __asm volatile
    (
        "msr msp, %0"
        :
        : "r"(value)
        : "memory"
    );
}

void BL_JumpToApplication(void)
{
    uint32_t appStackPointer;
    uint32_t appResetHandler;
    uint32_t i;

    BL_ApplicationEntry_t applicationEntry;

    appStackPointer =
        *(volatile uint32_t *)
        BL_APP_VECTOR_ADDRESS;

    appResetHandler =
        *(volatile uint32_t *)
        (BL_APP_VECTOR_ADDRESS + 4UL);

    /*
     * Initial MSP must be inside SRAM.
     */
    if (!BL_MemoryAddressInSram(
            appStackPointer))
    {
        return;
    }

    /*
     * Reset handler must have Thumb bit set.
     */
    if ((appResetHandler & 1UL) == 0UL)
    {
        return;
    }

    appResetHandler &= ~1UL;

    if (!BL_MemoryAddressInApplication(
            appResetHandler))
    {
        return;
    }

    /*
     * Disable interrupts.
     */
    __asm volatile
    (
        "cpsid i"
        ::: "memory"
    );

    /*
     * Stop SysTick.
     */
    *(volatile uint32_t *)
        BL_SYST_CSR_ADDRESS = 0UL;

    *(volatile uint32_t *)
        BL_SYST_RVR_ADDRESS = 0UL;

    *(volatile uint32_t *)
        BL_SYST_CVR_ADDRESS = 0UL;

    /*
     * Disable and clear all implemented NVIC interrupt banks.
     */
    for (i = 0UL;
         i < BL_NVIC_REGISTER_COUNT;
         i++)
    {
        *(volatile uint32_t *)
            (BL_NVIC_ICER_BASE + i * 4UL) =
            0xFFFFFFFFUL;

        *(volatile uint32_t *)
            (BL_NVIC_ICPR_BASE + i * 4UL) =
            0xFFFFFFFFUL;
    }

    /*
     * Relocate vector table.
     *
     * 0x8400 is 1024-byte aligned as required by TM4C123.
     */
    *(volatile uint32_t *)
        BL_SCB_VTOR_ADDRESS =
        BL_APP_VECTOR_ADDRESS;

    __asm volatile
    (
        "dsb"
        ::: "memory"
    );

    __asm volatile
    (
        "isb"
        ::: "memory"
    );

    /*
     * Load application stack pointer.
     */
    BL_SetMainStackPointer(
        appStackPointer);

    /*
     * Branch to application reset handler.
     */
    applicationEntry =
        (BL_ApplicationEntry_t)
        (appResetHandler | 1UL);

    applicationEntry();

    /*
     * Should never return.
     */
    while (1)
    {
    }
}