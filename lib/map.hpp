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

#ifndef __TRC_MAP_HPP__
#define __TRC_MAP_HPP__

#include <cstdint>

#include "effect.hpp"
#include "position.hpp"
#include "tile.hpp"

#include "utils.hpp"

namespace trc {
struct Map {
    static constexpr int TileBufferWidth = 18;
    static constexpr int TileBufferHeight = 14;
    static constexpr int TileBufferDepth = 8;
    static constexpr int RenderHeightMapSize =
            (TileBufferWidth + 2) * (TileBufferHeight + 2);

    uint8_t LightIntensity;
    uint8_t LightColor;

    trc::Position Position;

    const trc::Tile &Tile(const trc::Position &position) const {
        return Tile(position.X, position.Y, position.Z);
    }

    const trc::Tile &Tile(int X, int Y, int Z) const {
        trc::Assert(X >= 0 && Y >= 0 && Z >= 0);

        X %= TileBufferWidth;
        Y %= TileBufferHeight;
        Z %= TileBufferDepth;

        return Tiles[X + (Y + (Z * TileBufferHeight)) * TileBufferWidth];
    }

    trc::Tile &Tile(const trc::Position &position) {
        return Tile(position.X, position.Y, position.Z);
    }

    trc::Tile &Tile(int X, int Y, int Z) {
        trc::Assert(X >= 0 && Y >= 0 && Z >= 0);

        X %= TileBufferWidth;
        Y %= TileBufferHeight;
        Z %= TileBufferDepth;

        return Tiles[X + (Y + (Z * TileBufferHeight)) * TileBufferWidth];
    }

    uint8_t GetRenderHeight(int rX, int bY) const {
        if (rX > 0 && bY > 0) {
            trc::Assert((rX / 32) + ((bY / 32) * TileBufferWidth) <=
                        RenderHeightMapSize);

            return RenderHeightMap[(rX / 32) + ((bY / 32) * TileBufferWidth)];
        }

        return std::numeric_limits<uint8_t>::max();
    }

    void UpdateRenderHeight(int rX, int bY, uint8_t Z) {
        if (rX > 0 && bY > 0) {
            trc::Assert((rX / 32) + ((bY / 32) * TileBufferWidth) <=
                        RenderHeightMapSize);

            RenderHeightMap[(rX / 32) + ((bY / 32) * TileBufferWidth)] = Z;
        }
    }

    void Clear() {
        for (auto &tile : Tiles) {
            tile.Clear();
        }
    }

private:
    std::array<trc::Tile, TileBufferWidth * TileBufferHeight * TileBufferDepth>
            Tiles;
    std::array<uint8_t, RenderHeightMapSize> RenderHeightMap;
};
} // namespace trc

#endif /* __TRC_MAP_HPP__ */
