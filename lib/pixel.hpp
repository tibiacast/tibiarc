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

#ifndef __TRC_PIXEL_HPP__
#define __TRC_PIXEL_HPP__

#include <cstdint>

#include "utils.hpp"

namespace trc {

#ifdef _MSC_VER
#    pragma pack(push, 1)
#endif

struct
#ifndef _MSC_VER
        __attribute__((packed))
#endif
        Pixel {
    uint8_t Red;
    uint8_t Green;
    uint8_t Blue;
    uint8_t Alpha;

    Pixel(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
        : Red(red), Green(green), Blue(blue), Alpha(alpha) {
    }

    Pixel(uint8_t red, uint8_t green, uint8_t blue)
        : Pixel(red, green, blue, 0xFF) {
    }

    bool IsTransparent() const {
        return Alpha != 0xFF;
    }

    static Pixel TextColor(int color) {
        return Pixel((color / 36) * 51,
                     ((color / 6) % 6) * 51,
                     (color % 6) * 51);
    }

    static Pixel Transparent() {
        return Pixel(0, 0, 0, 0);
    }
};

#ifdef _MSC_VER
#    pragma pack(pop)
#endif

} // namespace trc

#endif /* __TRC_PIXEL_HPP__ */
