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

#include "textrenderer.hpp"
#include "characterset.hpp"

#include "utils.hpp"

#include <algorithm>

namespace trc {
namespace TextRenderer {

const static Pixel HighlightColor((23 / 36) * 51,
                                  ((23 / 6) % 6) * 51,
                                  (23 % 6) * 51);

struct TextRenderState {
    const TextTransform Transform;
    const trc::Font &Font;
    int DrawRaw;
    int Uppercase;
    int Highlight;
};

static bool Transform(TextRenderState &state, uint8_t in, uint8_t &out) {
    switch (in) {
    case '{':
        if (state.Transform == TextTransform::Highlight) {
            Assert(state.Highlight == 0);
            state.Highlight = 1;
            return false;
        }
        break;
    case '}':
        if (state.Transform == TextTransform::Highlight) {
            Assert(state.Highlight == 1);
            state.Highlight = 0;
            return false;
        }
        break;
    case '\n':
        return false;
    default:
        break;
    }

    out = CharacterSet::ToPrintable(in);

    switch (state.Transform) {
    case TextTransform::ProperCase:
        if (!state.DrawRaw) {
            if (state.Uppercase) {
                out = CharacterSet::ToUpper(out);
            }

            state.Uppercase = (out == ' ');
        }
        break;
    case TextTransform::LowerCase:
        out = CharacterSet::ToLower(out);
        break;
    case TextTransform::UpperCase:
        out = CharacterSet::ToUpper(out);
        break;
    default:
        break;
    }

    return true;
}

static void MeasureLineWidth(const TextRenderState &state,
                             size_t lineLength,
                             uint8_t *line,
                             size_t &result) {
    TextRenderState lineState = state;
    size_t stringWidth = 0;

    for (size_t idx = 0; idx < lineLength; idx++) {
        uint8_t character = line[idx];
        uint8_t printable;

        if (character == '\n') {
            break;
        }

        if (Transform(lineState, character, printable)) {
            stringWidth += state.Font.Characters[printable].Width;
        }
    }

    result = stringWidth;
}

static bool DetermineLine(TextRenderState &state,
                          size_t maxLength,
                          const std::string &text,
                          size_t start,
                          size_t &length,
                          size_t &width) {
    size_t idx, lastWordIdx, lineLength;
    bool hyphenate;
    uint8_t *line;

    lineLength = std::min(maxLength, text.size() - start);
    line = (uint8_t *)(text.data() + start);
    hyphenate = false;

    /* Clip line length to nearest word to avoid excessive hyphenation. */
    for (idx = 0, lastWordIdx = 0; idx < (text.size() - start); idx++) {
        uint8_t character = line[idx];

        if (character == '\n') {
            lineLength = idx + 1;
            break;
        } else {
            switch (character) {
            case '\0':
            case ' ':
                if (lastWordIdx && idx > lineLength) {
                    lineLength = std::min(lineLength, lastWordIdx);
                    break;
                }

                lastWordIdx = idx;
                break;
            case '{':
            case '}':
                if (state.Transform == TextTransform::Highlight) {
                    /* These aren't displayed and shouldn't count towards the
                     * maximum length. */
                    maxLength += 1;
                }
                break;
            }
        }
    }

    if (idx == (text.size() - start) && idx >= maxLength) {
        if (!lastWordIdx) {
            lineLength = lineLength - 1;
            length = lineLength;
            hyphenate = true;
        } else {
            lineLength = lastWordIdx;
        }
    }

    lineLength = std::min(lineLength, text.size() - start);
    length = lineLength;

    MeasureLineWidth(state, lineLength, line, width);

    /* Add in the length of the hyphen, if necessary. */
    if (hyphenate) {
        width += 8;
    }

    return hyphenate;
}

std::pair<size_t, size_t> MeasureBounds(const Font &font,
                                        const TextTransform transform,
                                        const size_t lineMaxLength,
                                        const std::string &text) {
    if (text.size() == 0) {
        return std::make_pair(0, 0);
    }

    TextRenderState state = {.Transform = transform,
                             .Font = font,
                             .DrawRaw = CharacterSet::IsUpper(text.front()),
                             .Uppercase = !CharacterSet::IsUpper(text.front()),
                             .Highlight = 0};

    size_t textWidth = 0, textHeight = 0;

    for (size_t lineStart = 0; lineStart < text.size();) {
        size_t lineLength, lineWidth;

        (void)DetermineLine(state,
                            lineMaxLength,
                            text,
                            lineStart,
                            lineLength,
                            lineWidth);

        lineStart += lineLength;

        textWidth = std::max(textWidth, lineWidth);
        textHeight += font.Height;
    }

    return std::make_pair(textWidth, textHeight);
}

void Render(const Font &font,
            const TextAlignment alignment,
            const TextTransform transform,
            const trc::Pixel &color,
            const int X,
            const int Y,
            const size_t lineMaxLength,
            const std::string &text,
            Canvas &canvas) {
    size_t lineLength, lineStart;
    size_t lineX, lineY;

    if (text.size() == 0) {
        return;
    }

    TextRenderState state = {.Transform = transform,
                             .Font = font,
                             .DrawRaw = CharacterSet::IsUpper(text.front()),
                             .Uppercase = !CharacterSet::IsUpper(text.front()),
                             .Highlight = 0};

    lineStart = 0;
    lineY = Y;

    do {
        bool hyphenateLine;
        size_t lineWidth;

        hyphenateLine = DetermineLine(state,
                                      lineMaxLength,
                                      text,
                                      lineStart,
                                      lineLength,
                                      lineWidth);

        switch (alignment) {
        case TextAlignment::Left:
            lineX = X;
            break;
        case TextAlignment::Center:
            lineX = std::max<size_t>(2, X - lineWidth / 2);
            break;
        case TextAlignment::Right:
            lineX = X - lineWidth;
            break;
        }

        lineWidth = 0;

        for (uint32_t idx = 0; idx < lineLength; idx++) {
            uint8_t printable;

            if (Transform(state, text[lineStart + idx], printable)) {
                const auto &character = font.Characters[printable];

                canvas.DrawCharacter(character.Sprite,
                                     state.Highlight ? HighlightColor : color,
                                     lineX + lineWidth,
                                     lineY);

                lineWidth += character.Width;
            }
        }

        if (hyphenateLine) {
            canvas.DrawCharacter(font.Characters['-'].Sprite,
                                 color,
                                 lineX + lineWidth,
                                 lineY);
        }

        lineStart += lineLength;
        lineY += font.Height;
    } while (lineStart < text.size());
}

} // namespace TextRenderer

} // namespace trc