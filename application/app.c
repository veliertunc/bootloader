#include <stdint.h>

#include "app.h"
#include "board.h"

void APP_Init(void)
{
    Board_LedOff(1U);
    Board_LedOff(2U);
    Board_LedOff(3U);
}

void APP_Run(void)
{
    while (1)
    {
        Board_LedToggle(1U);

        for (volatile uint32_t delay = 0UL;
             delay < 800000UL;
             ++delay)
        {
        }
    }
}