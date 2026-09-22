#ifndef BL_PACKET_H
#define BL_PACKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define BL_PACKET_SOF                    (0xAA55U)

#define BL_PACKET_MAX_PAYLOAD_SIZE       (240U)

#define BL_PACKET_HEADER_SIZE            (6U)
#define BL_PACKET_CRC_SIZE               (4U)

#define BL_PACKET_MIN_SIZE               \
    (BL_PACKET_HEADER_SIZE + BL_PACKET_CRC_SIZE)

#define BL_PACKET_MAX_SIZE               \
    (BL_PACKET_HEADER_SIZE +             \
     BL_PACKET_MAX_PAYLOAD_SIZE +        \
     BL_PACKET_CRC_SIZE)

/*
 * Commands.
 */
#define BL_CMD_CONNECT                   (0x01U)
#define BL_CMD_DISCONNECT                (0x02U)
#define BL_CMD_GET_VERSION               (0x03U)
#define BL_CMD_GET_INFO                  (0x04U)

#define BL_CMD_ERASE                     (0x10U)
#define BL_CMD_WRITE                     (0x11U)
#define BL_CMD_VERIFY                    (0x12U)
#define BL_CMD_ABORT                     (0x13U)

#define BL_CMD_RUN_APP                   (0x20U)
#define BL_CMD_RESET                     (0x21U)

/*
 * Responses.
 */
#define BL_CMD_ACK                       (0x80U)
#define BL_CMD_NACK                      (0x81U)
#define BL_CMD_BUSY                      (0x82U)
#define BL_CMD_CRC_ERROR                 (0x83U)
#define BL_CMD_SEQUENCE_ERROR            (0x84U)
#define BL_CMD_FLASH_ERROR               (0x85U)
#define BL_CMD_VERIFY_ERROR              (0x86U)
#define BL_CMD_INVALID_CMD               (0x87U)

typedef struct
{
    uint16_t sof;
    uint8_t command;
    uint8_t sequence;
    uint16_t payloadLength;

} BL_PacketHeader_t;

typedef struct
{
    BL_PacketHeader_t header;

    uint8_t payload[
        BL_PACKET_MAX_PAYLOAD_SIZE];

    uint32_t crc;

} BL_Packet_t;

typedef enum
{
    BL_PACKET_STATUS_OK = 0,

    BL_PACKET_STATUS_NULL_POINTER,
    BL_PACKET_STATUS_INVALID_SOF,
    BL_PACKET_STATUS_INVALID_LENGTH,
    BL_PACKET_STATUS_INVALID_CRC,
    BL_PACKET_STATUS_BUFFER_TOO_SMALL

} BL_PacketStatus_t;

BL_PacketStatus_t BL_PacketEncode(
    const BL_Packet_t *packet,
    uint8_t *buffer,
    uint32_t bufferSize,
    uint32_t *encodedLength);

BL_PacketStatus_t BL_PacketDecode(
    const uint8_t *buffer,
    uint32_t length,
    BL_Packet_t *packet);

uint32_t BL_PacketCalculateCRC(
    const BL_Packet_t *packet);

bool BL_PacketVerifyCRC(
    const BL_Packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif /* BL_PACKET_H */