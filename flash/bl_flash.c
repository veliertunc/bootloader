#include "bl_flash.h"

#include "bl_config.h"
#include "bl_memory.h"

#include "driverlib/flash.h"

#include <stddef.h>
#include <stdint.h>

/*
 * TM4C123 flash programming granularity used by this bootloader.
 */
#define BL_FLASH_PROGRAM_ALIGNMENT    (4UL)

/*
 * Erased flash value.
 */
#define BL_FLASH_ERASED_VALUE         (0xFFU)

/*
 * Convert four little-endian bytes into a uint32_t.
 *
 * The bootloader protocol and generated image are explicitly
 * little-endian, so we do not rely on the compiler's handling
 * of an unaligned uint32_t pointer.
 */
static uint32_t BL_FlashBytesToWord(
    const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

/**
 * @brief Verify that a flash range contains erased bytes.
 */
static bool BL_FlashVerifyErased(
    uint32_t address,
    uint32_t size)
{
    const volatile uint8_t *flash;
    uint32_t index;

    flash = (const volatile uint8_t *)address;

    for (index = 0UL; index < size; ++index)
    {
        if (flash[index] != BL_FLASH_ERASED_VALUE)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Verify programmed flash against source data.
 */
static bool BL_FlashVerifyData(
    uint32_t address,
    const uint8_t *data,
    uint32_t size)
{
    const volatile uint8_t *flash;
    uint32_t index;

    if (data == NULL)
    {
        return false;
    }

    flash = (const volatile uint8_t *)address;

    for (index = 0UL; index < size; ++index)
    {
        if (flash[index] != data[index])
        {
            return false;
        }
    }

    return true;
}

bool BL_FlashInit(void)
{
    /*
     * The TM4C123 flash controller does not require a separate
     * initialization sequence through DriverLib before FlashErase()
     * or FlashProgram().
     *
     * Keep this function as the abstraction boundary so additional
     * flash-controller initialization can be introduced later
     * without changing the rest of the bootloader.
     */
    return true;
}

bool BL_FlashAddressValid(
    uint32_t address,
    uint32_t size)
{
    /*
     * The most important protection in the flash layer:
     *
     * never allow bootloader commands to access
     * 0x00000000 - 0x00007FFF.
     *
     * The update area intentionally begins at 0x8000 because it
     * contains:
     *
     *     0x8000 - 0x801F : image header
     *     0x8020 - 0x83FF : reserved
     *     0x8400 - 0x3FFF : application
     */
    if (!BL_MemoryRangeInUpdateArea(
            address,
            size))
    {
        return false;
    }

    return true;
}

BL_FlashStatus_t BL_FlashEraseSector(
    uint32_t address)
{
    /*
     * A sector address must be aligned to the TM4C123
     * flash sector size.
     */
    if (!BL_MemoryAddressAligned(
            address,
            BL_FLASH_SECTOR_SIZE))
    {
        return BL_FLASH_STATUS_ERASE_ALIGNMENT_ERROR;
    }

    /*
     * The complete sector must belong to the update area.
     */
    if (!BL_MemoryRangeInUpdateArea(
            address,
            BL_FLASH_SECTOR_SIZE))
    {
        return BL_FLASH_STATUS_INVALID_ADDRESS;
    }

    /*
     * DriverLib performs the actual flash erase.
     */
    FlashErase(address);

    /*
     * Verify the complete sector after erasing.
     *
     * This gives the bootloader a deterministic error instead
     * of assuming that the controller operation succeeded.
     */
    if (!BL_FlashVerifyErased(
            address,
            BL_FLASH_SECTOR_SIZE))
    {
        return BL_FLASH_STATUS_ERASE_ERROR;
    }

    return BL_FLASH_STATUS_OK;
}

BL_FlashStatus_t BL_FlashEraseRange(
    uint32_t address,
    uint32_t size)
{
    uint32_t currentAddress;
    uint32_t remainingSize;

    if (size == 0UL)
    {
        return BL_FLASH_STATUS_INVALID_SIZE;
    }

    /*
     * Erase operations must start on a sector boundary.
     */
    if (!BL_MemoryAddressAligned(
            address,
            BL_FLASH_SECTOR_SIZE))
    {
        return BL_FLASH_STATUS_ERASE_ALIGNMENT_ERROR;
    }

    /*
     * The requested size must represent an integer number
     * of sectors.
     */
    if (!BL_MemoryAddressAligned(
            size,
            BL_FLASH_SECTOR_SIZE))
    {
        return BL_FLASH_STATUS_ERASE_ALIGNMENT_ERROR;
    }

    /*
     * Check the complete range before modifying any flash.
     *
     * This is important: if the request crosses into the
     * bootloader region, do not erase even the valid portion.
     */
    if (!BL_FlashAddressValid(
            address,
            size))
    {
        return BL_FLASH_STATUS_INVALID_ADDRESS;
    }

    currentAddress = address;
    remainingSize = size;

    while (remainingSize > 0UL)
    {
        BL_FlashStatus_t status;

        status = BL_FlashEraseSector(
            currentAddress);

        if (status != BL_FLASH_STATUS_OK)
        {
            return status;
        }

        currentAddress += BL_FLASH_SECTOR_SIZE;
        remainingSize -= BL_FLASH_SECTOR_SIZE;
    }

    return BL_FLASH_STATUS_OK;
}

BL_FlashStatus_t BL_FlashWrite(
    uint32_t address,
    const uint8_t *data,
    uint32_t size)
{
    uint32_t currentAddress;
    uint32_t remainingSize;
    const uint8_t *currentData;

    if (data == NULL)
    {
        return BL_FLASH_STATUS_PROGRAM_ERROR;
    }

    if (size == 0UL)
    {
        return BL_FLASH_STATUS_INVALID_SIZE;
    }

    /*
     * TM4C123 programming is performed as 32-bit words.
     */
    if (!BL_MemoryAddressAligned(
            address,
            BL_FLASH_PROGRAM_ALIGNMENT))
    {
        return BL_FLASH_STATUS_ALIGNMENT_ERROR;
    }

    if (!BL_MemoryAddressAligned(
            size,
            BL_FLASH_PROGRAM_ALIGNMENT))
    {
        return BL_FLASH_STATUS_ALIGNMENT_ERROR;
    }

    /*
     * Never permit programming outside the update area.
     *
     * This prevents a malformed host packet from modifying
     * the bootloader itself.
     */
    if (!BL_FlashAddressValid(
            address,
            size))
    {
        return BL_FLASH_STATUS_INVALID_ADDRESS;
    }

    currentAddress = address;
    currentData = data;
    remainingSize = size;

    while (remainingSize > 0UL)
    {
        uint32_t word;
        long result;

        /*
         * Convert explicitly from the bootloader's wire/image
         * little-endian representation to the uint32_t expected
         * by DriverLib.
         */
        word = BL_FlashBytesToWord(
            currentData);

        /*
         * FlashProgram() programs one 32-bit word here.
         *
         * The source is a local aligned uint32_t, so we never
         * pass the caller's potentially unaligned byte buffer
         * directly to DriverLib.
         */
        result = FlashProgram(
            &word,
            currentAddress,
            sizeof(word));

        if (result != 0L)
        {
            return BL_FLASH_STATUS_PROGRAM_ERROR;
        }

        /*
         * Verify this word immediately.
         *
         * Early verification makes the failure location
         * deterministic and prevents continuing after a
         * programming failure.
         */
        if (!BL_FlashVerifyData(
                currentAddress,
                currentData,
                sizeof(word)))
        {
            return BL_FLASH_STATUS_PROGRAM_ERROR;
        }

        currentAddress += sizeof(word);
        currentData += sizeof(word);
        remainingSize -= sizeof(word);
    }

    return BL_FLASH_STATUS_OK;
}