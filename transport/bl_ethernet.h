#ifndef BL_ETHERNET_H
#define BL_ETHERNET_H

#include <stdbool.h>
#include <stdint.h>

#define BL_ETHERNET_RX_BUFFER_SIZE (512UL)
#define BL_ETHERNET_TX_BUFFER_SIZE (512UL)

bool BL_Ethernet_Init(void);
void BL_Ethernet_DeInit(void);
bool BL_Ethernet_Receive(uint8_t *buffer, uint32_t bufferSize,
                         uint32_t *receivedLength);
bool BL_Ethernet_Transmit(const uint8_t *buffer, uint32_t length);
bool BL_Ethernet_Activity(void);

#endif
