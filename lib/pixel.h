/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2024 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __TRC_PIXEL_H__
#define __TRC_PIXEL_H__

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"

#define __DECL_TRC_PIXEL__                                                     \
    trc_pixel {                                                                \
        uint8_t Red;                                                           \
        uint8_t Green;                                                         \
        uint8_t Blue;                                                          \
        uint8_t Alpha;                                                         \
    }

#ifdef _MSC_VER
#    pragma pack(push, 1)
struct __DECL_TRC_PIXEL__;
#    pragma pack(pop)
#else
struct __attribute__((packed)) __DECL_TRC_PIXEL__;
#endif

TRC_UNUSED static void pixel_SetRGB(struct trc_pixel *pixel,
                                    uint8_t r,
                                    uint8_t g,
                                    uint8_t b) {
    pixel->Red = r;
    pixel->Green = g;
    pixel->Blue = b;
    pixel->Alpha = 0xFF;
}

TRC_UNUSED static void pixel_SetTextColor(struct trc_pixel *pixel, int color) {
    pixel_SetRGB(pixel,
                 (color / 36) * 51,
                 ((color / 6) % 6) * 51,
                 (color % 6) * 51);
}

TRC_UNUSED static void pixel_SetTransparent(struct trc_pixel *pixel) {
    memset(pixel, 0, sizeof(struct trc_pixel));
}

TRC_UNUSED static bool pixel_GetIsTransparent(const struct trc_pixel *pixel) {
    return pixel->Alpha == 0;
}

#endif /* __TRC_PIXEL_H__ */
