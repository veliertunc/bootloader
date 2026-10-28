#ifndef BL_TRANSPORT_CONFIG_H
#define BL_TRANSPORT_CONFIG_H

/*
 * Transport identifiers.
 *
 * These values are intentionally numeric because CMake can select
 * the transport with:
 *
 *     -DBL_TRANSPORT=0
 *
 * for example.
 */

#define BL_TRANSPORT_ID_RS232     (0U)
#define BL_TRANSPORT_ID_RS485     (1U)
#define BL_TRANSPORT_ID_SPI       (2U)
#define BL_TRANSPORT_ID_I2C       (3U)
#define BL_TRANSPORT_ID_ETHERNET  (4U)

/*
 * If no transport is specified by the build system, RS232 is the
 * default transport.
 */
#ifndef BL_TRANSPORT
#define BL_TRANSPORT             BL_TRANSPORT_ID_RS232
#endif

/*
 * Validate the selected transport.
 */
#if ((BL_TRANSPORT != BL_TRANSPORT_ID_RS232) && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_RS485) && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_SPI)  && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_I2C)  && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_ETHERNET))

#error "Invalid BL_TRANSPORT selection"

#endif

#endif /* BL_TRANSPORT_CONFIG_H */
