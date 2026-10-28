#include "bl_ethernet.h"

#include <stddef.h>

#include "board.h"
#include "bl_time.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/ssi.h"
#include "driverlib/sysctl.h"
#include "inc/hw_memmap.h"

/* W5500 is connected to SSI0: PA2 SCLK, PA4 MISO, PA5 MOSI. */
#define ETH_SSI_BASE       SSI0_BASE
#define ETH_CS_PIN         GPIO_PIN_4
#define ETH_RESET_PIN      GPIO_PIN_5
#define ETH_GPIO_BASE      GPIO_PORTB_BASE
#define ETH_SPI_CLOCK_HZ   (8000000UL)
#define ETH_SOCKET         (0U)
#define ETH_SOCKET_BLOCK   (1U)
#define ETH_TX_BLOCK       (2U)
#define ETH_RX_BLOCK       (3U)
#define ETH_RX_MASK        (0x07FFU)
#define ETH_RX_HEADER_SIZE (8U)
#define ETH_UDP_PORT       (5000U)
#define ETH_TIMEOUT        (1000UL)

#define W5_MR              (0x0000U)
#define W5_GAR             (0x0001U)
#define W5_SUBR            (0x0005U)
#define W5_SHAR            (0x0009U)
#define W5_SIPR            (0x000FU)
#define W5_VERSIONR        (0x0039U)
#define W5_SN_MR           (0x0000U)
#define W5_SN_CR           (0x0001U)
#define W5_SN_IR           (0x0002U)
#define W5_SN_SR           (0x0003U)
#define W5_SN_PORT         (0x0004U)
#define W5_SN_DIPR         (0x000CU)
#define W5_SN_DPORT        (0x0010U)
#define W5_SN_TX_FSR       (0x0020U)
#define W5_SN_TX_WR        (0x0024U)
#define W5_SN_RX_RSR       (0x0026U)
#define W5_SN_RX_RD        (0x0028U)
#define W5_SN_RXBUF_SIZE   (0x001EU)
#define W5_SN_TXBUF_SIZE   (0x001FU)

#define W5_CMD_OPEN        (0x01U)
#define W5_CMD_CLOSE       (0x10U)
#define W5_CMD_SEND        (0x20U)
#define W5_CMD_RECV        (0x40U)
#define W5_MODE_UDP        (0x02U)
#define W5_SR_UDP          (0x22U)
#define W5_IR_SEND_OK      (0x10U)
#define W5_IR_TIMEOUT      (0x08U)

/* Edit these defaults to match the local network. No DHCP is used. */
static const uint8_t g_mac[6] = {0x02U, 0x54U, 0x4DU, 0x34U, 0x12U, 0x34U};
static const uint8_t g_gateway[4] = {192U, 168U, 1U, 1U};
static const uint8_t g_subnet[4] = {255U, 255U, 255U, 0U};
static const uint8_t g_ip[4] = {192U, 168U, 1U, 50U};

static uint8_t g_peerIp[4];
static uint16_t g_peerPort;
static bool g_initialized;
static bool g_activity;

static void ETH_Select(void)
{
    GPIOPinWrite(ETH_GPIO_BASE, ETH_CS_PIN, 0U);
}

static void ETH_Deselect(void)
{
    GPIOPinWrite(ETH_GPIO_BASE, ETH_CS_PIN, ETH_CS_PIN);
}

static uint8_t ETH_Exchange(uint8_t value)
{
    uint32_t received;
    SSIDataPut(ETH_SSI_BASE, value);
    while (SSIBusy(ETH_SSI_BASE))
    {
    }
    SSIDataGet(ETH_SSI_BASE, &received);
    return (uint8_t)received;
}

static void ETH_Transfer(uint16_t address, uint8_t block,
                         uint8_t *data, uint16_t length, bool write)
{
    uint16_t i;
    ETH_Select();
    (void)ETH_Exchange((uint8_t)(address >> 8U));
    (void)ETH_Exchange((uint8_t)address);
    (void)ETH_Exchange((uint8_t)((block << 3U) | (write ? 0x04U : 0U)));
    for (i = 0U; i < length; i++)
    {
        uint8_t value = write ? data[i] : 0U;
        uint8_t result = ETH_Exchange(value);
        if (!write)
        {
            data[i] = result;
        }
    }
    ETH_Deselect();
}

static uint8_t ETH_Read8(uint16_t address, uint8_t block)
{
    uint8_t value = 0U;
    ETH_Transfer(address, block, &value, 1U, false);
    return value;
}

