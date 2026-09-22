#include "bl_transport_packetizer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * --------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------
 */

#define BL_PACKETIZER_SOF_BYTE_0       (0x55U)
#define BL_PACKETIZER_SOF_BYTE_1       (0xAAU)

#define BL_PACKETIZER_HEADER_SIZE      (6UL)
#define BL_PACKETIZER_CRC_SIZE         (4UL)

#define BL_PACKETIZER_MIN_PACKET_SIZE  \
    (BL_PACKETIZER_HEADER_SIZE + BL_PACKETIZER_CRC_SIZE)

#define BL_PACKETIZER_MAX_PACKET_SIZE  \
    (BL_PACKET_MAX_SIZE)

/*
 * --------------------------------------------------------------------------
 * Private functions
 * --------------------------------------------------------------------------
 */

static void BL_TransportPacketizerStart(
    BL_TransportPacketizer_t *packetizer)
{
    packetizer->length = 0U;
    packetizer->expectedLength = 0U;
    packetizer->state =
        BL_PACKETIZER_STATE_RECEIVING;
}

/*
 * --------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------
 */

void BL_TransportPacketizerInit(
    BL_TransportPacketizer_t *packetizer)
{
    if (packetizer == NULL)
    {
        return;
    }

    memset(
        packetizer,
        0,
        sizeof(*packetizer));

    packetizer->state =
        BL_PACKETIZER_STATE_IDLE;
}

void BL_TransportPacketizerReset(
    BL_TransportPacketizer_t *packetizer)
{
    if (packetizer == NULL)
    {
        return;
    }

    packetizer->length = 0U;
    packetizer->expectedLength = 0U;
    packetizer->state =
        BL_PACKETIZER_STATE_IDLE;
}

bool BL_TransportPacketizerPushByte(
    BL_TransportPacketizer_t *packetizer,
    uint8_t byte)
{
    uint16_t payloadLength;

    if (packetizer == NULL)
    {
        return false;
    }

    /*
     * A complete packet must be consumed before accepting more data.
     */
    if (packetizer->state ==
        BL_PACKETIZER_STATE_COMPLETE)
    {
        return false;
    }

    /*
     * An error requires an explicit reset.
     */
    if (packetizer->state ==
        BL_PACKETIZER_STATE_ERROR)
    {
        return false;
    }

    /*
     * ----------------------------------------------------------------------
     * Search for SOF.
     * ----------------------------------------------------------------------
     */
    if (packetizer->state ==
        BL_PACKETIZER_STATE_IDLE)
    {
        if (byte != BL_PACKETIZER_SOF_BYTE_0)
        {
            return true;
        }

        packetizer->buffer[0] = byte;
        packetizer->length = 1U;
        packetizer->expectedLength = 0U;

        packetizer->state =
            BL_PACKETIZER_STATE_RECEIVING;

        return true;
    }

    /*
     * ----------------------------------------------------------------------
     * Append byte.
     * ----------------------------------------------------------------------
     */

    if (packetizer->length >=
        BL_PACKETIZER_MAX_PACKET_SIZE)
    {
        packetizer->state =
            BL_PACKETIZER_STATE_ERROR;

        return false;
    }

    packetizer->buffer[
        packetizer->length
    ] = byte;

    ++packetizer->length;

    /*
     * ----------------------------------------------------------------------
     * Validate second SOF byte.
     * ----------------------------------------------------------------------
     */

    if (packetizer->length == 2U)
    {
        if (packetizer->buffer[1] !=
            BL_PACKETIZER_SOF_BYTE_1)
        {
            /*
             * The current byte may itself be the beginning of a
             * new SOF sequence.
             */
            if (byte == BL_PACKETIZER_SOF_BYTE_0)
            {
                packetizer->buffer[0] = byte;
                packetizer->length = 1U;

                return true;
            }

            BL_TransportPacketizerReset(
                packetizer);

            return true;
        }

        return true;
    }

    /*
     * ----------------------------------------------------------------------
     * Wait until complete packet header exists.
     * ----------------------------------------------------------------------
     *
     * Header:
     *
     *   byte 0: SOF low
     *   byte 1: SOF high
     *   byte 2: command
     *   byte 3: sequence
     *   byte 4: payload length low
     *   byte 5: payload length high
     */

    if (packetizer->length < BL_PACKETIZER_HEADER_SIZE)
    {
        return true;
    }

    /*
     * Calculate payload size.
     */
    payloadLength =
        (uint16_t)packetizer->buffer[4] |
        ((uint16_t)packetizer->buffer[5] << 8U);

    /*
     * Calculate complete packet length.
     */
    packetizer->expectedLength =
        BL_PACKETIZER_HEADER_SIZE +
        (uint32_t)payloadLength +
        BL_PACKETIZER_CRC_SIZE;

    /*
     * Validate packet size.
     */
    if (packetizer->expectedLength >
        BL_PACKETIZER_MAX_PACKET_SIZE)
    {
        packetizer->state =
            BL_PACKETIZER_STATE_ERROR;

        return false;
    }

    if (packetizer->expectedLength <
        BL_PACKETIZER_MIN_PACKET_SIZE)
    {
        packetizer->state =
            BL_PACKETIZER_STATE_ERROR;

        return false;
    }

    /*
     * ----------------------------------------------------------------------
     * Complete packet.
     * ----------------------------------------------------------------------
     */

    if (packetizer->length ==
        packetizer->expectedLength)
    {
        packetizer->state =
            BL_PACKETIZER_STATE_COMPLETE;
    }

    return true;
}

