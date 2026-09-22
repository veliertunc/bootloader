#include "bl_usb.h"

#include <stddef.h>

#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"

#include "usblib/usblib.h"
#include "usblib/usbcdc.h"
#include "usblib/usb-ids.h"
#include "usblib/usbbuffer.h"
#include "usblib/device/usbdcdc.h"

#include "bl_fifo.h"

#define BL_USB_GPIO_PERIPH          (SYSCTL_PERIPH_GPIOB)
#define BL_USB_SIGNAL_GPIO_PERIPH   (SYSCTL_PERIPH_GPIOD)
#define BL_USB_PERIPH               (SYSCTL_PERIPH_USB0)

#define BL_USB_GPIO_BASE            (GPIO_PORTB_BASE)
#define BL_USB_SIGNAL_GPIO_BASE     (GPIO_PORTD_BASE)

#define BL_USB_ID_PIN               (GPIO_PIN_0)
#define BL_USB_VBUS_PIN             (GPIO_PIN_1)

#define BL_USB_DM_PIN               (GPIO_PIN_4)
#define BL_USB_DP_PIN               (GPIO_PIN_5)

/*
 * USB CDC buffer storage.
 *
 * These buffers belong to TivaWare's USBBuffer layer.
 */
static uint8_t g_usbRxStorage[BL_USB_RX_BUFFER_SIZE];
static uint8_t g_usbTxStorage[BL_USB_TX_BUFFER_SIZE];

/*
 * USB CDC instance.
 */
static tUSBDCDCDevice g_usbCDCDevice;

/*
 * USBBuffer objects.
 */
static tUSBBuffer g_usbRxBuffer;
static tUSBBuffer g_usbTxBuffer;

/*
 * USB state.
 */
static volatile bool g_usbInitialized;
static volatile bool g_usbConnected;
static volatile bool g_usbActivity;
static volatile bool g_usbError;

static volatile uint32_t g_usbRxCount;
static volatile uint32_t g_usbTxCount;

/*
 * Forward declarations.
 */
static uint32_t BL_USBControlHandler(
    void *callbackData,
    uint32_t event,
    uint32_t messageValue,
    void *messageData);

static uint32_t BL_USBRxHandler(
    void *callbackData,
    uint32_t event,
    uint32_t messageValue,
    void *messageData);

static uint32_t BL_USBTxHandler(
    void *callbackData,
    uint32_t event,
    uint32_t messageValue,
    void *messageData);

/*
 * USB CDC string descriptors.
 *
 * These are UTF-16LE encoded USB string descriptors.
 */

static const uint8_t g_usbLanguageDescriptor[] =
{
    4U,
    USB_DTYPE_STRING,
    0x09U,
    0x04U
};

static const uint8_t g_usbManufacturerString[] =
{
    2U + (sizeof("TM4C123 Bootloader") - 1U) * 2U,
    USB_DTYPE_STRING,

    'T', 0,
    'M', 0,
    '4', 0,
    'C', 0,
    '1', 0,
    '2', 0,
    '3', 0,
    ' ', 0,
    'B', 0,
    'o', 0,
    'o', 0,
    't', 0,
    'l', 0,
    'o', 0,
    'a', 0,
    'd', 0,
    'e', 0,
    'r', 0
};

static const uint8_t g_usbProductString[] =
{
    2U + (sizeof("TM4C123 Multi-Transport Bootloader") - 1U) * 2U,
    USB_DTYPE_STRING,

    'T', 0,
    'M', 0,
    '4', 0,
    'C', 0,
    '1', 0,
    '2', 0,
    '3', 0,
    ' ', 0,
    'M', 0,
    'u', 0,
    'l', 0,
    't', 0,
    'i', 0,
    '-', 0,
    'T', 0,
    'r', 0,
    'a', 0,
    'n', 0,
    's', 0,
    'p', 0,
    'o', 0,
    'r', 0,
    't', 0,
    ' ', 0,
    'B', 0,
    'o', 0,
    'o', 0,
    't', 0,
    'l', 0,
    'o', 0,
    'a', 0,
    'd', 0,
    'e', 0,
    'r', 0
};

static const uint8_t g_usbSerialNumberString[] =
{
    2U + (sizeof("00000001") - 1U) * 2U,
    USB_DTYPE_STRING,

    '0', 0,
    '0', 0,
    '0', 0,
    '0', 0,
    '0', 0,
    '0', 0,
    '0', 0,
    '1', 0
};

