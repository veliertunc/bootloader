#ifndef BL_CRC32_H
#define BL_CRC32_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint32_t BL_CRC32_Calculate(
    const uint8_t *data,
    uint32_t length);

uint32_t BL_CRC32_Update(
    uint32_t crc,
    const uint8_t *data,
    uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* BL_CRC32_H */