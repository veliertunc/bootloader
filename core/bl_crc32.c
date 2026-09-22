#include "bl_crc32.h"

#include <stdint.h>

#define BL_CRC32_INITIAL_VALUE        (0xFFFFFFFFUL)

uint32_t BL_CRC32_Update(
    uint32_t crc,
    const uint8_t *data,
    uint32_t length)
{
    uint32_t i;
    uint32_t bit;

    if ((data == 0) && (length != 0UL))
    {
        return crc;
    }

    for (i = 0UL; i < length; i++)
    {
        crc ^= (uint32_t)data[i];

        for (bit = 0UL; bit < 8UL; bit++)
        {
            if ((crc & 1UL) != 0UL)
            {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

uint32_t BL_CRC32_Calculate(
    const uint8_t *data,
    uint32_t length)
{
    uint32_t crc;

    if ((data == 0) && (length != 0UL))
    {
        return 0UL;
    }

    crc = BL_CRC32_INITIAL_VALUE;

    crc = BL_CRC32_Update(
        crc,
        data,
        length);

    return ~crc;
}