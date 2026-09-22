#ifndef BL_USB_H
#define BL_USB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * USB CDC transport.
 *
 * CDC exposes the bootloader as a virtual serial port.
 */

#define BL_USB_RX_BUFFER_SIZE       (512UL)
#define BL_USB_TX_BUFFER_SIZE       (512UL)

#define BL_USB_MAX_PACKET_SIZE      (250UL)

/*
 * Development identifiers.
 *
 * These values follow the TI CDC example identity.
 *
 * IMPORTANT:
 * A production device should use a VID/PID legitimately assigned
 * to the product/vendor. Do not ship a product using a borrowed
 * development VID/PID.
 */
#define BL_USB_VENDOR_ID            (0x1CBEU)
#define BL_USB_PRODUCT_ID           (0x00FFU)

#define BL_USB_MAX_POWER_MA         (100U)

bool BL_USB_Init(void);

void BL_USB_DeInit(void);

bool BL_USB_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength);

bool BL_USB_Transmit(
    const uint8_t *buffer,
    uint32_t length);

bool BL_USB_Activity(void);

bool BL_USB_IsInitialized(void);

bool BL_USB_IsConnected(void);

bool BL_USB_HasError(void);

uint32_t BL_USB_GetRxCount(void);

uint32_t BL_USB_GetTxCount(void);

#ifdef __cplusplus
}
#endif

#endif /* BL_USB_H */