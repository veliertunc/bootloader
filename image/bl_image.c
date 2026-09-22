#include "bl_image.h"

#include "bl_config.h"
#include "bl_crc32.h"
#include "bl_memory.h"

#include <stddef.h>
#include <stdint.h>

/*
 * TM4C123 Cortex-M4 System Control Block.
 *
 * VTOR is located at SCB offset 0x008.
 *
 * We do not use CMSIS here because this project uses TivaWare
 * DriverLib together with the ARM GNU toolchain.
 */
#define BL_SCB_VTOR_ADDRESS    (0xE000ED08UL)

/*
 * Cortex-M vector table entries.
 */
#define BL_VECTOR_MSP_OFFSET           (0UL)
#define BL_VECTOR_RESET_HANDLER_OFFSET (4UL)

/*
 * Cortex-M4 requires the initial stack pointer to be aligned
 * to an 8-byte boundary when entering an exception/handler.
 */
#define BL_STACK_ALIGNMENT             (8UL)

/*
 * The image must contain at least the initial MSP and
 * Reset_Handler vector entries.
 */
#define BL_MIN_IMAGE_SIZE              (8UL)

/*
 * ARM Cortex-M instructions execute in Thumb state.
 *
 * The least significant bit of an exception/reset vector
 * must therefore be set.
 */
#define BL_THUMB_BIT                   (0x00000001UL)

/*
 * Vector table alignment.
 *
 * TM4C123 requires VTOR alignment suitable for the vector table.
 * The application linker script places the vector table at
 * 0x00008400, which is 1024-byte aligned.
 */
#define BL_VECTOR_TABLE_ALIGNMENT      (1024UL)

static BL_ImageHeader_t g_ImageHeader;
static bool g_ImageValid = false;

static uint32_t BL_ImageReadWord(uint32_t address)
{
    const volatile uint32_t *addressPointer;

    addressPointer = (const volatile uint32_t *)address;

    return *addressPointer;
}

static BL_ImageStatus_t BL_ImageValidateReservedFields(
    const BL_ImageHeader_t *header)
{
    if (header == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    if (header->reserved0 != 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_HEADER;
    }

    if (header->reserved1 != 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_HEADER;
    }

    if (header->reserved2 != 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_HEADER;
    }

    return BL_IMAGE_STATUS_OK;
}

static BL_ImageStatus_t BL_ImageValidateSize(
    const BL_ImageHeader_t *header)
{
    uint32_t imageEndAddress;

    if (header == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    if (header->imageSize == 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    /*
     * FlashProgram operates on 32-bit words and the generated
     * application image is padded to a 4-byte boundary.
     */
    if ((header->imageSize & 0x03UL) != 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    if (header->imageSize < BL_MIN_IMAGE_SIZE)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    if (header->imageSize > BL_APP_MAX_SIZE)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    imageEndAddress =
        BL_APP_START_ADDRESS +
        header->imageSize -
        1UL;

    /*
     * Explicit overflow protection.
     */
    if (imageEndAddress < BL_APP_START_ADDRESS)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    if (imageEndAddress > BL_APP_END_ADDRESS)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    return BL_IMAGE_STATUS_OK;
}

static BL_ImageStatus_t BL_ImageValidateVersion(
    const BL_ImageHeader_t *header)
{
    if (header == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    if (header->headerVersion != BL_IMAGE_HEADER_VERSION)
    {
        return BL_IMAGE_STATUS_INVALID_HEADER_VERSION;
    }

    /*
     * Version 0 is reserved for an uninitialized image.
     */
    if (header->imageVersion == 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_IMAGE_VERSION;
    }

    return BL_IMAGE_STATUS_OK;
}

static BL_ImageStatus_t BL_ImageGetImageRange(
    const BL_ImageHeader_t *header,
    uint32_t *startAddress,
    uint32_t *endAddress)
{
    uint32_t calculatedEndAddress;

    if (header == NULL ||
        startAddress == NULL ||
        endAddress == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    if (BL_ImageValidateSize(header) != BL_IMAGE_STATUS_OK)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    calculatedEndAddress =
        BL_APP_START_ADDRESS +
        header->imageSize -
        1UL;

    if (calculatedEndAddress < BL_APP_START_ADDRESS)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    *startAddress = BL_APP_START_ADDRESS;
    *endAddress = calculatedEndAddress;

    return BL_IMAGE_STATUS_OK;
}

static BL_ImageStatus_t BL_ImageValidateMSP(
    uint32_t initialMSP)
{
    if (!BL_MemoryAddressAligned(
            initialMSP,
            BL_STACK_ALIGNMENT))
    {
        return BL_IMAGE_STATUS_INVALID_MSP;
    }

    if (!BL_MemoryAddressInSram(initialMSP))
    {
        return BL_IMAGE_STATUS_INVALID_MSP;
    }

    return BL_IMAGE_STATUS_OK;
}

static BL_ImageStatus_t BL_ImageValidateResetHandler(
    uint32_t resetHandler,
    uint32_t imageStartAddress,
    uint32_t imageEndAddress)
{
    uint32_t resetAddress;

    /*
     * Cortex-M reset handler must contain the Thumb bit.
     */
    if ((resetHandler & BL_THUMB_BIT) == 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_RESET_HANDLER;
    }

    /*
     * Remove Thumb-state bit before checking the actual
     * instruction address.
     */
    resetAddress =
        resetHandler & ~BL_THUMB_BIT;

    /*
     * Cortex-M instructions are at least halfword aligned.
     */
    if ((resetAddress & 0x01UL) != 0UL)
    {
        return BL_IMAGE_STATUS_INVALID_RESET_HANDLER;
    }

    /*
     * Most importantly, do not merely check whether the reset
     * handler is somewhere inside the application partition.
     *
     * It must be inside the actual image described by imageSize.
     */
    if (resetAddress < imageStartAddress ||
        resetAddress > imageEndAddress)
    {
        return BL_IMAGE_STATUS_INVALID_RESET_HANDLER;
    }

    return BL_IMAGE_STATUS_OK;
}

BL_ImageStatus_t BL_ImageReadHeader(
    BL_ImageHeader_t *header)
{
    if (header == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    /*
     * The image header is intentionally outside the application
     * linker region.
     *
     * The host updater writes it at 0x00008000.
     */
    header->magic =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 0UL);

    header->headerVersion =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 4UL);

    header->imageVersion =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 8UL);

    header->imageSize =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 12UL);

    header->imageCRC =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 16UL);

    header->reserved0 =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 20UL);

    header->reserved1 =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 24UL);

    header->reserved2 =
        BL_ImageReadWord(
            BL_APP_HEADER_ADDRESS + 28UL);

    return BL_IMAGE_STATUS_OK;
}