static const uint8_t g_usbControlInterfaceString[] =
{
    2U + (sizeof("Bootloader CDC Control") - 1U) * 2U,
    USB_DTYPE_STRING,

    'B', 0,
    'o', 0,
    'o', 0,
    't', 0,
    'l', 0,
    'o', 0,
    'a', 0,
    'd', 0,
    'e', 0,
    'r', 0,
    ' ', 0,
    'C', 0,
    'D', 0,
    'C', 0,
    ' ', 0,
    'C', 0,
    'o', 0,
    'n', 0,
    't', 0,
    'r', 0,
    'o', 0,
    'l', 0
};

static const uint8_t g_usbConfigString[] =
{
    2U + (sizeof("TM4C123 Bootloader Configuration") - 1U) * 2U,
    USB_DTYPE_STRING,

    'T', 0,
    'M', 0,
    '4', 0,
    'C', 0,
    '1', 0,
    '2', 0,
    '3', 0,
    ' ', 0,
    'B', 0,
    'o', 0,
    'o', 0,
    't', 0,
    'l', 0,
    'o', 0,
    'a', 0,
    'd', 0,
    'e', 0,
    'r', 0,
    ' ', 0,
    'C', 0,
    'o', 0,
    'n', 0,
    'f', 0,
    'i', 0,
    'g', 0,
    'u', 0,
    'r', 0,
    'a', 0,
    't', 0,
    'i', 0,
    'o', 0,
    'n', 0
};

static const uint8_t *const g_usbStringDescriptors[] =
{
    g_usbLanguageDescriptor,
    g_usbManufacturerString,
    g_usbProductString,
    g_usbSerialNumberString,
    g_usbControlInterfaceString,
    g_usbConfigString
};

#define BL_USB_STRING_DESCRIPTOR_COUNT \
    (sizeof(g_usbStringDescriptors) / \
     sizeof(g_usbStringDescriptors[0]))

static uint32_t BL_USBControlHandler(
    void *callbackData,
    uint32_t event,
    uint32_t messageValue,
    void *messageData)
{
    (void)callbackData;
    (void)messageValue;
    (void)messageData;

    switch (event)
    {
        case USB_EVENT_CONNECTED:
        {
            g_usbConnected = true;
            g_usbActivity = true;

            USBBufferFlush(&g_usbRxBuffer);
            USBBufferFlush(&g_usbTxBuffer);

            break;
        }

        case USB_EVENT_DISCONNECTED:
        {
            g_usbConnected = false;
            g_usbActivity = true;

            break;
        }

        case USB_EVENT_SUSPEND:
        {
            g_usbActivity = true;
            break;
        }

        case USB_EVENT_RESUME:
        {
            g_usbActivity = true;
            break;
        }

        case USBD_CDC_EVENT_GET_LINE_CODING:
        {
            /*
             * The CDC class driver maintains the line coding.
             *
             * The bootloader does not use baud rate information
             * because USB itself controls the physical transport.
             */
            break;
        }

        case USBD_CDC_EVENT_SET_LINE_CODING:
        {
            /*
             * Accepted for normal CDC host compatibility.
             */
            g_usbActivity = true;
            break;
        }

        case USBD_CDC_EVENT_SET_CONTROL_LINE_STATE:
        {
            g_usbActivity = true;
            break;
        }

        case USBD_CDC_EVENT_SEND_BREAK:
        {
            g_usbActivity = true;
            break;
        }

        case USBD_CDC_EVENT_CLEAR_BREAK:
        {
            g_usbActivity = true;
            break;
        }

        default:
        {
            break;
        }
    }

    return 0U;
}

static uint32_t BL_USBRxHandler(
    void *callbackData,
    uint32_t event,
    uint32_t messageValue,
    void *messageData)
{
    (void)callbackData;
    (void)messageValue;
    (void)messageData;

    switch (event)
    {
        case USB_EVENT_RX_AVAILABLE:
        {
            g_usbActivity = true;
            break;
        }

        case USB_EVENT_DATA_REMAINING:
        {
            g_usbActivity = true;
            break;
        }

        case USB_EVENT_REQUEST_BUFFER:
        {
            g_usbActivity = true;
            break;
        }

        default:
        {
            break;
        }
    }

    return 0U;
}

static uint32_t BL_USBTxHandler(
    void *callbackData,
    uint32_t event,
    uint32_t messageValue,
    void *messageData)
{
    (void)callbackData;
    (void)messageValue;
    (void)messageData;

    switch (event)
    {
        case USB_EVENT_TX_COMPLETE:
        {
            g_usbActivity = true;
            break;
        }

        default:
        {
            break;
        }
    }

    return 0U;
}

