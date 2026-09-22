#ifndef BL_CAN_H
#define BL_CAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * CAN0 configuration
 *
 * TM4C123:
 *   PE4 = CAN0RX
 *   PE5 = CAN0TX
 */
#define BL_CAN_HOST_TO_TARGET_ID       (0x500UL)
#define BL_CAN_TARGET_TO_HOST_ID       (0x501UL)

#define BL_CAN_BITRATE                 (500000UL)

#define BL_CAN_RX_OBJECT               (1U)
#define BL_CAN_TX_OBJECT               (2U)

#define BL_CAN_FRAME_SIZE              (8U)

#define BL_CAN_RX_BUFFER_SIZE          (1024U)
#define BL_CAN_TX_BUFFER_SIZE          (1024U)

#define BL_CAN_TX_TIMEOUT_MS           (1000UL)

bool BL_CAN_Init(void);

void BL_CAN_DeInit(void);

bool BL_CAN_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength);

bool BL_CAN_Transmit(
    const uint8_t *buffer,
    uint32_t length);

bool BL_CAN_Activity(void);

bool BL_CAN_IsInitialized(void);

bool BL_CAN_IsBusError(void);

#ifdef __cplusplus
}
#endif

#endif