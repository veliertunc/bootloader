#include "bl_can.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bl_can_fragment.h"
#include "bl_fifo.h"
#include "bl_packet.h"
#include "bl_time.h"
#include "board.h"

#include "driverlib/can.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"

/*
 * --------------------------------------------------------------------------
 * Private constants
 * --------------------------------------------------------------------------
 */

#define BL_CAN_RX_GPIO_PERIPH       (SYSCTL_PERIPH_GPIOE)
#define BL_CAN_RX_GPIO_BASE         (GPIO_PORTE_BASE)

#define BL_CAN_PERIPH               (SYSCTL_PERIPH_CAN0)
#define BL_CAN_BASE                 (CAN0_BASE)

#define BL_CAN_RX_PIN               (GPIO_PIN_4)
#define BL_CAN_TX_PIN               (GPIO_PIN_5)

/*
 * Maximum number of bytes that can be reconstructed from one
 * bootloader packet.
 */
#define BL_CAN_REASSEMBLY_BUFFER_SIZE \
    (BL_PACKET_MAX_SIZE)

/*
 * --------------------------------------------------------------------------
 * Private storage
 * --------------------------------------------------------------------------
 */

/*
 * Bytes produced by CAN reassembly are placed into this FIFO.
 *
 * The foreground transport layer consumes these bytes.
 */
static uint8_t g_canRxStorage[BL_CAN_RX_BUFFER_SIZE];

static BL_FIFO_t g_canRxFifo;

/*
 * CAN fragmentation/reassembly state.
 */
static BL_CANReassembly_t g_canReassembly;

/*
 * Static buffer used when a complete packet is reconstructed.
 *
 * This intentionally does not live on the ISR stack.
 */
static uint8_t g_canReassembledPacket[
    BL_CAN_REASSEMBLY_BUFFER_SIZE
];

/*
 * TX frame data.
 *
 * CANMessageSet() consumes this data when the message object
 * is configured, so this storage can be reused after the
 * transmission completes.
 */
static uint8_t g_canTxFrameData[BL_CAN_FRAME_SIZE];

/*
 * State flags shared between interrupt and foreground code.
 */
static volatile bool g_canInitialized = false;
static volatile bool g_canActivity = false;
static volatile bool g_canBusOff = false;
static volatile bool g_canTxComplete = false;
static volatile bool g_canTxError = false;

/*
 * --------------------------------------------------------------------------
 * Private functions
 * --------------------------------------------------------------------------
 */

static void BL_CANConfigureGPIO(void);

static bool BL_CANConfigureRxObject(void);

static bool BL_CANSendFrame(
    const uint8_t *frameData,
    uint32_t frameLength);

static void BL_CANProcessReceivedFrame(
    const uint8_t *frameData,
    uint32_t frameLength);

static void BL_CANUpdateStatus(void);

static void BL_CAN_IRQHandler(void);

/*
 * --------------------------------------------------------------------------
 * GPIO configuration
 * --------------------------------------------------------------------------
 */

static void BL_CANConfigureGPIO(void)
{
    /*
     * Enable GPIOE.
     */
    SysCtlPeripheralEnable(BL_CAN_RX_GPIO_PERIPH);

    while (!SysCtlPeripheralReady(BL_CAN_RX_GPIO_PERIPH))
    {
    }

    /*
     * PE4 = CAN0RX
     * PE5 = CAN0TX
     */
    GPIOPinConfigure(GPIO_PE4_CAN0RX);
    GPIOPinConfigure(GPIO_PE5_CAN0TX);

    GPIOPinTypeCAN(
        BL_CAN_RX_GPIO_BASE,
        BL_CAN_RX_PIN | BL_CAN_TX_PIN);
}

/*
 * --------------------------------------------------------------------------
 * RX message object configuration
 * --------------------------------------------------------------------------
 */

