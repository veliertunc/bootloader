#include "bl_time.h"

#include "bl_config.h"
#include "board.h"

#include <stdbool.h>
#include <stdint.h>

#include "driverlib/interrupt.h"
#include "driverlib/systick.h"

static volatile uint32_t g_BLTimeTick = 0UL;
static bool g_BLTimeInitialized = false;

static void BL_TimeSysTickHandler(void)
{
    g_BLTimeTick++;
}

bool BL_TimeInit(void)
{
    uint32_t systemClockHz;

    systemClockHz = Board_GetSystemClock();

    if (systemClockHz == 0UL)
    {
        return false;
    }

    g_BLTimeTick = 0UL;

    SysTickDisable();

    /*
     * 1 kHz = one interrupt every 1 ms.
     */
    SysTickPeriodSet(systemClockHz / 1000UL);

    SysTickIntRegister(BL_TimeSysTickHandler);
    SysTickIntEnable();
    SysTickEnable();

    g_BLTimeInitialized = true;

    return true;
}

void BL_TimeDeInit(void)
{
    if (!g_BLTimeInitialized)
    {
        return;
    }

    SysTickDisable();
    SysTickIntDisable();
    SysTickIntUnregister();

    g_BLTimeInitialized = false;
}

uint32_t BL_TimeGetTick(void)
{
    return g_BLTimeTick;
}

uint32_t BL_TimeElapsed(
    uint32_t startTick,
    uint32_t endTick)
{
    /*
     * Unsigned arithmetic intentionally handles tick wraparound.
     */
    return endTick - startTick;
}

bool BL_TimeTimeoutExpired(
    uint32_t startTick,
    uint32_t timeoutMs)
{
    return BL_TimeElapsed(
               startTick,
               BL_TimeGetTick()) >= timeoutMs;
}