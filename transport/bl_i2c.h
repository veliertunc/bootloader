#include "bl_i2c.h"

#include <stddef.h>

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"

#include "bl_fifo.h"
#include "board.h"

#define BL_I2C_BASE                 (I2C0_BASE)

#define BL_I2C_GPIO_BASE            (GPIO_PORTB_BASE)

#define BL_I2C_I2C_PERIPH           (SYSCTL_PERIPH_I2C0)
#define BL_I2C_GPIO_PERIPH          (SYSCTL_PERIPH_GPIOB)

#define BL_I2C_GPIO_PINS            \
    (GPIO_PIN_2 | GPIO_PIN_3)

#define BL_I2C_SCL_PIN              (GPIO_PIN_2)
#define BL_I2C_SDA_PIN              (GPIO_PIN_3)

#define BL_I2C_INTERRUPT_FLAGS      \
    (I2C_SLAVE_INT_DATA |          \
     I2C_SLAVE_INT_START |         \
     I2C_SLAVE_INT_STOP)

static uint8_t g_i2cRxStorage[BL_I2C_RX_BUFFER_SIZE];
static uint8_t g_i2cTxStorage[BL_I2C_TX_BUFFER_SIZE];

static BL_FIFO_t g_i2cRxFifo;
static BL_FIFO_t g_i2cTxFifo;

static volatile bool g_i2cInitialized;
static volatile bool g_i2cActivity;
static volatile bool g_i2cError;

static volatile uint32_t g_i2cRxCount;
static volatile uint32_t g_i2cTxCount;

static volatile bool g_i2cReadTransaction;

static void BL_I2CPrepareTransmit(void);

static void BL_I2CInterruptHandler(void)
{
    uint32_t interruptStatus;
    uint32_t slaveStatus;

    /*
     * Clear interrupt sources early, as recommended by
     * TivaWare documentation.
     */
    interruptStatus = I2CSlaveIntStatus(
        BL_I2C_BASE,
        true);

    if (interruptStatus != 0U)
    {
        I2CSlaveIntClearEx(
            BL_I2C_BASE,
            interruptStatus);
    }

    slaveStatus = I2CSlaveStatus(BL_I2C_BASE);

    /*
     * A master is sending bytes to us.
     */
    if ((slaveStatus == I2C_SLAVE_ACT_RREQ) ||
        (slaveStatus == I2C_SLAVE_ACT_RREQ_FBR))
    {
        uint8_t value;

        value = (uint8_t)I2CSlaveDataGet(
            BL_I2C_BASE);

        if (!BL_FIFO_Push(
            &g_i2cRxFifo,
            value))
        {
            g_i2cError = true;
        }
        else
        {
            g_i2cRxCount++;
            g_i2cActivity = true;
        }
    }

    /*
     * A master is requesting bytes from us.
     */
    if (slaveStatus == I2C_SLAVE_ACT_TREQ)
    {
        BL_I2CPrepareTransmit();

        g_i2cActivity = true;
        g_i2cReadTransaction = true;
    }

    /*
     * START marks the beginning of a new I2C transaction.
     */
    if ((interruptStatus & I2C_SLAVE_INT_START) != 0U)
    {
        g_i2cActivity = true;
    }

    /*
     * STOP marks the end of the current transaction.
     */
    if ((interruptStatus & I2C_SLAVE_INT_STOP) != 0U)
    {
        g_i2cActivity = true;
        g_i2cReadTransaction = false;
    }
}

static void BL_I2CPrepareTransmit(void)
{
    uint8_t value;

    /*
     * If there is application/bootloader data waiting,
     * send it.
     */
    if (BL_FIFO_Pop(
        &g_i2cTxFifo,
        &value))
    {
        I2CSlaveDataPut(
            BL_I2C_BASE,
            value);

        g_i2cTxCount++;

        return;
    }

    /*
     * No response data is available yet.
     *
     * Return a neutral byte instead of stale data.
     */
    I2CSlaveDataPut(
        BL_I2C_BASE,
        0xFFU);
}

