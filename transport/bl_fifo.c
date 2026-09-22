#include "bl_fifo.h"

#include <stddef.h>

void BL_FIFO_Init(
    BL_FIFO_t *fifo,
    uint8_t *storage,
    uint32_t size)
{
    if ((fifo == NULL) ||
        (storage == NULL) ||
        (size < 2U))
    {
        return;
    }

    fifo->buffer = storage;
    fifo->size = size;
    fifo->head = 0U;
    fifo->tail = 0U;
}

void BL_FIFO_Reset(
    BL_FIFO_t *fifo)
{
    if (fifo == NULL)
    {
        return;
    }

    fifo->head = 0U;
    fifo->tail = 0U;
}

bool BL_FIFO_IsEmpty(
    const BL_FIFO_t *fifo)
{
    if (fifo == NULL)
    {
        return true;
    }

    return fifo->head == fifo->tail;
}

bool BL_FIFO_IsFull(
    const BL_FIFO_t *fifo)
{
    uint32_t nextHead;

    if (fifo == NULL)
    {
        return true;
    }

    nextHead =
        fifo->head + 1U;

    if (nextHead >= fifo->size)
    {
        nextHead = 0U;
    }

    return nextHead == fifo->tail;
}

bool BL_FIFO_Push(
    BL_FIFO_t *fifo,
    uint8_t value)
{
    uint32_t nextHead;

    if ((fifo == NULL) ||
        (fifo->buffer == NULL))
    {
        return false;
    }

    nextHead =
        fifo->head + 1U;

    if (nextHead >= fifo->size)
    {
        nextHead = 0U;
    }

    if (nextHead == fifo->tail)
    {
        return false;
    }

    fifo->buffer[fifo->head] =
        value;

    fifo->head = nextHead;

    return true;
}

bool BL_FIFO_Pop(
    BL_FIFO_t *fifo,
    uint8_t *value)
{
    uint32_t nextTail;

    if ((fifo == NULL) ||
        (fifo->buffer == NULL) ||
        (value == NULL))
    {
        return false;
    }

    if (fifo->head == fifo->tail)
    {
        return false;
    }

    *value =
        fifo->buffer[fifo->tail];

    nextTail =
        fifo->tail + 1U;

    if (nextTail >= fifo->size)
    {
        nextTail = 0U;
    }

    fifo->tail = nextTail;

    return true;
}

uint32_t BL_FIFO_Count(
    const BL_FIFO_t *fifo)
{
    if (fifo == NULL)
    {
        return 0U;
    }

    if (fifo->head >= fifo->tail)
    {
        return fifo->head -
               fifo->tail;
    }

    return
        fifo->size -
        fifo->tail +
        fifo->head;
}

uint32_t BL_FIFO_Free(
    const BL_FIFO_t *fifo)
{
    if (fifo == NULL)
    {
        return 0U;
    }

    /*
     * One slot is deliberately left unused so that
     * head == tail unambiguously means "empty".
     */
    return
        (fifo->size - 1U) -
        BL_FIFO_Count(fifo);
}