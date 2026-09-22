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

#define BL_TRANSPORT_ID_UART    (0U)
#define BL_TRANSPORT_ID_CAN     (1U)
#define BL_TRANSPORT_ID_SPI     (2U)
#define BL_TRANSPORT_ID_I2C     (3U)
#define BL_TRANSPORT_ID_USB     (4U)

/*
 * If no transport is specified by the build system, UART is the
 * default transport.
 */
#ifndef BL_TRANSPORT
#define BL_TRANSPORT             BL_TRANSPORT_ID_UART
#endif

/*
 * Validate the selected transport.
 */
#if ((BL_TRANSPORT != BL_TRANSPORT_ID_UART) && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_CAN)  && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_SPI)  && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_I2C)  && \
     (BL_TRANSPORT != BL_TRANSPORT_ID_USB))

#error "Invalid BL_TRANSPORT selection"

#endif

#endif /* BL_TRANSPORT_CONFIG_H */