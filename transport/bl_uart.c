#include "bl_uart.h"

#include "bl_config.h"
#include "bl_packet.h"
#include "bl_transport_config.h"

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"

#include "inc/hw_memmap.h"

#include <stddef.h>
#include <stdint.h>

/*
 * UART0 is connected to the TM4C123 LaunchPad's
 * virtual COM port through the onboard ICDI interface.
 *
 * PA0 = U0RX
 * PA1 = U0TX
 */

#define BL_UART_PERIPHERAL_GPIO      SYSCTL_PERIPH_GPIOA
#define BL_UART_PERIPHERAL_UART      SYSCTL_PERIPH_UART0

#define BL_UART_GPIO_BASE            GPIO_PORTA_BASE
#define BL_UART_BASE                 UART0_BASE

#define BL_UART_RX_PIN               GPIO_PIN_0
#define BL_UART_TX_PIN               GPIO_PIN_1

#define BL_UART_RX_CONFIG            GPIO_PA0_U0RX
#define BL_UART_TX_CONFIG            GPIO_PA1_U0TX

#if BL_TRANSPORT == BL_TRANSPORT_ID_RS485
#define BL_RS485_GPIO_BASE           GPIO_PORTB_BASE
#define BL_RS485_DE_PIN              GPIO_PIN_4
#endif

/*
 * SOF is serialized little-endian.
 *
 * BL_PACKET_SOF = 0xAA55
 *
 * Therefore the byte stream is:
 *
 *     0x55 0xAA
 */
#define BL_UART_SOF_BYTE_0           (0x55U)
#define BL_UART_SOF_BYTE_1           (0xAAU)

/*
 * Packet header:
 *
 *   SOF             2 bytes
 *   command         1 byte
 *   sequence        1 byte
 *   payload length  2 bytes
 *
 * Total:
 *
 *   6 bytes
 */
#define BL_UART_PACKET_HEADER_SIZE   (6UL)

/*
 * Packet CRC is four bytes.
 */
#define BL_UART_PACKET_CRC_SIZE      (4UL)

/*
 * Maximum packet length:
 *
 *   header       6
 *   payload    240
 *   CRC          4
 *
 *   total      250
 */
#define BL_UART_PACKET_MAX_SIZE      \
    (BL_PACKET_MAX_SIZE)

/**
 * @brief Read one byte from UART0.
 *
 * This function blocks until a byte is available.
 */
static uint8_t BL_UART_ReadByte(void)
{
    return (uint8_t)UARTCharGet(
        BL_UART_BASE);
}

/**
 * @brief Read a little-endian uint16_t from the UART stream.
 */
static uint16_t BL_UART_ReadUInt16(void)
{
    uint16_t value;
    uint8_t low;
    uint8_t high;

    low = BL_UART_ReadByte();
    high = BL_UART_ReadByte();

    value =
        (uint16_t)low |
        ((uint16_t)high << 8U);

    return value;
}

/**
 * @brief Read exactly length bytes from UART.
 */
static void BL_UART_ReadBytes(
    uint8_t *buffer,
    uint32_t length)
{
    uint32_t index;

    for (index = 0UL; index < length; ++index)
    {
        buffer[index] =
            BL_UART_ReadByte();
    }
}

bool BL_UART_Init(void)
{
    uint32_t systemClock;

    /*
     * Enable GPIOA.
     */
    SysCtlPeripheralEnable(
        BL_UART_PERIPHERAL_GPIO);

#if BL_TRANSPORT == BL_TRANSPORT_ID_RS485
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB))
    {
    }
    GPIOPinTypeGPIOOutput(BL_RS485_GPIO_BASE, BL_RS485_DE_PIN);
    GPIOPinWrite(BL_RS485_GPIO_BASE, BL_RS485_DE_PIN, 0U);
#endif

    while (!SysCtlPeripheralReady(
        BL_UART_PERIPHERAL_GPIO))
    {
    }

    /*
     * Enable UART0.
     */
    SysCtlPeripheralEnable(
        BL_UART_PERIPHERAL_UART);

    while (!SysCtlPeripheralReady(
        BL_UART_PERIPHERAL_UART))
    {
    }

    /*
     * Configure UART pins.
     */
    GPIOPinConfigure(
        BL_UART_RX_CONFIG);

    GPIOPinConfigure(
        BL_UART_TX_CONFIG);

    GPIOPinTypeUART(
        BL_UART_GPIO_BASE,
        BL_UART_RX_PIN |
        BL_UART_TX_PIN);

    /*
     * Disable UART before configuring it.
     */
    UARTDisable(
        BL_UART_BASE);

    /*
     * Get the actual system clock used by the board.
     *
     * This must match Board_Init().
     */
    systemClock =
        BL_CPU_FREQUENCY_HZ;

    /*
     * Configure:
     *
     *   115200 baud
     *   8 data bits
     *   1 stop bit
     *   no parity
     */
    UARTConfigSetExpClk(
        BL_UART_BASE,
        systemClock,
        BL_UART_BAUDRATE,
        UART_CONFIG_WLEN_8 |
        UART_CONFIG_STOP_ONE |
        UART_CONFIG_PAR_NONE);

    /*
     * Enable FIFOs.
     */
    UARTFIFOEnable(
        BL_UART_BASE);

    /*
     * Enable UART.
     */
    UARTEnable(
        BL_UART_BASE);

    /*
     * Flush any stale data already present in the RX FIFO.
     *
     * This is useful when the board is reset while the host
     * still has bytes in its serial buffer.
     */
    while (UARTCharsAvail(BL_UART_BASE))
    {
        (void)UARTCharGet(
            BL_UART_BASE);
    }

    return true;
}

