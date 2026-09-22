#ifndef BL_CAN_FRAGMENT_H
#define BL_CAN_FRAGMENT_H

#include <stdbool.h>
#include <stdint.h>

#include "bl_packet.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BL_CAN_DATA_BYTES_PER_FRAME     (6U)

typedef enum
{
    BL_CAN_FRAME_START = 0x01,
    BL_CAN_FRAME_DATA  = 0x02,
    BL_CAN_FRAME_END   = 0x03,
    BL_CAN_FRAME_ACK   = 0x04,
    BL_CAN_FRAME_NACK  = 0x05
} BL_CANFrameType_t;

typedef enum
{
    BL_CAN_STATE_IDLE = 0,
    BL_CAN_STATE_RECEIVING,
    BL_CAN_STATE_COMPLETE,
    BL_CAN_STATE_ERROR
} BL_CANState_t;

typedef struct
{
    uint8_t data[8];

} BL_CANFrame_t;

typedef struct
{
    uint8_t buffer[BL_PACKET_MAX_SIZE];

    uint32_t length;

    uint32_t expectedLength;

    uint8_t nextSequence;

    BL_CANState_t state;

} BL_CANReassembly_t;

typedef struct
{
    const uint8_t *buffer;

    uint32_t length;

    uint32_t offset;

    uint8_t sequence;

    bool startSent;
    bool endSent;

} BL_CANFragmentation_t;

void BL_CANFragment_Init(
    BL_CANReassembly_t *context);

void BL_CANFragment_Reset(
    BL_CANReassembly_t *context);

bool BL_CANFragment_ProcessFrame(
    BL_CANReassembly_t *context,
    const BL_CANFrame_t *frame);

bool BL_CANFragment_PacketReady(
    const BL_CANReassembly_t *context);

uint32_t BL_CANFragment_GetPacket(
    BL_CANReassembly_t *context,
    uint8_t *buffer,
    uint32_t bufferSize);

void BL_CANFragmentation_Init(
    BL_CANFragmentation_t *context,
    const uint8_t *buffer,
    uint32_t length);

bool BL_CANFragmentation_NextFrame(
    BL_CANFragmentation_t *context,
    BL_CANFrame_t *frame);

#ifdef __cplusplus
}
#endif

#endif