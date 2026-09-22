#include "usb_strings.h"

#include "usblib/usblib.h"

static const uint8_t g_pui8LangDescriptor[] =
{
    4,
    USB_DTYPE_STRING,
    USBShort(0x0409)
};

static const uint8_t g_pui8ManufacturerString[] =
{
    18,
    USB_DTYPE_STRING,

    'C',0,
    'u',0,
    's',0,
    't',0,
    'o',0,
    'm',0,
    ' ',0,
    'B',0
};

static const uint8_t g_pui8ProductString[] =
{
    32,
    USB_DTYPE_STRING,

    'T',0,
    'M',0,
    '4',0,
    'C',0,
    '1',0,
    '2',0,
    '3',0,
    ' ',0,
    'B',0,
    'o',0,
    'o',0,
    't',0,
    'l',0,
    'o',0,
    'a',0,
    'd',0,
    'e',0,
    'r',0
};

static const uint8_t g_pui8SerialString[] =
{
    18,
    USB_DTYPE_STRING,

    '0',0,
    '0',0,
    '0',0,
    '0',0,
    '0',0,
    '0',0,
    '0',0,
    '1',0
};

const uint8_t * const g_ppui8StringDescriptors[] =
{
    g_pui8LangDescriptor,
    g_pui8ManufacturerString,
    g_pui8ProductString,
    g_pui8SerialString
};

const uint32_t g_ui32StringDescriptorCount = 4U;