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

#ifndef __TRC_PICTURES_H__
#define __TRC_PICTURES_H__

#include <stdint.h>

#include "versions_decl.h"

#include "canvas.h"
#include "datareader.h"

enum TrcPictureIndex {
    PICTURE_INDEX_SPLASH_BACKGROUND = 0,
    PICTURE_INDEX_SPLASH_LOGO = 1,
    PICTURE_INDEX_TUTORIAL = 2,
    PICTURE_INDEX_FONT_UNBORDERED = 3,
    PICTURE_INDEX_ICONS = 4,
    PICTURE_INDEX_FONT_GAME = 5,
    PICTURE_INDEX_FONT_INTERFACE_SMALL = 6,
    PICTURE_INDEX_LIGHT_FALLBACKS = 7,
    PICTURE_INDEX_FONT_INTERFACE_LARGE = 8,

    PICTURE_INDEX_FIRST = PICTURE_INDEX_SPLASH_BACKGROUND,
    PICTURE_INDEX_LAST = PICTURE_INDEX_FONT_INTERFACE_LARGE,
};

#define PICTURE_MAX_COUNT (PICTURE_INDEX_LAST - PICTURE_INDEX_FIRST + 1)

struct trc_picture_file {
    uint32_t Signature;
    int Count;
    struct trc_canvas *Array[PICTURE_MAX_COUNT];
};

bool picture_GetPictureCanvas(const struct trc_version *version,
                              enum TrcPictureIndex index,
                              struct trc_canvas **result);

bool pictures_Load(struct trc_version *version, struct trc_data_reader *data);
void pictures_Free(struct trc_version *version);

#endif /* __TRC_PICTURES_H__ */
