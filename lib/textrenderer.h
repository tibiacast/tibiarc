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

#ifndef __TRC_TEXTRENDERER_H__
#define __TRC_TEXTRENDERER_H__

#include "fonts.h"

#define CONSTSTRLEN(a) (sizeof(a) - 1)

enum TrcTextAlignments {
    TEXTALIGNMENT_LEFT,
    TEXTALIGNMENT_CENTER,
    TEXTALIGNMENT_RIGHT,
};

enum TrcTextTransforms {
    TEXTTRANSFORM_NONE = 0,
    TEXTTRANSFORM_UPPERCASE = (1 << 0),
    TEXTTRANSFORM_LOWERCASE = (1 << 1),
    TEXTTRANSFORM_PROPERCASE = (1 << 2),
    TEXTTRANSFORM_HIGHLIGHT = (1 << 3),
};

void textrenderer_MeasureBounds(const struct trc_font *font,
                                const enum TrcTextTransforms transform,
                                const uint16_t lineMaxLength,
                                uint16_t stringLength,
                                const char *string,
                                uint32_t *textWidth,
                                uint32_t *textHeight);

/* The colors may be null, in which case they will default to White (font) and
 * Black (border). */

void textrenderer_Render(const struct trc_font *font,
                         const enum TrcTextAlignments alignment,
                         const enum TrcTextTransforms transform,
                         const struct trc_pixel *fontColor,
                         int X,
                         int Y,
                         const uint16_t lineMaxLength,
                         const uint16_t stringLength,
                         const char *string,
                         struct trc_canvas *canvas);

/* Helper macros, calling Render directly all the time would get ugly. Add more
 * as necessary. */

#define textrenderer_DrawRightAlignedString(font,                              \
                                            fontColor,                         \
                                            X,                                 \
                                            Y,                                 \
                                            stringLength,                      \
                                            string,                            \
                                            canvas)                            \
    textrenderer_Render(font,                                                  \
                        TEXTALIGNMENT_RIGHT,                                   \
                        TEXTTRANSFORM_NONE,                                    \
                        fontColor,                                             \
                        X,                                                     \
                        Y,                                                     \
                        ~0,                                                    \
                        stringLength,                                          \
                        string,                                                \
                        canvas)

#define textrenderer_DrawCenteredString(font,                                  \
                                        fontColor,                             \
                                        X,                                     \
                                        Y,                                     \
                                        stringLength,                          \
                                        string,                                \
                                        canvas)                                \
    textrenderer_Render(font,                                                  \
                        TEXTALIGNMENT_CENTER,                                  \
                        TEXTTRANSFORM_NONE,                                    \
                        fontColor,                                             \
                        X,                                                     \
                        Y,                                                     \
                        ~0,                                                    \
                        stringLength,                                          \
                        string,                                                \
                        canvas)
#define textrenderer_DrawCenteredProperCaseString(font,                        \
                                                  fontColor,                   \
                                                  X,                           \
                                                  Y,                           \
                                                  stringLength,                \
                                                  string,                      \
                                                  canvas)                      \
    textrenderer_Render(font,                                                  \
                        TEXTALIGNMENT_CENTER,                                  \
                        TEXTTRANSFORM_PROPERCASE,                              \
                        fontColor,                                             \
                        X,                                                     \
                        Y,                                                     \
                        ~0,                                                    \
                        stringLength,                                          \
                        string,                                                \
                        canvas)

#define textrenderer_DrawProperCaseString(font,                                \
                                          fontColor,                           \
                                          X,                                   \
                                          Y,                                   \
                                          stringLength,                        \
                                          string,                              \
                                          canvas)                              \
    textrenderer_Render(font,                                                  \
                        TEXTALIGNMENT_LEFT,                                    \
                        TEXTTRANSFORM_PROPERCASE,                              \
                        fontColor,                                             \
                        X,                                                     \
                        Y,                                                     \
                        ~0,                                                    \
                        stringLength,                                          \
                        string,                                                \
                        canvas)
#define textrenderer_DrawString(font,                                          \
                                fontColor,                                     \
                                X,                                             \
                                Y,                                             \
                                stringLength,                                  \
                                string,                                        \
                                canvas)                                        \
    textrenderer_Render(font,                                                  \
                        TEXTALIGNMENT_LEFT,                                    \
                        TEXTTRANSFORM_NONE,                                    \
                        fontColor,                                             \
                        X,                                                     \
                        Y,                                                     \
                        ~0,                                                    \
                        stringLength,                                          \
                        string,                                                \
                        canvas)

#endif /* __TRC_TEXTRENDERER_H__ */
