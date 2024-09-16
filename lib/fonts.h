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

#ifndef __TRC_FONTS_H__
#define __TRC_FONTS_H__

#include <stdint.h>

#include "canvas.h"
#include "sprites.h"

struct trc_font {
    bool IsBordered;
    int Height;

    struct {
        int Width;

        struct trc_sprite Sprite;
    } Characters[256];
};

struct trc_fonts {
    /* Messages, character names, etc */
    struct trc_font GameFont;
    /* Backpacks, option text, etc */
    struct trc_font InterfaceFontSmall;
    /* Soul, cap, etc */
    struct trc_font InterfaceFontLarge;
};

bool fonts_Load(struct trc_version *version);
void fonts_Free(struct trc_version *version);

#endif /* __TRC_FONTS_H__ */
