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

#ifndef __TRC_FONTS_HPP__
#define __TRC_FONTS_HPP__

#include <cstdint>

#include "canvas.hpp"
#include "pictures.hpp"
#include "sprites.hpp"

namespace trc {
struct Font {
    bool Bordered;
    int Height;

    struct {
        int Width;

        trc::Sprite Sprite;
    } Characters[256];

    Font(const Canvas &canvas,
         int width,
         int height,
         int spacing,
         bool bordered);

    Font(const Font &other) = delete;
};

struct Fonts {
    Font Game;
    Font InterfaceSmall;
    Font InterfaceLarge;

    Fonts(const Version &version);

    Fonts(const Fonts &other) = delete;
};

} // namespace trc

#endif /* __TRC_FONTS_HPP__ */
