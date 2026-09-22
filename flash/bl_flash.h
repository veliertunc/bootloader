#ifndef BL_FLASH_H
#define BL_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    BL_FLASH_STATUS_OK = 0,
    BL_FLASH_STATUS_INVALID_ADDRESS,
    BL_FLASH_STATUS_INVALID_SIZE,
    BL_FLASH_STATUS_ALIGNMENT_ERROR,
    BL_FLASH_STATUS_ERASE_ALIGNMENT_ERROR,
    BL_FLASH_STATUS_ERASE_ERROR,
    BL_FLASH_STATUS_PROGRAM_ERROR
} BL_FlashStatus_t;

/**
 * @brief Initialize the flash layer.
 *
 * @return true if the flash configuration is valid.
 */
bool BL_FlashInit(void);

/**
 * @brief Check whether a flash range can be accessed by the bootloader.
 *
 * The range must be inside the update area:
 *
 *     0x00008000 - 0x0003FFFF
 *
 * @param address Start address.
 * @param size    Number of bytes.
 *
 * @return true if the complete range is valid.
 */
bool BL_FlashAddressValid(
    uint32_t address,
    uint32_t size);

/**
 * @brief Erase one flash sector.
 *
 * The address must be aligned to BL_FLASH_SECTOR_SIZE.
 *
 * @param address Sector start address.
 *
 * @return Flash operation status.
 */
BL_FlashStatus_t BL_FlashEraseSector(
    uint32_t address);

/**
 * @brief Erase a range of flash.
 *
 * Both the start address and size must be sector aligned.
 *
 * @param address Start address.
 * @param size    Number of bytes to erase.
 *
 * @return Flash operation status.
 */
BL_FlashStatus_t BL_FlashEraseRange(
    uint32_t address,
    uint32_t size);

/**
 * @brief Program flash.
 *
 * TM4C123 flash programming is performed in 32-bit words.
 *
 * The address and size must therefore be 4-byte aligned.
 *
 * @param address Destination flash address.
 * @param data    Source data.
 * @param size    Number of bytes.
 *
 * @return Flash operation status.
 */
BL_FlashStatus_t BL_FlashWrite(
    uint32_t address,
    const uint8_t *data,
    uint32_t size);

#ifdef __cplusplus
}
#endif

#endif