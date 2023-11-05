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

#include "pictures.h"

#include "canvas.h"
#include "sprites.h"
#include "versions.h"

#include <stdlib.h>

#include "utils.h"

bool picture_GetPictureCanvas(const struct trc_version *version,
                              enum TrcPictureIndex index,
                              struct trc_canvas **result) {
    int translated;

    if (!version_TranslatePictureIndex(version, index, &translated)) {
        return trc_ReportError("Failed to translate picture index");
    }

    (*result) = (version->Pictures).Array[translated];

    return true;
}

static bool pictures_ReadPicture(struct trc_data_reader *reader,
                                 struct trc_canvas **picture) {
    int width, height;

    if (!datareader_ReadU8(reader, &width)) {
        return trc_ReportError("Failed to read width");
    }

    if (!datareader_ReadU8(reader, &height)) {
        return trc_ReportError("Failed to read height");
    }

    if (!datareader_Skip(reader, 3)) {
        return trc_ReportError("Failed to skip color key");
    }

    (*picture) = canvas_Create(width * 32, height * 32);
    canvas_Wipe(*picture);

    for (int yIdx = 0; yIdx < height; yIdx++) {
        for (int xIdx = 0; xIdx < width; xIdx++) {
            size_t spriteOffset;
            if (!datareader_ReadU32(reader, &spriteOffset)) {
                return trc_ReportError("Failed to read sprite offset");
            }

            struct trc_data_reader indexReader = *reader;
            if (!datareader_Seek(&indexReader, spriteOffset)) {
                return trc_ReportError("Failed to seek to picture data");
            }

            size_t dataLength;
            if (!datareader_ReadU16(&indexReader, &dataLength)) {
                return trc_ReportError("Failed to read buffer length");
            }

            if (dataLength > 0) {
                struct trc_data_reader spriteReader;
                struct trc_sprite sprite;

                if (!datareader_Slice(&indexReader,
                                      dataLength,
                                      &spriteReader)) {
                    return trc_ReportError("Failed to slice sprite data");
                }

                if (!sprites_Read(32, 32, spriteReader, &sprite)) {
                    return trc_ReportError("Failed to validate picture data");
                }

                canvas_Draw((*picture), &sprite, 32 * xIdx, 32 * yIdx, 32, 32);
                checked_deallocate((void *)sprite.Buffer);
            }
        }
    }

    return true;
}

bool pictures_Load(struct trc_version *version, struct trc_data_reader *data) {
    struct trc_picture_file *pictures = &version->Pictures;

    if (!datareader_ReadU32(data, &pictures->Signature)) {
        return trc_ReportError("Could not read picture signature");
    }

    if (!datareader_ReadU16(data, &pictures->Count)) {
        return trc_ReportError("Could not read picture count");
    }

    if (pictures->Count > PICTURE_MAX_COUNT) {
        return trc_ReportError("Picture count is out of range (%u).",
                               pictures->Count);
    }

    for (int i = 0; i < pictures->Count; i++) {
        struct trc_canvas **canvases = pictures->Array;

        if (!pictures_ReadPicture(data, &canvases[i])) {
            return trc_ReportError("Could not read picture %u.", i);
        }
#ifdef DUMP_PIC
        {
            char path[128];
            snprintf(path, sizeof(path), "pict_%i.bmp", i);
            canvas_Dump(path, canvases[i]);
        }
#endif
    }

    return true;
}

void pictures_Free(struct trc_version *version) {
    struct trc_picture_file *pictures = &version->Pictures;

    for (int i = 0; i < pictures->Count; i++) {
        canvas_Free(pictures->Array[i]);
    }
}
