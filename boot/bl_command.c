#include "bl_command.h"

#include <stddef.h>
#include <string.h>

#include "driverlib/sysctl.h"

#include "bl_context.h"
#include "bl_config.h"
#include "bl_transport.h"
#include "bl_flash.h"
#include "bl_image.h"
#include "bl_jump.h"
#include "bl_memory.h"
#include "bl_state.h"
#include "bl_version.h"

static uint32_t BL_CommandReadU32(
    const uint8_t *data)
{
    return
        ((uint32_t)data[0]) |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static void BL_CommandWriteU32(
    uint8_t *data,
    uint32_t value)
{
    data[0] =
        (uint8_t)(value & 0xFFUL);

    data[1] =
        (uint8_t)((value >> 8) & 0xFFUL);

    data[2] =
        (uint8_t)((value >> 16) & 0xFFUL);

    data[3] =
        (uint8_t)((value >> 24) & 0xFFUL);
}

static void BL_CommandResponseInit(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)memset(
        response,
        0,
        sizeof(*response));

    response->command =
        BL_CMD_NACK;

    response->sequence =
        packet->header.sequence;

    response->payloadLength = 4U;
}

static void BL_CommandSetStatus(
    BL_Response_t *response,
    BL_CommandStatus_t status)
{
    response->payloadLength = 4U;

    BL_CommandWriteU32(
        response->payload,
        (uint32_t)status);
}

static void BL_CommandSetAck(
    BL_Response_t *response)
{
    response->command =
        BL_CMD_ACK;

    BL_CommandSetStatus(
        response,
        BL_COMMAND_STATUS_OK);
}

static void BL_CommandSetError(
    BL_Response_t *response,
    BL_CommandStatus_t status)
{
    response->command =
        BL_CMD_NACK;

    BL_CommandSetStatus(
        response,
        status);
}

