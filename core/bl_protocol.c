#include "bl_protocol.h"

#include <stddef.h>
#include <string.h>

#include "bl_config.h"
#include "bl_context.h"
#include "bl_crc32.h"
#include "bl_flash.h"
#include "bl_image.h"
#include "bl_memory.h"
#include "bl_state.h"
#include "bl_transport.h"

/*
 * --------------------------------------------------------------------------
 * Internal protocol constants
 * --------------------------------------------------------------------------
 */

#define BL_PROTOCOL_ERASE_PAYLOAD_SIZE       (8U)
#define BL_PROTOCOL_WRITE_HEADER_SIZE        (8U)
#define BL_PROTOCOL_VERIFY_PAYLOAD_SIZE      (8U)

#define BL_PROTOCOL_VERSION_PAYLOAD_SIZE     (4U)
#define BL_PROTOCOL_INFO_PAYLOAD_SIZE        (24U)

#define BL_PROTOCOL_ACK_PAYLOAD_SIZE         (1U)
#define BL_PROTOCOL_ERROR_PAYLOAD_SIZE       (1U)

/*
 * Error payload values.
 *
 * The response command already identifies the broad error category.
 * The payload provides the command that caused the error.
 */
#define BL_PROTOCOL_ERROR_PAYLOAD_COMMAND_INDEX    (0U)

/*
 * --------------------------------------------------------------------------
 * Duplicate response cache
 * --------------------------------------------------------------------------
 *
 * A host may retransmit a packet because it did not receive the response.
 *
 * If the same command/sequence pair arrives again, we return the cached
 * response instead of executing the command again.
 *
 * This is especially important for:
 *
 *   ERASE
 *   WRITE
 *   VERIFY
 *
 * because these commands can have side effects.
 */

static bool g_responseCacheValid;
static uint8_t g_responseCacheCommand;
static uint8_t g_responseCacheSequence;
static BL_Packet_t g_responseCache;

/*
 * --------------------------------------------------------------------------
 * Local helpers
 * --------------------------------------------------------------------------
 */

static void BL_ProtocolClearCache(void)
{
    g_responseCacheValid = false;
    g_responseCacheCommand = 0U;
    g_responseCacheSequence = 0U;

    memset(&g_responseCache, 0, sizeof(g_responseCache));
}

static void BL_ProtocolBuildResponse(
    BL_Packet_t *response,
    uint8_t command,
    uint8_t sequence)
{
    memset(response, 0, sizeof(*response));

    response->header.sof = BL_PACKET_SOF;
    response->header.command = command;
    response->header.sequence = sequence;
    response->header.payloadLength = 0U;
    response->crc = 0U;
}

static void BL_ProtocolSetPayload(
    BL_Packet_t *response,
    const uint8_t *payload,
    uint16_t payloadLength)
{
    if ((payload != NULL) && (payloadLength > 0U))
    {
        memcpy(
            response->payload,
            payload,
            payloadLength);
    }

    response->header.payloadLength = payloadLength;
}

static void BL_ProtocolFinalizeResponse(BL_Packet_t *response)
{
    response->crc = BL_PacketCalculateCRC(response);
}

static void BL_ProtocolCacheResponse(
    const BL_Packet_t *request,
    const BL_Packet_t *response)
{
    g_responseCache = *response;

    g_responseCacheValid = true;
    g_responseCacheCommand = request->header.command;
    g_responseCacheSequence = request->header.sequence;
}

static bool BL_ProtocolIsDuplicate(
    const BL_Packet_t *request)
{
    if (!g_responseCacheValid)
    {
        return false;
    }

    if (g_responseCacheCommand != request->header.command)
    {
        return false;
    }

    if (g_responseCacheSequence != request->header.sequence)
    {
        return false;
    }

    return true;
}

static void BL_ProtocolCopyCachedResponse(
    BL_Packet_t *response)
{
    *response = g_responseCache;
}

static void BL_ProtocolBuildAck(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    uint8_t payload;

    payload = request->header.command;

    BL_ProtocolBuildResponse(
        response,
        BL_CMD_ACK,
        request->header.sequence);

    BL_ProtocolSetPayload(
        response,
        &payload,
        BL_PROTOCOL_ACK_PAYLOAD_SIZE);

    BL_ProtocolFinalizeResponse(response);
}

static void BL_ProtocolBuildError(
    const BL_Packet_t *request,
    BL_ResponseCommand_t errorCommand,
    BL_Packet_t *response)
{
    uint8_t payload;

    payload = request->header.command;

    BL_ProtocolBuildResponse(
        response,
        (uint8_t)errorCommand,
        request->header.sequence);

    BL_ProtocolSetPayload(
        response,
        &payload,
        BL_PROTOCOL_ERROR_PAYLOAD_SIZE);

    BL_ProtocolFinalizeResponse(response);
}