static bool BL_CANConfigureRxObject(void)
{
    tCANMsgObject messageObject;

    memset(&messageObject, 0, sizeof(messageObject));

    messageObject.ui32MsgID =
        BL_CAN_HOST_TO_TARGET_ID;

    /*
     * Standard 11-bit CAN ID.
     */
    messageObject.ui32MsgIDMask =
        0x7FFUL;

    messageObject.ui32Flags =
        MSG_OBJ_RX_INT_ENABLE |
        MSG_OBJ_USE_ID_FILTER;

    messageObject.ui32MsgLen =
        BL_CAN_FRAME_SIZE;

    CANMessageSet(
        BL_CAN_BASE,
        BL_CAN_RX_OBJECT,
        &messageObject,
        MSG_OBJ_TYPE_RX);

    return true;
}

/*
 * --------------------------------------------------------------------------
 * CAN status
 * --------------------------------------------------------------------------
 */

static void BL_CANUpdateStatus(void)
{
    uint32_t status;

    if (!g_canInitialized)
    {
        return;
    }

    status = CANStatusGet(
        BL_CAN_BASE,
        CAN_STS_CONTROL);

    /*
     * EWARN and EPASS are diagnostic states.
     *
     * They are not treated as a fatal transport failure.
     *
     * BUS_OFF is the condition that prevents normal communication.
     */
    if ((status & CAN_STATUS_BUS_OFF) != 0U)
    {
        g_canBusOff = true;
    }
}

/*
 * --------------------------------------------------------------------------
 * RX frame processing
 * --------------------------------------------------------------------------
 */

static void BL_CANProcessReceivedFrame(
    const uint8_t *frameData,
    uint32_t frameLength)
{
    bool complete;
    uint32_t packetLength;

    if (frameData == NULL)
    {
        return;
    }

    if (frameLength != BL_CAN_FRAME_SIZE)
    {
        return;
    }

    g_canActivity = true;

    complete = BL_CANFragment_ProcessFrame(
        &g_canReassembly,
        frameData,
        frameLength);

    if (!complete)
    {
        return;
    }

    packetLength = 0U;

    if (!BL_CANFragment_GetPacket(
            &g_canReassembly,
            g_canReassembledPacket,
            sizeof(g_canReassembledPacket),
            &packetLength))
    {
        return;
    }

    /*
     * Before pushing a complete packet into the FIFO, make sure
     * enough room exists for the entire packet.
     *
     * This prevents partially inserting a packet.
     */
    if (BL_FIFO_Free(&g_canRxFifo) < packetLength)
    {
        /*
         * The complete packet cannot be queued.
         *
         * Resetting the FIFO is intentionally NOT done here because
         * doing so would destroy unrelated data that may already be
         * waiting for the foreground transport layer.
         *
         * The packet is therefore discarded.
         */
        return;
    }

    for (uint32_t index = 0U;
         index < packetLength;
         ++index)
    {
        if (!BL_FIFO_Push(
                &g_canRxFifo,
                g_canReassembledPacket[index]))
        {
            /*
             * This should not happen because Free() was checked above.
             */
            break;
        }
    }
}

/*
 * --------------------------------------------------------------------------
 * CAN interrupt handler
 * --------------------------------------------------------------------------
 */

static void BL_CAN_IRQHandler(void)
{
    uint32_t interruptStatus;

    interruptStatus = CANIntStatus(
        BL_CAN_BASE,
        CAN_INT_STS_CAUSE);

    /*
     * Status interrupt.
     */
    if (interruptStatus == CAN_INT_INTID_STATUS)
    {
        BL_CANUpdateStatus();

        /*
         * The status interrupt is cleared by reading the controller
         * status through CANStatusGet().
         */
    }
    /*
     * RX message object.
     */
    else if (interruptStatus == BL_CAN_RX_OBJECT)
    {
        tCANMsgObject messageObject;
        uint8_t frameData[BL_CAN_FRAME_SIZE];

        memset(&messageObject, 0, sizeof(messageObject));
        memset(frameData, 0, sizeof(frameData));

        messageObject.pui8MsgData =
            frameData;

        messageObject.ui32MsgLen =
            BL_CAN_FRAME_SIZE;

        /*
         * Reading with bClrPendingInt = true clears the pending
         * interrupt associated with this message object.
         */
        CANMessageGet(
            BL_CAN_BASE,
            BL_CAN_RX_OBJECT,
            &messageObject,
            true);

        /*
         * Process the complete CAN frame.
         */
        BL_CANProcessReceivedFrame(
            frameData,
            messageObject.ui32MsgLen);

        /*
         * Re-arm the RX message object.
         */
        BL_CANConfigureRxObject();
    }
    /*
     * TX message object.
     */
    else if (interruptStatus == BL_CAN_TX_OBJECT)
    {
        CANIntClear(
            BL_CAN_BASE,
            BL_CAN_TX_OBJECT);

        g_canTxComplete = true;
    }
    else
    {
        /*
         * Unknown object interrupt.
         *
         * Nothing else to process here.
         */
    }
}

