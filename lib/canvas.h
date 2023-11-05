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

#ifndef __TRC_CANVAS_H__
#define __TRC_CANVAS_H__

#include <stdint.h>

#include "fonts.h"
#include "pixel.h"
#include "sprites.h"

#include "utils.h"

#ifndef LEVEL1_DCACHE_LINESIZE
#    error "LEVEL1_DCACHE_LINESIZE must be #defined"
#endif

#ifdef DEBUG
/* Cache alignment can hide out-of-bounds reads, so we'll turn it off in debug
 * mode. */
#    undef LEVEL1_DCACHE_LINESIZE
#    define LEVEL1_DCACHE_LINESIZE 1
#endif

#ifdef __GNUC__
#    define TRC_ALIGNED(X) __attribute__((aligned(X)))
#else
#    define TRC_ALIGNED(X) __declspec(align(X))
#endif

struct trc_canvas {
    int Width;
    int Height;
    int Stride;

    TRC_ALIGNED(LEVEL1_DCACHE_LINESIZE) uint8_t Buffer[];
};

struct trc_canvas *canvas_Create(int width, int height);
void canvas_Free(struct trc_canvas *canvas);

void canvas_Wipe(struct trc_canvas *canvas);

/* Debug function for dumping a canvas as a 24-bit BMP file. */
void canvas_Dump(const char *path, const struct trc_canvas *canvas);

#define __canvas_At(Canvas, X, Y)                                              \
    (&(Canvas)->Buffer[((X) * sizeof(struct trc_pixel)) +                      \
                       ((Y) * (Canvas)->Stride)])
#ifdef __cplusplus
TRC_UNUSED static const struct trc_pixel *canvas_GetPixel(
        const struct trc_canvas *canvas,
        int x,
        int y) {
    return (const struct trc_pixel *)__canvas_At(canvas, x, y);
}

TRC_UNUSED static struct trc_pixel *canvas_GetPixel(struct trc_canvas *canvas,
                                                    int x,
                                                    int y) {
    return (struct trc_pixel *)__canvas_At(canvas, x, y);
}
#else
#    define canvas_GetPixel(Canvas, X, Y)                                      \
        (ASSERT((X) < (Canvas)->Width && (Y) < (Canvas)->Height),              \
         _Generic((Canvas),                                                    \
                  const struct trc_canvas * :                                  \
                     (const struct trc_pixel *)__canvas_At((Canvas), (X), (Y)),\
                  struct trc_canvas * :                                        \
                     (struct trc_pixel *)__canvas_At((Canvas), (X), (Y))       \
                  ))
#endif

void canvas_ExtractSprite(const struct trc_canvas *canvas,
                          int x,
                          int y,
                          int width,
                          int height,
                          int *renderWidth,
                          int *renderHeight,
                          struct trc_sprite *result);

void canvas_DrawRectangle(struct trc_canvas *canvas,
                          const struct trc_pixel *color,
                          int x,
                          int y,
                          int width,
                          int height);

void canvas_DrawCharacter(struct trc_canvas *canvas,
                          const struct trc_sprite *sprite,
                          const struct trc_pixel *fontColor,
                          int x,
                          int y);

void canvas_Draw(struct trc_canvas *canvas,
                 const struct trc_sprite *sprite,
                 int x,
                 int y,
                 int width,
                 int height);
void canvas_Tint(struct trc_canvas *canvas,
                 const struct trc_sprite *sprite,
                 int x,
                 int y,
                 int width,
                 int height,
                 int head,
                 int primary,
                 int secondary,
                 int detail);

/* Clones `from` onto `canvas` at the given coordinates. */
void canvas_RescaleClone(struct trc_canvas *canvas,
                         int leftX,
                         int topY,
                         int rightX,
                         int bottomY,
                         const struct trc_canvas *from);

#endif /* __TRC_CANVAS_H__ */
