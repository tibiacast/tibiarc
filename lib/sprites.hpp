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

#ifndef __TRC_SPRITES_HPP__
#define __TRC_SPRITES_HPP__

#include <cstdint>
#include <unordered_map>

#include "datareader.hpp"
#include "versions_decl.hpp"

namespace trc {

/* Forward-declare Canvas to break a circular dependency. */
class Canvas;

struct Sprite {
    int Width;
    int Height;
    size_t Size;
    const uint8_t *Buffer;

    enum class Trim { None, Right };

    /** @brief Extracts a sprite from the given canvas */
    Sprite(const Canvas &canvas,
           size_t x,
           size_t y,
           size_t width,
           size_t height,
           Trim trim = Trim::None);

    /** @brief Loads a sprite from the given data stream */
    Sprite(DataReader &data, size_t width, size_t height);

    Sprite &operator=(Sprite &&other);

    Sprite();
    ~Sprite();

    Sprite(const Sprite &) = delete;
};

struct SpriteFile {
    std::unordered_map<uint32_t, Sprite> Sprites;

public:
    const uint32_t Signature;

    SpriteFile(const VersionBase &version, DataReader data);

    const Sprite &Get(uint32_t index) const {
        auto it = Sprites.find(index);

        if (it != Sprites.end()) {
            return it->second;
        }

        /* Ignore failures by returning a dummy sprite that won't be drawn:
         * it's pretty common for sprite files out in the wild to be subtly
         * corrupt. */
        static Sprite fallback;
        return fallback;
    }

    SpriteFile(const SpriteFile &other) = delete;
};

} // namespace trc

#endif /* __TRC_SPRITES_HPP__ */