static BL_CommandStatus_t BL_CommandConnect(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    BL_ContextReset();

    BL_ContextSetActive(true);

    BL_ContextSetStartAddress(
        BL_APP_START_ADDRESS);

    BL_StateSet(
        BL_STATE_WAIT_CONNECTION);

    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandDisconnect(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    BL_ContextSetActive(false);

    BL_StateSet(
        BL_STATE_IDLE);

    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandGetVersion(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    response->command =
        BL_CMD_ACK;

    response->payloadLength = 12U;

    BL_CommandWriteU32(
        &response->payload[0],
        BL_VERSION_MAJOR);

    BL_CommandWriteU32(
        &response->payload[4],
        BL_VERSION_MINOR);

    BL_CommandWriteU32(
        &response->payload[8],
        BL_VERSION_PATCH);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandGetInfo(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    response->command =
        BL_CMD_ACK;

    response->payloadLength = 24U;

    BL_CommandWriteU32(
        &response->payload[0],
        BL_FLASH_START_ADDRESS);

    BL_CommandWriteU32(
        &response->payload[4],
        BL_FLASH_END_ADDRESS);

    BL_CommandWriteU32(
        &response->payload[8],
        BL_APP_START_ADDRESS);

    BL_CommandWriteU32(
        &response->payload[12],
        BL_APP_END_ADDRESS);

    BL_CommandWriteU32(
        &response->payload[16],
        BL_APP_MAX_SIZE);

    /*
     * Transport capability mask.
     *
     * UART + CAN currently enabled.
     */
    BL_CommandWriteU32(
        &response->payload[20],
        (1UL << BL_TRANSPORT_RS232) |
        (1UL << BL_TRANSPORT_RS485) |
        (1UL << BL_TRANSPORT_SPI) |
        (1UL << BL_TRANSPORT_I2C) |
        (1UL << BL_TRANSPORT_ETHERNET));

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandErase(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    uint32_t address;
    uint32_t size;

    if (packet->header.payloadLength != 8U)
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    address =
        BL_CommandReadU32(
            &packet->payload[0]);

    size =
        BL_CommandReadU32(
            &packet->payload[4]);

    if (!BL_MemoryRangeInUpdateArea(
            address,
            size))
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    if (!BL_MemoryAddressAligned(
            address,
            BL_FLASH_SECTOR_SIZE))
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    if (!BL_MemoryAddressAligned(
            size,
            BL_FLASH_SECTOR_SIZE))
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    BL_StateSet(
        BL_STATE_ERASE);

    if (BL_FlashEraseRange(
            address,
            size) != BL_FLASH_STATUS_OK)
    {
        BL_StateSet(
            BL_STATE_ERROR);

        response->command =
            BL_CMD_FLASH_ERROR;

        response->payloadLength = 0U;

        return BL_COMMAND_STATUS_FLASH_ERROR;
    }

    BL_ContextSetStartAddress(
        address);

    BL_ContextSetBytesReceived(0UL);

    BL_StateSet(
        BL_STATE_PROGRAM);

    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandWrite(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    uint32_t address;
    uint32_t size;

    if (packet->header.payloadLength < 8U)
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    address =
        BL_CommandReadU32(
            &packet->payload[0]);

    size =
        BL_CommandReadU32(
            &packet->payload[4]);

    if (size == 0UL)
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    if (size >
        (BL_PACKET_MAX_PAYLOAD_SIZE - 8UL))
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    if (packet->header.payloadLength !=
        (uint16_t)(8UL + size))
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    if (!BL_MemoryAddressAligned(
            address,
            4UL) ||
        !BL_MemoryAddressAligned(
            size,
            4UL))
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    if (!BL_MemoryRangeInUpdateArea(
            address,
            size))
    {
        BL_CommandSetError(
            response,
            BL_COMMAND_STATUS_INVALID_PARAMETER);

        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    BL_StateSet(
        BL_STATE_PROGRAM);

    if (BL_FlashWrite(
            address,
            &packet->payload[8],
            size) != BL_FLASH_STATUS_OK)
    {
        BL_StateSet(
            BL_STATE_ERROR);

        response->command =
            BL_CMD_FLASH_ERROR;

        response->payloadLength = 0U;

        return BL_COMMAND_STATUS_FLASH_ERROR;
    }

    BL_ContextSetBytesReceived(
        BL_ContextGetBytesReceived() + size);

    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandVerify(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    BL_StateSet(
        BL_STATE_VERIFY);

    if (!BL_ImageIsValid())
    {
        BL_StateSet(
            BL_STATE_ERROR);

        response->command =
            BL_CMD_VERIFY_ERROR;

        response->payloadLength = 0U;

        return BL_COMMAND_STATUS_VERIFY_ERROR;
    }

    BL_StateSet(
        BL_STATE_COMPLETE);

    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandAbort(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    BL_ContextReset();

    BL_StateSet(
        BL_STATE_IDLE);

    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandRunApplication(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    if (!BL_ImageIsValid())
    {
        response->command =
            BL_CMD_VERIFY_ERROR;

        response->payloadLength = 0U;

        return BL_COMMAND_STATUS_VERIFY_ERROR;
    }

    /*
     * The protocol layer transmits the ACK first.
     * The actual jump is performed after transmission by
     * the boot command path.
     */
    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

static BL_CommandStatus_t BL_CommandReset(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    (void)packet;

    BL_CommandSetAck(response);

    return BL_COMMAND_STATUS_OK;
}

BL_CommandStatus_t BL_CommandProcess(
    const BL_Packet_t *packet,
    BL_Response_t *response)
{
    if ((packet == NULL) ||
        (response == NULL))
    {
        return BL_COMMAND_STATUS_INVALID_PARAMETER;
    }

    BL_CommandResponseInit(
        packet,
        response);

    switch (packet->header.command)
    {
        case BL_CMD_CONNECT:
            return BL_CommandConnect(
                packet,
                response);

        case BL_CMD_DISCONNECT:
            return BL_CommandDisconnect(
                packet,
                response);

        case BL_CMD_GET_VERSION:
            return BL_CommandGetVersion(
                packet,
                response);

        case BL_CMD_GET_INFO:
            return BL_CommandGetInfo(
                packet,
                response);

        case BL_CMD_ERASE:
            return BL_CommandErase(
                packet,
                response);

        case BL_CMD_WRITE:
            return BL_CommandWrite(
                packet,
                response);

        case BL_CMD_VERIFY:
            return BL_CommandVerify(
                packet,
                response);

        case BL_CMD_ABORT:
            return BL_CommandAbort(
                packet,
                response);

        case BL_CMD_RUN_APP:
            return BL_CommandRunApplication(
                packet,
                response);

        case BL_CMD_RESET:
            return BL_CommandReset(
                packet,
                response);

        default:
            response->command =
                BL_CMD_INVALID_CMD;

            response->payloadLength = 0U;

            return BL_COMMAND_STATUS_INVALID_COMMAND;
    }
}
