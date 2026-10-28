#include <stdint.h>
#include <stdbool.h>

#include "board.h"

#include "inc/hw_gpio.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"

#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"

#define BOARD_LED_RED_PIN      GPIO_PIN_1
#define BOARD_LED_BLUE_PIN     GPIO_PIN_2
#define BOARD_LED_GREEN_PIN    GPIO_PIN_3

#define BOARD_LED_ALL_PINS     \
    (BOARD_LED_RED_PIN |       \
     BOARD_LED_BLUE_PIN |      \
     BOARD_LED_GREEN_PIN)

#define BOARD_SW1_PIN          GPIO_PIN_4
#define BOARD_SW2_PIN          GPIO_PIN_0

static uint32_t g_systemClockHz = 0UL;

static uint8_t Board_LedToPin(uint8_t led)
{
    switch (led)
    {
        case BOARD_LED_RED:
            return BOARD_LED_RED_PIN;

        case BOARD_LED_BLUE:
            return BOARD_LED_BLUE_PIN;

        case BOARD_LED_GREEN:
            return BOARD_LED_GREEN_PIN;

        default:
            return 0U;
    }
}

bool Board_Init(void)
{
    /*
     * Configure 80 MHz system clock.
     *
     * EK-TM4C123GXL uses a 16 MHz crystal.
     */
    g_systemClockHz =
        SysCtlClockFreqSet(
            SYSCTL_XTAL_16MHZ |
            SYSCTL_OSC_MAIN |
            SYSCTL_USE_PLL |
            SYSCTL_CFG_VCO_480,
            80000000UL);

    if (g_systemClockHz != 80000000UL)
    {
        return false;
    }

    /*
     * Enable GPIOA and GPIOF.
     */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA))
    {
    }

    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
    {
    }

    /*
     * PF0 is locked because it is NMI by default.
     *
     * Unlock PF0 before using it as SW2.
     */
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;

    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= GPIO_PIN_0;

    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0UL;

    /*
     * LEDs.
     */
    GPIOPinTypeGPIOOutput(
        GPIO_PORTF_BASE,
        BOARD_LED_ALL_PINS);

    GPIOPinWrite(
        GPIO_PORTF_BASE,
        BOARD_LED_ALL_PINS,
        0U);

    /*
     * Buttons.
     *
     * Buttons are active-low on the LaunchPad.
     */
    GPIOPinTypeGPIOInput(
        GPIO_PORTF_BASE,
        BOARD_SW1_PIN | BOARD_SW2_PIN);

    GPIOPadConfigSet(
        GPIO_PORTF_BASE,
        BOARD_SW1_PIN | BOARD_SW2_PIN,
        GPIO_STRENGTH_2MA,
        GPIO_PIN_TYPE_STD_WPU);

    return true;
}

uint32_t Board_GetSystemClock(void)
{
    return g_systemClockHz;
}

void Board_Reset(void)
{
    SysCtlReset();
    while (1)
    {
    }
}

bool Board_Sw1Pressed(void)
{
    return GPIOPinRead(
               GPIO_PORTF_BASE,
               BOARD_SW1_PIN) == 0U;
}

bool Board_Sw2Pressed(void)
{
    return GPIOPinRead(
               GPIO_PORTF_BASE,
               BOARD_SW2_PIN) == 0U;
}

void Board_LedOn(uint8_t led)
{
    uint8_t pin = Board_LedToPin(led);

    if (pin != 0U)
    {
        GPIOPinWrite(
            GPIO_PORTF_BASE,
            pin,
            pin);
    }
}

void Board_LedOff(uint8_t led)
{
    uint8_t pin = Board_LedToPin(led);

    if (pin != 0U)
    {
        GPIOPinWrite(
            GPIO_PORTF_BASE,
            pin,
            0U);
    }
}

void Board_LedToggle(uint8_t led)
{
    uint8_t pin = Board_LedToPin(led);

    if (pin != 0U)
    {
        uint32_t current =
            (uint32_t)GPIOPinRead(
                GPIO_PORTF_BASE,
                pin);

        GPIOPinWrite(
            GPIO_PORTF_BASE,
            pin,
            (uint8_t)(current ^ (uint32_t)pin));
    }
}
