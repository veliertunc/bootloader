#include "bl_spi.h"

#include <stddef.h>
#include <string.h>

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/ssi.h"
#include "driverlib/sysctl.h"

#include "bl_fifo.h"
#include "bl_time.h"
#include "board.h"

#define BL_SPI_SSI_BASE              (SSI0_BASE)
#define BL_SPI_GPIO_BASE             (GPIO_PORTA_BASE)

#define BL_SPI_SSI_PERIPH            (SYSCTL_PERIPH_SSI0)
#define BL_SPI_GPIO_PERIPH           (SYSCTL_PERIPH_GPIOA)

#define BL_SPI_GPIO_PINS             \
    (GPIO_PIN_2 | GPIO_PIN_3 |       \
     GPIO_PIN_4 | GPIO_PIN_5)

#define BL_SPI_RX_PIN                (GPIO_PIN_4)
#define BL_SPI_TX_PIN                (GPIO_PIN_5)
#define BL_SPI_CLK_PIN               (GPIO_PIN_2)
#define BL_SPI_FSS_PIN               (GPIO_PIN_3)

#define BL_SPI_RX_INTERRUPT_FLAGS    \
    (SSI_RXFF | SSI_RXTO | SSI_RXOR)

#define BL_SPI_TX_INTERRUPT_FLAGS    \
    (SSI_TXFF)

#define BL_SPI_INTERRUPT_FLAGS       \
    (BL_SPI_RX_INTERRUPT_FLAGS | BL_SPI_TX_INTERRUPT_FLAGS)

static uint8_t g_spiRxStorage[BL_SPI_RX_BUFFER_SIZE];
static uint8_t g_spiTxStorage[BL_SPI_TX_BUFFER_SIZE];

static BL_FIFO_t g_spiRxFifo;
static BL_FIFO_t g_spiTxFifo;

static volatile bool g_spiInitialized;
static volatile bool g_spiActivity;
static volatile bool g_spiError;

static volatile uint32_t g_spiRxCount;
static volatile uint32_t g_spiTxCount;

static void BL_SPIFlushHardwareRx(void);

static void BL_SPIFillHardwareTx(void);

static void BL_SPIInterruptHandler(void)
{
    uint32_t status;

    status = SSIIntStatus(
        BL_SPI_SSI_BASE,
        true);

    /*
     * Receive path.
     *
     * SSIDataGetNonBlocking() drains the hardware RX FIFO.
     */
    if ((status & (SSI_RXFF | SSI_RXTO | SSI_RXOR)) != 0U)
    {
        uint32_t data;

        while (SSIDataGetNonBlocking(
            BL_SPI_SSI_BASE,
            &data) != 0)
        {
            if (!BL_FIFO_Push(
                &g_spiRxFifo,
                (uint8_t)(data & 0xFFU)))
            {
                /*
                 * Software RX FIFO overflow.
                 */
                g_spiError = true;
            }
            else
            {
                g_spiRxCount++;
                g_spiActivity = true;
            }
        }

        /*
         * SSI_RXOR indicates hardware receive FIFO overrun.
         */
        if ((status & SSI_RXOR) != 0U)
        {
            g_spiError = true;
        }
    }

    /*
     * Transmit path.
     *
     * The TM4C is the SPI slave, so the host supplies the clock.
     * We therefore keep the hardware TX FIFO filled whenever data
     * is available in the software TX FIFO.
     */
    if ((status & SSI_TXFF) != 0U)
    {
        BL_SPIFillHardwareTx();
    }

    SSIIntClear(
        BL_SPI_SSI_BASE,
        status);
}

static void BL_SPIFlushHardwareRx(void)
{
    uint32_t data;

    while (SSIDataGetNonBlocking(
        BL_SPI_SSI_BASE,
        &data) != 0)
    {
        /*
         * Discard stale data.
         */
    }
}

static void BL_SPIFillHardwareTx(void)
{
    uint8_t value;

    /*
     * SSIDataPutNonBlocking() returns zero when the hardware
     * TX FIFO cannot accept another word.
     */
    while (BL_FIFO_Pop(
        &g_spiTxFifo,
        &value))
    {
        if (SSIDataPutNonBlocking(
            BL_SPI_SSI_BASE,
            (uint32_t)value) == 0)
        {
            /*
             * Hardware FIFO is full.
             *
             * Put the byte back is not possible with the current
             * generic FIFO API, so this should not happen because
             * this function is only called while TXFF is asserted.
             *
             * The TX FIFO interrupt will run again after space
             * becomes available.
             */
            g_spiError = true;
            break;
        }

        g_spiTxCount++;
    }

    /*
     * If no more software data exists, disable the TX FIFO
     * interrupt. Otherwise it would continuously fire while
     * the FIFO is available.
     */
    if (BL_FIFO_IsEmpty(&g_spiTxFifo))
    {
        SSIIntDisable(
            BL_SPI_SSI_BASE,
            SSI_TXFF);
    }
}

