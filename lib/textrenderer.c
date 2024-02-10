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

#include "textrenderer.h"
#include "characterset.h"

#include "utils.h"

const static struct trc_pixel DefaultFontColor = {.Red = 0xFF,
                                                  .Green = 0xFF,
                                                  .Blue = 0xFF,
                                                  .Alpha = 0xFF};
const static struct trc_pixel HighlightColor = {.Red = (23 / 36) * 51,
                                                .Green = ((23 / 6) % 6) * 51,
                                                .Blue = (23 % 6) * 51,
                                                .Alpha = 0xFF};

struct trc_textrenderer_state {
    const enum TrcTextTransforms Transform;
    const struct trc_font *Font;
    int DrawRaw;
    int Uppercase;
    int Highlight;
};

static bool textrenderer_Transform(struct trc_textrenderer_state *state,
                                   uint8_t in,
                                   uint8_t *out) {
    switch (in) {
    case '{':
        if (state->Transform & TEXTTRANSFORM_HIGHLIGHT) {
            ASSERT(state->Highlight == 0);
            state->Highlight = 1;
            return false;
        }
        break;
    case '}':
        if (state->Transform & TEXTTRANSFORM_HIGHLIGHT) {
            ASSERT(state->Highlight == 1);
            state->Highlight = 0;
            return false;
        }
        break;
    case '\n':
        return false;
    default:
        break;
    }

    uint8_t character = characterset_ConvertPrintable(in);

    switch (state->Transform) {
    case TEXTTRANSFORM_PROPERCASE:
        if (!state->DrawRaw) {
            if (state->Uppercase) {
                character = characterset_ToUpper(character);
            }

            state->Uppercase = (character == ' ');
        }
        break;
    case TEXTTRANSFORM_LOWERCASE:
        character = characterset_ToLower(character);
        break;
    case TEXTTRANSFORM_UPPERCASE:
        character = characterset_ToUpper(character);
        break;
    default:
        break;
    }

    *out = character;
    return true;
}

static void textrenderer_MeasureLineWidth(
        const struct trc_textrenderer_state *state,
        uint16_t lineLength,
        const char *line,
        uint32_t *result) {
    struct trc_textrenderer_state lineState = *state;
    int stringWidth = 0;

    for (int characterIdx = 0; characterIdx < lineLength; characterIdx++) {
        uint8_t character = line[characterIdx];

        if (character == '\n') {
            break;
        }

        if (textrenderer_Transform(&lineState, character, &character)) {
            stringWidth += (state->Font)->Characters[character].Width;
        }
    }

    (*result) = stringWidth;
}

static bool textrenderer_DetermineLine(struct trc_textrenderer_state *state,
                                       uint32_t maxLength,
                                       uint32_t stringLength,
                                       const char *string,
                                       uint32_t start,
                                       uint32_t *length,
                                       uint32_t *width) {
    uint32_t idx, lastWordIdx, lineLength;
    bool hyphenate;
    uint8_t *line;

    lineLength = MIN(maxLength, stringLength - start);
    line = (uint8_t *)&string[start];
    hyphenate = false;

    /* Clip line length to nearest word to avoid excessive hyphenation. */
    for (idx = 0, lastWordIdx = 0; idx < (stringLength - start); idx++) {
        uint8_t character = line[idx];

        if (character == '\n') {
            lineLength = idx + 1;
            break;
        } else {
            switch (character) {
            case '\0':
            case ' ':
                if (lastWordIdx && idx > lineLength) {
                    lineLength = MIN(lineLength, lastWordIdx);
                    break;
                }

                lastWordIdx = idx;
                break;
            case '{':
            case '}':
                if (state->Transform & TEXTTRANSFORM_HIGHLIGHT) {
                    /* These aren't displayed and shouldn't count towards the
                     * maximum length. */
                    maxLength += 1;
                }
                break;
            }
        }
    }

    if (idx == (stringLength - start) && idx >= maxLength) {
        if (!lastWordIdx) {
            lineLength = lineLength - 1;
            *length = lineLength;
            hyphenate = true;
        } else {
            lineLength = lastWordIdx;
        }
    }

    lineLength = MIN(lineLength, stringLength - start);
    *length = lineLength;

    textrenderer_MeasureLineWidth(state,
                                  (uint16_t)lineLength,
                                  &string[start],
                                  width);

    /* Add in the length of the hyphen, if necessary. */
    if (hyphenate) {
        *width += 8;
    }

    return hyphenate;
}