bool BL_I2C_Init(void)
{
    /*
     * Enable GPIOB and I2C0.
     */
    SysCtlPeripheralEnable(BL_I2C_GPIO_PERIPH);
    SysCtlPeripheralEnable(BL_I2C_I2C_PERIPH);

    while (!SysCtlPeripheralReady(BL_I2C_GPIO_PERIPH))
    {
    }

    while (!SysCtlPeripheralReady(BL_I2C_I2C_PERIPH))
    {
    }

    /*
     * Configure:
     *
     * PB2 -> I2C0SCL
     * PB3 -> I2C0SDA
     */
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);

    GPIOPinTypeI2C(
        BL_I2C_GPIO_BASE,
        BL_I2C_SCL_PIN);

    GPIOPinTypeI2C(
        BL_I2C_GPIO_BASE,
        BL_I2C_SDA_PIN);

    /*
     * Initialize software FIFOs.
     */
    BL_FIFO_Init(
        &g_i2cRxFifo,
        g_i2cRxStorage,
        sizeof(g_i2cRxStorage));

    BL_FIFO_Init(
        &g_i2cTxFifo,
        g_i2cTxStorage,
        sizeof(g_i2cTxStorage));

    g_i2cActivity = false;
    g_i2cError = false;
    g_i2cRxCount = 0U;
    g_i2cTxCount = 0U;
    g_i2cReadTransaction = false;

    /*
     * Initialize the peripheral as an I2C slave.
     *
     * IMPORTANT:
     *
     * We do NOT call I2CMasterInitExpClk().
     *
     * The TM4C123 is the slave and the external programmer
     * is the I2C master.
     */
    I2CSlaveInit(
        BL_I2C_BASE,
        BL_I2C_SLAVE_ADDRESS);

    /*
     * Register interrupt handler.
     */
    I2CIntRegister(
        BL_I2C_BASE,
        BL_I2CInterruptHandler);

    /*
     * Enable:
     *
     * START
     * DATA
     * STOP
     */
    I2CSlaveIntEnableEx(
        BL_I2C_BASE,
        BL_I2C_INTERRUPT_FLAGS);

    /*
     * Enable NVIC interrupt.
     */
    IntEnable(INT_I2C0);

    g_i2cInitialized = true;

    return true;
}

void BL_I2C_DeInit(void)
{
    if (!g_i2cInitialized)
    {
        return;
    }

    /*
     * Disable NVIC interrupt.
     */
    IntDisable(INT_I2C0);

    /*
     * Disable I2C slave interrupts.
     */
    I2CSlaveIntDisableEx(
        BL_I2C_BASE,
        BL_I2C_INTERRUPT_FLAGS);

    I2CIntUnregister(BL_I2C_BASE);

    /*
     * Disable I2C slave peripheral.
     */
    I2CSlaveDisable(BL_I2C_BASE);

    BL_FIFO_Reset(&g_i2cRxFifo);
    BL_FIFO_Reset(&g_i2cTxFifo);

    g_i2cInitialized = false;
    g_i2cActivity = false;
    g_i2cError = false;
    g_i2cReadTransaction = false;
}

bool BL_I2C_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength)
{
    uint32_t count;
    uint8_t value;

    if ((buffer == NULL) ||
        (receivedLength == NULL))
    {
        return false;
    }

    if (bufferSize == 0U)
    {
        return false;
    }

    if (!g_i2cInitialized)
    {
        return false;
    }

    count = 0U;

    while ((count < bufferSize) &&
           BL_FIFO_Pop(
               &g_i2cRxFifo,
               &value))
    {
        buffer[count] = value;
        count++;
    }

    *receivedLength = count;

    return true;
}

bool BL_I2C_Transmit(
    const uint8_t *buffer,
    uint32_t length)
{
    uint32_t index;

    if ((buffer == NULL) ||
        (length == 0U))
    {
        return false;
    }

    if (!g_i2cInitialized)
    {
        return false;
    }

    /*
     * Queue the complete response.
     *
     * The actual physical transfer occurs when the external
     * I2C master performs a READ transaction.
     */
    for (index = 0U; index < length; index++)
    {
        if (!BL_FIFO_Push(
            &g_i2cTxFifo,
            buffer[index]))
        {
            g_i2cError = true;
            return false;
        }
    }

    g_i2cActivity = true;

    return true;
}

bool BL_I2C_Activity(void)
{
    bool activity;

    activity = g_i2cActivity;
    g_i2cActivity = false;

    return activity;
}

bool BL_I2C_IsInitialized(void)
{
    return g_i2cInitialized;
}

bool BL_I2C_HasError(void)
{
    return g_i2cError;
}

uint32_t BL_I2C_GetRxCount(void)
{
    return g_i2cRxCount;
}

uint32_t BL_I2C_GetTxCount(void)
{
    return g_i2cTxCount;
}