bool BL_SPI_Init(void)
{
    uint32_t systemClock;

    if (g_spiInitialized)
    {
        return true;
    }

    systemClock = Board_GetSystemClock();

    if (systemClock == 0U)
    {
        return false;
    }

    /*
     * Enable SSI0 and GPIOA.
     */
    SysCtlPeripheralEnable(BL_SPI_SSI_PERIPH);
    SysCtlPeripheralEnable(BL_SPI_GPIO_PERIPH);

    while (!SysCtlPeripheralReady(BL_SPI_SSI_PERIPH))
    {
    }

    while (!SysCtlPeripheralReady(BL_SPI_GPIO_PERIPH))
    {
    }

    /*
     * Configure SSI0 pin mux.
     *
     * PA2 -> SSI0CLK
     * PA3 -> SSI0FSS
     * PA4 -> SSI0RX
     * PA5 -> SSI0TX
     */
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA3_SSI0FSS);
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);

    GPIOPinTypeSSI(
        BL_SPI_GPIO_BASE,
        BL_SPI_GPIO_PINS);

    /*
     * Initialize software FIFOs before enabling interrupts.
     */
    BL_FIFO_Init(
        &g_spiRxFifo,
        g_spiRxStorage,
        sizeof(g_spiRxStorage));

    BL_FIFO_Init(
        &g_spiTxFifo,
        g_spiTxStorage,
        sizeof(g_spiTxStorage));

    g_spiActivity = false;
    g_spiError = false;
    g_spiRxCount = 0U;
    g_spiTxCount = 0U;

    /*
     * Configure SSI0 as an SPI slave.
     *
     * The host supplies SCLK.
     *
     * Mode 0:
     *   CPOL = 0
     *   CPHA = 0
     */
    SSIDisable(BL_SPI_SSI_BASE);

    SSIConfigSetExpClk(
        BL_SPI_SSI_BASE,
        systemClock,
        SSI_FRF_MOTO_MODE_0,
        SSI_MODE_SLAVE,
        BL_SPI_CLOCK_HZ,
        8U);

    /*
     * Remove any stale RX data from a previous state.
     */
    BL_SPIFlushHardwareRx();

    /*
     * Clear pending SSI interrupt flags.
     */
    SSIIntClear(
        BL_SPI_SSI_BASE,
        BL_SPI_INTERRUPT_FLAGS);

    /*
     * Register ISR.
     */
    SSIIntRegister(
        BL_SPI_SSI_BASE,
        BL_SPIInterruptHandler);

    /*
     * Enable RX-related interrupts.
     *
     * TXFF is deliberately enabled only when there is
     * transmit data waiting.
     */
    SSIIntEnable(
        BL_SPI_SSI_BASE,
        BL_SPI_RX_INTERRUPT_FLAGS);

    /*
     * Enable the SSI peripheral.
     */
    SSIEnable(BL_SPI_SSI_BASE);

    /*
     * Enable the SSI0 NVIC interrupt.
     */
    IntEnable(INT_SSI0);

    g_spiInitialized = true;

    return true;
}

void BL_SPI_DeInit(void)
{
    if (!g_spiInitialized)
    {
        return;
    }

    /*
     * Disable NVIC interrupt first.
     */
    IntDisable(INT_SSI0);

    /*
     * Disable SSI interrupts.
     */
    SSIIntDisable(
        BL_SPI_SSI_BASE,
        BL_SPI_INTERRUPT_FLAGS);

    SSIIntUnregister(BL_SPI_SSI_BASE);

    /*
     * Disable SSI peripheral.
     */
    SSIDisable(BL_SPI_SSI_BASE);

    BL_FIFO_Reset(&g_spiRxFifo);
    BL_FIFO_Reset(&g_spiTxFifo);

    g_spiInitialized = false;
    g_spiActivity = false;
    g_spiError = false;
}

bool BL_SPI_Receive(
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

    if (!g_spiInitialized)
    {
        return false;
    }

    count = 0U;

    while ((count < bufferSize) &&
           BL_FIFO_Pop(
               &g_spiRxFifo,
               &value))
    {
        buffer[count] = value;
        count++;
    }

    *receivedLength = count;

    return true;
}

bool BL_SPI_Transmit(
    const uint8_t *buffer,
    uint32_t length)
{
    uint32_t index;
    uint32_t startTick;

    if ((buffer == NULL) ||
        (length == 0U))
    {
        return false;
    }

    if (!g_spiInitialized)
    {
        return false;
    }

    /*
     * Wait for enough room in the software TX FIFO.
     *
     * The current common transport API is synchronous, so we
     * don't return until the complete response has been queued.
     */
    startTick = BL_TimeGetTick();

    for (index = 0U; index < length; index++)
    {
        while (!BL_FIFO_Push(
            &g_spiTxFifo,
            buffer[index]))
        {
            /*
             * The host must clock the SPI bus so that bytes
             * can leave the hardware/software TX path.
             */
            BL_SPIFillHardwareTx();

            if (BL_TimeTimeoutExpired(
                startTick,
                BL_SPI_TX_TIMEOUT_MS))
            {
                g_spiError = true;
                return false;
            }
        }
    }

    /*
     * Immediately fill the SSI hardware FIFO.
     */
    BL_SPIFillHardwareTx();

    /*
     * Enable TX FIFO interrupt so the hardware FIFO will
     * continue being refilled as the host clocks data.
     */
    SSIIntEnable(
        BL_SPI_SSI_BASE,
        SSI_TXFF);

    g_spiActivity = true;

    return true;
}

bool BL_SPI_Activity(void)
{
    bool activity;

    activity = g_spiActivity;
    g_spiActivity = false;

    return activity;
}

bool BL_SPI_IsInitialized(void)
{
    return g_spiInitialized;
}

bool BL_SPI_HasError(void)
{
    return g_spiError;
}

uint32_t BL_SPI_GetRxCount(void)
{
    return g_spiRxCount;
}

uint32_t BL_SPI_GetTxCount(void)
{
    return g_spiTxCount;
}