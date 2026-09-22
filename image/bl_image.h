#ifndef BL_IMAGE_H
#define BL_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BL_IMAGE_MAGIC  (0x424C4150UL)
/*
 * "BLAP"
 */

typedef enum
{
    BL_IMAGE_STATUS_OK = 0,

    BL_IMAGE_STATUS_INVALID_HEADER,

    BL_IMAGE_STATUS_INVALID_MAGIC,

    BL_IMAGE_STATUS_INVALID_SIZE,

    BL_IMAGE_STATUS_INVALID_MSP,

    BL_IMAGE_STATUS_INVALID_RESET_HANDLER,

    BL_IMAGE_STATUS_INVALID_CRC

} BL_ImageStatus_t;

typedef struct
{
    uint32_t magic;

    uint32_t version;

    uint32_t imageSize;

    uint32_t imageCRC;

    uint32_t reserved0;

    uint32_t reserved1;

    uint32_t reserved2;

    uint32_t reserved3;

} BL_ImageHeader_t;

BL_ImageStatus_t BL_ImageVerify(
    uint32_t imageSize,
    uint32_t imageCRC);

bool BL_ImageIsValid(void);

const BL_ImageHeader_t *
BL_ImageGetHeader(void);

bool BL_ImageReadHeader(
    BL_ImageHeader_t *header);

#ifdef __cplusplus
}
#endif

#endif