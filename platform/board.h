#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*
 * EK-TM4C123GXL
 *
 * LEDs:
 *   PF1 = Red
 *   PF2 = Blue
 *   PF3 = Green
 *
 * Buttons:
 *   PF4 = SW1
 *   PF0 = SW2
 */

#define BOARD_LED_RED       (1U)
#define BOARD_LED_BLUE      (2U)
#define BOARD_LED_GREEN     (3U)

#define BOARD_SW1           (1U)
#define BOARD_SW2           (2U)

bool Board_Init(void);
void Board_Reset(void);

uint32_t Board_GetSystemClock(void);

bool Board_Sw1Pressed(void);
bool Board_Sw2Pressed(void);

void Board_LedOn(uint8_t led);
void Board_LedOff(uint8_t led);
void Board_LedToggle(uint8_t led);

#ifdef __cplusplus
}
#endif

#endif