bool BL_TransportPacketizerPush(
    BL_TransportPacketizer_t *packetizer,
    const uint8_t *data,
    uint32_t length,
    uint32_t *consumed,
    bool *packetReady)
{
    if (packetizer == NULL)
    {
        return false;
    }

    if (data == NULL)
    {
        return false;
    }

    if (consumed == NULL)
    {
        return false;
    }

    if (packetReady == NULL)
    {
        return false;
    }

    *consumed = 0U;
    *packetReady = false;

    if (length == 0U)
    {
        return true;
    }

    for (uint32_t index = 0U;
         index < length;
         ++index)
    {
        if (!BL_TransportPacketizerPushByte(
                packetizer,
                data[index]))
        {
            /*
             * The byte causing the error was consumed from the
             * transport buffer.
             */
            *consumed = index + 1U;

            return false;
        }

        *consumed = index + 1U;

        if (packetizer->state ==
            BL_PACKETIZER_STATE_COMPLETE)
        {
            *packetReady = true;

            return true;
        }
    }

    return true;
}

bool BL_TransportPacketizerIsPacketReady(
    const BL_TransportPacketizer_t *packetizer)
{
    if (packetizer == NULL)
    {
        return false;
    }

    return (
        packetizer->state ==
        BL_PACKETIZER_STATE_COMPLETE);
}

bool BL_TransportPacketizerGetPacket(
    BL_TransportPacketizer_t *packetizer,
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *packetLength)
{
    uint32_t length;

    if (packetizer == NULL)
    {
        return false;
    }

    if (buffer == NULL)
    {
        return false;
    }

    if (packetLength == NULL)
    {
        return false;
    }

    *packetLength = 0U;

    if (packetizer->state !=
        BL_PACKETIZER_STATE_COMPLETE)
    {
        return false;
    }

    length = packetizer->length;

    if (bufferSize < length)
    {
        return false;
    }

    memcpy(
        buffer,
        packetizer->buffer,
        length);

    *packetLength = length;

    BL_TransportPacketizerReset(
        packetizer);

    return true;
}

BL_PacketizerState_t BL_TransportPacketizerGetState(
    const BL_TransportPacketizer_t *packetizer)
{
    if (packetizer == NULL)
    {
        return BL_PACKETIZER_STATE_ERROR;
    }

    return packetizer->state;
}

uint32_t BL_TransportPacketizerGetLength(
    const BL_TransportPacketizer_t *packetizer)
{
    if (packetizer == NULL)
    {
        return 0U;
    }

    return packetizer->length;
}

uint32_t BL_TransportPacketizerGetExpectedLength(
    const BL_TransportPacketizer_t *packetizer)
{
    if (packetizer == NULL)
    {
        return 0U;
    }

    return packetizer->expectedLength;
}