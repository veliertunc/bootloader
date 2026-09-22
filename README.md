# Overview

Bootloader for TM4C123GH6PM using TivaWare C Series

# Pin Assignment

## UART
- PA0 = U0RX
- PA1 = U0TX

## CAN
- PE4 -> CAN0RX
- PE5 -> CAN0TX

## SPI
- PA2 -> SSI0CLK
- PA4 -> SSI0RX
- PA5 -> SSI0TX
- PA3 -> CS

## I2C
- Address = 0x42
- PB2 = SCL
- PB3 = SDA

## USB
- PB0 = USB0ID
- PB1 = USB0VBUS
- PD4 = USB0DM
- PD5 = USB0DP