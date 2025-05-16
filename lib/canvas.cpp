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

#include "canvas.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "utils.hpp"

#ifndef LEVEL1_DCACHE_LINESIZE
#    error "LEVEL1_DCACHE_LINESIZE must be #defined"
#endif

#ifndef NDEBUG
/* Cache alignment can hide out-of-bounds reads, so we'll turn it off in debug
 * mode. */
#    undef LEVEL1_DCACHE_LINESIZE
#    define LEVEL1_DCACHE_LINESIZE 1
#endif

#define LEVEL1_DCACHE_LINEMASK (LEVEL1_DCACHE_LINESIZE - 1)

namespace trc {

static void *AlignedAllocate(size_t alignment, size_t size) {
    /* While aligned_alloc is provided by C11, it isn't supported on Windows
     * and has to be emulated in user space.
     *
     * While MSVC provides _aligned_malloc to help with this, MinGW doesn't
     * have anything at all, so we'll do it ourselves as this kind of
     * allocation is very rare. */
    AbortUnless((alignment & -alignment) == alignment);
    AbortUnless(size % alignment == 0);
    AbortUnless(size < (SIZE_MAX - sizeof(ptrdiff_t)));
    AbortUnless(alignment < (SIZE_MAX - size - sizeof(ptrdiff_t)));

    if (alignment <= sizeof(ptrdiff_t)) {
        return std::malloc(size);
    }

#if defined(_WIN32)
    uintptr_t base, actual;

    base = (uintptr_t)std::malloc(size + sizeof(ptrdiff_t) + alignment);
    AbortUnless(base);

    actual = (base + sizeof(ptrdiff_t) + (alignment - 1)) & ~(alignment - 1);
    ((ptrdiff_t *)actual)[-1] = actual - base;

    return (void *)actual;
#else
    void *result;

    result = std::aligned_alloc(alignment, size);
    AbortUnless(result);

    return result;
#endif
}

static void AlignedDeallocate(void *ptr) {
    if (ptr) {
#if defined(_WIN32)
        ptr = (void *)(((uintptr_t)ptr) - ((ptrdiff_t *)ptr)[-1]);
#endif
        std::free(ptr);
    }
}

Canvas::Canvas(int width, int height, Canvas::Type kind)
    : Kind(kind), Width(width), Height(height) {
    static_assert((LEVEL1_DCACHE_LINESIZE & -LEVEL1_DCACHE_LINESIZE) ==
                          LEVEL1_DCACHE_LINESIZE,
                  "Assumed cache line size must be a power of 2");
    if (kind == Canvas::Type::Internal) {
        /* Cache-align each line and overallocate the canvas as most encoding
         * libraries (e.g. libswscale) work best with aligned input, and are
         * often sloppy with regards to out-of-bound reads. */
        Stride = (width * sizeof(trc::Pixel) + LEVEL1_DCACHE_LINEMASK) &
                 ~LEVEL1_DCACHE_LINEMASK;
        Buffer = (uint8_t *)AlignedAllocate(LEVEL1_DCACHE_LINESIZE,
                                            (Height + 1) * Stride);
    }
}

Canvas::Canvas(int width, int height, int stride, uint8_t *buffer)
    : Kind(Canvas::Type::Slice),
      Width(width),
      Height(height),
      Stride(stride),
      Buffer(buffer) {
}

const Canvas Canvas::Slice(int leftX, int topY, int rightX, int bottomY) {
    return Canvas(rightX - leftX,
                  bottomY - topY,
                  Stride,
                  (uint8_t *)&GetPixel(leftX, topY));
}

Canvas::~Canvas() {
    if (Kind == Type::Internal) {
        AlignedDeallocate(Buffer);
    }

    /* FIXME: C++ migration, keep parent alive somehow. */
}

void Canvas::DrawRectangle(const trc::Pixel &color,
                           const int x,
                           const int y,
                           const int width,
                           const int height) {
    if (!((x < Width) && (y < Height) && (x + width >= 0) &&
          (y + height >= 0))) {
        return;
    }

    for (int xIdx = std::max(x, 0); xIdx < std::min(x + width, Width); xIdx++) {
        memcpy(&GetPixel(xIdx, y), &color, sizeof(trc::Pixel));
    }

    for (int yIdx = std::max(y + 1, 0); yIdx < std::min(y + height, Height);
         yIdx++) {
        int pixelsToCopy = std::min(x + width, Width) - std::max(x, 0);

        if (pixelsToCopy > 0) {
            memcpy(&GetPixel(std::max(x, 0), yIdx),
                   &GetPixel(std::max(x, 0), y),
                   sizeof(trc::Pixel) * pixelsToCopy);
        }
    }
}

void Canvas::DrawCharacter(const trc::Sprite &sprite,
                           const trc::Pixel &fontColor,
                           const int x,
                           const int y) {
    unsigned pixelIdx = 0, byteIdx = 0;

    if (!((x < Width) && (y < Height) && (x + sprite.Width >= 0) &&
          (y + sprite.Height >= 0))) {
        return;
    }

    while (byteIdx < sprite.Size) {
        int pixelCount;

        /* Skipping over transparent pixels. */
        pixelIdx += ((uint16_t)sprite.Buffer[byteIdx + 0] << 0x00) |
                    ((uint16_t)sprite.Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        /* How many colored pixels follow? */
        pixelCount = ((uint16_t)sprite.Buffer[byteIdx + 0] << 0x00) |
                     ((uint16_t)sprite.Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        while (pixelCount > 0) {
            /* Where are we currently at in the sprite? */
            int fromX = pixelIdx % sprite.Width;
            int fromY = pixelIdx / sprite.Width;

            /* And where on the destination canvas? */
            int targetX = x + fromX;
            int targetY = y + fromY;

            if (targetY >= Height) {
                /* Outside the bottom of the canvas. Nothing to do but
                 * return. */
                return;
            } else if (targetY < 0) {
                /* Above canvas. Skip what pixels are left in block or until
                 * we get on canvas. */
                int skipCount = std::min(sprite.Width - fromX -
                                                 (1 + targetY) * sprite.Width,
                                         pixelCount);
                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX >= Width) {
                /* Right of canvas. Skip what pixels are left in block or
                 * sprite row. */
                int skipCount = std::min(sprite.Width - fromX, pixelCount);
                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX < 0) {
                /* Left of canvas. Skip what pixels are left in block or
                 * until we get on canvas. */
                int skipCount = std::min(-targetX, pixelCount);
                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else {
                /* On the canvas. Tint with what pixels are left in block or
                 * until edge of sprite or canvas. */
                int tintCount =
                        std::min(std::min(sprite.Width - fromX, pixelCount),
                                 Width - targetX);
                int targetIdx =
                        (targetX * sizeof(trc::Pixel)) + (targetY * Stride);
                unsigned stopIdx = byteIdx + tintCount * sizeof(trc::Pixel);

                while (byteIdx < stopIdx) {
                    const trc::Pixel &tintKey =
                            *(trc::Pixel *)&sprite.Buffer[byteIdx + 0];
                    trc::Pixel &targetPixel = *(trc::Pixel *)&Buffer[targetIdx];

                    targetPixel.Red = (tintKey.Red * fontColor.Red) >> 8;
                    targetPixel.Green = (tintKey.Green * fontColor.Green) >> 8;
                    targetPixel.Blue = (tintKey.Blue * fontColor.Blue) >> 8;
                    targetPixel.Alpha = fontColor.Alpha;

                    targetIdx += sizeof(trc::Pixel);
                    byteIdx += sizeof(trc::Pixel);
                }

                pixelCount -= tintCount;
                pixelIdx += tintCount;
            }
        }
    }
}

void Canvas::Tint(const trc::Sprite &sprite,
                  const int x,
                  const int y,
                  const int width,
                  const int height,
                  const int head,
                  const int primary,
                  const int secondary,
                  const int detail) {
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

    if (!((x < Width) && (y < Height) && (x + sprite.Width >= 0) &&
          (y + sprite.Height >= 0))) {
        return;
    }

    while (byteIdx < sprite.Size) {
        int pixelCount;

        /* Skipping over transparent pixels. */
        pixelIdx += ((uint16_t)sprite.Buffer[byteIdx + 0] << 0x00) |
                    ((uint16_t)sprite.Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        /* How many colored pixels follow? */
        pixelCount = ((uint16_t)sprite.Buffer[byteIdx + 0] << 0x00) |
                     ((uint16_t)sprite.Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        while (pixelCount > 0) {
            /* Where are we currently at in the sprite? */
            int fromX = pixelIdx % sprite.Width;
            int fromY = pixelIdx / sprite.Width;

            /* And where on the destination canvas? */
            int targetX = x + fromX;
            int targetY = y + fromY;

            if (targetY >= Height || fromY >= height) {
                /* Outside the bottom of the canvas. Nothing to do but
                 * return. */
                return;
            } else if (targetY < 0) {
                /* Above canvas. Skip what pixels are left in block or until
                 * we get on canvas. */
                int skipCount = std::min(sprite.Width - fromX -
                                                 (1 + targetY) * sprite.Width,
                                         pixelCount);
                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX >= Width || fromX >= width) {
                /* Right of canvas. Skip what pixels are left in block or
                 * sprite row.
                 */
                int skipCount = std::min(sprite.Width - fromX, pixelCount);
                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX < 0) {
                /* Left of canvas. Skip what pixels are left in block or
                 * until we get on canvas. */
                int skipCount = std::min(-targetX, pixelCount);
                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else {
                /* On the canvas. Tint with what pixels are left in block or
                 * until edge of sprite or canvas. */
                int tintCount =
                        std::min(std::min(sprite.Width - fromX, pixelCount),
                                 Width - targetX);
                int targetIdx =
                        (targetX * sizeof(trc::Pixel)) + (targetY * Stride);
                unsigned stopIdx = byteIdx + std::min(width, tintCount) *
                                                     sizeof(trc::Pixel);

                while (byteIdx < stopIdx) {
                    const trc::Pixel &tintKey =
                            *(trc::Pixel *)&sprite.Buffer[byteIdx];
                    trc::Pixel *targetPixel = (trc::Pixel *)&Buffer[targetIdx];

                    if (tintKey.Red == 0 && tintKey.Green == 0 &&
                        tintKey.Blue == 0xFF) {
                        targetPixel->Red = targetPixel->Red * detailR >> 8;
                        targetPixel->Green = targetPixel->Green * detailG >> 8;
                        targetPixel->Blue = targetPixel->Blue * detailB >> 8;
                    } else if (tintKey.Red == 0xFF && tintKey.Green == 0xFF &&
                               tintKey.Blue == 0) {
                        targetPixel->Red = targetPixel->Red * headR >> 8;
                        targetPixel->Green = targetPixel->Green * headG >> 8;
                        targetPixel->Blue = targetPixel->Blue * headB >> 8;
                    } else if (tintKey.Red == 0 && tintKey.Green == 0xFF &&
                               tintKey.Blue == 0) {
                        targetPixel->Red = targetPixel->Red * secondaryR >> 8;
                        targetPixel->Green =
                                targetPixel->Green * secondaryG >> 8;
                        targetPixel->Blue = targetPixel->Blue * secondaryB >> 8;
                    } else if (tintKey.Red == 0xFF && tintKey.Green == 0 &&
                               tintKey.Blue == 0) {
                        targetPixel->Red = targetPixel->Red * primaryR >> 8;
                        targetPixel->Green = targetPixel->Green * primaryG >> 8;
                        targetPixel->Blue = targetPixel->Blue * primaryB >> 8;
                    }

                    targetIdx += sizeof(trc::Pixel);
                    byteIdx += sizeof(trc::Pixel);
                }

                byteIdx += (tintCount - std::min(width, tintCount)) *
                           sizeof(trc::Pixel);

                pixelCount -= tintCount;
                pixelIdx += tintCount;
            }
        }
    }
}

void Canvas::Draw(const trc::Sprite &sprite,
                  const int x,
                  const int y,
                  const int width,
                  const int height) {
    unsigned pixelIdx = 0, byteIdx = 0;

    if (!((x < Width) && (y < Height) && (x + sprite.Width >= 0) &&
          (y + sprite.Height >= 0))) {
        return;
    }

    while (byteIdx < sprite.Size) {
        int pixelCount;

        /* Skipping over transparent pixels. */
        pixelIdx += ((uint16_t)sprite.Buffer[byteIdx + 0] << 0x00) |
                    ((uint16_t)sprite.Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        /* How many colored pixels follow? */
        pixelCount = ((uint16_t)sprite.Buffer[byteIdx + 0] << 0x00) |
                     ((uint16_t)sprite.Buffer[byteIdx + 1] << 0x08);
        byteIdx += 2;

        while (pixelCount > 0) {
            /* Where are we currently at in the sprite? */
            int fromX = pixelIdx % sprite.Width;
            int fromY = pixelIdx / sprite.Width;

            /* And where on the destination canvas? */
            int targetX = x + fromX;
            int targetY = y + fromY;

            if (targetY >= Height || fromY >= height) {
                /* Outside the bottom of the canvas. Nothing to do but
                 * return. */
                return;
            } else if (targetY < 0) {
                /* Above canvas. Skip what pixels are left in block or until
                 * we get on canvas. */
                int skipCount = std::min(sprite.Width - fromX -
                                                 (1 + targetY) * sprite.Width,
                                         pixelCount);

                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX >= Width || fromX >= width) {
                /* Right of canvas. Skip what pixels are left in block or
                 * sprite row.
                 */
                int skipCount = std::min(sprite.Width - fromX, pixelCount);

                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else if (targetX < 0) {
                /* Left of canvas. Skip what pixels are left in block or
                 * until we get on canvas. */
                int skipCount = std::min(-targetX, pixelCount);

                byteIdx += skipCount * sizeof(trc::Pixel);
                pixelCount -= skipCount;
                pixelIdx += skipCount;
            } else {
                /* On the canvas. Copy what pixels are left in block or
                 * until edge of sprite or canvas. */
                unsigned destIdx, copyCount;

                copyCount = std::min(std::min(sprite.Width - fromX, pixelCount),
                                     Width - targetX);
                destIdx = (targetX * sizeof(trc::Pixel)) + (targetY * Stride);

                Assert(width > 0);
                memcpy(&Buffer[destIdx],
                       &sprite.Buffer[byteIdx],
                       std::min(copyCount, (unsigned)width) *
                               sizeof(trc::Pixel));

                byteIdx += copyCount * sizeof(trc::Pixel);
                pixelCount -= copyCount;
                pixelIdx += copyCount;
            }
        }
    }
}

void Canvas::Wipe() {
    DrawRectangle(Pixel(0, 0, 0, 0), 0, 0, Width, Height);
}

void Canvas::Dump(const char *path) const {
    const int FILE_HEADER_SIZE = 10, INFO_HEADER_SIZE = 40;
    uint32_t u32;
    uint16_t u16;

    FILE *bmp = fopen(path, "wb");

    /* File header. */
    fwrite("BM", 2, 1, bmp);
    u32 = (24 * Height * Width) + FILE_HEADER_SIZE + INFO_HEADER_SIZE;
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = 0; /* reserved? */
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = FILE_HEADER_SIZE + INFO_HEADER_SIZE;
    fwrite(&u32, sizeof(u32), 1, bmp);

    /* Info header */
    u32 = INFO_HEADER_SIZE;
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = Width;
    fwrite(&u32, sizeof(u32), 1, bmp);
    u32 = Height;
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

    for (int y = (Height - 1); y >= 0; y--) {
        for (int x = 0; x < Width; x++) {
            const trc::Pixel &pixel = GetPixel(x, y);
            fwrite(&pixel.Blue, 1, 1, bmp);
            fwrite(&pixel.Green, 1, 1, bmp);
            fwrite(&pixel.Red, 1, 1, bmp);
        }

        if (Width % 4) {
            static const char padding_bytes[3] = {0, 0, 0};
            fwrite(padding_bytes, 1, 4 - (Width % 4), bmp);
        }
    }

    fclose(bmp);
}

} // namespace trc
