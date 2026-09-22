#include "bl_i2c.h"

#include "bl_packet.h"
#include "bl_fifo.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"

#include "inc/hw_i2c.h"
#include "inc/hw_ints.h"

#define BL_I2C_REG_STATUS     0x00
#define BL_I2C_REG_LENGTH_L   0x01
#define BL_I2C_REG_LENGTH_H   0x02
#define BL_I2C_REG_DATA       0x03

static volatile bool g_i2cInitialized;
static volatile bool g_i2cActivity;

static uint8_t g_rxStorage[1024];
static uint8_t g_txStorage[1024];

static BL_FIFO_t g_rxFifo;
static BL_FIFO_t g_txFifo;

static void BL_I2C_HandleReceive(void)
{
    uint8_t byte;

    byte =
        (uint8_t)
        I2CSlaveDataGet(
            I2C0_BASE);

    BL_SPIFIFO_Push(
        &g_rxFifo,
        byte);

    g_i2cActivity = true;
}

static void BL_I2C_HandleTransmit(void)
{
    uint8_t byte;

    if (!BL_SPIFIFO_Pop(
            &g_txFifo,
            &byte))
    {
        byte = 0xFFU;
    }

    I2CSlaveDataPut(
        I2C0_BASE,
        byte);

    g_i2cActivity = true;
}

void I2C0_IRQHandler(void)
{
    uint32_t status;

    status =
        I2CSlaveStatus(
            I2C0_BASE);

    I2CSlaveIntClear(
        I2C0_BASE);

    if (status &
        I2C_SLAVE_ACT_RREQ)
    {
        BL_I2C_HandleTransmit();
    }

    if (status &
        I2C_SLAVE_ACT_TREQ)
    {
        BL_I2C_HandleReceive();
    }
}

bool BL_I2C_Init(void)
{
    SysCtlPeripheralEnable(
        SYSCTL_PERIPH_GPIOB);

    SysCtlPeripheralEnable(
        SYSCTL_PERIPH_I2C0);

    while (!SysCtlPeripheralReady(
                SYSCTL_PERIPH_I2C0))
    {
    }

    GPIOPinConfigure(
        GPIO_PB2_I2C0SCL);

    GPIOPinConfigure(
        GPIO_PB3_I2C0SDA);

    GPIOPinTypeI2CSCL(
        GPIO_PORTB_BASE,
        GPIO_PIN_2);

    GPIOPinTypeI2C(
        GPIO_PORTB_BASE,
        GPIO_PIN_3);

    I2CSlaveInit(
        I2C0_BASE,
        BL_I2C_SLAVE_ADDRESS);

    I2CSlaveEnable(
        I2C0_BASE);

    BL_SPIFIFO_Init(
        &g_rxFifo,
        g_rxStorage,
        sizeof(g_rxStorage));

    BL_SPIFIFO_Init(
        &g_txFifo,
        g_txStorage,
        sizeof(g_txStorage));

    I2CSlaveIntEnableEx(
        I2C0_BASE,
        I2C_SLAVE_INT_DATA);

    IntEnable(INT_I2C0);

    g_i2cInitialized = true;

    return true;
}

bool BL_I2C_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength)
{
    uint32_t count = 0U;

    while (count < bufferSize)
    {
        if (!BL_SPIFIFO_Pop(
                &g_rxFifo,
                &buffer[count]))
        {
            break;
        }

        count++;
    }

    *receivedLength = count;

    return count > 0U;
}

bool BL_I2C_Transmit(
    const uint8_t *buffer,
    uint32_t length)
{
    uint32_t i;

    for (i = 0U; i < length; i++)
    {
        if (!BL_SPIFIFO_Push(
                &g_txFifo,
                buffer[i]))
        {
            return false;
        }
    }

    return true;
}

bool BL_I2C_Activity(void)
{
    bool activity;

    activity = g_i2cActivity;

    g_i2cActivity = false;

    return activity;
}