static void BL_ProtocolBuildSequenceError(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    uint8_t payload;

    payload = BL_ContextGetExpectedSequence();

    BL_ProtocolBuildResponse(
        response,
        BL_CMD_SEQUENCE_ERROR,
        request->header.sequence);

    BL_ProtocolSetPayload(
        response,
        &payload,
        1U);

    BL_ProtocolFinalizeResponse(response);
}

static uint32_t BL_ProtocolReadU32(
    const uint8_t *data)
{
    return
        ((uint32_t)data[0] << 0U) |
        ((uint32_t)data[1] << 8U) |
        ((uint32_t)data[2] << 16U) |
        ((uint32_t)data[3] << 24U);
}

static void BL_ProtocolWriteU32(
    uint8_t *data,
    uint32_t value)
{
    data[0] = (uint8_t)(value >> 0U);
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static void BL_ProtocolAdvanceSequence(void)
{
    uint8_t sequence;

    sequence = BL_ContextGetExpectedSequence();

    sequence++;

    BL_ContextSetExpectedSequence(sequence);
}

/*
 * --------------------------------------------------------------------------
 * Command handlers
 * --------------------------------------------------------------------------
 */

static bool BL_ProtocolHandleConnect(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    /*
     * CONNECT establishes a new protocol session.
     *
     * The CONNECT sequence itself must be accepted with the current
     * expected sequence.
     */
    BL_ContextReset();

    BL_ContextSetActive(true);

    BL_ContextSetExpectedSequence(
        (uint8_t)(request->header.sequence + 1U));

    BL_StateSet(BL_STATE_WAIT_CONNECTION);

    BL_ProtocolBuildAck(request, response);

    return true;
}

static bool BL_ProtocolHandleDisconnect(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    BL_ProtocolBuildAck(request, response);

    BL_ContextSetActive(false);

    BL_ContextSetExpectedSequence(
        (uint8_t)(request->header.sequence + 1U));

    BL_StateSet(BL_STATE_IDLE);

    return true;
}

static bool BL_ProtocolHandleGetVersion(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    uint8_t payload[BL_PROTOCOL_VERSION_PAYLOAD_SIZE];

    BL_ProtocolWriteU32(
        payload,
        BL_BOOTLOADER_VERSION);

    BL_ProtocolBuildResponse(
        response,
        BL_CMD_ACK,
        request->header.sequence);

    BL_ProtocolSetPayload(
        response,
        payload,
        sizeof(payload));

    BL_ProtocolFinalizeResponse(response);

    return true;
}

static bool BL_ProtocolHandleGetInfo(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    BL_InfoResponse_t info;
    uint8_t payload[BL_PROTOCOL_INFO_PAYLOAD_SIZE];

    memset(&info, 0, sizeof(info));

    info.bootloaderVersion = BL_BOOTLOADER_VERSION;
    info.flashStart = BL_FLASH_START_ADDRESS;
    info.flashSize = BL_FLASH_SIZE;
    info.applicationStart = BL_APP_START_ADDRESS;
    info.applicationMaxSize = BL_APP_MAX_SIZE;

    /*
     * UART and CAN are currently implemented transports.
     *
     * SPI/I2C/USB capability bits can be enabled as each transport
     * becomes production-ready.
     */
    info.capabilities =
        BL_CAPABILITY_UART |
        BL_CAPABILITY_CAN;

    BL_ProtocolWriteU32(
        &payload[0],
        info.bootloaderVersion);

    BL_ProtocolWriteU32(
        &payload[4],
        info.flashStart);

    BL_ProtocolWriteU32(
        &payload[8],
        info.flashSize);

    BL_ProtocolWriteU32(
        &payload[12],
        info.applicationStart);

    BL_ProtocolWriteU32(
        &payload[16],
        info.applicationMaxSize);

    BL_ProtocolWriteU32(
        &payload[20],
        info.capabilities);

    BL_ProtocolBuildResponse(
        response,
        BL_CMD_ACK,
        request->header.sequence);

    BL_ProtocolSetPayload(
        response,
        payload,
        sizeof(payload));

    BL_ProtocolFinalizeResponse(response);

    return true;
}

static bool BL_ProtocolHandleErase(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    uint32_t address;
    uint32_t size;
    BL_FlashStatus_t status;

    if (request->header.payloadLength !=
        BL_PROTOCOL_ERASE_PAYLOAD_SIZE)
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    address = BL_ProtocolReadU32(
        &request->payload[0]);

    size = BL_ProtocolReadU32(
        &request->payload[4]);

    if (!BL_MemoryRangeInUpdateArea(address, size))
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    BL_StateSet(BL_STATE_ERASE);

    status = BL_FlashEraseRange(
        address,
        size);

    if (status != BL_FLASH_STATUS_OK)
    {
        BL_StateSet(BL_STATE_ERROR);

        BL_ProtocolBuildError(
            request,
            BL_CMD_FLASH_ERROR,
            response);

        return true;
    }

    /*
     * The host generally sends the application image beginning at
     * BL_APP_HEADER_ADDRESS.
     */
    BL_ContextSetStartAddress(address);
    BL_ContextSetBytesReceived(0UL);

    BL_StateSet(BL_STATE_PROGRAM);

    BL_ProtocolBuildAck(
        request,
        response);

    return true;
}

static bool BL_ProtocolHandleWrite(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    uint32_t address;
    uint32_t size;
    uint32_t dataSize;
    BL_FlashStatus_t status;

    if (request->header.payloadLength <
        BL_PROTOCOL_WRITE_HEADER_SIZE)
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    address = BL_ProtocolReadU32(
        &request->payload[0]);

    size = BL_ProtocolReadU32(
        &request->payload[4]);

    dataSize =
        (uint32_t)request->header.payloadLength -
        BL_PROTOCOL_WRITE_HEADER_SIZE;

    /*
     * The declared size must exactly match the actual packet data.
     */
    if (size != dataSize)
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    if (size == 0UL)
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    if (!BL_MemoryRangeInUpdateArea(address, size))
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    /*
     * Application image data is programmed in multiples of 4 bytes.
     */
    if (!BL_MemoryAddressAligned(address, 4UL) ||
        !BL_MemoryAddressAligned(size, 4UL))
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    BL_StateSet(BL_STATE_PROGRAM);

    status = BL_FlashWrite(
        address,
        &request->payload[BL_PROTOCOL_WRITE_HEADER_SIZE],
        size);

    if (status != BL_FLASH_STATUS_OK)
    {
        BL_StateSet(BL_STATE_ERROR);

        BL_ProtocolBuildError(
            request,
            BL_CMD_FLASH_ERROR,
            response);

        return true;
    }

    BL_ContextSetBytesReceived(
        BL_ContextGetBytesReceived() + size);

    BL_ProtocolBuildAck(
        request,
        response);

    return true;
}

static bool BL_ProtocolHandleVerify(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    uint32_t imageSize;
    uint32_t imageCRC;
    BL_ImageStatus_t status;

    if (request->header.payloadLength !=
        BL_PROTOCOL_VERIFY_PAYLOAD_SIZE)
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    imageSize = BL_ProtocolReadU32(
        &request->payload[0]);

    imageCRC = BL_ProtocolReadU32(
        &request->payload[4]);

    if (imageSize == 0UL)
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_VERIFY_ERROR,
            response);

        return true;
    }

    BL_ContextSetImageSize(imageSize);
    BL_ContextSetImageCRC(imageCRC);

    BL_StateSet(BL_STATE_VERIFY);

    status = BL_ImageVerify(
        imageSize,
        imageCRC);

    if (status != BL_IMAGE_STATUS_OK)
    {
        BL_StateSet(BL_STATE_ERROR);

        BL_ProtocolBuildError(
            request,
            BL_CMD_VERIFY_ERROR,
            response);

        return true;
    }

    BL_StateSet(BL_STATE_COMPLETE);

    BL_ProtocolBuildAck(
        request,
        response);

    return true;
}

