#ifndef BL_STATE_H
#define BL_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    BL_STATE_IDLE = 0,

    BL_STATE_WAIT_CONNECTION,

    BL_STATE_ERASE,

    BL_STATE_PROGRAM,

    BL_STATE_VERIFY,

    BL_STATE_COMPLETE,

    BL_STATE_ERROR

} BL_State_t;

/*
 * State subsystem initialization.
 */
void BL_StateInit(void);

/*
 * Set current bootloader state.
 */
void BL_StateSet(
    BL_State_t state);

/*
 * Get current bootloader state.
 */
BL_State_t BL_StateGet(void);

/*
 * Convert state to string.
 *
 * Useful for UART debug output,
 * logging and diagnostics.
 */
const char *
BL_StateToString(
    BL_State_t state);

BL_State_t BL_StateGetPrevious(void);

#ifdef __cplusplus
}
#endif

#endif