#ifndef BL_SPI_H
#define BL_SPI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BL_SPI_RX_BUFFER_SIZE        (512UL)
#define BL_SPI_TX_BUFFER_SIZE        (512UL)

#define BL_SPI_CLOCK_HZ              (4000000UL)
#define BL_SPI_TX_TIMEOUT_MS         (1000UL)

bool BL_SPI_Init(void);

void BL_SPI_DeInit(void);

bool BL_SPI_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength);

bool BL_SPI_Transmit(
    const uint8_t *buffer,
    uint32_t length);

bool BL_SPI_Activity(void);

bool BL_SPI_IsInitialized(void);

bool BL_SPI_HasError(void);

uint32_t BL_SPI_GetRxCount(void);

uint32_t BL_SPI_GetTxCount(void);

#ifdef __cplusplus
}
#endif

#endif /* BL_SPI_H */