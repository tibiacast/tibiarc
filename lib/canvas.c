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

#include "canvas.h"
#include "characterset.h"

#include <stdlib.h>

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

#define LEVEL1_DCACHE_LINEMASK (LEVEL1_DCACHE_LINESIZE - 1)

struct trc_canvas *canvas_Create(int width, int height) {
    struct trc_canvas *canvas;
    int stride;

    _Static_assert((LEVEL1_DCACHE_LINESIZE & -LEVEL1_DCACHE_LINESIZE) ==
                           LEVEL1_DCACHE_LINESIZE,
                   "Assumed cache line size must be a power of 2");

    /* Cache-align each line and overallocate the canvas as most encoding
     * libraries (e.g. libswscale) work best with aligned input, and are often
     * sloppy with regards to out-of-bound reads. */
    stride = (width * sizeof(struct trc_pixel) + LEVEL1_DCACHE_LINEMASK) &
             ~LEVEL1_DCACHE_LINEMASK;

    canvas =
            (struct trc_canvas *)checked_allocate(1, sizeof(struct trc_canvas));
    canvas->Buffer = checked_aligned_allocate(LEVEL1_DCACHE_LINESIZE,
                                              (height + 1) * stride);
    canvas->Width = width;
    canvas->Height = height;
    canvas->Stride = stride;

    return canvas;
}

void canvas_Slice(struct trc_canvas *source,
                  int leftX,
                  int topY,
                  int rightX,
                  int bottomY,
                  struct trc_canvas *out) {
    out->Width = (rightX - leftX);
    out->Height = (bottomY - topY);
    out->Stride = source->Stride;
    out->Buffer = (uint8_t *)canvas_GetPixel(source, leftX, topY);
}

void canvas_Free(struct trc_canvas *canvas) {
    checked_aligned_deallocate(canvas->Buffer);
    checked_deallocate(canvas);
}

