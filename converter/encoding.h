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

#ifndef __TRC_ENCODING_H__
#define __TRC_ENCODING_H__

#include <stdint.h>

#include "canvas.h"

enum TrcEncodeLibrary {
    ENCODE_LIBRARY_LIBAV,
    ENCODE_LIBRARY_SDL2,
    ENCODE_LIBRARY_INERT
};

struct trc_encoder {
    bool (*Open)(struct trc_encoder *handle,
                 const char *format,
                 const char *encoding,
                 const char *flags,
                 int xResolution,
                 int yResolution,
                 int frameRate,
                 const char *path);
    bool (*WriteFrame)(struct trc_encoder *handle, struct trc_canvas *frame);
    void (*Free)(struct trc_encoder *handle);
};

struct trc_encoder *encoder_Create(enum TrcEncodeLibrary library);

bool encoder_Open(struct trc_encoder *handle,
                  const char *format,
                  const char *encoding,
                  const char *flags,
                  int xResolution,
                  int yResolution,
                  int frameRate,
                  const char *path);
void encoder_Free(struct trc_encoder *handle);

bool encoder_WriteFrame(struct trc_encoder *handle, struct trc_canvas *frame);

#endif /* __TRC_ENCODING_H__ */
