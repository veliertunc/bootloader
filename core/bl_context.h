#ifndef BL_CONTEXT_H
#define BL_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    /*
     * Protocol
     */
    bool active;

    uint8_t expectedSequence;

    /*
     * Transfer
     */
    uint32_t startAddress;

    uint32_t bytesReceived;

    /*
     * Image
     */
    uint32_t imageSize;

    uint32_t imageCRC;

} BL_Context_t;

/*
 * Initialize context subsystem.
 */
void BL_ContextInit(void);

/*
 * Reset current update session.
 */
void BL_ContextReset(void);

/*
 * Active session.
 */
bool BL_ContextIsActive(void);

void BL_ContextSetActive(
    bool active);

/*
 * Sequence.
 */
uint8_t BL_ContextGetExpectedSequence(void);

void BL_ContextSetExpectedSequence(
    uint8_t sequence);

/*
 * Start address.
 */
uint32_t BL_ContextGetStartAddress(void);

void BL_ContextSetStartAddress(
    uint32_t address);

/*
 * Bytes received.
 */
uint32_t BL_ContextGetBytesReceived(void);

void BL_ContextSetBytesReceived(
    uint32_t bytes);

void BL_ContextAddBytesReceived(
    uint32_t bytes);

/*
 * Image size.
 */
uint32_t BL_ContextGetImageSize(void);

void BL_ContextSetImageSize(
    uint32_t size);

/*
 * Image CRC.
 */
uint32_t BL_ContextGetImageCRC(void);

void BL_ContextSetImageCRC(
    uint32_t crc);

/*
 * Direct access.
 */
const BL_Context_t *
BL_ContextGet(void);

#ifdef __cplusplus
}
#endif

#endif