/*
 * --------------------------------------------------------------------------
 * TX frame
 * --------------------------------------------------------------------------
 */

static bool BL_CANSendFrame(
    const uint8_t *frameData,
    uint32_t frameLength)
{
    tCANMsgObject messageObject;
    uint32_t startTick;

    if (frameData == NULL)
    {
        return false;
    }

    if (frameLength != BL_CAN_FRAME_SIZE)
    {
        return false;
    }

    if (!g_canInitialized)
    {
        return false;
    }

    /*
     * Do not attempt transmission while bus-off.
     */
    BL_CANUpdateStatus();

    if (g_canBusOff)
    {
        return false;
    }

    /*
     * Copy frame data into static storage.
     */
    memcpy(
        g_canTxFrameData,
        frameData,
        BL_CAN_FRAME_SIZE);

    memset(
        &messageObject,
        0,
        sizeof(messageObject));

    messageObject.ui32MsgID =
        BL_CAN_TARGET_TO_HOST_ID;

    messageObject.ui32MsgIDMask =
        0U;

    messageObject.ui32Flags =
        MSG_OBJ_TX_INT_ENABLE;

    messageObject.ui32MsgLen =
        BL_CAN_FRAME_SIZE;

    messageObject.pui8MsgData =
        g_canTxFrameData;

    g_canTxComplete = false;
    g_canTxError = false;

    CANMessageSet(
        BL_CAN_BASE,
        BL_CAN_TX_OBJECT,
        &messageObject,
        MSG_OBJ_TYPE_TX);

    /*
     * Wait synchronously for the TX message-object interrupt.
     *
     * This keeps the transport interface simple:
     *
     *     BL_CAN_Transmit()
     *
     * does not return until the complete fragment frame has been
     * accepted for transmission.
     */
    startTick = BL_TimeGetTick();

    while (!g_canTxComplete)
    {
        BL_CANUpdateStatus();

        if (g_canBusOff)
        {
            g_canTxError = true;
            break;
        }

        if (BL_TimeTimeoutExpired(
                startTick,
                BL_CAN_TX_TIMEOUT_MS))
        {
            g_canTxError = true;
            break;
        }
    }

    if (g_canTxError)
    {
        return false;
    }

    g_canActivity = true;

    return true;
}

/*
 * --------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------
 */

bool BL_CAN_Init(void)
{
    /*
     * Already initialized.
     */
    if (g_canInitialized)
    {
        return true;
    }

    /*
     * Enable CAN0 peripheral.
     */
    SysCtlPeripheralEnable(BL_CAN_PERIPH);

    while (!SysCtlPeripheralReady(BL_CAN_PERIPH))
    {
    }

    /*
     * Configure CAN pins.
     */
    BL_CANConfigureGPIO();

    /*
     * Initialize software state.
     */
    BL_FIFO_Init(
        &g_canRxFifo,
        g_canRxStorage,
        sizeof(g_canRxStorage));

    BL_CANFragment_Init(
        &g_canReassembly);

    memset(
        g_canReassembledPacket,
        0,
        sizeof(g_canReassembledPacket));

    memset(
        g_canTxFrameData,
        0,
        sizeof(g_canTxFrameData));

    g_canActivity = false;
    g_canBusOff = false;
    g_canTxComplete = false;
    g_canTxError = false;

    /*
     * Initialize CAN controller.
     */
    CANInit(BL_CAN_BASE);

    /*
     * Configure 500 kbit/s classical CAN.
     *
     * The source clock is the 80 MHz system clock.
     */
    if (CANBitRateSet(
            BL_CAN_BASE,
            Board_GetSystemClock(),
            BL_CAN_BITRATE) == 0U)
    {
        return false;
    }

    /*
     * Automatically retry CAN transmissions when arbitration
     * is lost.
     */
    CANRetrySet(
        BL_CAN_BASE,
        true);

    /*
     * Configure RX message object.
     */
    if (!BL_CANConfigureRxObject())
    {
        return false;
    }

    /*
     * Register interrupt handler.
     */
    CANIntRegister(
        BL_CAN_BASE,
        BL_CAN_IRQHandler);

    /*
     * Enable:
     *
     *   MASTER  - enables CAN interrupts
     *   ERROR   - controller error interrupts
     *   STATUS  - status interrupts
     */
    CANIntEnable(
        BL_CAN_BASE,
        CAN_INT_MASTER |
        CAN_INT_ERROR |
        CAN_INT_STATUS);

    /*
     * Enable CAN controller.
     */
    CANEnable(BL_CAN_BASE);

    g_canInitialized = true;

    /*
     * Perform an initial status check.
     */
    BL_CANUpdateStatus();

    /*
     * If the controller starts in bus-off, initialization has
     * not produced a usable transport.
     */
    if (g_canBusOff)
    {
        return false;
    }

    return true;
}

