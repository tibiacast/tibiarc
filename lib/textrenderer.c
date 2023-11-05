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
                                                  .Blue = 0xFF};
const static struct trc_pixel HighlightColor = {.Red = (23 / 36) * 51,
                                                .Green = ((23 / 6) % 6) * 51,
                                                .Blue = (23 % 6) * 51};

static void textrenderer_MeasureLineWidth(const struct trc_font *font,
                                          uint16_t lineLength,
                                          const char *line,
                                          uint32_t *result) {
    int stringWidth = 0;

    for (int characterIdx = 0; characterIdx < lineLength; characterIdx++) {
        uint8_t currentCharacter = line[characterIdx];

        switch (currentCharacter) {
        case '\n':
            break;
        default:
            currentCharacter = characterset_ConvertPrintable(currentCharacter);
            stringWidth += font->Characters[currentCharacter].Width;
        }
    }

    (*result) = stringWidth;
}

void textrenderer_MeasureBounds(const struct trc_font *font,
                                const uint16_t lineMaxLength,
                                uint16_t stringLength,
                                const char *string,
                                uint32_t *textWidth,
                                uint32_t *textHeight) {
    uint32_t lineLength, lineStart;

    (*textHeight) = 0;
    (*textWidth) = 0;
    lineStart = 0;

    do {
        uint32_t characterIdx, lastWordIdx;
        uint32_t hyphenateLine, lineWidth;
        const char *currentLine;

        lineLength = MIN(lineMaxLength, stringLength - lineStart);
        currentLine = &string[lineStart];
        hyphenateLine = 0;

        /* Clip line length to nearest word to avoid excessive hyphenation. */
        for (characterIdx = 0, lastWordIdx = 0;
             characterIdx < (stringLength - lineStart);
             characterIdx++) {
            uint8_t currentCharacter = currentLine[characterIdx];

            if (currentCharacter == '\n') {
                lineLength = characterIdx + 1;
                break;
            } else {
                switch (currentCharacter) {
                case '\0':
                case ' ':
                    if (lastWordIdx && characterIdx > lineMaxLength) {
                        lineLength = MIN(lineLength, lastWordIdx);

                        break;
                    }

                    lastWordIdx = characterIdx;
                    break;
                }
            }
        }

        if (characterIdx == (stringLength - lineStart) &&
            characterIdx >= lineMaxLength) {
            if (!lastWordIdx) {
                lineLength = lineMaxLength - 1;
                hyphenateLine = 1;
            } else {
                lineLength = lastWordIdx;
            }
        }

        textrenderer_MeasureLineWidth(font,
                                      (uint16_t)lineLength,
                                      currentLine,
                                      &lineWidth);

        /* Add in the length of the hyphen, if necessary. */
        if (hyphenateLine) {
            lineWidth += 8;
        }

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
    const struct trc_pixel *drawColor;
    int upperCase, drawRaw;
    int lineX, lineY;

    if (fontColor == NULL) {
        fontColor = &DefaultFontColor;
    }

#ifdef DEBUG
    switch (transform & ~TEXTTRANSFORM_HIGHLIGHT) {
    case TEXTTRANSFORM_PROPERCASE:
    case TEXTTRANSFORM_LOWERCASE:
    case TEXTTRANSFORM_UPPERCASE:
    case TEXTTRANSFORM_NONE:
        break;
    default:
        ASSERT(0);
    }
#endif

    drawRaw = characterset_IsUpper(string[0]);
    drawColor = fontColor;
    upperCase = !drawRaw;

    lineStart = 0;
    lineY = Y;

    do {
        uint32_t characterIdx, hyphenateLine, lineWidth;
        const char *currentLine;

        lineLength = MIN(lineMaxLength, stringLength - lineStart);
        currentLine = &string[lineStart];
        hyphenateLine = 0;

        /* Don't bother calculating this if it'll be a single line anyhow. */
        if (lineMaxLength <= stringLength) {
            uint32_t lastWordIdx;

            /* Clip line length to the nearest word to avoid excessive
             * hyphenation. */
            for (characterIdx = 0, lastWordIdx = 0;
                 characterIdx < (stringLength - lineStart);
                 characterIdx++) {
                uint8_t currentCharacter = currentLine[characterIdx];

                if (currentCharacter == '\n') {
                    lineLength = characterIdx + 1;
                    break;
                } else {
                    switch (currentLine[characterIdx]) {
                    case '\0':
                    case ' ':
                        if (lastWordIdx && characterIdx > lineMaxLength) {
                            lineLength = MIN(lineLength - 1, lastWordIdx);

                            break;
                        }

                        lastWordIdx = characterIdx;
                        break;
                    case '{':
                    case '}':
                        if (transform & TEXTTRANSFORM_HIGHLIGHT) {
                            lineLength -= 1;
                        }
                        break;
                    }
                }
            }

            if (characterIdx == (stringLength - lineStart) &&
                characterIdx >= lineMaxLength) {
                if (!lastWordIdx) {
                    lineLength = lineMaxLength - 1;
                    hyphenateLine = 1;
                } else {
                    lineLength = lastWordIdx;
                }
            }
        }

        textrenderer_MeasureLineWidth(font,
                                      (uint16_t)lineLength,
                                      currentLine,
                                      &lineWidth);

        /* Add in the length of the hyphen, if necessary. */
        if (hyphenateLine) {
            lineWidth += 8;
        }

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

        for (characterIdx = 0; characterIdx < lineLength; characterIdx++) {
            uint8_t currentCharacter = currentLine[characterIdx];

            switch (currentCharacter) {
            case '\n':
                /* Just skip it. */
                break;
            default:
                currentCharacter =
                        characterset_ConvertPrintable(currentCharacter);

                switch (transform) {
                case TEXTTRANSFORM_PROPERCASE:
                    if (!drawRaw) {
                        if (upperCase) {
                            currentCharacter =
                                    characterset_ToUpper(currentCharacter);
                        }

                        upperCase = (currentCharacter == ' ');
                    }
                    break;
                case TEXTTRANSFORM_LOWERCASE:
                    currentCharacter = characterset_ToLower(currentCharacter);
                    break;
                case TEXTTRANSFORM_UPPERCASE:
                    currentCharacter = characterset_ToUpper(currentCharacter);
                    break;
                default:
                    break;
                }

                if (transform & TEXTTRANSFORM_HIGHLIGHT) {
                    switch (currentCharacter) {
                    case '{':
                        drawColor = &HighlightColor;
                        continue;
                    case '}':
                        ASSERT(drawColor == &HighlightColor);
                        drawColor = fontColor;
                        continue;
                    default:
                        break;
                    }
                }

                canvas_DrawCharacter(canvas,
                                     &font->Characters[currentCharacter].Sprite,
                                     drawColor,
                                     lineX + lineWidth,
                                     lineY);

                lineWidth += font->Characters[currentCharacter].Width;
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
