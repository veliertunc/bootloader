#include <stdint.h>

extern int main(void);

extern uint32_t __StackTop;
extern uint32_t __data_load__;
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;

void ResetISR(void);

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

static void DefaultISR(void);

#define WEAK_ALIAS_DEFAULT(handler) \
    void handler(void) __attribute__((weak, alias("DefaultISR")))

WEAK_ALIAS_DEFAULT(NMI_Handler);
WEAK_ALIAS_DEFAULT(HardFault_Handler);
WEAK_ALIAS_DEFAULT(MemManage_Handler);
WEAK_ALIAS_DEFAULT(BusFault_Handler);
WEAK_ALIAS_DEFAULT(UsageFault_Handler);
WEAK_ALIAS_DEFAULT(SVC_Handler);
WEAK_ALIAS_DEFAULT(DebugMon_Handler);
WEAK_ALIAS_DEFAULT(PendSV_Handler);
WEAK_ALIAS_DEFAULT(SysTick_Handler);

void ResetISR(void)
{
    uint32_t *src;
    uint32_t *dst;

    src = &__data_load__;
    dst = &__data_start__;

    while (dst < &__data_end__)
    {
        *dst++ = *src++;
    }

    dst = &__bss_start__;

    while (dst < &__bss_end__)
    {
        *dst++ = 0UL;
    }

    (void)main();

    while (1)
    {
    }
}

static void DefaultISR(void)
{
    while (1)
    {
    }
}

void (* const g_pfnVectors[155])(void)
    __attribute__((section(".isr_vector"),
                   used,
                   aligned(1024))) =
{
    [0] = (void (*)(void))(&__StackTop),

    [1] = ResetISR,

    [2] = NMI_Handler,
    [3] = HardFault_Handler,
    [4] = MemManage_Handler,
    [5] = BusFault_Handler,
    [6] = UsageFault_Handler,

    [7] = DefaultISR,
    [8] = DefaultISR,
    [9] = DefaultISR,
    [10] = DefaultISR,

    [11] = SVC_Handler,
    [12] = DebugMon_Handler,

    [13] = DefaultISR,

    [14] = PendSV_Handler,
    [15] = SysTick_Handler,

    [16 ... 154] = DefaultISR
};