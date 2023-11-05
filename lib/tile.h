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

#ifndef __TRC_TILE_H__
#define __TRC_TILE_H__

#include "canvas.h"
#include "effect.h"
#include "object.h"
#include "versions_decl.h"

#define MAX_OBJECTS_PER_TILE 10
#define MAX_EFFECTS_PER_TILE 8

#define TILE_STACKPOSITION_TOP 0xFF
#define TILE_OBJECT_CREATURE 0x63

struct trc_tile {
    uint8_t ObjectCount;
    uint8_t GraphicalIndex;

    /* Text and numerical effects are mutually exclusive, the latter replacing
     * the former. */
    union {
        struct {
            uint8_t TextIndex;
            struct trc_text_effect TextEffects[MAX_EFFECTS_PER_TILE];
        };

        struct {
            uint8_t NumericalIndex;
            struct trc_numerical_effect NumericalEffects[MAX_EFFECTS_PER_TILE];
        };
    };

    struct trc_graphical_effect GraphicalEffects[MAX_EFFECTS_PER_TILE];
    struct trc_object Objects[MAX_OBJECTS_PER_TILE];
};

void tile_AddGraphicalEffect(struct trc_tile *tile,
                             uint8_t effectId,
                             uint32_t currentTick);

/* WARNING: Numerical effects replaced text effects, and their use must not be
 * mixed in the same game state. */
void tile_AddNumericalEffect(struct trc_tile *tile,
                             uint8_t color,
                             uint32_t value,
                             uint32_t currentTick);

/* WARNING: Numerical effects replaced text effects, and their use must not be
 * mixed in the same game state. */
void tile_AddTextEffect(struct trc_tile *tile,
                        uint8_t color,
                        uint16_t length,
                        const char *message,
                        uint32_t currentTick);

bool tile_InsertObject(const struct trc_version *version,
                       struct trc_tile *tile,
                       struct trc_object *object,
                       uint8_t stackPosition);
bool tile_SetObject(const struct trc_version *version,
                    struct trc_tile *tile,
                    struct trc_object *object,
                    uint8_t stackPosition);
bool tile_CopyObject(const struct trc_version *version,
                     struct trc_tile *tile,
                     struct trc_object *object,
                     uint8_t stackPosition);
bool tile_RemoveObject(const struct trc_version *version,
                       struct trc_tile *tile,
                       uint8_t stackPosition);

void tile_Clear(struct trc_tile *tile);

#endif /* __TRC_TILE_H__ */
