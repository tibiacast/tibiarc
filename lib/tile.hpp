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

#ifndef __TRC_TILE_HPP__
#define __TRC_TILE_HPP__

#include "canvas.hpp"
#include "effect.hpp"
#include "object.hpp"
#include "versions_decl.hpp"

#include <array>

namespace trc {
struct Tile {
    static constexpr uint8_t MaxEffects = 8;
    static constexpr uint8_t MaxObjects = 10;
    static constexpr uint8_t StackPositionTop = 0xFF;

    uint8_t ObjectCount = 0;
    uint8_t GraphicalIndex = 0;

    uint8_t NumericalIndex = 0;
    std::array<NumericalEffect, MaxEffects> NumericalEffects;

    std::array<GraphicalEffect, MaxEffects> GraphicalEffects;
    std::array<Object, MaxObjects> Objects;

    void AddGraphicalEffect(uint8_t effectId, uint32_t currentTick);

    void AddNumericalEffect(uint8_t color,
                            uint32_t value,
                            uint32_t currentTick);

    void InsertObject(const trc::Version &version,
                      const Object &object,
                      uint8_t stackPosition);
    Object &GetObject(const trc::Version &version, uint8_t stackPosition);
    void SetObject(const trc::Version &version,
                   const Object &object,
                   uint8_t stackPosition);
    void RemoveObject(const trc::Version &version, uint8_t stackPosition);

    void Clear();
};

} // namespace trc

#endif /* __TRC_TILE_HPP__ */