static size_t canvas_Extract(const struct trc_canvas *canvas,
                             int x,
                             int y,
                             int width,
                             int height,
                             int *renderWidth,
                             int *renderHeight,
                             uint8_t *buffer) {
    int bufferIdx, lengthIdx, runLength, transparent;
    const struct trc_pixel *currentPixel;

    (*renderWidth) = 0;
    (*renderHeight) = 0;

    bufferIdx = 2;
    lengthIdx = 0;
    runLength = 0;

    currentPixel = canvas_GetPixel(canvas, x, y);
    transparent = pixel_GetIsTransparent(currentPixel);

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

    for (int yIdx = 0; yIdx < height; yIdx++) {
        for (int xIdx = 0; xIdx < width; xIdx++) {
            currentPixel = canvas_GetPixel(canvas, xIdx + x, yIdx + y);

            if (!pixel_GetIsTransparent(currentPixel)) {
                (*renderWidth) = MAX((*renderWidth), xIdx);
                (*renderHeight) = MAX((*renderHeight), yIdx);

                if (transparent) {
                    if (buffer) {
                        buffer[lengthIdx + 0] = (runLength >> 0) & 0xFF;
                        buffer[lengthIdx + 1] = (runLength >> 8) & 0xFF;
                    }

                    lengthIdx = bufferIdx;
                    bufferIdx += 2;

                    transparent = 0;
                    runLength = 0;
                }

                _Static_assert(sizeof(struct trc_pixel) == 4 &&
                               _Alignof(struct trc_pixel) == 1);
                if (buffer) {
                    *(struct trc_pixel *)&buffer[bufferIdx] = (*currentPixel);
                }

                bufferIdx += sizeof(struct trc_pixel);
                runLength++;
            } else {
                if (!transparent) {
                    if (buffer) {
                        buffer[lengthIdx + 0] = (runLength >> 0) & 0xFF;
                        buffer[lengthIdx + 1] = (runLength >> 8) & 0xFF;
                    }

                    lengthIdx = bufferIdx;
                    bufferIdx += 2;

                    transparent = 1;
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

void canvas_ExtractSprite(const struct trc_canvas *canvas,
                          int x,
                          int y,
                          int width,
                          int height,
                          int *renderWidth,
                          int *renderHeight,
                          struct trc_sprite *result) {
    /* These will be set later in font loading. */
    result->Width = width;
    result->Height = height;
    result->Buffer = NULL;

    (*renderWidth) = 0;
    (*renderHeight) = 0;

    result->BufferLength = canvas_Extract(canvas,
                                          x,
                                          y,
                                          width,
                                          height,
                                          renderWidth,
                                          renderHeight,
                                          NULL);

    uint8_t *buffer = (uint8_t *)checked_allocate(1, result->BufferLength);
    canvas_Extract(canvas,
                   x,
                   y,
                   width,
                   height,
                   renderWidth,
                   renderHeight,
                   buffer);

    result->Buffer = buffer;
}

void canvas_DrawRectangle(struct trc_canvas *canvas,
                          const struct trc_pixel *color,
                          int x,
                          int y,
                          int width,
                          int height) {
    if (!((x < canvas->Width) && (y < canvas->Height) && (x + width >= 0) &&
          (y + height >= 0))) {
        return;
    }

    for (int xIdx = MAX(x, 0); xIdx < MIN(x + width, canvas->Width); xIdx++) {
        memcpy(canvas_GetPixel(canvas, xIdx, y),
               color,
               sizeof(struct trc_pixel));
    }

    for (int yIdx = MAX(y + 1, 0); yIdx < MIN(y + height, canvas->Height);
         yIdx++) {
        int pixelsToCopy = MIN(x + width, canvas->Width) - MAX(x, 0);

        if (pixelsToCopy > 0) {
            void *currentLine, *firstLine;

            currentLine = canvas_GetPixel(canvas, MAX(x, 0), yIdx);
            firstLine = canvas_GetPixel(canvas, MAX(x, 0), y);

            memcpy(currentLine,
                   firstLine,
                   sizeof(struct trc_pixel) * pixelsToCopy);
        }
    }
}

void canvas_DrawCharacter(struct trc_canvas *canvas,
                          const struct trc_sprite *sprite,
                          const struct trc_pixel *fontColor,
                          int x,
                          int y) {
    unsigned pixelIdx = 0, byteIdx = 0;

    if (!((x < canvas->Width) && (y < canvas->Height) &&
          (x + sprite->Width >= 0) && (y + sprite->Height >= 0))) {
        return;
    }

    while (byteIdx < sprite->BufferLength) {
        int pixelCount;

        /* Skipping over transparent pixels. */
        pixelIdx += ((uint16_t)sprite->Buffer[byteIdx + 0] << 0x00) |
                    ((uint16_t)sprite->Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        /* How many colored pixels follow? */
        pixelCount = ((uint16_t)sprite->Buffer[byteIdx + 0] << 0x00) |
                     ((uint16_t)sprite->Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        while (pixelCount > 0) {
            /* Where are we currently at in the sprite? */
            int fromX = pixelIdx % sprite->Width;
            int fromY = pixelIdx / sprite->Width;

            /* And where on the destination canvas? */
            int targetX = x + fromX;
            int targetY = y + fromY;

            if (targetY >= canvas->Height) {
                /* Outside the bottom of the canvas. Nothing to do but
                 * return. */
                return;
            } else if (targetY < 0) {
                /* Above canvas. Skip what pixels are left in block or until
                 * we get on canvas. */
                int skipCount = MIN(sprite->Width - fromX -
                                            (1 + targetY) * sprite->Width,
                                    pixelCount);
                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX >= canvas->Width) {
                /* Right of canvas. Skip what pixels are left in block or
                 * sprite row. */
                int skipCount = MIN(sprite->Width - fromX, pixelCount);
                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX < 0) {
                /* Left of canvas. Skip what pixels are left in block or
                 * until we get on canvas. */
                int skipCount = MIN(-targetX, pixelCount);
                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else {
                /* On the canvas. Tint with what pixels are left in block or
                 * until edge of sprite or canvas. */
                int tintCount = MIN(MIN(sprite->Width - fromX, pixelCount),
                                    canvas->Width - targetX);
                int targetIdx = (targetX * sizeof(struct trc_pixel)) +
                                (targetY * canvas->Stride);
                unsigned stopIdx =
                        byteIdx + tintCount * sizeof(struct trc_pixel);

                while (byteIdx < stopIdx) {
                    const struct trc_pixel *tintKey =
                            (struct trc_pixel *)&sprite->Buffer[byteIdx + 0];
                    struct trc_pixel *targetPixel =
                            (struct trc_pixel *)&canvas->Buffer[targetIdx];

                    targetPixel->Red = (tintKey->Red * fontColor->Red) >> 8;
                    targetPixel->Green =
                            (tintKey->Green * fontColor->Green) >> 8;
                    targetPixel->Blue = (tintKey->Blue * fontColor->Blue) >> 8;

                    targetIdx += sizeof(struct trc_pixel);
                    byteIdx += sizeof(struct trc_pixel);
                }

                pixelCount -= tintCount;
                pixelIdx += tintCount;
            }
        }
    }
}

void canvas_Tint(struct trc_canvas *canvas,
                 const struct trc_sprite *sprite,
                 int x,
                 int y,
                 int width,
                 int height,
                 int head,
                 int primary,
                 int secondary,
                 int detail) {
    static unsigned colorMap[] = {
            0xFFFFFF, 0xFFD4BF, 0xFFE9BF, 0xFFFFBF, 0xE9FFBF, 0xD4FFBF,
            0xBFFFBF, 0xBFFFD4, 0xBFFFE9, 0xBFFFFF, 0xBFE9FF, 0xBFD4FF,
            0xBFBFFF, 0xD4BFFF, 0xE9BFFF, 0xFFBFFF, 0xFFBFE9, 0xFFBFD4,
            0xFFBFBF, 0xDADADA, 0xBF9F8F, 0xBFAF8F, 0xBFBF8F, 0xAFBF8F,
            0x9FBF8F, 0x8FBF8F, 0x8FBF9F, 0x8FBFAF, 0x8FBFBF, 0x8FAFBF,
            0x8F9FBF, 0x8F8FBF, 0x9F8FBF, 0xAF8FBF, 0xBF8FBF, 0xBF8FAF,
            0xBF8F9F, 0xBF8F8F, 0xB6B6B6, 0xBF7F5F, 0xBFAF8F, 0xBFBF5F,
            0x9FBF5F, 0x7FBF5F, 0x5FBF5F, 0x5FBF7F, 0x5FBF9F, 0x5FBFBF,
            0x5F9FBF, 0x5F7FBF, 0x5F5FBF, 0x7F5FBF, 0x9F5FBF, 0xBF5FBF,
            0xBF5F9F, 0xBF5F7F, 0xBF5F5F, 0x919191, 0xBF6A3F, 0xBF943F,
            0xBFBF3F, 0x94BF3F, 0x6ABF3F, 0x3FBF3F, 0x3FBF6A, 0x3FBF94,
            0x3FBFBF, 0x3F94BF, 0x3F6ABF, 0x3F3FBF, 0x6A3FBF, 0x943FBF,
            0xBF3FBF, 0xBF3F94, 0xBF3F6A, 0xBF3F3F, 0x6D6D6D, 0xFF5500,
            0xFFAA00, 0xFFFF00, 0xAAFF00, 0x54FF00, 0x00FF00, 0x00FF54,
            0x00FFAA, 0x00FFFF, 0x00A9FF, 0x0055FF, 0x0000FF, 0x5500FF,
            0xA900FF, 0xFE00FF, 0xFF00AA, 0xFF0055, 0xFF0000, 0x484848,
            0xBF3F00, 0xBF7F00, 0xBFBF00, 0x7FBF00, 0x3FBF00, 0x00BF00,
            0x00BF3F, 0x00BF7F, 0x00BFBF, 0x007FBF, 0x003FBF, 0x0000BF,
            0x3F00BF, 0x7F00BF, 0xBF00BF, 0xBF007F, 0xBF003F, 0xBF0000,
            0x242424, 0x7F2A00, 0x7F5500, 0x7F7F00, 0x557F00, 0x2A7F00,
            0x007F00, 0x007F2A, 0x007F55, 0x007F7F, 0x00547F, 0x002A7F,
            0x00007F, 0x2A007F, 0x54007F, 0x7F007F, 0x7F0055, 0x7F002A,
            0x7F0000,
    };

    const unsigned headB = colorMap[head] >> 0 & 255;
    const unsigned headG = colorMap[head] >> 8 & 255;
    const unsigned headR = colorMap[head] >> 16 & 255;

    const unsigned primaryB = colorMap[primary] >> 0 & 255;
    const unsigned primaryG = colorMap[primary] >> 8 & 255;
    const unsigned primaryR = colorMap[primary] >> 16 & 255;

    const unsigned secondaryB = colorMap[secondary] >> 0 & 255;
    const unsigned secondaryG = colorMap[secondary] >> 8 & 255;
    const unsigned secondaryR = colorMap[secondary] >> 16 & 255;

    const unsigned detailB = colorMap[detail] >> 0 & 255;
    const unsigned detailG = colorMap[detail] >> 8 & 255;
    const unsigned detailR = colorMap[detail] >> 16 & 255;

    unsigned pixelIdx = 0, byteIdx = 0;

    if (!((x < canvas->Width) && (y < canvas->Height) &&
          (x + sprite->Width >= 0) && (y + sprite->Height >= 0))) {
        return;
    }

    while (byteIdx < sprite->BufferLength) {
        int pixelCount;

        /* Skipping over transparent pixels. */
        pixelIdx += ((uint16_t)sprite->Buffer[byteIdx + 0] << 0x00) |
                    ((uint16_t)sprite->Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        /* How many colored pixels follow? */
        pixelCount = ((uint16_t)sprite->Buffer[byteIdx + 0] << 0x00) |
                     ((uint16_t)sprite->Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        while (pixelCount > 0) {
            /* Where are we currently at in the sprite? */
            int fromX = pixelIdx % sprite->Width;
            int fromY = pixelIdx / sprite->Width;

            /* And where on the destination canvas? */
            int targetX = x + fromX;
            int targetY = y + fromY;

            if (targetY >= canvas->Height || fromY >= height) {
                /* Outside the bottom of the canvas. Nothing to do but
                 * return. */
                return;
            } else if (targetY < 0) {
                /* Above canvas. Skip what pixels are left in block or until
                 * we get on canvas. */
                int skipCount = MIN(sprite->Width - fromX -
                                            (1 + targetY) * sprite->Width,
                                    pixelCount);
                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX >= canvas->Width || fromX >= width) {
                /* Right of canvas. Skip what pixels are left in block or
                 * sprite row.
                 */
                int skipCount = MIN(sprite->Width - fromX, pixelCount);
                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX < 0) {
                /* Left of canvas. Skip what pixels are left in block or
                 * until we get on canvas. */
                int skipCount = MIN(-targetX, pixelCount);
                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else {
                /* On the canvas. Tint with what pixels are left in block or
                 * until edge of sprite or canvas. */
                int tintCount = MIN(MIN(sprite->Width - fromX, pixelCount),
                                    canvas->Width - targetX);
                int targetIdx = (targetX * sizeof(struct trc_pixel)) +
                                (targetY * canvas->Stride);
                unsigned stopIdx = byteIdx + MIN(width, tintCount) *
                                                     sizeof(struct trc_pixel);

                while (byteIdx < stopIdx) {
                    const struct trc_pixel *tintKey =
                            (struct trc_pixel *)&sprite->Buffer[byteIdx];
                    struct trc_pixel *targetPixel =
                            (struct trc_pixel *)&canvas->Buffer[targetIdx];

                    if (tintKey->Red == 0 && tintKey->Green == 0 &&
                        tintKey->Blue == 0xFF) {
                        targetPixel->Red = targetPixel->Red * detailR >> 8;
                        targetPixel->Green = targetPixel->Green * detailG >> 8;
                        targetPixel->Blue = targetPixel->Blue * detailB >> 8;
                    } else if (tintKey->Red == 0xFF && tintKey->Green == 0xFF &&
                               tintKey->Blue == 0) {
                        targetPixel->Red = targetPixel->Red * headR >> 8;
                        targetPixel->Green = targetPixel->Green * headG >> 8;
                        targetPixel->Blue = targetPixel->Blue * headB >> 8;
                    } else if (tintKey->Red == 0 && tintKey->Green == 0xFF &&
                               tintKey->Blue == 0) {
                        targetPixel->Red = targetPixel->Red * secondaryR >> 8;
                        targetPixel->Green =
                                targetPixel->Green * secondaryG >> 8;
                        targetPixel->Blue = targetPixel->Blue * secondaryB >> 8;
                    } else if (tintKey->Red == 0xFF && tintKey->Green == 0 &&
                               tintKey->Blue == 0) {
                        targetPixel->Red = targetPixel->Red * primaryR >> 8;
                        targetPixel->Green = targetPixel->Green * primaryG >> 8;
                        targetPixel->Blue = targetPixel->Blue * primaryB >> 8;
                    }

                    targetIdx += sizeof(struct trc_pixel);
                    byteIdx += sizeof(struct trc_pixel);
                }

                byteIdx += (tintCount - MIN(width, tintCount)) *
                           sizeof(struct trc_pixel);

                pixelCount -= tintCount;
                pixelIdx += tintCount;
            }
        }
    }
}

void canvas_Draw(struct trc_canvas *canvas,
                 const struct trc_sprite *sprite,
                 int x,
                 int y,
                 int width,
                 int height) {
    unsigned pixelIdx = 0, byteIdx = 0;

    if (!((x < canvas->Width) && (y < canvas->Height) &&
          (x + sprite->Width >= 0) && (y + sprite->Height >= 0))) {
        return;
    }

    while (byteIdx < sprite->BufferLength) {
        int pixelCount;

        /* Skipping over transparent pixels. */
        pixelIdx += ((uint16_t)sprite->Buffer[byteIdx + 0] << 0x00) |
                    ((uint16_t)sprite->Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        /* How many colored pixels follow? */
        pixelCount = ((uint16_t)sprite->Buffer[byteIdx + 0] << 0x00) |
                     ((uint16_t)sprite->Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        while (pixelCount > 0) {
            /* Where are we currently at in the sprite? */
            int fromX = pixelIdx % sprite->Width;
            int fromY = pixelIdx / sprite->Width;

            /* And where on the destination canvas? */
            int targetX = x + fromX;
            int targetY = y + fromY;

            if (targetY >= canvas->Height || fromY >= height) {
                /* Outside the bottom of the canvas. Nothing to do but
                 * return. */
                return;
            } else if (targetY < 0) {
                /* Above canvas. Skip what pixels are left in block or until
                 * we get on canvas. */
                int skipCount = MIN(sprite->Width - fromX -
                                            (1 + targetY) * sprite->Width,
                                    pixelCount);

                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX >= canvas->Width || fromX >= width) {
                /* Right of canvas. Skip what pixels are left in block or
                 * sprite row.
                 */
                int skipCount = MIN(sprite->Width - fromX, pixelCount);

                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX < 0) {
                /* Left of canvas. Skip what pixels are left in block or
                 * until we get on canvas. */
                int skipCount = MIN(-targetX, pixelCount);

                byteIdx += skipCount * sizeof(struct trc_pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else {
                /* On the canvas. Copy what pixels are left in block or
                 * until edge of sprite or canvas. */
                unsigned destIdx, copyCount;

                copyCount = MIN(MIN(sprite->Width - fromX, pixelCount),
                                canvas->Width - targetX);
                destIdx = (targetX * sizeof(struct trc_pixel)) +
                          (targetY * canvas->Stride);

                ASSERT(width > 0);
                memcpy(&canvas->Buffer[destIdx],
                       &sprite->Buffer[byteIdx],
                       MIN(copyCount, (unsigned)width) *
                               sizeof(struct trc_pixel));

                byteIdx += copyCount * sizeof(struct trc_pixel);
                pixelCount -= copyCount;
                pixelIdx += copyCount;
            }
        }
    }
}

static void canvas_CopyRect(struct trc_canvas *canvas,
                            int dX,
                            int dY,
                            const struct trc_canvas *from,
                            int sX,
                            int sY,
                            int width,
                            int height) {
    int lineOffset, lineWidth;

    if (!((dX < canvas->Width) && (dY < canvas->Height) && (dX + width >= 0) &&
          (dY + height >= 0))) {
        return;
    }

    height = MIN(MIN(height, canvas->Height - dY), from->Height - sY);

    lineWidth = MIN(MIN(width, from->Width - sX), canvas->Width - dX);
    lineOffset = MAX(MAX(-sY, -dY), 0);

    for (; lineOffset < height; lineOffset++) {
        memcpy(canvas_GetPixel(canvas, dX, dY + lineOffset),
               canvas_GetPixel(from, sX, sY + lineOffset),
               lineWidth * sizeof(struct trc_pixel));
    }
}

/* Naive bilinear rescaling intended only as a fallback for when the encode
 * backends don't provide anything better. */
void canvas_RescaleClone(struct trc_canvas *canvas,
                         int leftX,
                         int topY,
                         int rightX,
                         int bottomY,
                         const struct trc_canvas *from) {
    float scaleFactorX, scaleFactorY;
    int width, height;

    width = rightX - leftX;
    height = bottomY - topY;
    ASSERT(width >= 0 && height >= 0);

    scaleFactorX = (float)width / (float)from->Width;
    scaleFactorY = (float)height / (float)from->Height;

    if (scaleFactorX == 1 && scaleFactorY == 1) {
        /* Hacky fast path for when we don't need rescaling, speeds up testing
         * quite a lot. */
        canvas_CopyRect(canvas,
                        leftX,
                        topY,
                        from,
                        0,
                        0,
                        from->Width,
                        from->Height);
        return;
    }

    for (int toY = 0; toY < height; toY++) {
        const int fromY0 = MIN(toY / scaleFactorY, from->Height - 1);
        const int fromY1 = MIN(fromY0 + 1, from->Height - 1);
        const float subOffsY = (toY / scaleFactorY) - fromY0;

        for (int toX = 0; toX < width; toX++) {
            const int fromX0 = MIN(toX / scaleFactorX, from->Width - 1);
            const int fromX1 = MIN(fromX0 + 1, from->Width - 1);
            const float subOffsX = (toX / scaleFactorX) - fromX0;

            const struct trc_pixel *srcPixels[4];
            struct trc_pixel *dstPixel;

            float outRed, outGreen, outBlue, outAlpha;

            srcPixels[0] = canvas_GetPixel(from, fromX0, fromY0);
            srcPixels[1] = canvas_GetPixel(from, fromX1, fromY0);
            srcPixels[2] = canvas_GetPixel(from, fromX0, fromY1);
            srcPixels[3] = canvas_GetPixel(from, fromX1, fromY1);

            outRed = srcPixels[0]->Red * (1 - subOffsX) * (1 - subOffsY);
            outGreen = srcPixels[0]->Green * (1 - subOffsX) * (1 - subOffsY);
            outBlue = srcPixels[0]->Blue * (1 - subOffsX) * (1 - subOffsY);
            outAlpha = srcPixels[0]->Alpha * (1 - subOffsX) * (1 - subOffsY);

            outRed += srcPixels[1]->Red * (subOffsX) * (1 - subOffsY);
            outGreen += srcPixels[1]->Green * (subOffsX) * (1 - subOffsY);
            outBlue += srcPixels[1]->Blue * (subOffsX) * (1 - subOffsY);
            outAlpha += srcPixels[1]->Alpha * (subOffsX) * (1 - subOffsY);

            outRed += srcPixels[2]->Red * (1 - subOffsX) * (subOffsY);
            outGreen += srcPixels[2]->Green * (1 - subOffsX) * (subOffsY);
            outBlue += srcPixels[2]->Blue * (1 - subOffsX) * (subOffsY);
            outAlpha += srcPixels[2]->Alpha * (1 - subOffsX) * (subOffsY);

            outRed += srcPixels[3]->Red * (subOffsX) * (subOffsY);
            outGreen += srcPixels[3]->Green * (subOffsX) * (subOffsY);
            outBlue += srcPixels[3]->Blue * (subOffsX) * (subOffsY);
            outAlpha += srcPixels[3]->Alpha * (subOffsX) * (subOffsY);

            dstPixel = canvas_GetPixel(canvas, toX + leftX, toY + topY);

            dstPixel->Red = (uint8_t)(outRed + 0.5);
            dstPixel->Green = (uint8_t)(outGreen + 0.5);
            dstPixel->Blue = (uint8_t)(outBlue + 0.5);
            dstPixel->Alpha = (uint8_t)(outAlpha + 0.5);
        }
    }
}

void canvas_Wipe(struct trc_canvas *canvas) {
    struct trc_pixel transparentPixel;

    pixel_SetTransparent(&transparentPixel);

    canvas_DrawRectangle(canvas,
                         &transparentPixel,
                         0,
                         0,
                         canvas->Width,
                         canvas->Height);
}

void canvas_Dump(const char *path, const struct trc_canvas *canvas) {
    const int FILE_HEADER_SIZE = 10, INFO_HEADER_SIZE = 40;

    struct trc_pixel *pixels = (struct trc_pixel *)canvas->Buffer;
    uint32_t u32;
    uint16_t u16;

    FILE *bmp = fopen(path, "wb");

    /* File header. */
    fwrite("BM", 2, 1, bmp);
    u32 = (24 * canvas->Height * canvas->Width) + FILE_HEADER_SIZE +
          INFO_HEADER_SIZE;
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = 0; /* reserved? */
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = FILE_HEADER_SIZE + INFO_HEADER_SIZE;
    fwrite(&u32, sizeof(u32), 1, bmp);

    /* Info header */
    u32 = INFO_HEADER_SIZE;
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = canvas->Width;
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = canvas->Height;
    fwrite(&u32, sizeof(u32), 1, bmp);
    u16 = 1; /* plane count */
    fwrite(&u16, sizeof(u16), 1, bmp);
    u16 = 24; /* bit depth */
    fwrite(&u16, sizeof(u16), 1, bmp);
    u32 = 0; /* compression */
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = 0; /* image size */
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = 0; /* horizontal resolution */
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = 0; /* vertical resolution */
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = 0; /* colors in palette */
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = 0; /* important colors */
    fwrite(&u32, sizeof(u32), 1, bmp);

    for (int y = (canvas->Height - 1); y >= 0; y--) {
        for (int x = 0; x < canvas->Width; x++) {
            struct trc_pixel *pixel = &pixels[x + y * canvas->Stride];
            fwrite(&pixel->Blue, 1, 1, bmp);
            fwrite(&pixel->Green, 1, 1, bmp);
            fwrite(&pixel->Red, 1, 1, bmp);
        }

        if (canvas->Width % 4) {
            static const char padding_bytes[3] = {0, 0, 0};
            fwrite(padding_bytes, 1, 4 - (canvas->Width % 4), bmp);
        }
    }

    fclose(bmp);
}
