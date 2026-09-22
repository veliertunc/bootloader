#include "bl_context.h"

#include <string.h>

static BL_Context_t g_Context;

void BL_ContextInit(void)
{
    memset(
        &g_Context,
        0,
        sizeof(g_Context));
}

void BL_ContextReset(void)
{
    bool active;

    active =
        g_Context.active;

    memset(
        &g_Context,
        0,
        sizeof(g_Context));

    g_Context.active =
        active;
}

void BL_ContextReset(void)
{
    bool active;

    active =
        g_Context.active;

    memset(
        &g_Context,
        0,
        sizeof(g_Context));

    g_Context.active =
        active;
}

void BL_ContextReset(void)
{
    memset(
        &g_Context,
        0,
        sizeof(g_Context));
}

bool BL_ContextIsActive(void)
{
    return g_Context.active;
}

void BL_ContextSetActive(
    bool active)
{
    g_Context.active =
        active;
}

uint8_t
BL_ContextGetExpectedSequence(void)
{
    return
        g_Context.expectedSequence;
}

void BL_ContextSetExpectedSequence(
    uint8_t sequence)
{
    g_Context.expectedSequence =
        sequence;
}

uint32_t
BL_ContextGetStartAddress(void)
{
    return
        g_Context.startAddress;
}

void BL_ContextSetStartAddress(
    uint32_t address)
{
    g_Context.startAddress =
        address;
}

uint32_t
BL_ContextGetBytesReceived(void)
{
    return
        g_Context.bytesReceived;
}

void BL_ContextSetBytesReceived(
    uint32_t bytes)
{
    g_Context.bytesReceived =
        bytes;
}

void BL_ContextAddBytesReceived(
    uint32_t bytes)
{
    g_Context.bytesReceived +=
        bytes;
}

uint32_t
BL_ContextGetImageSize(void)
{
    return
        g_Context.imageSize;
}

void BL_ContextSetImageSize(
    uint32_t size)
{
    g_Context.imageSize =
        size;
}

uint32_t
BL_ContextGetImageCRC(void)
{
    return
        g_Context.imageCRC;
}

void BL_ContextSetImageCRC(
    uint32_t crc)
{
    g_Context.imageCRC =
        crc;
}

const BL_Context_t *
BL_ContextGet(void)
{
    return
        &g_Context;
}

