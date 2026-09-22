#ifndef BL_TIME_H
#define BL_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize the bootloader time base.
 *
 * Configures SysTick for a 1 kHz interrupt.
 *
 * @return true if initialization succeeds.
 * @return false if the system clock is invalid.
 */
bool BL_TimeInit(void);

/**
 * @brief Deinitialize the bootloader time base.
 *
 * Disables SysTick and unregisters the interrupt handler.
 */
void BL_TimeDeInit(void);

/**
 * @brief Get the current bootloader tick.
 *
 * The tick increments every 1 ms.
 *
 * @return Current tick count.
 */
uint32_t BL_TimeGetTick(void);

/**
 * @brief Get elapsed time between two tick values.
 *
 * Unsigned subtraction intentionally handles uint32_t wraparound.
 *
 * @param startTick Starting tick.
 * @param endTick Ending tick.
 *
 * @return Elapsed milliseconds.
 */
uint32_t BL_TimeElapsed(
    uint32_t startTick,
    uint32_t endTick);

/**
 * @brief Determine whether a timeout has expired.
 *
 * @param startTick Tick at which the timeout started.
 * @param timeoutMs Timeout in milliseconds.
 *
 * @return true if the timeout has expired.
 */
bool BL_TimeTimeoutExpired(
    uint32_t startTick,
    uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif