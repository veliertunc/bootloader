#ifndef BL_TRANSPORT_PACKETIZER_H
#define BL_TRANSPORT_PACKETIZER_H

#include <stdbool.h>
#include <stdint.h>

#include "bl_packet.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    BL_PACKETIZER_STATE_IDLE = 0,
    BL_PACKETIZER_STATE_RECEIVING,
    BL_PACKETIZER_STATE_COMPLETE,
    BL_PACKETIZER_STATE_ERROR
} BL_PacketizerState_t;

typedef struct
{
    uint8_t buffer[BL_PACKET_MAX_SIZE];

    uint32_t length;

    uint32_t expectedLength;

    BL_PacketizerState_t state;

} BL_TransportPacketizer_t;

/*
 * Initialize the packetizer.
 */
void BL_TransportPacketizerInit(
    BL_TransportPacketizer_t *packetizer);

/*
 * Reset the packetizer.
 *
 * Any incomplete packet is discarded.
 */
void BL_TransportPacketizerReset(
    BL_TransportPacketizer_t *packetizer);

/*
 * Push one byte into the packetizer.
 */
bool BL_TransportPacketizerPushByte(
    BL_TransportPacketizer_t *packetizer,
    uint8_t byte);

/*
 * Push raw transport data into the packetizer.
 *
 * consumed:
 *     Number of bytes consumed from the input buffer.
 *
 * packetReady:
 *     Set to true when a complete packet has been assembled.
 *
 * Important:
 *
 * The function may consume fewer bytes than "length".
 *
 * Example:
 *
 *     [packet A][packet B]
 *
 * The first call can return:
 *
 *     consumed = length of packet A
 *     packetReady = true
 *
 * The caller can then process packet A and feed the remaining
 * bytes containing packet B on the next call.
 */
bool BL_TransportPacketizerPush(
    BL_TransportPacketizer_t *packetizer,
    const uint8_t *data,
    uint32_t length,
    uint32_t *consumed,
    bool *packetReady);

/*
 * Returns true when a complete packet is available.
 */
bool BL_TransportPacketizerIsPacketReady(
    const BL_TransportPacketizer_t *packetizer);

/*
 * Copies the complete encoded packet into buffer.
 *
 * The packetizer is reset after successful extraction.
 */
bool BL_TransportPacketizerGetPacket(
    BL_TransportPacketizer_t *packetizer,
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *packetLength);

/*
 * Returns current packetizer state.
 */
BL_PacketizerState_t BL_TransportPacketizerGetState(
    const BL_TransportPacketizer_t *packetizer);

/*
 * Returns number of bytes currently buffered.
 */
uint32_t BL_TransportPacketizerGetLength(
    const BL_TransportPacketizer_t *packetizer);

/*
 * Returns expected complete packet length.
 *
 * Returns zero while the packet header is incomplete.
 */
uint32_t BL_TransportPacketizerGetExpectedLength(
    const BL_TransportPacketizer_t *packetizer);

#ifdef __cplusplus
}
#endif

#endif /* BL_TRANSPORT_PACKETIZER_H */