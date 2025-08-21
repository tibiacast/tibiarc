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

#include "tile.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "versions.hpp"

namespace trc {
static int GetStackPriority(const Version &version, const Object &object) {
    if (object.IsCreature()) {
        return 4;
    }

    return version.GetItem(object.Id).Properties.StackPriority;
}

void Tile::Clear() {
    ObjectCount = 0;
    GraphicalIndex = 0;
    GraphicalEffects = {};
    NumericalIndex = 0;
    NumericalEffects = {};
}

void Tile::AddGraphicalEffect(uint8_t effectId, uint32_t currentTick) {
    auto &effect = GraphicalEffects[GraphicalIndex];

    effect.StartTick = currentTick;
    effect.Id = effectId;

    GraphicalIndex = (GraphicalIndex + 1) % Tile::MaxEffects;
}

void Tile::AddNumericalEffect(uint8_t color,
                              uint32_t value,
                              uint32_t currentTick) {
    /* Merge effects that happen at roughly the same time. */
    for (int effectIdx = 0; effectIdx < Tile::MaxEffects; effectIdx++) {
        auto &effect = NumericalEffects[effectIdx];

        if ((effect.StartTick + 200) > currentTick && effect.Color == color) {
            effect.StartTick = currentTick;
            effect.Value += value;

            return;
        }
    }

    auto &effect = NumericalEffects[NumericalIndex];
    effect.StartTick = currentTick;
    effect.Color = color;
    effect.Value = value;

    NumericalIndex = (NumericalIndex + 1) % Tile::MaxEffects;
}

void Tile::RemoveObject(const Version &version, uint8_t stackPosition) {
    if (version.Features.ModernStacking) {
        if (stackPosition >= ObjectCount) {
            throw InvalidDataError();
        }
    } else {
        /* Under the old stacking rules, removing an object from an empty tile
         * is a no-op. */
        if (ObjectCount == 0) {
            return;
        }

        if (stackPosition >= ObjectCount) {
            stackPosition = ObjectCount - 1;
        }
    }

    for (int moveIdx = stackPosition; moveIdx <= (ObjectCount - 2); moveIdx++) {
        Objects[moveIdx] = Objects[moveIdx + 1];
    }

    ObjectCount--;
}

Object &Tile::GetObject(const Version &version, uint8_t stackPosition) {
    if (version.Features.ModernStacking) {
        if (stackPosition >= ObjectCount) {
            throw InvalidDataError();
        }
    } else {
        if (ObjectCount == 0) {
            throw InvalidDataError();
        }

        if (stackPosition >= ObjectCount) {
            stackPosition = ObjectCount - 1;
        }
    }

    return Objects[stackPosition];
}

void Tile::SetObject(const Version &version,
                     const Object &object,
                     uint8_t stackPosition) {
    uint8_t positionLimit;

    if (version.Features.ModernStacking) {
        positionLimit = ObjectCount;
    } else {
        positionLimit = std::max<uint8_t>(ObjectCount + 1, MaxObjects);
    }

    if (stackPosition >= positionLimit) {
        throw InvalidDataError();
    }

    Objects[stackPosition] = object;
}

void Tile::InsertObject(const Version &version,
                        const Object &object,
                        uint8_t stackPosition) {
    if (stackPosition == Tile::StackPositionTop) {
        int stackPriority = GetStackPriority(version, object);

        for (int stackIdx = 0;
             stackIdx < std::min<int>(ObjectCount, MaxObjects);
             stackIdx++) {
            int checkPriority, foundPosition;

            checkPriority = GetStackPriority(version, Objects[stackIdx]);

            if (object.IsCreature() && version.Features.ModernStacking) {
                foundPosition = checkPriority > stackPriority;
            } else {
                foundPosition = checkPriority >= stackPriority;
            }

            if (foundPosition) {
                ObjectCount = std::min<uint8_t>(MaxObjects - 1, ObjectCount);

                for (int moveIdx = ObjectCount - 1; moveIdx >= stackIdx;
                     moveIdx--) {
                    Objects[moveIdx + 1] = Objects[moveIdx];
                }

                Objects[stackIdx] = object;
                ObjectCount++;

                return;
            }
        }

        if (ObjectCount < MaxObjects) {
            Objects[ObjectCount] = object;
            ObjectCount++;
        }
    } else {
        if (stackPosition > ObjectCount) {
            throw InvalidDataError();
        }

        ObjectCount = std::min<uint8_t>(MaxObjects - 1, ObjectCount);

        for (int moveIdx = ObjectCount - 1; moveIdx >= stackPosition;
             moveIdx--) {
            Objects[moveIdx + 1] = Objects[moveIdx];
        }

        Objects[stackPosition] = object;
        ObjectCount++;
    }
}

} // namespace trc