bool BL_USB_Init(void)
{
    void *cdcInstance;

    if (g_usbInitialized)
    {
        return true;
    }

    /*
     * Enable USB controller.
     */
    SysCtlPeripheralEnable(BL_USB_PERIPH);

    while (!SysCtlPeripheralReady(BL_USB_PERIPH))
    {
    }

    /*
     * Enable USB GPIOs.
     */
    SysCtlPeripheralEnable(BL_USB_GPIO_PERIPH);

    while (!SysCtlPeripheralReady(BL_USB_GPIO_PERIPH))
    {
    }

    SysCtlPeripheralEnable(BL_USB_SIGNAL_GPIO_PERIPH);

    while (!SysCtlPeripheralReady(BL_USB_SIGNAL_GPIO_PERIPH))
    {
    }

    /*
     * USB PLL.
     *
     * The TM4C123 USB device requires the USB clock infrastructure
     * provided by the USB PLL.
     */
    SysCtlUSBPLLEnable();

    /*
     * USB ID and VBUS.
     */
    GPIOPinTypeUSBAnalog(
        BL_USB_GPIO_BASE,
        BL_USB_ID_PIN | BL_USB_VBUS_PIN);

    /*
     * USB D- and D+.
     */
    GPIOPinTypeUSBAnalog(
        BL_USB_SIGNAL_GPIO_BASE,
        BL_USB_DM_PIN | BL_USB_DP_PIN);

    /*
     * Initialize the TivaWare USB buffers.
     */
    USBBufferInit(&g_usbRxBuffer);
    USBBufferInit(&g_usbTxBuffer);

    /*
     * Force USB device mode.
     *
     * This bootloader is always a USB device.
     */
    USBStackModeSet(
        0U,
        eUSBModeForceDevice,
        0);

    /*
     * Initialize CDC.
     *
     * USBDCDCInit() performs the CDC device initialization and
     * attaches the device to the USB bus.
     */
    cdcInstance = USBDCDCInit(
        0U,
        &g_usbCDCDevice);

    if (cdcInstance == NULL)
    {
        g_usbError = true;
        return false;
    }

    g_usbConnected = false;
    g_usbActivity = false;
    g_usbRxCount = 0U;
    g_usbTxCount = 0U;
    g_usbInitialized = true;

    return true;
}

void BL_USB_DeInit(void)
{
    if (!g_usbInitialized)
    {
        return;
    }

    USBDCDCTerm(&g_usbCDCDevice);

    USBBufferFlush(&g_usbRxBuffer);
    USBBufferFlush(&g_usbTxBuffer);

    g_usbInitialized = false;
    g_usbConnected = false;
    g_usbActivity = false;
}

bool BL_USB_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength)
{
    uint32_t available;
    uint32_t length;

    if ((buffer == NULL) ||
        (receivedLength == NULL))
    {
        return false;
    }

    if (bufferSize == 0U)
    {
        return false;
    }

    if (!g_usbInitialized)
    {
        return false;
    }

    available = USBBufferDataAvailable(
        &g_usbRxBuffer);

    if (available == 0U)
    {
        *receivedLength = 0U;
        return true;
    }

    length = available;

    if (length > bufferSize)
    {
        length = bufferSize;
    }

    length = USBBufferRead(
        &g_usbRxBuffer,
        buffer,
        length);

    if (length > 0U)
    {
        g_usbRxCount += length;
        g_usbActivity = true;
    }

    *receivedLength = length;

    return true;
}

bool BL_USB_Transmit(
    const uint8_t *buffer,
    uint32_t length)
{
    uint32_t written;
    uint32_t offset;

    if ((buffer == NULL) ||
        (length == 0U))
    {
        return false;
    }

    if (!g_usbInitialized)
    {
        return false;
    }

    if (!g_usbConnected)
    {
        return false;
    }

    /*
     * USBBufferWrite() handles the underlying CDC packetization.
     *
     * The CDC driver itself limits individual packet writes to
     * the USB full-speed endpoint packet size.
     */
    offset = 0U;

    while (offset < length)
    {
        written = USBBufferWrite(
            &g_usbTxBuffer,
            (uint8_t *)&buffer[offset],
            length - offset);

        if (written == 0U)
        {
            /*
             * No buffer space currently available.
             *
             * The USB stack will continue transmitting through
             * its interrupt-driven mechanism.
             */
            g_usbError = true;
            return false;
        }

        offset += written;
        g_usbTxCount += written;
    }

    g_usbActivity = true;

    return true;
}

bool BL_USB_Activity(void)
{
    bool activity;

    activity = g_usbActivity;
    g_usbActivity = false;

    return activity;
}

bool BL_USB_IsInitialized(void)
{
    return g_usbInitialized;
}

bool BL_USB_IsConnected(void)
{
    return g_usbConnected;
}

bool BL_USB_HasError(void)
{
    return g_usbError;
}

uint32_t BL_USB_GetRxCount(void)
{
    return g_usbRxCount;
}

uint32_t BL_USB_GetTxCount(void)
{
    return g_usbTxCount;
}