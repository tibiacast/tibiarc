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

#ifndef __TRC_CANVAS_HPP__
#define __TRC_CANVAS_HPP__

#include <cstdint>
#include <filesystem>

#include "pixel.hpp"
#include "sprites.hpp"

#include "utils.hpp"

namespace trc {

/* FIXME: C++ migration, make this a template with a creation/destruction
 * routine so we can plug alternative backends into it. */
class Canvas {
    Canvas(int width, int height, int stride, uint8_t *buffer);

public:
    enum class Type { Internal, External, Slice } Kind;

    int Width;
    int Height;
    int Stride;
    uint8_t *Buffer;

    Canvas(int width, int height, Type kind = Type::Internal);
    ~Canvas();

    Canvas(const Canvas &other) = delete;

    /* Creates a sub-canvas referring to a part of the original canvas, useful
     * for rendering within certain bounds like the game viewport.
     *
     * The sub-canvas does not need to be freed. */
    const Canvas Slice(int leftX, int topY, int rightX, int bottomY);

    void Wipe();

    /* Debug function for dumping a canvas as a 24-bit BMP file. */
    void Dump(const std::filesystem::path &path) const;

    Pixel &GetPixel(int x, int y) {
        int offset = x * sizeof(Pixel);
        return *(Pixel *)&Buffer[offset + (y * Stride)];
    }

    const Pixel &GetPixel(int x, int y) const {
        int offset = x * sizeof(Pixel);
        return *(const Pixel *)&Buffer[offset + (y * Stride)];
    }

    void DrawRectangle(const Pixel &color, int x, int y, int width, int height);

    void DrawCharacter(const Sprite &sprite,
                       const Pixel &fontColor,
                       int x,
                       int y);

    void Draw(const Sprite &sprite, int x, int y, int width, int height);

    void Tint(const Sprite &sprite,
              int x,
              int y,
              int width,
              int height,
              int head,
              int primary,
              int secondary,
              int detail);
};
} // namespace trc

#endif /* __TRC_CANVAS_HPP__ */
