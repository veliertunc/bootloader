#ifndef BL_TRANSPORT_H
#define BL_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "bl_transport_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    BL_TRANSPORT_RS232 = BL_TRANSPORT_ID_RS232,
    BL_TRANSPORT_RS485 = BL_TRANSPORT_ID_RS485,
    BL_TRANSPORT_SPI = BL_TRANSPORT_ID_SPI,
    BL_TRANSPORT_I2C = BL_TRANSPORT_ID_I2C,
    BL_TRANSPORT_ETHERNET = BL_TRANSPORT_ID_ETHERNET
} BL_TransportType_t;

/*
 * Generic transport driver interface.
 *
 * All functions operate on raw bytes.
 *
 * Packet framing belongs above this layer.
 */
typedef struct
{
    bool (*Init)(void);

    void (*DeInit)(void);

    bool (*Receive)(
        uint8_t *buffer,
        uint32_t bufferSize,
        uint32_t *receivedLength);

    bool (*Transmit)(
        const uint8_t *buffer,
        uint32_t length);

    bool (*Activity)(void);

} BL_TransportDriver_t;

/*
 * Initializes the selected transport.
 */
bool BL_TransportInit(BL_TransportType_t type);

/*
 * Deinitializes the currently selected transport.
 */
void BL_TransportDeInit(void);

/*
 * Receives raw bytes from the selected transport.
 */
bool BL_TransportReceive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength);

/*
 * Transmits raw bytes using the selected transport.
 */
bool BL_TransportTransmit(
    const uint8_t *buffer,
    uint32_t length);

/*
 * Returns and clears transport activity state.
 */
bool BL_TransportActivity(void);

/*
 * Returns the currently selected transport.
 */
BL_TransportType_t BL_TransportGetType(void);

/*
 * Returns true when the transport has been initialized.
 */
bool BL_TransportIsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* BL_TRANSPORT_H */
