#include "bl_memory.h"

#include "bl_config.h"

/*
 * ============================================================================
 * Private helpers
 * ============================================================================
 */

/**
 * @brief Check whether an address is within an inclusive address range.
 *
 * @param address Address to check.
 * @param start   First valid address.
 * @param end     Last valid address.
 *
 * @return true if the address is inside the range.
 * @return false otherwise.
 */
static bool BL_MemoryAddressInRange(
    uint32_t address,
    uint32_t start,
    uint32_t end)
{
    return (address >= start) &&
           (address <= end);
}

/**
 * @brief Check whether an address range is completely inside an inclusive
 *        memory range.
 *
 * The function assumes the caller has already verified that @p size is
 * non-zero and that address + size does not overflow.
 *
 * @param address Address at which the range starts.
 * @param size    Number of bytes in the range.
 * @param start   First valid address.
 * @param end     Last valid address.
 *
 * @return true if the complete range is inside the requested region.
 * @return false otherwise.
 */
static bool BL_MemoryRangeInRange(
    uint32_t address,
    uint32_t size,
    uint32_t start,
    uint32_t end)
{
    uint32_t lastAddress;

    if (!BL_MemoryRangeValid(address, size))
    {
        return false;
    }

    /*
     * BL_MemoryRangeValid() guarantees that this calculation cannot
     * overflow.
     */
    lastAddress = address + size - 1UL;

    return (address >= start) &&
           (lastAddress <= end);
}

/*
 * ============================================================================
 * Public API
 * ============================================================================
 */

bool BL_MemoryAddressAligned(
    uint32_t address,
    uint32_t alignment)
{
    /*
     * Alignment of zero is meaningless.
     */
    if (alignment == 0UL)
    {
        return false;
    }

    /*
     * Alignment must be a power of two.
     *
     * Examples:
     *
     *     1  -> valid
     *     2  -> valid
     *     4  -> valid
     *     8  -> valid
     *     16 -> valid
     *
     *     3  -> invalid
     *     6  -> invalid
     */
    if ((alignment & (alignment - 1UL)) != 0UL)
    {
        return false;
    }

    return (address & (alignment - 1UL)) == 0UL;
}

bool BL_MemoryRangeValid(
    uint32_t address,
    uint32_t size)
{
    uint32_t lastAddress;

    /*
     * A zero-length memory operation is not considered a valid memory
     * range by the bootloader.
     */
    if (size == 0UL)
    {
        return false;
    }

    /*
     * Calculate the last byte rather than address + size.
     *
     * Example:
     *
     * address = 0x1000
     * size    = 4
     *
     * valid bytes:
     *
     * 0x1000
     * 0x1001
     * 0x1002
     * 0x1003
     *
     * Therefore:
     *
     * lastAddress = address + size - 1
     */
    lastAddress = address + size - 1UL;

    /*
     * Unsigned overflow causes the resulting value to wrap below the
     * starting address.
     */
    if (lastAddress < address)
    {
        return false;
    }

    return true;
}

bool BL_MemoryAddressInFlash(
    uint32_t address)
{
    return BL_MemoryAddressInRange(
        address,
        BL_FLASH_START_ADDRESS,
        BL_FLASH_END_ADDRESS);
}

bool BL_MemoryRangeInFlash(
    uint32_t address,
    uint32_t size)
{
    return BL_MemoryRangeInRange(
        address,
        size,
        BL_FLASH_START_ADDRESS,
        BL_FLASH_END_ADDRESS);
}

bool BL_MemoryAddressInBootloader(
    uint32_t address)
{
    return BL_MemoryAddressInRange(
        address,
        BL_BOOT_START_ADDRESS,
        BL_BOOT_END_ADDRESS);
}

bool BL_MemoryRangeInBootloader(
    uint32_t address,
    uint32_t size)
{
    return BL_MemoryRangeInRange(
        address,
        size,
        BL_BOOT_START_ADDRESS,
        BL_BOOT_END_ADDRESS);
}

bool BL_MemoryAddressInApplication(
    uint32_t address)
{
    return BL_MemoryAddressInRange(
        address,
        BL_APP_START_ADDRESS,
        BL_APP_END_ADDRESS);
}

bool BL_MemoryRangeInApplication(
    uint32_t address,
    uint32_t size)
{
    return BL_MemoryRangeInRange(
        address,
        size,
        BL_APP_START_ADDRESS,
        BL_APP_END_ADDRESS);
}

bool BL_MemoryRangeInUpdateArea(
    uint32_t address,
    uint32_t size)
{
    /*
     * The update area currently contains:
     *
     *     0x00008000 - 0x0000801F
     *     Image header
     *
     *     0x00008020 - 0x000083FF
     *     Reserved space
     *
     *     0x00008400 - 0x0003FFFF
     *     Application image
     *
     * The update operation is therefore allowed to modify the entire
     * update region, but NEVER the bootloader region.
     */
    return BL_MemoryRangeInRange(
        address,
        size,
        BL_UPDATE_START_ADDRESS,
        BL_UPDATE_END_ADDRESS);
}

bool BL_MemoryRangeInSram(
    uint32_t address,
    uint32_t size)
{
    return BL_MemoryRangeInRange(
        address,
        size,
        BL_SRAM_START_ADDRESS,
        BL_SRAM_END_ADDRESS);
}

bool BL_MemoryAddressInSram(
    uint32_t address)
{
    return BL_MemoryAddressInRange(
        address,
        BL_SRAM_START_ADDRESS,
        BL_SRAM_END_ADDRESS);
}

bool BL_MemoryRangesOverlap(
    uint32_t address1,
    uint32_t size1,
    uint32_t address2,
    uint32_t size2)
{
    uint32_t end1;
    uint32_t end2;

    if (!BL_MemoryRangeValid(
        address1,
        size1))
    {
        return false;
    }

    if (!BL_MemoryRangeValid(
        address2,
        size2))
    {
        return false;
    }

    end1 = address1 + size1 - 1UL;
    end2 = address2 + size2 - 1UL;

    /*
     * Two ranges do NOT overlap if one is completely before the other.
     *
     * Otherwise they overlap.
     */
    return !((end1 < address2) ||
             (end2 < address1));
}