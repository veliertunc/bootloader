#include "bl_boot.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bl_config.h"
#include "bl_image.h"
#include "bl_jump.h"
#include "bl_packet.h"
#include "bl_protocol.h"
#include "bl_time.h"
#include "bl_transport.h"
#include "bl_transport_packetizer.h"
#include "board.h"

/*
 * --------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------
 */

#define BL_BOOT_TIMEOUT_MS             (3000UL)

#define BL_BOOT_RX_BUFFER_SIZE         (256UL)

#define BL_BOOT_TX_BUFFER_SIZE         \
    (BL_PACKET_MAX_SIZE)

#define BL_BOOT_PACKET_BUFFER_SIZE     \
    (BL_PACKET_MAX_SIZE)

/*
 * --------------------------------------------------------------------------
 * Private state
 * --------------------------------------------------------------------------
 */

static uint8_t g_bootRxBuffer[
    BL_BOOT_RX_BUFFER_SIZE
];

static uint8_t g_bootTxBuffer[
    BL_BOOT_TX_BUFFER_SIZE
];

static uint8_t g_bootPacketBuffer[
    BL_BOOT_PACKET_BUFFER_SIZE
];

static BL_TransportPacketizer_t
    g_bootPacketizer;

static uint32_t g_bootActivityTick;

/*
 * --------------------------------------------------------------------------
 * Private functions
 * --------------------------------------------------------------------------
 */

static bool BL_BootShouldEnterBootloader(void);

static bool BL_BootProcessRawPacket(
    const uint8_t *data,
    uint32_t length);

static bool BL_BootBuildCRCErrorResponse(
    const uint8_t *data,
    uint32_t length,
    BL_Packet_t *response);

static bool BL_BootSendResponse(
    const BL_Packet_t *response);

static void BL_BootExecuteAction(
    BL_CommandAction_t action);

static bool BL_BootProcessTransportData(
    const uint8_t *data,
    uint32_t length);

static bool BL_BootReceiveAndProcess(void);

static void BL_BootHandleInitFailure(void);

/*
 * --------------------------------------------------------------------------
 * Bootloader entry decision
 * --------------------------------------------------------------------------
 */

static bool BL_BootShouldEnterBootloader(void)
{
    /*
     * SW2 explicitly requests firmware-update mode.
     */
    if (Board_Sw2Pressed())
    {
        return true;
    }

    /*
     * No valid application means the bootloader must remain active.
     */
    if (!BL_ImageIsValid())
    {
        return true;
    }

    return false;
}

/*
 * --------------------------------------------------------------------------
 * Initialization failure
 * --------------------------------------------------------------------------
 */

static void BL_BootHandleInitFailure(void)
{
    Board_LedOn(BOARD_LED_RED);

    while (true)
    {
        Board_LedToggle(
            BOARD_LED_RED);

        /*
         * Simple diagnostic delay.
         *
         * The normal time subsystem may not be available if
         * initialization failed.
         */
        for (volatile uint32_t delay = 0U;
             delay < 4000000UL;
             ++delay)
        {
        }
    }
}

/*
 * --------------------------------------------------------------------------
 * CRC error response
 * --------------------------------------------------------------------------
 */

static bool BL_BootBuildCRCErrorResponse(
    const uint8_t *data,
    uint32_t length,
    BL_Packet_t *response)
{
    if (data == NULL)
    {
        return false;
    }

    if (response == NULL)
    {
        return false;
    }

    /*
     * Need at least:
     *
     * SOF        2
     * command    1
     * sequence   1
     * length     2
     */
    if (length < 6U)
    {
        return false;
    }

    /*
     * Verify SOF.
     */
    if (data[0] != 0x55U ||
        data[1] != 0xAAU)
    {
        return false;
    }

    memset(
        response,
        0,
        sizeof(*response));

    response->header.sof =
        BL_PACKET_SOF;

    response->header.command =
        BL_CMD_CRC_ERROR;

    response->header.sequence =
        data[3];

    response->header.payloadLength =
        0U;

    response->crc =
        BL_PacketCalculateCRC(
            response);

    return true;
}

