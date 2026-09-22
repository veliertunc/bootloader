#ifndef BL_COMMAND_H
#define BL_COMMAND_H

#include <stdint.h>

#include "bl_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    BL_COMMAND_STATUS_OK = 0,

    BL_COMMAND_STATUS_INVALID_COMMAND,
    BL_COMMAND_STATUS_INVALID_STATE,
    BL_COMMAND_STATUS_INVALID_PARAMETER,

    BL_COMMAND_STATUS_FLASH_ERROR,
    BL_COMMAND_STATUS_VERIFY_ERROR,

    BL_COMMAND_STATUS_BUSY,
    BL_COMMAND_STATUS_ABORTED

} BL_CommandStatus_t;

typedef struct
{
    uint8_t command;
    uint8_t sequence;

    uint8_t payload[BL_PACKET_MAX_PAYLOAD_SIZE];

    uint16_t payloadLength;

} BL_Response_t;

BL_CommandStatus_t BL_CommandProcess(
    const BL_Packet_t *packet,
    BL_Response_t *response);

#ifdef __cplusplus
}
#endif

#endif /* BL_COMMAND_H */