void BL_CAN_DeInit(void)
{
    if (!g_canInitialized)
    {
        return;
    }

    /*
     * Disable CAN interrupts.
     */
    CANIntDisable(
        BL_CAN_BASE,
        CAN_INT_MASTER |
        CAN_INT_ERROR |
        CAN_INT_STATUS);

    /*
     * Remove registered interrupt handler.
     */
    CANIntUnregister(BL_CAN_BASE);

    /*
     * Disable CAN controller.
     */
    CANDisable(BL_CAN_BASE);

    /*
     * Reset software state.
     */
    BL_FIFO_Reset(&g_canRxFifo);

    BL_CANFragment_Init(
        &g_canReassembly);

    g_canActivity = false;
    g_canBusOff = false;
    g_canTxComplete = false;
    g_canTxError = false;
    g_canInitialized = false;
}

bool BL_CAN_Receive(
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *receivedLength)
{
    uint8_t value;
    uint32_t count;

    if (buffer == NULL)
    {
        return false;
    }

    if (receivedLength == NULL)
    {
        return false;
    }

    *receivedLength = 0U;

    if (bufferSize == 0U)
    {
        return false;
    }

    if (!g_canInitialized)
    {
        return false;
    }

    count = 0U;

    while (count < bufferSize)
    {
        if (!BL_FIFO_Pop(
                &g_canRxFifo,
                &value))
        {
            break;
        }

        buffer[count] = value;
        ++count;
    }

    *receivedLength = count;

    return true;
}

bool BL_CAN_Transmit(
    const uint8_t *buffer,
    uint32_t length)
{
    BL_CANFragmentation_t fragmentation;
    uint8_t frame[BL_CAN_FRAME_SIZE];
    uint32_t frameLength;

    if (buffer == NULL)
    {
        return false;
    }

    if (length == 0U)
    {
        return false;
    }

    if (length > BL_PACKET_MAX_SIZE)
    {
        return false;
    }

    if (!g_canInitialized)
    {
        return false;
    }

    BL_CANUpdateStatus();

    if (g_canBusOff)
    {
        return false;
    }

    /*
     * Create the CAN fragmentation context.
     */
    BL_CANFragmentation_Init(
        &fragmentation,
        buffer,
        length);

    /*
     * Generate and transmit every CAN frame.
     */
    while (BL_CANFragmentation_NextFrame(
               &fragmentation,
               frame,
               sizeof(frame),
               &frameLength))
    {
        if (!BL_CANSendFrame(
                frame,
                frameLength))
        {
            return false;
        }
    }

    return true;
}

bool BL_CAN_Activity(void)
{
    bool activity;

    activity = g_canActivity;

    g_canActivity = false;

    return activity;
}

bool BL_CAN_IsInitialized(void)
{
    return g_canInitialized;
}

bool BL_CAN_IsBusError(void)
{
    if (!g_canInitialized)
    {
        return false;
    }

    BL_CANUpdateStatus();

    return g_canBusOff;
}