static void ETH_Write8(uint16_t address, uint8_t block, uint8_t value)
{
    ETH_Transfer(address, block, &value, 1U, true);
}

static uint16_t ETH_Read16(uint16_t address, uint8_t block)
{
    uint8_t data[2];
    ETH_Transfer(address, block, data, sizeof(data), false);
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static void ETH_Write16(uint16_t address, uint8_t block, uint16_t value)
{
    uint8_t data[2] = {(uint8_t)(value >> 8U), (uint8_t)value};
    ETH_Transfer(address, block, data, sizeof(data), true);
}

static bool ETH_WaitCommand(void)
{
    uint32_t start = BL_TimeGetTick();
    while (ETH_Read8(W5_SN_CR, ETH_SOCKET_BLOCK) != 0U)
    {
        if (BL_TimeTimeoutExpired(start, ETH_TIMEOUT))
        {
            return false;
        }
    }
    return true;
}

static bool ETH_Command(uint8_t command)
{
    ETH_Write8(W5_SN_CR, ETH_SOCKET_BLOCK, command);
    return ETH_WaitCommand();
}

static void ETH_WriteCommon(uint16_t address, const uint8_t *data,
                            uint16_t length)
{
    ETH_Transfer(address, 0U, (uint8_t *)data, length, true);
}

static void ETH_ConfigureNetwork(void)
{
    ETH_WriteCommon(W5_GAR, g_gateway, sizeof(g_gateway));
    ETH_WriteCommon(W5_SUBR, g_subnet, sizeof(g_subnet));
    ETH_WriteCommon(W5_SHAR, g_mac, sizeof(g_mac));
    ETH_WriteCommon(W5_SIPR, g_ip, sizeof(g_ip));
}

bool BL_Ethernet_Init(void)
{
    uint32_t clock = Board_GetSystemClock();
    if (g_initialized)
    {
        return true;
    }
    if (clock == 0U)
    {
        return false;
    }

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA) ||
           !SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB) ||
           !SysCtlPeripheralReady(SYSCTL_PERIPH_SSI0))
    {
    }
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5);
    GPIOPinTypeGPIOOutput(ETH_GPIO_BASE, ETH_CS_PIN | ETH_RESET_PIN);
    ETH_Deselect();
    GPIOPinWrite(ETH_GPIO_BASE, ETH_RESET_PIN, 0U);

    SSIDisable(ETH_SSI_BASE);
    SSIConfigSetExpClk(ETH_SSI_BASE, clock, SSI_FRF_MOTO_MODE_0,
                       SSI_MODE_MASTER, ETH_SPI_CLOCK_HZ, 8U);
    SSIEnable(ETH_SSI_BASE);

    /* Hardware reset, then wait for the module to boot. */
    for (volatile uint32_t delay = 0U; delay < 80000U; delay++)
    {
    }
    GPIOPinWrite(ETH_GPIO_BASE, ETH_RESET_PIN, ETH_RESET_PIN);
    for (volatile uint32_t delay = 0U; delay < 80000U; delay++)
    {
    }
    if (ETH_Read8(W5_VERSIONR, 0U) != 0x04U)
    {
        SSIDisable(ETH_SSI_BASE);
        return false;
    }

    ETH_Write8(W5_MR, 0U, 0x80U);
    for (volatile uint32_t delay = 0U; delay < 80000U; delay++)
    {
    }
    ETH_ConfigureNetwork();
    (void)ETH_Command(W5_CMD_CLOSE);
    ETH_Write8(W5_SN_RXBUF_SIZE, ETH_SOCKET_BLOCK, 2U);
    ETH_Write8(W5_SN_TXBUF_SIZE, ETH_SOCKET_BLOCK, 2U);
    ETH_Write8(W5_SN_MR, ETH_SOCKET_BLOCK, W5_MODE_UDP);
    ETH_Write16(W5_SN_PORT, ETH_SOCKET_BLOCK, ETH_UDP_PORT);
    if (!ETH_Command(W5_CMD_OPEN) ||
        (ETH_Read8(W5_SN_SR, ETH_SOCKET_BLOCK) != W5_SR_UDP))
    {
        SSIDisable(ETH_SSI_BASE);
        return false;
    }
    g_peerPort = 0U;
    g_activity = false;
    g_initialized = true;
    return true;
}

void BL_Ethernet_DeInit(void)
{
    if (g_initialized)
    {
        (void)ETH_Command(W5_CMD_CLOSE);
        SSIDisable(ETH_SSI_BASE);
        g_initialized = false;
    }
}

