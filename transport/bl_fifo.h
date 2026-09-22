#ifndef BL_FIFO_H
#define BL_FIFO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint8_t *buffer;

    uint32_t size;

    volatile uint32_t head;
    volatile uint32_t tail;

} BL_FIFO_t;

void BL_FIFO_Init(
    BL_FIFO_t *fifo,
    uint8_t *storage,
    uint32_t size);

void BL_FIFO_Reset(
    BL_FIFO_t *fifo);

bool BL_FIFO_Push(
    BL_FIFO_t *fifo,
    uint8_t value);

bool BL_FIFO_Pop(
    BL_FIFO_t *fifo,
    uint8_t *value);

bool BL_FIFO_IsEmpty(
    const BL_FIFO_t *fifo);

bool BL_FIFO_IsFull(
    const BL_FIFO_t *fifo);

uint32_t BL_FIFO_Count(
    const BL_FIFO_t *fifo);

uint32_t BL_FIFO_Free(
    const BL_FIFO_t *fifo);

#ifdef __cplusplus
}
#endif

#endif