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

#ifndef __TRC_SPRITES_H__
#define __TRC_SPRITES_H__

#include <stdbool.h>
#include <stdint.h>

#include "datareader.h"
#include "versions_decl.h"

struct trc_sprite {
    int Width;
    int Height;
    size_t BufferLength;
    const uint8_t *Buffer;
};

struct trc_spritefile {
    uint32_t IndexSize;
    uint32_t Signature;
    uint32_t Count;

    struct trc_sprite *Sprites;
};

bool sprites_Load(struct trc_version *version, struct trc_data_reader *data);
void sprites_Free(struct trc_version *version);

bool sprites_GetObjectSprite(const struct trc_version *version,
                             uint32_t id,
                             struct trc_sprite **result);
bool sprites_Read(int width,
                  int height,
                  struct trc_data_reader reader,
                  struct trc_sprite *sprite);

#endif /* __TRC_SPRITES_H__ */
