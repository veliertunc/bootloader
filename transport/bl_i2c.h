#ifndef BL_I2C_H
#define BL_I2C_H

#include <stdbool.h>
#include <stdint.h>

#define BL_I2C_SLAVE_ADDRESS       (0x42U)
#define BL_I2C_RX_BUFFER_SIZE      (1024UL)
#define BL_I2C_TX_BUFFER_SIZE      (1024UL)

bool BL_I2C_Init(void);
void BL_I2C_DeInit(void);
bool BL_I2C_Receive(uint8_t *buffer, uint32_t bufferSize,
                    uint32_t *receivedLength);
bool BL_I2C_Transmit(const uint8_t *buffer, uint32_t length);
bool BL_I2C_Activity(void);
bool BL_I2C_IsInitialized(void);
bool BL_I2C_HasError(void);
uint32_t BL_I2C_GetRxCount(void);
uint32_t BL_I2C_GetTxCount(void);

#endif
