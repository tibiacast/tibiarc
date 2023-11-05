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

#include "sprites.h"

#include "datareader.h"
#include "versions.h"

#include "utils.h"

/* To avoid running out of memory on version mismatches (where sprite count may
 * be either 16 or 32 bits), we'll set a reasonably-high upper bound to error
 * out gracefully with an error message instead. */
#define MAX_SPRITE_COUNT (1 << 20)

static bool sprites_Validate(int width,
                             int height,
                             struct trc_data_reader reader,
                             size_t *convertedLength) {
    size_t remaining, required;

    required = 0;
    remaining = width * height;

    while (reader.Position < reader.Length) {
        uint16_t transparent;
        uint16_t opaque;

        required += 4;

        if (!datareader_ReadU16(&reader, &transparent)) {
            return false;
        }

        if (remaining < transparent) {
            return false;
        }

        remaining -= transparent;

        if (!datareader_ReadU16(&reader, &opaque)) {
            return false;
        }

        if (remaining < opaque) {
            return false;
        }

        _Static_assert(sizeof(struct trc_pixel) == 4 &&
                       _Alignof(struct trc_pixel) == 1);
        required += opaque * 4;
        remaining -= opaque;

        if (!datareader_Skip(&reader, opaque * 3)) {
            return false;
        }
    }

    *convertedLength = required;
    return true;
}

bool sprites_Read(int width,
                  int height,
                  struct trc_data_reader reader,
                  struct trc_sprite *sprite) {
    size_t convertedLength;
    uint8_t *converted;

    if (!sprites_Validate(width, height, reader, &convertedLength)) {
        return false;
    }

    converted = (uint8_t *)checked_allocate(1, convertedLength);

    sprite->Width = width;
    sprite->Height = height;
    sprite->BufferLength = convertedLength;
    sprite->Buffer = converted;

    while (reader.Position < reader.Length) {
        uint16_t transparent;
        uint16_t opaque;

        ABORT_UNLESS(datareader_ReadU16(&reader, &transparent));
        converted[0] = ((uint16_t)transparent >> 0x00);
        converted[1] = ((uint16_t)transparent >> 0x08);
        converted += 2;

        ABORT_UNLESS(datareader_ReadU16(&reader, &opaque));
        converted[0] = ((uint16_t)opaque >> 0x00);
        converted[1] = ((uint16_t)opaque >> 0x08);
        converted += 2;

        while (opaque > 0) {
            uint8_t colors[3];

            ABORT_UNLESS(datareader_ReadRaw(&reader, 3, colors));

            _Static_assert(sizeof(struct trc_pixel) == 4 &&
                           _Alignof(struct trc_pixel) == 1);
            converted[0] = colors[0];
            converted[1] = colors[1];
            converted[2] = colors[2];
            converted[3] = 0xFF;

            converted += 4;
            opaque--;
        }
    }

    ASSERT(&sprite->Buffer[convertedLength] == converted);
    return true;
}

bool sprites_GetObjectSprite(const struct trc_version *version,
                             uint32_t id,
                             struct trc_sprite **result) {
    const struct trc_spritefile *sprites = &version->Sprites;

    if (id > sprites->Count) {
        return trc_ReportError("Invalid sprite index");
    }

    *result = &sprites->Sprites[id];
    return true;
}

static void sprites_LoadObjectSprite(const struct trc_version *version,
                                     struct trc_data_reader *reader,
                                     struct trc_sprite *result) {
    struct trc_data_reader spriteReader;
    size_t bufferLength;

    /* Skip color key */
    if (!datareader_Skip(reader, 3)) {
        return;
    }

    if (!datareader_ReadU16(reader, &bufferLength)) {
        return;
    }

    if (!datareader_Slice(reader, bufferLength, &spriteReader)) {
        return;
    }

    /* Ignore failures: it's pretty common for the sprite files out in the wild
     * to be subtly corrupt. Skipping this is benign as we'll simply not draw
     * the sprite when asked. */
    (void)sprites_Read(32, 32, spriteReader, result);
}

bool sprites_Load(struct trc_version *version, struct trc_data_reader *data) {
    struct trc_spritefile *sprites = &version->Sprites;

    if (version->Features.SpriteIndexU32) {
        sprites->IndexSize = 4;
    } else {
        sprites->IndexSize = 2;
    }

    if (datareader_ReadU32(data, &sprites->Signature)) {
        uint32_t spriteCount;

        if ((sprites->IndexSize == 4)
                    ? datareader_ReadU32(data, &spriteCount)
                    : datareader_ReadU16(data, &spriteCount)) {
            size_t basePosition, indexEnd;

            if (spriteCount > MAX_SPRITE_COUNT) {
                return trc_ReportError("Sprite count out of range");
            }

            sprites->Count = spriteCount;

            basePosition = datareader_Tell(data);
            indexEnd = basePosition + sprites->Count * sizeof(uint32_t);

            /* The empty sprite, 0, is not stored in the file per se but is
             * nevertheless valid. */
            sprites->Count += 1;
            sprites->Sprites = (struct trc_sprite *)checked_allocate(
                    sprites->Count,
                    sizeof(struct trc_sprite));

            for (uint32_t id = 1; id < sprites->Count; id++) {
                size_t indexOffset, spriteOffset;

                indexOffset = basePosition + (id - 1) * sizeof(uint32_t);
                if (!datareader_Seek(data, indexOffset)) {
                    continue;
                }

                if (!datareader_ReadU32(data, &spriteOffset)) {
                    continue;
                }

                if (spriteOffset < indexEnd ||
                    !datareader_Seek(data, spriteOffset)) {
                    continue;
                }

                sprites_LoadObjectSprite(version, data, &sprites->Sprites[id]);
            }

            return true;
        }
    }

    return trc_ReportError("Corrupt sprite file");
}

void sprites_Free(struct trc_version *version) {
    struct trc_spritefile *sprites = &version->Sprites;

    for (uint32_t id = 0; id < sprites->Count; id++) {
        checked_deallocate((void *)sprites->Sprites[id].Buffer);
    }

    checked_deallocate(sprites->Sprites);
}