void BL_UART_DeInit(void)
{
    UARTDisable(
        BL_UART_BASE);
}

bool BL_UART_ReceivePacket(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength)
{
    uint8_t firstByte;
    uint8_t secondByte;
    uint8_t command;
    uint8_t sequence;
    uint16_t payloadLength;
    uint32_t totalLength;

    if (buffer == NULL ||
        receivedLength == NULL)
    {
        return false;
    }

    if (bufferSize < BL_UART_PACKET_HEADER_SIZE)
    {
        return false;
    }

    *receivedLength = 0UL;

    /*
     * Search for the first SOF byte.
     *
     * We deliberately do not assume that the UART stream begins
     * exactly at a packet boundary. This allows recovery after
     * noise, dropped bytes or a host restart.
     */
    do
    {
        firstByte =
            BL_UART_ReadByte();

    } while (firstByte !=
             BL_UART_SOF_BYTE_0);

    /*
     * Verify the second SOF byte.
     *
     * If it is wrong, continue searching rather than accepting
     * a false packet boundary.
     */
    secondByte =
        BL_UART_ReadByte();

    if (secondByte != BL_UART_SOF_BYTE_1)
    {
        /*
         * The second byte might itself be the beginning of
         * another SOF sequence.
         *
         * Rather than attempting complicated overlap handling
         * here, restart packet synchronization.
         */
        return false;
    }

    /*
     * Store SOF.
     */
    buffer[0] = firstByte;
    buffer[1] = secondByte;

    /*
     * Command.
     */
    command =
        BL_UART_ReadByte();

    buffer[2] = command;

    /*
     * Sequence.
     */
    sequence =
        BL_UART_ReadByte();

    buffer[3] = sequence;

    /*
     * Payload length.
     */
    payloadLength =
        BL_UART_ReadUInt16();

    buffer[4] =
        (uint8_t)(payloadLength & 0xFFU);

    buffer[5] =
        (uint8_t)((payloadLength >> 8U) & 0xFFU);

    /*
     * Never allow a malicious or corrupted length field to
     * overflow the receive buffer.
     */
    if (payloadLength >
        BL_PACKET_MAX_PAYLOAD_SIZE)
    {
        return false;
    }

    totalLength =
        BL_UART_PACKET_HEADER_SIZE +
        (uint32_t)payloadLength +
        BL_UART_PACKET_CRC_SIZE;

    if (totalLength > bufferSize)
    {
        return false;
    }

    /*
     * Receive payload.
     */
    if (payloadLength > 0U)
    {
        BL_UART_ReadBytes(
            &buffer[BL_UART_PACKET_HEADER_SIZE],
            payloadLength);
    }

    /*
     * Receive CRC.
     */
    BL_UART_ReadBytes(
        &buffer[
            BL_UART_PACKET_HEADER_SIZE +
            payloadLength],
        BL_UART_PACKET_CRC_SIZE);

    *receivedLength =
        totalLength;

    return true;
}

bool BL_UART_Transmit(
    const uint8_t *buffer,
    uint32_t length)
{
    uint32_t index;

    if (buffer == NULL)
    {
        return false;
    }

    if (length == 0UL)
    {
        return true;
    }

#if BL_TRANSPORT == BL_TRANSPORT_ID_RS485
    GPIOPinWrite(BL_RS485_GPIO_BASE, BL_RS485_DE_PIN, BL_RS485_DE_PIN);
#endif

    for (index = 0UL; index < length; ++index)
    {
        UARTCharPut(
            BL_UART_BASE,
            buffer[index]);
    }

    /*
     * UARTCharPut() waits for FIFO availability, but the final
     * byte may still be physically transmitting.
     *
     * Wait until the UART is no longer busy before returning.
     */
    while (UARTBusy(BL_UART_BASE))
    {
    }

#if BL_TRANSPORT == BL_TRANSPORT_ID_RS485
    GPIOPinWrite(BL_RS485_GPIO_BASE, BL_RS485_DE_PIN, 0U);
#endif

    return true;
}

bool BL_UART_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength)
{
    uint32_t count = 0U;

    if ((buffer == NULL) || (receivedLength == NULL) ||
        (bufferSize == 0U))
    {
        return false;
    }

    *receivedLength = 0U;
    while ((count < bufferSize) && UARTCharsAvail(BL_UART_BASE))
    {
        int32_t value = UARTCharGetNonBlocking(BL_UART_BASE);
        if (value < 0)
        {
            break;
        }
        buffer[count++] = (uint8_t)value;
    }
    *receivedLength = count;
    return true;
}

bool BL_UART_Activity(void)
{
    return UARTCharsAvail(
        BL_UART_BASE);
}