bool BL_Ethernet_Receive(uint8_t *buffer, uint32_t bufferSize,
                         uint32_t *receivedLength)
{
    uint16_t available;
    uint16_t payloadLength;
    uint16_t readPointer;
    uint8_t header[ETH_RX_HEADER_SIZE];
    uint32_t i;
    if ((buffer == NULL) || (receivedLength == NULL) || !g_initialized)
    {
        return false;
    }
    *receivedLength = 0U;
    available = ETH_Read16(W5_SN_RX_RSR, ETH_SOCKET_BLOCK);
    if (available < ETH_RX_HEADER_SIZE)
    {
        return true;
    }
    readPointer = ETH_Read16(W5_SN_RX_RD, ETH_SOCKET_BLOCK);
    ETH_Transfer((uint16_t)(readPointer & ETH_RX_MASK), ETH_RX_BLOCK,
                 header, sizeof(header), false);
    payloadLength = (uint16_t)(((uint16_t)header[6] << 8U) | header[7]);
    if ((uint32_t)payloadLength + ETH_RX_HEADER_SIZE > available)
    {
        return true;
    }
    for (i = 0U; i < 4U; i++)
    {
        g_peerIp[i] = header[i];
    }
    g_peerPort = (uint16_t)(((uint16_t)header[4] << 8U) | header[5]);
    if ((payloadLength <= bufferSize) && (payloadLength != 0U))
    {
        uint16_t dataPointer = (uint16_t)((readPointer + ETH_RX_HEADER_SIZE) & ETH_RX_MASK);
        ETH_Transfer(dataPointer, ETH_RX_BLOCK, buffer, payloadLength, false);
        *receivedLength = payloadLength;
    }
    readPointer = (uint16_t)(readPointer + ETH_RX_HEADER_SIZE + payloadLength);
    ETH_Write16(W5_SN_RX_RD, ETH_SOCKET_BLOCK, readPointer);
    if (!ETH_Command(W5_CMD_RECV))
    {
        return false;
    }
    g_activity = true;
    return true;
}

bool BL_Ethernet_Transmit(const uint8_t *buffer, uint32_t length)
{
    uint16_t writePointer;
    uint32_t start;
    uint8_t status;
    if ((buffer == NULL) || (length == 0U) || (length > 1400U) ||
        !g_initialized || (g_peerPort == 0U))
    {
        return false;
    }
    for (uint32_t i = 0U; i < 4U; i++)
    {
        ETH_Write8((uint16_t)(W5_SN_DIPR + i), ETH_SOCKET_BLOCK, g_peerIp[i]);
    }
    ETH_Write16(W5_SN_DPORT, ETH_SOCKET_BLOCK, g_peerPort);
    start = BL_TimeGetTick();
    while (ETH_Read16(W5_SN_TX_FSR, ETH_SOCKET_BLOCK) < length)
    {
        if (BL_TimeTimeoutExpired(start, ETH_TIMEOUT))
        {
            return false;
        }
    }
    writePointer = ETH_Read16(W5_SN_TX_WR, ETH_SOCKET_BLOCK);
    ETH_Transfer((uint16_t)(writePointer & ETH_RX_MASK), ETH_TX_BLOCK,
                 (uint8_t *)buffer, (uint16_t)length, true);
    ETH_Write16(W5_SN_TX_WR, ETH_SOCKET_BLOCK,
                (uint16_t)(writePointer + (uint16_t)length));
    ETH_Write8(W5_SN_IR, ETH_SOCKET_BLOCK, W5_IR_SEND_OK | W5_IR_TIMEOUT);
    if (!ETH_Command(W5_CMD_SEND))
    {
        return false;
    }
    start = BL_TimeGetTick();
    do
    {
        status = ETH_Read8(W5_SN_IR, ETH_SOCKET_BLOCK);
        if ((status & W5_IR_TIMEOUT) != 0U)
        {
            ETH_Write8(W5_SN_IR, ETH_SOCKET_BLOCK, W5_IR_TIMEOUT);
            return false;
        }
        if ((status & W5_IR_SEND_OK) != 0U)
        {
            ETH_Write8(W5_SN_IR, ETH_SOCKET_BLOCK, W5_IR_SEND_OK);
            g_activity = true;
            return true;
        }
    } while (!BL_TimeTimeoutExpired(start, ETH_TIMEOUT));
    return false;
}

bool BL_Ethernet_Activity(void)
{
    bool active = g_activity;
    g_activity = false;
    return active;
}
