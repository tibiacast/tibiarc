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

#include "sprites.hpp"

#include "datareader.hpp"
#include "versions.hpp"

#include "utils.hpp"

#include <tuple>

namespace trc {

static std::tuple<size_t, size_t, size_t, size_t> MeasureSpriteBounds(
        const Canvas &canvas,
        ssize_t x,
        ssize_t y,
        ssize_t width,
        ssize_t height,
        Sprite::Trim trim) {
    if (!(CheckRange((x + width), 0, canvas.Width - 1) &&
          CheckRange((y + height), 0, canvas.Height - 1))) {
        return std::make_tuple(0, 0, 0, 0);
    } else if (trim != Sprite::Trim::None) {
        ssize_t leftX, rightX, bottomY, topY;

        leftX = x;
        topY = y;
        rightX = x + width;
        bottomY = y + height;

        if (trim == Sprite::Trim::Right) {
            rightX = leftX;
        }

        for (ssize_t yIdx = y; yIdx < (y + height); yIdx++) {
            for (ssize_t xIdx = x; xIdx < (x + width); xIdx++) {
                if (!canvas.GetPixel(xIdx, yIdx).IsTransparent()) {
                    leftX = std::min(leftX, xIdx);
                    topY = std::min(topY, yIdx);
                    rightX = std::max(rightX, xIdx + 1);
                    bottomY = std::max(bottomY, yIdx + 1);
                }
            }
        }

        return std::make_tuple(std::min(leftX, rightX),
                               std::min(topY, bottomY),
                               std::max(leftX, rightX),
                               std::max(topY, bottomY));
    }

    return std::make_tuple(x, y, x + width, y + height);
}

static size_t ExtractSprite(const Canvas &canvas,
                            ssize_t leftX,
                            ssize_t topY,
                            ssize_t rightX,
                            ssize_t bottomY,
                            uint8_t *buffer) {
    if (!(CheckRange(rightX, 0, canvas.Width) &&
          CheckRange(bottomY, 0, canvas.Height))) {
        return 0;
    }

    int bufferIdx, lengthIdx, runLength;
    bool transparent;

    bufferIdx = 2;
    lengthIdx = 0;
    runLength = 0;

    transparent = canvas.GetPixel(leftX, topY).IsTransparent();

    /* If the first pixel is solid, we need to emit an empty transparent pixel
     * block to get things started. */
    if (!transparent) {
        if (buffer) {
            buffer[lengthIdx + 0] = 0;
            buffer[lengthIdx + 1] = 0;
        }

        lengthIdx = bufferIdx;
        bufferIdx += 2;
    }

    for (ssize_t y = topY; y < bottomY; y++) {
        for (ssize_t x = leftX; x < rightX; x++) {
            const Pixel &currentPixel = canvas.GetPixel(x, y);

            if (!currentPixel.IsTransparent()) {
                if (transparent) {
                    if (buffer) {
                        buffer[lengthIdx + 0] = (runLength >> 0) & 0xFF;
                        buffer[lengthIdx + 1] = (runLength >> 8) & 0xFF;
                    }

                    lengthIdx = bufferIdx;
                    bufferIdx += 2;

                    transparent = false;
                    runLength = 0;
                }

                static_assert(sizeof(trc::Pixel) == 4 &&
                                      alignof(trc::Pixel) == 1,
                              "Pixel struct must be byte-aligned");
                if (buffer) {
                    *(trc::Pixel *)&buffer[bufferIdx] = currentPixel;
                }

                bufferIdx += sizeof(trc::Pixel);
                runLength++;
            } else {
                if (!transparent) {
                    if (buffer) {
                        buffer[lengthIdx + 0] = (runLength >> 0) & 0xFF;
                        buffer[lengthIdx + 1] = (runLength >> 8) & 0xFF;
                    }

                    lengthIdx = bufferIdx;
                    bufferIdx += 2;

                    transparent = true;
                    runLength = 0;
                }

                runLength++;
            }
        }
    }

    /* In case we haven't flushed the previous run length yet, do so. */
    if (runLength > 0) {
        if (buffer) {
            buffer[lengthIdx + 0] = (runLength >> 0) & 0xFF;
            buffer[lengthIdx + 1] = (runLength >> 8) & 0xFF;
        }

        if (transparent) {
            if (buffer) {
                buffer[lengthIdx + 2] = 0;
                buffer[lengthIdx + 3] = 0;
            }

            bufferIdx += 2;
        }
    }

    return bufferIdx;
}

static std::pair<size_t, uint8_t *> ReadSprite(size_t width,
                                               size_t height,
                                               DataReader &reader) {
    size_t consumed = 0, required = 0;

    auto validator = reader;

    while (validator.Remaining() > 0) {
        auto transparent = validator.ReadU16();
        auto opaque = validator.ReadU16();

        consumed += transparent + opaque;
        required += 4;

        static_assert(sizeof(Pixel) == 4 && alignof(Pixel) == 1,
                      "Pixel struct must be byte-aligned");

        validator.Skip(opaque * 3);
        required += opaque * 4;
    }

    if (consumed > width * height) {
        throw InvalidDataError();
    }

    auto converted = new uint8_t[required];

    for (size_t i = 0; reader.Remaining() > 0;) {
        uint16_t transparent;
        uint16_t opaque;

        transparent = reader.ReadU16();
        converted[i + 0] = ((uint16_t)transparent >> 0x00);
        converted[i + 1] = ((uint16_t)transparent >> 0x08);
        i += 2;

        opaque = reader.ReadU16();
        converted[i + 0] = ((uint16_t)opaque >> 0x00);
        converted[i + 1] = ((uint16_t)opaque >> 0x08);
        i += 2;

        while (opaque > 0) {
            converted[i + 0] = reader.ReadU8();
            converted[i + 1] = reader.ReadU8();
            converted[i + 2] = reader.ReadU8();
            converted[i + 3] = 0xFF;
            i += 4;

            opaque--;
        }
    }

    return std::make_pair(required, converted);
}

Sprite::Sprite(const Canvas &canvas,
               size_t x,
               size_t y,
               size_t width,
               size_t height,
               Trim trim) {
    auto [leftX, topY, rightX, bottomY] =
            MeasureSpriteBounds(canvas, x, y, width, height, trim);

    if (leftX < rightX && topY < bottomY) {
        size_t size = ExtractSprite(canvas, leftX, topY, rightX, bottomY, NULL);

        if (size > 0) {
            auto buffer = new uint8_t[size];
            (void)ExtractSprite(canvas, leftX, topY, rightX, bottomY, buffer);
            Buffer = buffer;

            Size = size;
            Width = rightX - leftX;
            Height = bottomY - topY;

            Assert(CheckRange(Width, 0, width));
            Assert(CheckRange(Height, 0, height));

            return;
        }
    }

    /* Ignore failures: this simplifies icon handling as icons that aren't
     * present in a version -- and therefore will not be rendered in the
     * first place -- will be glossed over. */
    Width = 0;
    Height = 0;
    Buffer = nullptr;
    Size = 0;
}

Sprite::Sprite(DataReader &data, size_t width, size_t height)
    : Width(width), Height(height) {
    try {
        std::tie(Size, Buffer) = ReadSprite(width, height, data);
    } catch ([[maybe_unused]] const InvalidDataError &err) {
        Buffer = nullptr;
        Size = 0;
    }
}

Sprite::Sprite() : Width(0), Height(0), Size(0), Buffer(nullptr) {
    /* Null sprite: valid but simply won't be drawn. */
}

Sprite &Sprite::operator=(Sprite &&other) {
    Width = other.Width;
    Height = other.Height;
    Size = other.Size;

    std::swap(Buffer, other.Buffer);

    return *this;
}

Sprite::~Sprite() {
    if (Buffer != nullptr) {
        delete[] Buffer;
    }
}

SpriteFile::SpriteFile(const VersionBase &version, DataReader data) {
    Signature = data.ReadU32();

    uint32_t count;
    if (version.Features.SpriteIndexU32) {
        /* To avoid running out of memory on version mismatches, we'll set a
         * reasonably-high upper bound to error out quicker. */
        count = data.ReadU32<1, 1 << 20u>();
    } else {
        count = data.ReadU16();
    }

    auto indexEnd = data.Tell() + count * sizeof(uint32_t);

    /* The empty sprite 0 is not stored in the file per se but is nevertheless
     * considered present; add it and bump ids by 1. */
    (void)Sprites.emplace(std::piecewise_construct,
                          std::forward_as_tuple(0),
                          std::forward_as_tuple());

    for (uint32_t id = 1; id <= count; id++) {
        auto spriteOffset = data.ReadU32();

        if (spriteOffset < indexEnd) {
            /* Ignore failures: it's pretty common for sprite files out in the
             * wild to be subtly corrupt. Skipping this is benign as we'll
             * simply not draw the sprite when asked. */
            continue;
        }

        try {
            auto spriteReader = data.Seek(spriteOffset);

            /* color key */
            spriteReader.Skip(3);

            auto spriteData = spriteReader.Slice(spriteReader.ReadU16());
            (void)Sprites.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(id),
                                  std::forward_as_tuple(spriteData, 32, 32));
        } catch ([[maybe_unused]] const InvalidDataError &err) {
            (void)err;
        }
    }
}

} // namespace trc
