#include <stdint.h>

#include "bl_image.h"

/*
 * Initial placeholder header.
 *
 * The post-build image tool will replace:
 *   magic
 *   version
 *   imageSize
 *   imageCRC
 *   reserved0
 *   reserved1
 *   reserved2
 *   reserved3
 *
 * The other fields are fixed.
 */
__attribute__((section(".app_image_header")))
__attribute__((used))
const BL_ImageHeader_t g_applicationImageHeader =
{
    .magic =
        BL_IMAGE_MAGIC,
    
    .version =
        0x00010000UL,

    .imageSize =
        0UL,

    .imageCRC =
        0UL,

    .reserved0 =
        0UL,

    .reserved1 =
        0UL,

    .reserved2 =
        0UL

    .reserved3 =
        0UL

};