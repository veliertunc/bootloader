#ifndef BL_MEMORY_H
#define BL_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern const uint32_t __boot_flash_start__;
extern const uint32_t __boot_flash_end__;

extern const uint32_t __app_flash_start__;
extern const uint32_t __app_flash_end__;

extern const uint32_t __metadata_start__;
extern const uint32_t __metadata_end__;

extern const uint32_t __ram_start__;
extern const uint32_t __ram_end__;

bool BL_MemoryAddressAligned(
    uint32_t address,
    uint32_t alignment);

bool BL_MemoryRangeValid(
    uint32_t address,
    uint32_t size);

bool BL_MemoryAddressInFlash(
    uint32_t address);

bool BL_MemoryRangeInFlash(
    uint32_t address,
    uint32_t size);

bool BL_MemoryAddressInBootloader(
    uint32_t address);

bool BL_MemoryRangeInBootloader(
    uint32_t address,
    uint32_t size);

bool BL_MemoryAddressInApplication(
    uint32_t address);

bool BL_MemoryRangeInApplication(
    uint32_t address,
    uint32_t size);

bool BL_MemoryRangeInUpdateArea(
    uint32_t address,
    uint32_t size);

bool BL_MemoryAddressInSram(
    uint32_t address);

bool BL_MemoryRangeInSram(
    uint32_t address,
    uint32_t size);

bool BL_MemoryRangesOverlap(
    uint32_t address1,
    uint32_t size1,
    uint32_t address2,
    uint32_t size2);

#ifdef __cplusplus
}
#endif

#endif /* BL_MEMORY_H */