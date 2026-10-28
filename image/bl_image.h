#ifndef BL_IMAGE_H
#define BL_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BL_IMAGE_STATUS_OK = 0,
    BL_IMAGE_STATUS_NULL_POINTER,
    BL_IMAGE_STATUS_INVALID_HEADER,
    BL_IMAGE_STATUS_INVALID_MAGIC,
    BL_IMAGE_STATUS_INVALID_HEADER_VERSION,
    BL_IMAGE_STATUS_INVALID_IMAGE_VERSION,
    BL_IMAGE_STATUS_INVALID_SIZE,
    BL_IMAGE_STATUS_INVALID_MSP,
    BL_IMAGE_STATUS_INVALID_RESET_HANDLER,
    BL_IMAGE_STATUS_INVALID_VECTOR_TABLE,
    BL_IMAGE_STATUS_INVALID_CRC
} BL_ImageStatus_t;

typedef struct
{
    uint32_t magic;
    uint32_t headerVersion;
    uint32_t imageVersion;
    uint32_t imageSize;
    uint32_t imageCRC;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
} BL_ImageHeader_t;

BL_ImageStatus_t BL_ImageReadHeader(BL_ImageHeader_t *header);
BL_ImageStatus_t BL_ImageValidateHeader(const BL_ImageHeader_t *header);
BL_ImageStatus_t BL_ImageValidateVectorTable(const BL_ImageHeader_t *header);
uint32_t BL_ImageCalculateCRC(const BL_ImageHeader_t *header);
BL_ImageStatus_t BL_ImageVerifyHeader(const BL_ImageHeader_t *header);
BL_ImageStatus_t BL_ImageVerify(uint32_t expectedSize, uint32_t expectedCRC);
BL_ImageStatus_t BL_ImageValidate(void);
bool BL_ImageIsValid(void);
uint32_t BL_ImageGetSize(void);
uint32_t BL_ImageGetCRC(void);
uint32_t BL_ImageGetVersion(void);

#endif
