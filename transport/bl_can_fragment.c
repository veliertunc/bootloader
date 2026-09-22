#include "bl_can_fragment.h"

#include <stddef.h>
#include <string.h>

void BL_CANFragment_Init(
    BL_CANReassembly_t *context)
{
    if (context == NULL)
    {
        return;
    }

    memset(
        context,
        0,
        sizeof(*context));

    context->state =
        BL_CAN_STATE_IDLE;
}

void BL_CANFragment_Reset(
    BL_CANReassembly_t *context)
{
    BL_CANFragment_Init(
        context);
}

static bool BL_CANProcessStartFrame(
    BL_CANReassembly_t *context,
    const BL_CANFrame_t *frame)
{
    uint32_t length;

    if ((context == NULL) ||
        (frame == NULL))
    {
        return false;
    }

    length =
        (uint32_t)frame->data[2] |
        ((uint32_t)frame->data[3] << 8U);

    if ((length == 0U) ||
        (length > BL_PACKET_MAX_SIZE))
    {
        context->state =
            BL_CAN_STATE_ERROR;

        return false;
    }

    context->length = 0U;

    context->expectedLength =
        length;

    context->nextSequence = 1U;

    context->state =
        BL_CAN_STATE_RECEIVING;

    return true;
}

static bool BL_CANProcessDataFrame(
    BL_CANReassembly_t *context,
    const BL_CANFrame_t *frame)
{
    uint32_t remaining;
    uint32_t copyLength;

    if ((context == NULL) ||
        (frame == NULL))
    {
        return false;
    }

    if (context->state !=
        BL_CAN_STATE_RECEIVING)
    {
        return false;
    }

    if (frame->data[1] !=
        context->nextSequence)
    {
        context->state =
            BL_CAN_STATE_ERROR;

        return false;
    }

    remaining =
        context->expectedLength -
        context->length;

    copyLength =
        remaining >
        BL_CAN_DATA_BYTES_PER_FRAME
        ? BL_CAN_DATA_BYTES_PER_FRAME
        : remaining;

    if (copyLength == 0U)
    {
        context->state =
            BL_CAN_STATE_ERROR;

        return false;
    }

    memcpy(
        &context->buffer[
            context->length],
        &frame->data[2],
        copyLength);

    context->length +=
        copyLength;

    context->nextSequence++;

    return true;
}

static bool BL_CANProcessEndFrame(
    BL_CANReassembly_t *context,
    const BL_CANFrame_t *frame)
{
    if ((context == NULL) ||
        (frame == NULL))
    {
        return false;
    }

    if (context->state !=
        BL_CAN_STATE_RECEIVING)
    {
        return false;
    }

    if (frame->data[1] !=
        context->nextSequence)
    {
        context->state =
            BL_CAN_STATE_ERROR;

        return false;
    }

    if (context->length !=
        context->expectedLength)
    {
        context->state =
            BL_CAN_STATE_ERROR;

        return false;
    }

    context->state =
        BL_CAN_STATE_COMPLETE;

    return true;
}

bool BL_CANFragment_ProcessFrame(
    BL_CANReassembly_t *context,
    const BL_CANFrame_t *frame)
{
    if ((context == NULL) ||
        (frame == NULL))
    {
        return false;
    }

    switch (frame->data[0])
    {
        case BL_CAN_FRAME_START:

            /*
             * A new START always begins a new transaction.
             */
            return BL_CANProcessStartFrame(
                context,
                frame);

        case BL_CAN_FRAME_DATA:

            return BL_CANProcessDataFrame(
                context,
                frame);

        case BL_CAN_FRAME_END:

            return BL_CANProcessEndFrame(
                context,
                frame);

        default:

            context->state =
                BL_CAN_STATE_ERROR;

            return false;
    }
}

bool BL_CANFragment_PacketReady(
    const BL_CANReassembly_t *context)
{
    if (context == NULL)
    {
        return false;
    }

    return context->state ==
           BL_CAN_STATE_COMPLETE;
}

uint32_t BL_CANFragment_GetPacket(
    BL_CANReassembly_t *context,
    uint8_t *buffer,
    uint32_t bufferSize)
{
    uint32_t length;

    if ((context == NULL) ||
        (buffer == NULL))
    {
        return 0U;
    }

    if (!BL_CANFragment_PacketReady(
            context))
    {
        return 0U;
    }

    length =
        context->length;

    if (length > bufferSize)
    {
        return 0U;
    }

    memcpy(
        buffer,
        context->buffer,
        length);

    BL_CANFragment_Reset(
        context);

    return length;
}

void BL_CANFragmentation_Init(
    BL_CANFragmentation_t *context,
    const uint8_t *buffer,
    uint32_t length)
{
    if (context == NULL)
    {
        return;
    }

    memset(
        context,
        0,
        sizeof(*context));

    context->buffer = buffer;
    context->length = length;
}

static void BL_CANBuildStartFrame(
    BL_CANFragmentation_t *context,
    BL_CANFrame_t *frame)
{
    memset(
        frame,
        0,
        sizeof(*frame));

    frame->data[0] =
        BL_CAN_FRAME_START;

    frame->data[1] =
        context->sequence++;

    frame->data[2] =
        (uint8_t)(
            context->length & 0xFFU);

    frame->data[3] =
        (uint8_t)(
            (context->length >> 8U) & 0xFFU);

    context->startSent = true;
}

static void BL_CANBuildDataFrame(
    BL_CANFragmentation_t *context,
    BL_CANFrame_t *frame)
{
    uint32_t remaining;
    uint32_t count;

    memset(
        frame,
        0,
        sizeof(*frame));

    frame->data[0] =
        BL_CAN_FRAME_DATA;

    frame->data[1] =
        context->sequence++;

    remaining =
        context->length -
        context->offset;

    count =
        remaining >
        BL_CAN_DATA_BYTES_PER_FRAME
        ? BL_CAN_DATA_BYTES_PER_FRAME
        : remaining;

    memcpy(
        &frame->data[2],
        &context->buffer[
            context->offset],
        count);

    context->offset +=
        count;
}

static void BL_CANBuildEndFrame(
    BL_CANFragmentation_t *context,
    BL_CANFrame_t *frame)
{
    memset(
        frame,
        0,
        sizeof(*frame));

    frame->data[0] =
        BL_CAN_FRAME_END;

    frame->data[1] =
        context->sequence++;

    context->endSent = true;
}

bool BL_CANFragmentation_NextFrame(
    BL_CANFragmentation_t *context,
    BL_CANFrame_t *frame)
{
    if ((context == NULL) ||
        (frame == NULL))
    {
        return false;
    }

    if (!context->startSent)
    {
        BL_CANBuildStartFrame(
            context,
            frame);

        return true;
    }

    if (context->offset <
        context->length)
    {
        BL_CANBuildDataFrame(
            context,
            frame);

        return true;
    }

    if (!context->endSent)
    {
        BL_CANBuildEndFrame(
            context,
            frame);

        return true;
    }

    return false;
}