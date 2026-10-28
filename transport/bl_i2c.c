#include "bl_i2c.h"

#include <stddef.h>

#include "bl_fifo.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#define BL_I2C_RX_SIZE (BL_I2C_RX_BUFFER_SIZE)
#define BL_I2C_TX_SIZE (BL_I2C_TX_BUFFER_SIZE)
#define BL_I2C_IRQS (I2C_SLAVE_INT_DATA | I2C_SLAVE_INT_START | I2C_SLAVE_INT_STOP)

static uint8_t g_rxStorage[BL_I2C_RX_SIZE];
static uint8_t g_txStorage[BL_I2C_TX_SIZE];
static BL_FIFO_t g_rxFifo;
static BL_FIFO_t g_txFifo;
static volatile bool g_initialized;
static volatile bool g_activity;
static volatile bool g_error;
static volatile uint32_t g_rxCount;
static volatile uint32_t g_txCount;

static void BL_I2C_InterruptHandler(void)
{
    uint32_t irq = I2CSlaveIntStatus(I2C0_BASE, true);
    uint32_t status = I2CSlaveStatus(I2C0_BASE);

    if (irq != 0U)
    {
        I2CSlaveIntClearEx(I2C0_BASE, irq);
    }

    if ((status == I2C_SLAVE_ACT_RREQ) ||
        (status == I2C_SLAVE_ACT_RREQ_FBR))
    {
        if (BL_FIFO_Push(&g_rxFifo,
                         (uint8_t)I2CSlaveDataGet(I2C0_BASE)))
        {
            g_rxCount++;
        }
        else
        {
            g_error = true;
        }
        g_activity = true;
    }
    else if (status == I2C_SLAVE_ACT_TREQ)
    {
        uint8_t value = 0xFFU;
        if (BL_FIFO_Pop(&g_txFifo, &value))
        {
            g_txCount++;
        }
        I2CSlaveDataPut(I2C0_BASE, value);
        g_activity = true;
    }

    if ((irq & (I2C_SLAVE_INT_START | I2C_SLAVE_INT_STOP)) != 0U)
    {
        g_activity = true;
    }
}

bool BL_I2C_Init(void)
{
    if (g_initialized)
    {
        return true;
    }

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB) ||
           !SysCtlPeripheralReady(SYSCTL_PERIPH_I2C0))
    {
    }

    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    BL_FIFO_Init(&g_rxFifo, g_rxStorage, sizeof(g_rxStorage));
    BL_FIFO_Init(&g_txFifo, g_txStorage, sizeof(g_txStorage));
    g_activity = false;
    g_error = false;
    g_rxCount = 0U;
    g_txCount = 0U;

    I2CSlaveInit(I2C0_BASE, BL_I2C_SLAVE_ADDRESS);
    I2CSlaveIntEnableEx(I2C0_BASE, BL_I2C_IRQS);
    I2CIntRegister(I2C0_BASE, BL_I2C_InterruptHandler);
    I2CSlaveEnable(I2C0_BASE);
    IntEnable(INT_I2C0);
    g_initialized = true;
    return true;
}

void BL_I2C_DeInit(void)
{
    if (!g_initialized)
    {
        return;
    }
    IntDisable(INT_I2C0);
    I2CSlaveIntDisableEx(I2C0_BASE, BL_I2C_IRQS);
    I2CIntUnregister(I2C0_BASE);
    I2CSlaveDisable(I2C0_BASE);
    BL_FIFO_Reset(&g_rxFifo);
    BL_FIFO_Reset(&g_txFifo);
    g_initialized = false;
    g_activity = false;
}

bool BL_I2C_Receive(uint8_t *buffer, uint32_t bufferSize,
                    uint32_t *receivedLength)
{
    uint32_t count = 0U;
    if ((buffer == NULL) || (receivedLength == NULL) ||
        (bufferSize == 0U) || !g_initialized)
    {
        return false;
    }
    while ((count < bufferSize) && BL_FIFO_Pop(&g_rxFifo, &buffer[count]))
    {
        count++;
    }
    *receivedLength = count;
    return true;
}

bool BL_I2C_Transmit(const uint8_t *buffer, uint32_t length)
{
    uint32_t i;
    if ((buffer == NULL) || (length == 0U) || !g_initialized ||
        (length > BL_FIFO_Free(&g_txFifo)))
    {
        return false;
    }
    for (i = 0U; i < length; i++)
    {
        if (!BL_FIFO_Push(&g_txFifo, buffer[i]))
        {
            g_error = true;
            return false;
        }
    }
    return true;
}

bool BL_I2C_Activity(void)
{
    bool activity = g_activity;
    g_activity = false;
    return activity;
}

bool BL_I2C_IsInitialized(void) { return g_initialized; }
bool BL_I2C_HasError(void) { return g_error; }
uint32_t BL_I2C_GetRxCount(void) { return g_rxCount; }
uint32_t BL_I2C_GetTxCount(void) { return g_txCount; }