BL_ImageStatus_t BL_ImageValidateHeader(
    const BL_ImageHeader_t *header)
{
    BL_ImageStatus_t status;

    if (header == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    if (header->magic != BL_IMAGE_MAGIC)
    {
        return BL_IMAGE_STATUS_INVALID_MAGIC;
    }

    status = BL_ImageValidateVersion(header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    status = BL_ImageValidateSize(header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    status = BL_ImageValidateReservedFields(header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    return BL_IMAGE_STATUS_OK;
}

BL_ImageStatus_t BL_ImageValidateVectorTable(
    const BL_ImageHeader_t *header)
{
    uint32_t initialMSP;
    uint32_t resetHandler;
    uint32_t imageStartAddress;
    uint32_t imageEndAddress;

    if (header == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    if (BL_ImageValidateHeader(header) !=
        BL_IMAGE_STATUS_OK)
    {
        return BL_IMAGE_STATUS_INVALID_HEADER;
    }

    /*
     * The vector table must be located at the canonical
     * application start address.
     */
    if (!BL_MemoryAddressAligned(
            BL_APP_VECTOR_ADDRESS,
            BL_VECTOR_TABLE_ALIGNMENT))
    {
        return BL_IMAGE_STATUS_INVALID_VECTOR_TABLE;
    }

    /*
     * The vector table itself must lie inside the flash
     * application partition.
     */
    if (!BL_MemoryRangeInApplication(
            BL_APP_VECTOR_ADDRESS,
            BL_MIN_IMAGE_SIZE))
    {
        return BL_IMAGE_STATUS_INVALID_VECTOR_TABLE;
    }

    if (BL_ImageGetImageRange(
            header,
            &imageStartAddress,
            &imageEndAddress) !=
        BL_IMAGE_STATUS_OK)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    /*
     * The vector table must also be contained inside the
     * actual image, not merely inside the application partition.
     */
    if ((BL_APP_VECTOR_ADDRESS < imageStartAddress) ||
        ((BL_APP_VECTOR_ADDRESS +
          BL_MIN_IMAGE_SIZE -
          1UL) > imageEndAddress))
    {
        return BL_IMAGE_STATUS_INVALID_VECTOR_TABLE;
    }

    /*
     * Vector table entry 0:
     *
     * Initial Main Stack Pointer.
     */
    initialMSP =
        BL_ImageReadWord(
            BL_APP_VECTOR_ADDRESS +
            BL_VECTOR_MSP_OFFSET);

    if (BL_ImageValidateMSP(initialMSP) !=
        BL_IMAGE_STATUS_OK)
    {
        return BL_IMAGE_STATUS_INVALID_MSP;
    }

    /*
     * Vector table entry 1:
     *
     * Reset_Handler.
     */
    resetHandler =
        BL_ImageReadWord(
            BL_APP_VECTOR_ADDRESS +
            BL_VECTOR_RESET_HANDLER_OFFSET);

    if (BL_ImageValidateResetHandler(
            resetHandler,
            imageStartAddress,
            imageEndAddress) !=
        BL_IMAGE_STATUS_OK)
    {
        return BL_IMAGE_STATUS_INVALID_RESET_HANDLER;
    }

    return BL_IMAGE_STATUS_OK;
}

uint32_t BL_ImageCalculateCRC(
    const BL_ImageHeader_t *header)
{
    if (header == NULL)
    {
        return 0UL;
    }

    if (BL_ImageValidateHeader(header) !=
        BL_IMAGE_STATUS_OK)
    {
        return 0UL;
    }

    /*
     * IMPORTANT:
     *
     * CRC starts at 0x8400.
     *
     * The 32-byte header at 0x8000 is NOT part of the CRC.
     */
    return BL_CRC32_Calculate(
        (const uint8_t *)BL_APP_START_ADDRESS,
        header->imageSize);
}

BL_ImageStatus_t BL_ImageVerifyHeader(
    const BL_ImageHeader_t *header)
{
    BL_ImageStatus_t status;
    uint32_t calculatedCRC;

    if (header == NULL)
    {
        return BL_IMAGE_STATUS_NULL_POINTER;
    }

    status = BL_ImageValidateHeader(header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    status = BL_ImageValidateVectorTable(header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    calculatedCRC =
        BL_ImageCalculateCRC(header);

    if (calculatedCRC != header->imageCRC)
    {
        return BL_IMAGE_STATUS_INVALID_CRC;
    }

    return BL_IMAGE_STATUS_OK;
}

BL_ImageStatus_t BL_ImageVerify(
    uint32_t expectedSize,
    uint32_t expectedCRC)
{
    BL_ImageHeader_t header;
    BL_ImageStatus_t status;
    uint32_t calculatedCRC;

    status = BL_ImageReadHeader(&header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    status = BL_ImageValidateHeader(&header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    /*
     * The protocol VERIFY command supplies the expected
     * image metadata independently.
     *
     * This protects against verifying a different image
     * than the host intended.
     */
    if (header.imageSize != expectedSize)
    {
        return BL_IMAGE_STATUS_INVALID_SIZE;
    }

    if (header.imageCRC != expectedCRC)
    {
        return BL_IMAGE_STATUS_INVALID_CRC;
    }

    status = BL_ImageValidateVectorTable(&header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        return status;
    }

    calculatedCRC =
        BL_ImageCalculateCRC(&header);

    if (calculatedCRC != expectedCRC)
    {
        return BL_IMAGE_STATUS_INVALID_CRC;
    }

    return BL_IMAGE_STATUS_OK;
}

BL_ImageStatus_t BL_ImageValidate(void)
{
    BL_ImageHeader_t header;
    BL_ImageStatus_t status;

    status = BL_ImageReadHeader(&header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        g_ImageValid = false;
        return status;
    }

    status = BL_ImageVerifyHeader(&header);

    if (status != BL_IMAGE_STATUS_OK)
    {
        g_ImageValid = false;
        return status;
    }

    g_ImageHeader = header;
    g_ImageValid = true;

    return BL_IMAGE_STATUS_OK;
}

bool BL_ImageIsValid(void)
{
    return g_ImageValid;
}

uint32_t BL_ImageGetSize(void)
{
    if (!g_ImageValid)
    {
        return 0UL;
    }

    return g_ImageHeader.imageSize;
}

uint32_t BL_ImageGetCRC(void)
{
    if (!g_ImageValid)
    {
        return 0UL;
    }

    return g_ImageHeader.imageCRC;
}

uint32_t BL_ImageGetVersion(void)
{
    if (!g_ImageValid)
    {
        return 0UL;
    }

    return g_ImageHeader.imageVersion;
}