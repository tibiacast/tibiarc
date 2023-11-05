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

struct trc_encoder_inert {
    struct trc_encoder Base;
};

static bool inert_Open(struct trc_encoder_inert *encoder,
                       const char *format,
                       const char *encoding,
                       const char *flags,
                       int xResolution,
                       int yResolution,
                       int frameRate,
                       const char *path) {
    (void)encoder;
    (void)format;
    (void)encoding;
    (void)flags;
    (void)xResolution;
    (void)yResolution;
    (void)frameRate;
    (void)path;

    return true;
}

static bool inert_WriteFrame(struct trc_encoder_inert *encoder,
                             struct trc_canvas *frame) {
    return true;
}

static void inert_Free(struct trc_encoder_inert *file) {
    checked_deallocate(file);
}

struct trc_encoder *inert_Create() {
    struct trc_encoder_inert *encoder = (struct trc_encoder_inert *)
            checked_allocate(1, sizeof(struct trc_encoder_inert));

    encoder->Base.Open = (bool (*)(struct trc_encoder *,
                                   const char *,
                                   const char *,
                                   const char *,
                                   int,
                                   int,
                                   int,
                                   const char *))inert_Open;
    encoder->Base.WriteFrame = (bool (*)(struct trc_encoder *,
                                         struct trc_canvas *))inert_WriteFrame;
    encoder->Base.Free = (void (*)(struct trc_encoder *))inert_Free;

    return (struct trc_encoder *)encoder;
}