/*
 * --------------------------------------------------------------------------
 * Response transmission
 * --------------------------------------------------------------------------
 */

static bool BL_BootSendResponse(
    const BL_Packet_t *response)
{
    uint32_t encodedLength;

    if (response == NULL)
    {
        return false;
    }

    memset(
        g_bootTxBuffer,
        0,
        sizeof(g_bootTxBuffer));

    if (BL_PacketEncode(
            response,
            g_bootTxBuffer,
            sizeof(g_bootTxBuffer),
            &encodedLength) != BL_PACKET_STATUS_OK)
    {
        return false;
    }

    return BL_TransportTransmit(
        g_bootTxBuffer,
        encodedLength);
}

/*
 * --------------------------------------------------------------------------
 * Action execution
 * --------------------------------------------------------------------------
 */

static void BL_BootExecuteAction(
    BL_CommandAction_t action)
{
    switch (action)
    {
        case BL_COMMAND_ACTION_RUN_APP:

            /*
             * Validate immediately before changing execution
             * context.
             */
            if (BL_ImageIsValid())
            {
                BL_JumpToApplication();
            }

            break;

        case BL_COMMAND_ACTION_RESET:

            /*
             * This should not return.
             */
            Board_Reset();

            break;

        case BL_COMMAND_ACTION_NONE:

        default:
            break;
    }
}

/*
 * --------------------------------------------------------------------------
 * Raw packet processing
 * --------------------------------------------------------------------------
 */

static bool BL_BootProcessRawPacket(
    const uint8_t *data,
    uint32_t length)
{
    BL_Packet_t request;
    BL_Packet_t response;
    BL_CommandAction_t action;
    BL_PacketStatus_t status;

    if (data == NULL)
    {
        return false;
    }

    if (length == 0U)
    {
        return false;
    }

    memset(
        &request,
        0,
        sizeof(request));

    memset(
        &response,
        0,
        sizeof(response));

    action =
        BL_COMMAND_ACTION_NONE;

    /*
     * Decode the complete packet.
     */
    status = BL_PacketDecode(
        data,
        length,
        &request);

    if (status != BL_PACKET_STATUS_OK)
    {
        /*
         * CRC failure has a dedicated protocol response.
         */
        if (status ==
            BL_PACKET_STATUS_INVALID_CRC)
        {
            if (BL_BootBuildCRCErrorResponse(
                    data,
                    length,
                    &response))
            {
                return BL_BootSendResponse(
                    &response);
            }
        }

        return false;
    }

    /*
     * Protocol layer performs:
     *
     *   command validation
     *   sequence validation
     *   flash operations
     *   image verification
     *   response construction
     */
    if (!BL_ProtocolProcessPacket(
            &request,
            &response,
            &action))
    {
        return false;
    }

    /*
     * Response must be transmitted before RUN_APP or RESET.
     */
    if (!BL_BootSendResponse(
            &response))
    {
        return false;
    }

    /*
     * Execute any deferred control-flow action.
     */
    BL_BootExecuteAction(
        action);

    return true;
}

/*
 * --------------------------------------------------------------------------
 * Transport data processing
 * --------------------------------------------------------------------------
 */

static bool BL_BootProcessTransportData(
    const uint8_t *data,
    uint32_t length)
{
    uint32_t offset;
    uint32_t consumed;
    uint32_t packetLength;
    bool packetReady;

    if (data == NULL)
    {
        return false;
    }

    if (length == 0U)
    {
        return true;
    }

    offset = 0U;

    while (offset < length)
    {
        consumed = 0U;
        packetReady = false;

        if (!BL_TransportPacketizerPush(
                &g_bootPacketizer,
                &data[offset],
                length - offset,
                &consumed,
                &packetReady))
        {
            /*
             * Reset after a malformed packet stream.
             */
            BL_TransportPacketizerReset(
                &g_bootPacketizer);

            return false;
        }

        offset += consumed;

        if (!packetReady)
        {
            /*
             * All bytes have been consumed but a complete packet
             * is not available yet.
             */
            break;
        }

        packetLength = 0U;

        if (!BL_TransportPacketizerGetPacket(
                &g_bootPacketizer,
                g_bootPacketBuffer,
                sizeof(g_bootPacketBuffer),
                &packetLength))
        {
            return false;
        }

        /*
         * Process one complete bootloader packet.
         */
        BL_BootProcessRawPacket(
            g_bootPacketBuffer,
            packetLength);

        /*
         * The loop continues if additional raw bytes remain.
         *
         * This allows:
         *
         *     packet A + packet B + packet C
         *
         * to be received in one transport operation.
         */
    }

    return true;
}

