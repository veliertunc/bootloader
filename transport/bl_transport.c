#include "bl_transport.h"

#include <stddef.h>

#if BL_TRANSPORT == BL_TRANSPORT_ID_RS232 || BL_TRANSPORT == BL_TRANSPORT_ID_RS485
#include "bl_uart.h"
#define BL_SELECTED_TYPE ((BL_TransportType_t)BL_TRANSPORT)
#define BL_SELECTED_INIT BL_UART_Init
#define BL_SELECTED_DEINIT BL_UART_DeInit
#define BL_SELECTED_RECEIVE BL_UART_Receive
#define BL_SELECTED_TRANSMIT BL_UART_Transmit
#define BL_SELECTED_ACTIVITY BL_UART_Activity
#elif BL_TRANSPORT == BL_TRANSPORT_ID_SPI
#include "bl_spi.h"
#define BL_SELECTED_TYPE BL_TRANSPORT_SPI
#define BL_SELECTED_INIT BL_SPI_Init
#define BL_SELECTED_DEINIT BL_SPI_DeInit
#define BL_SELECTED_RECEIVE BL_SPI_Receive
#define BL_SELECTED_TRANSMIT BL_SPI_Transmit
#define BL_SELECTED_ACTIVITY BL_SPI_Activity
#elif BL_TRANSPORT == BL_TRANSPORT_ID_I2C
#include "bl_i2c.h"
#define BL_SELECTED_TYPE BL_TRANSPORT_I2C
#define BL_SELECTED_INIT BL_I2C_Init
#define BL_SELECTED_DEINIT BL_I2C_DeInit
#define BL_SELECTED_RECEIVE BL_I2C_Receive
#define BL_SELECTED_TRANSMIT BL_I2C_Transmit
#define BL_SELECTED_ACTIVITY BL_I2C_Activity
#elif BL_TRANSPORT == BL_TRANSPORT_ID_ETHERNET
#include "bl_ethernet.h"
#define BL_SELECTED_TYPE BL_TRANSPORT_ETHERNET
#define BL_SELECTED_INIT BL_Ethernet_Init
#define BL_SELECTED_DEINIT BL_Ethernet_DeInit
#define BL_SELECTED_RECEIVE BL_Ethernet_Receive
#define BL_SELECTED_TRANSMIT BL_Ethernet_Transmit
#define BL_SELECTED_ACTIVITY BL_Ethernet_Activity
#endif

static bool g_initialized;

bool BL_TransportInit(BL_TransportType_t type)
{
    if (g_initialized)
    {
        BL_TransportDeInit();
    }
    if (type != BL_SELECTED_TYPE || !BL_SELECTED_INIT())
    {
        return false;
    }
    g_initialized = true;
    return true;
}

void BL_TransportDeInit(void)
{
    if (g_initialized)
    {
        BL_SELECTED_DEINIT();
        g_initialized = false;
    }
}

bool BL_TransportReceive(uint8_t *buffer, uint32_t bufferSize,
                         uint32_t *receivedLength)
{
    if (!g_initialized || buffer == NULL || receivedLength == NULL)
    {
        return false;
    }
    return BL_SELECTED_RECEIVE(buffer, bufferSize, receivedLength);
}

bool BL_TransportTransmit(const uint8_t *buffer, uint32_t length)
{
    return g_initialized && buffer != NULL &&
           BL_SELECTED_TRANSMIT(buffer, length);
}

bool BL_TransportActivity(void)
{
    return g_initialized && BL_SELECTED_ACTIVITY();
}

BL_TransportType_t BL_TransportGetType(void)
{
    return BL_SELECTED_TYPE;
}

bool BL_TransportIsInitialized(void)
{
    return g_initialized;
}
