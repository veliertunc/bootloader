#include "bl_packet.h"

#include <stdint.h>
#include <stdbool.h>

#include "bl_crc32.h"

static void BL_PacketWriteU16(
    uint8_t *buffer,
    uint16_t value)
{
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8U);
}

static uint16_t BL_PacketReadU16(
    const uint8_t *buffer)
{
    return
        (uint16_t)buffer[0] |
        ((uint16_t)buffer[1] << 8U);
}

static void BL_PacketWriteU32(
    uint8_t *buffer,
    uint32_t value)
{
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8U);
    buffer[2] = (uint8_t)(value >> 16U);
    buffer[3] = (uint8_t)(value >> 24U);
}

static uint32_t BL_PacketReadU32(
    const uint8_t *buffer)
{
    return
        ((uint32_t)buffer[0]) |
        ((uint32_t)buffer[1] << 8U) |
        ((uint32_t)buffer[2] << 16U) |
        ((uint32_t)buffer[3] << 24U);
}

uint32_t BL_PacketCalculateCRC(
    const BL_Packet_t *packet)
{
    uint8_t data[
        BL_PACKET_HEADER_SIZE +
        BL_PACKET_MAX_PAYLOAD_SIZE];

    uint32_t index;

    if (packet == NULL)
    {
        return 0UL;
    }

    if (packet->header.payloadLength >
        BL_PACKET_MAX_PAYLOAD_SIZE)
    {
        return 0UL;
    }

    /*
     * Header.
     */
    BL_PacketWriteU16(
        &data[0],
        BL_PACKET_SOF);

    data[2] =
        packet->header.command;

    data[3] =
        packet->header.sequence;

    BL_PacketWriteU16(
        &data[4],
        packet->header.payloadLength);

    /*
     * Payload.
     */
    for (index = 0UL;
         index < packet->header.payloadLength;
         index++)
    {
        data[
            BL_PACKET_HEADER_SIZE + index] =
            packet->payload[index];
    }

    return BL_CRC32_Calculate(
        data,
        BL_PACKET_HEADER_SIZE +
        packet->header.payloadLength);
}

bool BL_PacketVerifyCRC(
    const BL_Packet_t *packet)
{
    uint32_t calculatedCRC;

    if (packet == NULL)
    {
        return false;
    }

    calculatedCRC =
        BL_PacketCalculateCRC(packet);

    return (
        calculatedCRC == packet->crc);
}

BL_PacketStatus_t BL_PacketEncode(
    const BL_Packet_t *packet,
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *encodedLength)
{
    uint32_t totalLength;
    uint32_t index;

    if ((packet == NULL) ||
        (buffer == NULL) ||
        (encodedLength == NULL))
    {
        return BL_PACKET_STATUS_NULL_POINTER;
    }

    if (packet->header.payloadLength >
        BL_PACKET_MAX_PAYLOAD_SIZE)
    {
        return BL_PACKET_STATUS_INVALID_LENGTH;
    }

    totalLength =
        BL_PACKET_HEADER_SIZE +
        packet->header.payloadLength +
        BL_PACKET_CRC_SIZE;

    if (bufferSize < totalLength)
    {
        return BL_PACKET_STATUS_BUFFER_TOO_SMALL;
    }

    /*
     * SOF.
     */
    BL_PacketWriteU16(
        &buffer[0],
        BL_PACKET_SOF);

    /*
     * Header.
     */
    buffer[2] =
        packet->header.command;

    buffer[3] =
        packet->header.sequence;

    BL_PacketWriteU16(
        &buffer[4],
        packet->header.payloadLength);

    /*
     * Payload.
     */
    for (index = 0UL;
         index < packet->header.payloadLength;
         index++)
    {
        buffer[
            BL_PACKET_HEADER_SIZE + index] =
            packet->payload[index];
    }

    /*
     * CRC.
     */
    BL_PacketWriteU32(
        &buffer[
            BL_PACKET_HEADER_SIZE +
            packet->header.payloadLength],
        packet->crc);

    *encodedLength = totalLength;

    return BL_PACKET_STATUS_OK;
}

BL_PacketStatus_t BL_PacketDecode(
    const uint8_t *buffer,
    uint32_t length,
    BL_Packet_t *packet)
{
    uint16_t payloadLength;
    uint32_t expectedLength;
    uint32_t index;

    if ((buffer == NULL) ||
        (packet == NULL))
    {
        return BL_PACKET_STATUS_NULL_POINTER;
    }

    if (length < BL_PACKET_MIN_SIZE)
    {
        return BL_PACKET_STATUS_INVALID_LENGTH;
    }

    if (BL_PacketReadU16(&buffer[0]) !=
        BL_PACKET_SOF)
    {
        return BL_PACKET_STATUS_INVALID_SOF;
    }

    payloadLength =
        BL_PacketReadU16(&buffer[4]);

    if (payloadLength >
        BL_PACKET_MAX_PAYLOAD_SIZE)
    {
        return BL_PACKET_STATUS_INVALID_LENGTH;
    }

    expectedLength =
        BL_PACKET_HEADER_SIZE +
        payloadLength +
        BL_PACKET_CRC_SIZE;

    if (length != expectedLength)
    {
        return BL_PACKET_STATUS_INVALID_LENGTH;
    }

    packet->header.sof =
        BL_PACKET_SOF;

    packet->header.command =
        buffer[2];

    packet->header.sequence =
        buffer[3];

    packet->header.payloadLength =
        payloadLength;

    for (index = 0UL;
         index < payloadLength;
         index++)
    {
        packet->payload[index] =
            buffer[
                BL_PACKET_HEADER_SIZE + index];
    }

    packet->crc =
        BL_PacketReadU32(
            &buffer[
                BL_PACKET_HEADER_SIZE +
                payloadLength]);

    if (!BL_PacketVerifyCRC(packet))
    {
        return BL_PACKET_STATUS_INVALID_CRC;
    }

    return BL_PACKET_STATUS_OK;
}