/*
 * --------------------------------------------------------------------------
 * Receive/process
 * --------------------------------------------------------------------------
 */

static bool BL_BootReceiveAndProcess(void)
{
    uint32_t receivedLength;

    receivedLength = 0U;

    if (!BL_TransportReceive(
            g_bootRxBuffer,
            sizeof(g_bootRxBuffer),
            &receivedLength))
    {
        return false;
    }

    if (receivedLength == 0U)
    {
        return false;
    }

    return BL_BootProcessTransportData(
        g_bootRxBuffer,
        receivedLength);
}

/*
 * --------------------------------------------------------------------------
 * Initialization
 * --------------------------------------------------------------------------
 */

bool BL_BootInit(void)
{
    memset(
        g_bootRxBuffer,
        0,
        sizeof(g_bootRxBuffer));

    memset(
        g_bootTxBuffer,
        0,
        sizeof(g_bootTxBuffer));

    memset(
        g_bootPacketBuffer,
        0,
        sizeof(g_bootPacketBuffer));

    /*
     * Board:
     *
     * clock
     * GPIO
     * LEDs
     * buttons
     */
    if (!Board_Init())
    {
        BL_BootHandleInitFailure();
        return false;
    }

    /*
     * Millisecond time base.
     */
    if (!BL_TimeInit())
    {
        BL_BootHandleInitFailure();
        return false;
    }

    /*
     * Packetizer.
     */
    BL_TransportPacketizerInit(
        &g_bootPacketizer);

    /*
     * Transport selection is compile-time controlled by:
     *
     *     BL_TRANSPORT
     */
    if (!BL_TransportInit(
            (BL_TransportType_t)BL_TRANSPORT))
    {
        BL_BootHandleInitFailure();
        return false;
    }

    /*
     * Protocol state.
     */
    BL_ProtocolInit();

    g_bootActivityTick =
        BL_TimeGetTick();

    return true;
}

/*
 * --------------------------------------------------------------------------
 * Main bootloader
 * --------------------------------------------------------------------------
 */

void BL_Boot(void)
{
    /*
     * If an application exists and no explicit update request
     * is present, automatically start it.
     */
    if (!BL_BootShouldEnterBootloader())
    {
        BL_JumpToApplication();

        /*
         * Should never return.
         */
        while (true)
        {
        }
    }

    /*
     * Bootloader mode.
     */
    BL_StateSet(
        BL_STATE_WAIT_CONNECTION);

    Board_LedOff(
        BOARD_LED_RED);

    Board_LedOff(
        BOARD_LED_BLUE);

    Board_LedOn(
        BOARD_LED_GREEN);

    while (true)
    {
        /*
         * Process currently available transport data.
         */
        if (BL_BootReceiveAndProcess())
        {
            g_bootActivityTick =
                BL_TimeGetTick();
        }

        /*
         * Transport-specific activity can also reset the
         * connection timeout.
         */
        if (BL_TransportActivity())
        {
            g_bootActivityTick =
                BL_TimeGetTick();
        }

        /*
         * Automatically leave bootloader after inactivity when
         * a valid application exists.
         */
        if (!Board_Sw2Pressed() &&
            BL_ImageIsValid())
        {
            if (BL_TimeTimeoutExpired(
                    g_bootActivityTick,
                    BL_BOOT_TIMEOUT_MS))
            {
                BL_JumpToApplication();
            }
        }

        /*
         * Alive indicator.
         */
        if ((BL_TimeGetTick() & 0x1FFUL) == 0U)
        {
            Board_LedToggle(
                BOARD_LED_GREEN);
        }
    }
}