static bool BL_ProtocolHandleAbort(
    const BL_Packet_t *request,
    BL_Packet_t *response)
{
    BL_ProtocolBuildAck(
        request,
        response);

    BL_ContextReset();

    BL_ContextSetActive(true);

    BL_StateSet(BL_STATE_WAIT_CONNECTION);

    return true;
}

static bool BL_ProtocolHandleRunApplication(
    const BL_Packet_t *request,
    BL_Packet_t *response,
    BL_CommandAction_t *action)
{
    if (!BL_ImageIsValid())
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_VERIFY_ERROR,
            response);

        return true;
    }

    BL_ProtocolBuildAck(
        request,
        response);

    *action = BL_COMMAND_ACTION_RUN_APP;

    return true;
}

static bool BL_ProtocolHandleReset(
    const BL_Packet_t *request,
    BL_Packet_t *response,
    BL_CommandAction_t *action)
{
    BL_ProtocolBuildAck(
        request,
        response);

    *action = BL_COMMAND_ACTION_RESET;

    return true;
}

/*
 * --------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------
 */

void BL_ProtocolInit(void)
{
    BL_ProtocolClearCache();

    BL_ContextSetExpectedSequence(0U);
}

bool BL_ProtocolProcessPacket(
    const BL_Packet_t *request,
    BL_Packet_t *response,
    BL_CommandAction_t *action)
{
    uint8_t expectedSequence;
    bool success;

    if ((request == NULL) ||
        (response == NULL) ||
        (action == NULL))
    {
        return false;
    }

    *action = BL_COMMAND_ACTION_NONE;

    /*
     * CONNECT is special:
     *
     * It establishes a new session and therefore must be handled
     * before requiring an active session.
     */
    if (request->header.command == BL_CMD_CONNECT)
    {
        /*
         * Duplicate CONNECT should be handled by the normal cache
         * only if it belongs to the currently active session.
         */
        if (BL_ProtocolIsDuplicate(request))
        {
            BL_ProtocolCopyCachedResponse(response);

            return true;
        }

        success = BL_ProtocolHandleConnect(
            request,
            response);

        if (success)
        {
            BL_ProtocolCacheResponse(
                request,
                response);
        }

        return success;
    }

    /*
     * All other commands require an active protocol session.
     */
    if (!BL_ContextIsActive())
    {
        BL_ProtocolBuildError(
            request,
            BL_CMD_NACK,
            response);

        return true;
    }

    /*
     * Duplicate request:
     *
     * Return exactly the response generated previously.
     *
     * Do not execute the command again.
     */
    if (BL_ProtocolIsDuplicate(request))
    {
        BL_ProtocolCopyCachedResponse(response);

        return true;
    }

    /*
     * Sequence must match the expected sequence number.
     */
    expectedSequence =
        BL_ContextGetExpectedSequence();

    if (request->header.sequence != expectedSequence)
    {
        BL_ProtocolBuildSequenceError(
            request,
            response);

        /*
         * Sequence errors are intentionally not cached and do not
         * advance the sequence counter.
         */
        return true;
    }

    success = false;

    switch (request->header.command)
    {
        case BL_CMD_DISCONNECT:
        {
            success = BL_ProtocolHandleDisconnect(
                request,
                response);

            break;
        }

        case BL_CMD_GET_VERSION:
        {
            success = BL_ProtocolHandleGetVersion(
                request,
                response);

            break;
        }

        case BL_CMD_GET_INFO:
        {
            success = BL_ProtocolHandleGetInfo(
                request,
                response);

            break;
        }

        case BL_CMD_ERASE:
        {
            success = BL_ProtocolHandleErase(
                request,
                response);

            break;
        }

        case BL_CMD_WRITE:
        {
            success = BL_ProtocolHandleWrite(
                request,
                response);

            break;
        }

        case BL_CMD_VERIFY:
        {
            success = BL_ProtocolHandleVerify(
                request,
                response);

            break;
        }

        case BL_CMD_ABORT:
        {
            success = BL_ProtocolHandleAbort(
                request,
                response);

            break;
        }

        case BL_CMD_RUN_APP:
        {
            success = BL_ProtocolHandleRunApplication(
                request,
                response,
                action);

            break;
        }

        case BL_CMD_RESET:
        {
            success = BL_ProtocolHandleReset(
                request,
                response,
                action);

            break;
        }

        default:
        {
            BL_ProtocolBuildError(
                request,
                BL_CMD_INVALID_CMD,
                response);

            /*
             * Invalid commands are not successful protocol commands.
             * Therefore the sequence is not advanced.
             */
            return true;
        }
    }

    if (!success)
    {
        return false;
    }

    /*
     * Only successful protocol processing advances the sequence.
     *
     * Error responses such as FLASH_ERROR and VERIFY_ERROR are cached,
     * but the sequence must NOT advance.
     *
     * ACK is identified by the response command.
     */
    if (response->header.command == BL_CMD_ACK)
    {
        BL_ProtocolAdvanceSequence();
    }

    /*
     * Cache the response for retransmission.
     *
     * This also caches deterministic error responses, which prevents
     * duplicate packets from repeatedly executing a failed operation.
     */
    BL_ProtocolCacheResponse(
        request,
        response);

    return true;
}

static uint32_t BL_ProtocolGetCapabilities(void)
{
    switch (BL_TransportGetType())
    {
        case BL_TRANSPORT_UART:
            return BL_CAPABILITY_UART;

        case BL_TRANSPORT_CAN:
            return BL_CAPABILITY_CAN;

        case BL_TRANSPORT_SPI:
            return BL_CAPABILITY_SPI;

        case BL_TRANSPORT_I2C:
            return BL_CAPABILITY_I2C;

        case BL_TRANSPORT_USB:
            return BL_CAPABILITY_USB;

        default:
            return 0UL;
    }
}