void textrenderer_MeasureBounds(const struct trc_font *font,
                                const enum TrcTextTransforms transform,
                                const uint16_t lineMaxLength,
                                uint16_t stringLength,
                                const char *string,
                                uint32_t *textWidth,
                                uint32_t *textHeight) {
    uint32_t lineLength, lineStart;

    struct trc_textrenderer_state state = {
            .Font = font,
            .Transform = transform,
            .DrawRaw = characterset_IsUpper(string[0]),
            .Uppercase = !characterset_IsUpper(string[0]),
            .Highlight = 0};

#ifndef NDEBUG
    switch (state.Transform & ~TEXTTRANSFORM_HIGHLIGHT) {
    case TEXTTRANSFORM_PROPERCASE:
    case TEXTTRANSFORM_LOWERCASE:
    case TEXTTRANSFORM_UPPERCASE:
    case TEXTTRANSFORM_NONE:
        break;
    default:
        ASSERT(0);
    }
#endif

    (*textHeight) = 0;
    (*textWidth) = 0;
    lineStart = 0;

    do {
        uint32_t lineWidth;

        (void)textrenderer_DetermineLine(&state,
                                         lineMaxLength,
                                         stringLength,
                                         string,
                                         lineStart,
                                         &lineLength,
                                         &lineWidth);

        lineStart += lineLength;

        (*textWidth) = MAX(*textWidth, lineWidth);
        (*textHeight) += font->Height;
    } while (lineStart < stringLength);
}

void textrenderer_Render(const struct trc_font *font,
                         const enum TrcTextAlignments alignment,
                         const enum TrcTextTransforms transform,
                         const struct trc_pixel *fontColor,
                         int X,
                         int Y,
                         const uint16_t lineMaxLength,
                         const uint16_t stringLength,
                         const char *string,
                         struct trc_canvas *canvas) {
    uint32_t lineLength, lineStart;
    int lineX, lineY;

    struct trc_textrenderer_state state = {
            .Font = font,
            .Transform = transform,
            .DrawRaw = characterset_IsUpper(string[0]),
            .Uppercase = !characterset_IsUpper(string[0]),
            .Highlight = 0};

#ifndef NDEBUG
    switch (state.Transform & ~TEXTTRANSFORM_HIGHLIGHT) {
    case TEXTTRANSFORM_PROPERCASE:
    case TEXTTRANSFORM_LOWERCASE:
    case TEXTTRANSFORM_UPPERCASE:
    case TEXTTRANSFORM_NONE:
        break;
    default:
        ASSERT(0);
    }
#endif

    if (fontColor == NULL) {
        fontColor = &DefaultFontColor;
    }

    lineStart = 0;
    lineY = Y;

    do {
        bool hyphenateLine;
        uint32_t lineWidth;

        hyphenateLine = textrenderer_DetermineLine(&state,
                                                   lineMaxLength,
                                                   stringLength,
                                                   string,
                                                   lineStart,
                                                   &lineLength,
                                                   &lineWidth);

        switch (alignment) {
        case TEXTALIGNMENT_LEFT:
            lineX = X;
            break;
        case TEXTALIGNMENT_CENTER:
            lineX = MAX(2, X - lineWidth / 2);
            break;
        case TEXTALIGNMENT_RIGHT:
            lineX = X - lineWidth;
            break;
        }

        lineWidth = 0;

        for (uint32_t idx = 0; idx < lineLength; idx++) {
            uint8_t character;

            if (textrenderer_Transform(&state,
                                       string[lineStart + idx],
                                       &character)) {
                canvas_DrawCharacter(canvas,
                                     &font->Characters[character].Sprite,
                                     state.Highlight ? &HighlightColor
                                                     : fontColor,
                                     lineX + lineWidth,
                                     lineY);

                lineWidth += font->Characters[character].Width;
            }
        }

        if (hyphenateLine) {
            canvas_DrawCharacter(canvas,
                                 &font->Characters['-'].Sprite,
                                 fontColor,
                                 lineX + lineWidth,
                                 lineY);
        }

        lineStart += lineLength;
        lineY += font->Height;
    } while (lineStart < stringLength);
}
