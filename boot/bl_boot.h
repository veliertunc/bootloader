#ifndef BL_BOOT_H
#define BL_BOOT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Initializes all bootloader subsystems.
 *
 * Initialization order:
 *
 *     Board
 *       ↓
 *     Time
 *       ↓
 *     Transport
 *       ↓
 *     Protocol
 */
bool BL_BootInit(void);

/*
 * Main bootloader execution loop.
 *
 * This function does not return under normal operation.
 */
void BL_Boot(void);

#ifdef __cplusplus
}
#endif

#endif /* BL_BOOT_H */