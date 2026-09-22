#ifndef BL_CONFIG_H
#define BL_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * --------------------------------------------------------------------------
 * MCU
 * --------------------------------------------------------------------------
 */

#define BL_CPU_FREQUENCY_HZ          (80000000UL)

/*
 * --------------------------------------------------------------------------
 * Flash layout
 * --------------------------------------------------------------------------
 */

#define BL_FLASH_START_ADDRESS       (0x00000000UL)
#define BL_FLASH_SIZE                (0x00040000UL)
#define BL_FLASH_END_ADDRESS         (0x0003FFFFUL)

#define BL_FLASH_SECTOR_SIZE         (1024UL)

/*
 * Bootloader
 */

#define BL_BOOT_START_ADDRESS        (0x00000000UL)
#define BL_BOOT_SIZE                 (0x00008000UL)
#define BL_BOOT_END_ADDRESS          (0x00007FFFUL)

/*
 * Update area
 *
 * Header:
 *     0x8000 - 0x801F
 *
 * Reserved:
 *     0x8020 - 0x83FF
 *
 * Application:
 *     0x8400 - 0x3FFFF
 */

#define BL_UPDATE_START_ADDRESS      (0x00008000UL)
#define BL_UPDATE_END_ADDRESS        (0x0003FFFFUL)

/*
 * Image header
 */

#define BL_APP_HEADER_ADDRESS        (0x00008000UL)
#define BL_APP_HEADER_SIZE           (32UL)

/*
 * Application vector table
 */

#define BL_APP_VECTOR_ADDRESS        (0x00008400UL)
#define BL_APP_START_ADDRESS         (BL_APP_VECTOR_ADDRESS)
#define BL_APP_END_ADDRESS           (0x0003FFFFUL)

#define BL_APP_MAX_SIZE              \
    (BL_APP_END_ADDRESS - BL_APP_START_ADDRESS + 1UL)

/*
 * --------------------------------------------------------------------------
 * SRAM
 * --------------------------------------------------------------------------
 */

#define BL_SRAM_START_ADDRESS        (0x20000000UL)
#define BL_SRAM_SIZE                 (0x00008000UL)
#define BL_SRAM_END_ADDRESS          (0x20007FFFUL)

/*
 * --------------------------------------------------------------------------
 * Image
 * --------------------------------------------------------------------------
 */

#define BL_IMAGE_MAGIC               (0x544D3443UL)
#define BL_IMAGE_HEADER_VERSION      (0x00010000UL)

/*
 * --------------------------------------------------------------------------
 * UART
 * --------------------------------------------------------------------------
 */

#define BL_UART_BAUDRATE             (115200UL)

/*
 * --------------------------------------------------------------------------
 * Packet
 * --------------------------------------------------------------------------
 */

#define BL_PACKET_SOF                (0xAA55U)

#define BL_PACKET_MAX_PAYLOAD_SIZE   (240UL)

#define BL_PACKET_HEADER_SIZE        (6UL)
#define BL_PACKET_CRC_SIZE           (4UL)

#define BL_PACKET_MIN_SIZE           \
    (BL_PACKET_HEADER_SIZE + BL_PACKET_CRC_SIZE)

#define BL_PACKET_MAX_SIZE           \
    (BL_PACKET_HEADER_SIZE +         \
     BL_PACKET_MAX_PAYLOAD_SIZE +    \
     BL_PACKET_CRC_SIZE)

/*
 * --------------------------------------------------------------------------
 * Protocol commands
 * --------------------------------------------------------------------------
 */

#define BL_CMD_CONNECT               (0x01U)
#define BL_CMD_DISCONNECT            (0x02U)
#define BL_CMD_GET_VERSION           (0x03U)
#define BL_CMD_GET_INFO              (0x04U)

#define BL_CMD_ERASE                 (0x10U)
#define BL_CMD_WRITE                 (0x11U)
#define BL_CMD_VERIFY                (0x12U)
#define BL_CMD_ABORT                 (0x13U)

#define BL_CMD_RUN_APP               (0x20U)
#define BL_CMD_RESET                 (0x21U)

/*
 * --------------------------------------------------------------------------
 * Protocol responses
 * --------------------------------------------------------------------------
 */

#define BL_CMD_ACK                  (0x80U)
#define BL_CMD_NACK                 (0x81U)
#define BL_CMD_BUSY                 (0x82U)
#define BL_CMD_CRC_ERROR            (0x83U)
#define BL_CMD_SEQUENCE_ERROR       (0x84U)
#define BL_CMD_FLASH_ERROR          (0x85U)
#define BL_CMD_VERIFY_ERROR         (0x86U)
#define BL_CMD_INVALID_CMD          (0x87U)

/*
 * --------------------------------------------------------------------------
 * Application version
 * --------------------------------------------------------------------------
 */

#define BL_BOOTLOADER_VERSION_MAJOR  (1UL)
#define BL_BOOTLOADER_VERSION_MINOR  (0UL)
#define BL_BOOTLOADER_VERSION_PATCH  (0UL)


#define BL_MAX_PACKET_SIZE          (512U)

#define BL_MAX_FRAME_SIZE           (1024U)

#define BL_USB_REPORT_SIZE          (64U)

#define BL_USB_REPORT_HEADER_SIZE   (4U)

#define BL_USB_REPORT_PAYLOAD_SIZE  \
(
    BL_USB_REPORT_SIZE -
    BL_USB_REPORT_HEADER_SIZE
)

#ifdef __cplusplus
}
#endif

#endif /* BL_CONFIG_H */