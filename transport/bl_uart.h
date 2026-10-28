#ifndef BL_UART_H
#define BL_UART_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize the bootloader UART transport.
 *
 * UART0:
 *   PA0 - U0RX
 *   PA1 - U0TX
 *
 * Configuration:
 *   115200 baud
 *   8 data bits
 *   No parity
 *   1 stop bit
 *
 * @return true if initialization succeeded.
 */
bool BL_UART_Init(void);

/**
 * @brief Deinitialize the UART transport.
 */
void BL_UART_DeInit(void);

/**
 * @brief Receive one complete bootloader packet.
 *
 * This is a blocking polling implementation.
 *
 * The function searches for the packet SOF, reads the packet
 * header, payload and CRC, and returns the serialized packet.
 *
 * @param buffer         Destination buffer.
 * @param bufferSize     Size of destination buffer.
 * @param receivedLength Number of bytes received.
 *
 * @return true when a complete packet has been received.
 */
bool BL_UART_ReceivePacket(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength);

/**
 * @brief Transmit serialized bootloader packet data.
 *
 * @param buffer Data to transmit.
 * @param length Number of bytes.
 *
 * @return true if transmission completed.
 */
bool BL_UART_Transmit(
    const uint8_t *buffer,
    uint32_t length);

bool BL_UART_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength);

/**
 * @brief Check whether UART activity is available.
 *
 * @return true when at least one byte is waiting in the UART RX FIFO.
 */
bool BL_UART_Activity(void);

#ifdef __cplusplus
}
#endif

#endif
