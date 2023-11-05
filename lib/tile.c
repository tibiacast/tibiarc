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

#include "tile.h"
#include "types.h"
#include "utils.h"
#include "versions.h"

static bool tile_GetStackPriority(const struct trc_version *version,
                                  struct trc_object *object,
                                  int *result) {
    if (object->Id == TILE_OBJECT_CREATURE) {
        (*result) = 4;
    } else {
        struct trc_entitytype *itemType;

        if (!types_GetItem(version, object->Id, &itemType)) {
            return trc_ReportError("Item type not found");
        }

        (*result) = itemType->Properties.StackPriority;
    }

    return true;
}

void tile_Clear(struct trc_tile *tile) {
    tile->ObjectCount = 0;
    tile->GraphicalIndex = 0;
    tile->GraphicalEffects[0].Id = 0;
    tile->NumericalIndex = 0;
    tile->NumericalEffects[0].Value = 0;
    tile->TextIndex = 0;
    tile->TextEffects[0].Length = 0;
}

void tile_AddGraphicalEffect(struct trc_tile *tile,
                             uint8_t effectId,
                             uint32_t currentTick) {
    struct trc_graphical_effect *effect =
            &tile->GraphicalEffects[tile->GraphicalIndex];

    effect->StartTick = currentTick;
    effect->Id = effectId;

    tile->GraphicalIndex = (tile->GraphicalIndex + 1) % MAX_EFFECTS_PER_TILE;
}

void tile_AddNumericalEffect(struct trc_tile *tile,
                             uint8_t color,
                             uint32_t value,
                             uint32_t currentTick) {
    struct trc_numerical_effect *effect;

    /* Merge effects that happen at roughly the same time. */
    for (int effectIdx = 0; effectIdx < MAX_EFFECTS_PER_TILE; effectIdx++) {
        effect = &tile->NumericalEffects[effectIdx];

        if ((effect->StartTick + 200) > currentTick && effect->Color == color) {
            effect->StartTick = currentTick;
            effect->Value += value;

            return;
        }
    }

    effect = &tile->NumericalEffects[tile->NumericalIndex];
    effect->StartTick = currentTick;
    effect->Color = color;
    effect->Value = value;

    tile->NumericalIndex = (tile->NumericalIndex + 1) % MAX_EFFECTS_PER_TILE;
}

void tile_AddTextEffect(struct trc_tile *tile,
                        uint8_t color,
                        uint16_t length,
                        const char *message,
                        uint32_t currentTick) {
    struct trc_text_effect *effect = &tile->TextEffects[tile->TextIndex];

    effect->StartTick = currentTick;
    effect->Color = color;
    effect->Length = MIN(length, sizeof(effect->Message));

    memcpy(effect->Message, message, effect->Length);

    tile->TextIndex = (tile->TextIndex + 1) % MAX_EFFECTS_PER_TILE;
}

bool tile_RemoveObject(const struct trc_version *version,
                       struct trc_tile *tile,
                       uint8_t stackPosition) {
    if (version->Features.ModernStacking) {
        if (stackPosition >= tile->ObjectCount) {
            return trc_ReportError("Stack position was out of bounds");
        }
    } else {
        if (stackPosition >= tile->ObjectCount) {
            stackPosition = tile->ObjectCount - 1;
        }
    }

    for (int moveIdx = stackPosition; moveIdx <= (tile->ObjectCount - 2);
         moveIdx++) {
        tile->Objects[moveIdx] = tile->Objects[moveIdx + 1];
    }

    tile->ObjectCount--;
    return true;
}

bool tile_CopyObject(const struct trc_version *version,
                     struct trc_tile *tile,
                     struct trc_object *object,
                     uint8_t stackPosition) {
    if (version->Features.ModernStacking) {
        if (stackPosition >= tile->ObjectCount) {
            return trc_ReportError("Stack position was out of bounds");
        }
    } else {
        if (stackPosition >= tile->ObjectCount) {
            stackPosition = tile->ObjectCount - 1;
        }
    }

    (*object) = tile->Objects[stackPosition];
    return true;
}

bool tile_SetObject(const struct trc_version *version,
                    struct trc_tile *tile,
                    struct trc_object *object,
                    uint8_t stackPosition) {
    int positionLimit;

    if (version->Features.ModernStacking) {
        positionLimit = tile->ObjectCount;
    } else {
        positionLimit = MAX(tile->ObjectCount + 1, MAX_OBJECTS_PER_TILE);
    }

    if (stackPosition >= positionLimit) {
        return trc_ReportError("Stack position was out of bounds");
    } else {
        tile->Objects[stackPosition] = (*object);

        return true;
    }
}

bool tile_InsertObject(const struct trc_version *version,
                       struct trc_tile *tile,
                       struct trc_object *object,
                       uint8_t stackPosition) {
    if (stackPosition == TILE_STACKPOSITION_TOP) {
        int stackPriority = 0;

        if (!tile_GetStackPriority(version, object, &stackPriority)) {
            return trc_ReportError(
                    "Failed to get stack priority of item passed "
                    "to insert");
        }

        for (int stackIdx = 0;
             stackIdx < MIN(tile->ObjectCount, MAX_OBJECTS_PER_TILE);
             stackIdx++) {
            int checkPriority = 0, foundPosition;

            if (!tile_GetStackPriority(version,
                                       &tile->Objects[stackIdx],
                                       &checkPriority)) {
                return trc_ReportError("Failed to get stack priority of item "
                                       "stored in map");
            }

            if (object->Id == TILE_OBJECT_CREATURE &&
                version->Features.ModernStacking) {
                foundPosition = checkPriority > stackPriority;
            } else {
                foundPosition = checkPriority >= stackPriority;
            }

            if (foundPosition) {
                tile->ObjectCount =
                        MIN(MAX_OBJECTS_PER_TILE - 1, tile->ObjectCount);

                for (int moveIdx = tile->ObjectCount - 1; moveIdx >= stackIdx;
                     moveIdx--) {
                    tile->Objects[moveIdx + 1] = tile->Objects[moveIdx];
                }

                tile->Objects[stackIdx] = (*object);
                tile->ObjectCount++;

                return true;
            }
        }

        if (tile->ObjectCount < MAX_OBJECTS_PER_TILE) {
            tile->Objects[tile->ObjectCount] = (*object);
            tile->ObjectCount++;
        }
    } else {
        if (stackPosition > tile->ObjectCount) {
            return trc_ReportError("Stack position was out of bounds");
        } else {
            tile->ObjectCount =
                    MIN(MAX_OBJECTS_PER_TILE - 1, tile->ObjectCount);

            for (int moveIdx = tile->ObjectCount - 1; moveIdx >= stackPosition;
                 moveIdx--) {
                tile->Objects[moveIdx + 1] = tile->Objects[moveIdx];
            }

            tile->Objects[stackPosition] = (*object);
            tile->ObjectCount++;
        }
    }

    return true;
}
