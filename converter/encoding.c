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

#include "encoding.h"

#include "utils.h"

struct trc_encoder *encoder_Create(enum TrcEncodeLibrary library) {
    switch (library) {
#ifndef DISABLE_LIBAV
    case ENCODE_LIBRARY_LIBAV: {
        extern struct trc_encoder *libav_Create(void);
        return libav_Create();
    }
#endif
    case ENCODE_LIBRARY_INERT: {
        extern struct trc_encoder *inert_Create(void);
        return inert_Create();
    }
    default:
        abort();
    }
}

bool encoder_Open(struct trc_encoder *handle,
                  const char *format,
                  const char *encoding,
                  const char *flags,
                  int xResolution,
                  int yResolution,
                  int frameRate,
                  const char *path) {
    return handle->Open(handle,
                        format,
                        encoding,
                        flags,
                        xResolution,
                        yResolution,
                        frameRate,
                        path);
}

bool encoder_WriteFrame(struct trc_encoder *encoder, struct trc_canvas *frame) {
    return encoder->WriteFrame(encoder, frame);
}

void encoder_Free(struct trc_encoder *encoder) {
    encoder->Free(encoder);
}
