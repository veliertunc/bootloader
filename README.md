# TM4C123 firmware bootloader

Bootloader and sample application for the EK-TM4C123GXL (TM4C123GH6PM). The bootloader accepts the same CRC-protected update protocol over one selected transport per build: RS232, RS485, SPI, I²C, or Ethernet through a W5500 module.

## Flash layout

| Region | Address | Size |
| --- | ---: | ---: |
| Bootloader | `0x00000000` | 32 KiB |
| Image header | `0x00008000` | 32 bytes |
| Reserved | `0x00008020` | 992 bytes |
| Application vectors and image | `0x00008400` | 95 KiB |

The updater checks the image header, image size, CRC-32, initial stack pointer, and reset vector before starting the application. The bootloader enters update mode when no valid application is installed or SW2 is held during reset.

## Hardware connections

| Transport | TM4C123 pins | External hardware / notes |
| --- | --- | --- |
| RS232 | PA0 U0RX, PA1 U0TX | RS232 level-shifter/transceiver; UART 115200 8-N-1 |
| RS485 | PA0 U0RX, PA1 U0TX, PB4 DE | RS485 transceiver; DE is high only during transmit. Tie `/RE` to DE if receive echo should be disabled. |
| SPI | PA2 SSI0CLK, PA3 SSI0FSS, PA4 SSI0RX, PA5 SSI0TX | TM4C is SPI slave, mode 0, 8-bit; host supplies clock. The host adds a magic/length/CRC frame around each protocol packet. |
| I²C | PB2 I2C0SCL, PB3 I2C0SDA | TM4C is slave at 7-bit address `0x42`; external master initiates writes and reads. Pull-ups are required. |
| Ethernet | PA2 SSI0CLK, PA4 SSI0RX, PA5 SSI0TX, PB4 CS, PB5 RESET | W5500 module, UDP socket 0, default device IP `192.168.1.50`, UDP port `5000`. No Ethernet controller is built into the TM4C123. |

RS485 direction and W5500 chip select reuse PB4 in mutually exclusive builds. SPI and W5500 also share SSI0 pins in mutually exclusive builds.

The W5500 network values are compiled into `transport/bl_ethernet.c`: MAC `02:54:4D:34:12:34`, subnet `255.255.255.0`, gateway `192.168.1.1`, and IP `192.168.1.50`. Change these values to suit the network before building. The updater sends one bootloader packet per UDP datagram; the W5500 replies to the address and source port of the most recent request.

## Build

Install ARM GNU Embedded Toolchain, CMake, Ninja, Python 3, and TI TivaWare C Series. Configure the TivaWare directory and select one transport:

```powershell
cmake -S . -B build -G Ninja `
  -DTIVAWARE_DIR="D:/TI/TivaWare_C_Series" `
  -DBL_TRANSPORT=RS232
cmake --build build --parallel
```

Set `BL_TRANSPORT` to `RS232`, `RS485`, `SPI`, `I2C`, or `ETHERNET`. CMake builds both firmware images and packages the application image. Outputs include `bootloader.elf/.bin/.hex`, `application.elf/.bin`, `image_header.bin`, `application_image.bin/.hex`, and `tm4c123_full_image.bin/.hex`.

The full flash image is for initial programming. Later updates should use the host updater so the image metadata and CRC are checked by the bootloader.

## Updating firmware

Install the Python host dependency with `python -m pip install -r host/requirements.txt`, then run from the `host` directory.

Serial (RS232 or RS485 adapter):

```powershell
python bl_update.py --port COM5 --image ../build/application_image.bin
```

W5500 Ethernet (PC must have a route to the configured static IP):

```powershell
python bl_update.py --ethernet 192.168.1.50 --udp-port 5000 `
  --image ../build/application_image.bin
```

I²C master adapter on Linux (install `host/requirements.txt` first):

```powershell
python bl_update.py --i2c-bus 1 --image ../build/application_image.bin
```

SPI hosts can use `bl_update_spi.py` with the backend configured for their SPI controller. SPI framing is little-endian: `0xAA55AA55`, 32-bit packet length, protocol packet, and CRC-32 of that packet. I²C hosts write a complete protocol packet to address `0x42`, then read the queued response bytes; the response packet's 6-byte header gives its total length.
