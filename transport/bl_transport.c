#include "bl_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bl_can.h"
#include "bl_i2c.h"
#include "bl_spi.h"
#include "bl_uart.h"
#include "bl_usb.h"

/*
 * --------------------------------------------------------------------------
 * Transport driver declarations
 * --------------------------------------------------------------------------
 *
 * The transport-specific modules all expose the same logical interface.
 */

static const BL_TransportDriver_t g_transportDrivers[] =
{
    [BL_TRANSPORT_UART] =
    {
        .Init = BL_UART_Init,
        .DeInit = BL_UART_DeInit,
        .Receive = BL_UART_Receive,
        .Transmit = BL_UART_Transmit,
        .Activity = BL_UART_Activity
    },

    [BL_TRANSPORT_CAN] =
    {
        .Init = BL_CAN_Init,
        .DeInit = BL_CAN_DeInit,
        .Receive = BL_CAN_Receive,
        .Transmit = BL_CAN_Transmit,
        .Activity = BL_CAN_Activity
    },

    [BL_TRANSPORT_SPI] =
    {
        .Init = BL_SPI_Init,
        .DeInit = BL_SPI_DeInit,
        .Receive = BL_SPI_Receive,
        .Transmit = BL_SPI_Transmit,
        .Activity = BL_SPI_Activity
    },

    [BL_TRANSPORT_I2C] =
    {
        .Init = BL_I2C_Init,
        .DeInit = BL_I2C_DeInit,
        .Receive = BL_I2C_Receive,
        .Transmit = BL_I2C_Transmit,
        .Activity = BL_I2C_Activity
    },

    [BL_TRANSPORT_USB] =
    {
        .Init = BL_USB_Init,
        .DeInit = BL_USB_DeInit,
        .Receive = BL_USB_Receive,
        .Transmit = BL_USB_Transmit,
        .Activity = BL_USB_Activity
    }
};

static const BL_TransportDriver_t g_uartDriver =
{
    .Init = BL_UART_Init,
    .DeInit = BL_UART_DeInit,
    .Receive = BL_UART_Receive,
    .Transmit = BL_UART_Transmit,
    .Activity = BL_UART_Activity
};

static const BL_TransportDriver_t g_canDriver =
{
    .Init = BL_CAN_Init,
    .DeInit = BL_CAN_DeInit,
    .Receive = BL_CAN_Receive,
    .Transmit = BL_CAN_Transmit,
    .Activity = BL_CAN_Activity
};

static const BL_TransportDriver_t g_spiDriver =
{
    .Init = BL_SPI_Init,
    .DeInit = BL_SPI_DeInit,
    .Receive = BL_SPI_Receive,
    .Transmit = BL_SPI_Transmit,
    .Activity = BL_SPI_Activity
};

static const BL_TransportDriver_t g_i2cDriver =
{
    .Init = BL_I2C_Init,
    .DeInit = BL_I2C_DeInit,
    .Receive = BL_I2C_Receive,
    .Transmit = BL_I2C_Transmit,
    .Activity = BL_I2C_Activity
};

static const BL_TransportDriver_t g_usbDriver =
{
    .Init = BL_USB_Init,
    .DeInit = BL_USB_DeInit,
    .Receive = BL_USB_Receive,
    .Transmit = BL_USB_Transmit,
    .Activity = BL_USB_Activity
};

/*
 * --------------------------------------------------------------------------
 * Private state
 * --------------------------------------------------------------------------
 */

static const BL_TransportDriver_t *g_transportDriver = NULL;

static BL_TransportType_t g_transportType =
    BL_TRANSPORT_UART;

static bool g_transportInitialized = false;

/*
 * --------------------------------------------------------------------------
 * Private functions
 * --------------------------------------------------------------------------
 */

static const BL_TransportDriver_t *
BL_TransportGetDriver(
    BL_TransportType_t type)
{
    switch (type)
    {
        case BL_TRANSPORT_UART:
            return &g_uartDriver;

        case BL_TRANSPORT_CAN:
            return &g_canDriver;

        case BL_TRANSPORT_SPI:
            return &g_spiDriver;

        case BL_TRANSPORT_I2C:
            return &g_i2cDriver;

        case BL_TRANSPORT_USB:
            return &g_usbDriver;

        default:
            return NULL;
    }
}

/*
 * --------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------
 */

bool BL_TransportInit(BL_TransportType_t type)
{
    const BL_TransportDriver_t *driver;

    /*
     * If a transport is already active, remove it first.
     */
    if (g_transportInitialized)
    {
        BL_TransportDeInit();
    }

    driver = BL_TransportGetDriver(type);

    if (driver == NULL)
    {
        return false;
    }

    if (driver->Init == NULL)
    {
        return false;
    }

    if (!driver->Init())
    {
        return false;
    }

    g_transportDriver = driver;
    g_transportType = type;
    g_transportInitialized = true;

    return true;
}

void BL_TransportDeInit(void)
{
    if (!g_transportInitialized)
    {
        return;
    }

    if (g_transportDriver != NULL)
    {
        if (g_transportDriver->DeInit != NULL)
        {
            g_transportDriver->DeInit();
        }
    }

    g_transportDriver = NULL;
    g_transportInitialized = false;
}

bool BL_TransportReceive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength)
{
    if (!g_transportInitialized)
    {
        return false;
    }

    if (g_transportDriver == NULL)
    {
        return false;
    }

    if (g_transportDriver->Receive == NULL)
    {
        return false;
    }

    return g_transportDriver->Receive(
        buffer,
        bufferSize,
        receivedLength);
}

bool BL_TransportTransmit(
    const uint8_t *buffer,
    uint32_t length)
{
    if (!g_transportInitialized)
    {
        return false;
    }

    if (g_transportDriver == NULL)
    {
        return false;
    }

    if (g_transportDriver->Transmit == NULL)
    {
        return false;
    }

    return g_transportDriver->Transmit(
        buffer,
        length);
}

bool BL_TransportActivity(void)
{
    if (!g_transportInitialized)
    {
        return false;
    }

    if (g_transportDriver == NULL)
    {
        return false;
    }

    if (g_transportDriver->Activity == NULL)
    {
        return false;
    }

    return g_transportDriver->Activity();
}

BL_TransportType_t BL_TransportGetType(void)
{
    return g_transportType;
}

bool BL_TransportIsInitialized(void)
{
    return g_transportInitialized;
}