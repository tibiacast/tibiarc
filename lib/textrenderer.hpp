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

#ifndef __TRC_TEXTRENDERER_HPP__
#define __TRC_TEXTRENDERER_HPP__

#include "fonts.hpp"

#include <utility>

namespace trc {

enum class TextAlignment { Left, Center, Right };
enum class TextTransform { None, UpperCase, LowerCase, ProperCase, Highlight };

namespace TextRenderer {

std::pair<size_t, size_t> MeasureBounds(const Font &font,
                                        const TextTransform transform,
                                        const size_t lineMaxLength,
                                        const std::string &text);

void Render(const Font &font,
            const TextAlignment alignment,
            const TextTransform transform,
            const trc::Pixel &color,
            int X,
            int Y,
            const size_t lineMaxLength,
            const std::string &text,
            trc::Canvas &canvas);

/* Helper macros, calling Render directly all the time would get ugly. Add more
 * as necessary. */

static inline void DrawRightAlignedString(const Font &font,
                                          const trc::Pixel &color,
                                          int X,
                                          int Y,
                                          const std::string &text,
                                          trc::Canvas &canvas) {
    Render(font,
           TextAlignment::Right,
           TextTransform::None,
           color,
           X,
           Y,
           ~(size_t)0,
           text,
           canvas);
}

static inline void DrawCenteredString(const Font &font,
                                      const trc::Pixel &color,
                                      int X,
                                      int Y,
                                      const std::string &text,
                                      trc::Canvas &canvas) {
    Render(font,
           TextAlignment::Center,
           TextTransform::None,
           color,
           X,
           Y,
           ~(size_t)0,
           text,
           canvas);
}

static inline void DrawCenteredProperCaseString(const Font &font,
                                                const trc::Pixel &color,
                                                int X,
                                                int Y,
                                                const std::string &text,
                                                trc::Canvas &canvas) {
    Render(font,
           TextAlignment::Center,
           TextTransform::ProperCase,
           color,
           X,
           Y,
           ~(size_t)0,
           text,
           canvas);
}

static inline void DrawProperCaseString(const Font &font,
                                        const trc::Pixel &color,
                                        int X,
                                        int Y,
                                        const std::string &text,
                                        trc::Canvas &canvas) {
    Render(font,
           TextAlignment::Left,
           TextTransform::ProperCase,
           color,
           X,
           Y,
           ~(size_t)0,
           text,
           canvas);
}

static inline void DrawString(const Font &font,
                              const trc::Pixel &color,
                              int X,
                              int Y,
                              const std::string &text,
                              trc::Canvas &canvas) {
    Render(font,
           TextAlignment::Left,
           TextTransform::None,
           color,
           X,
           Y,
           ~(size_t)0,
           text,
           canvas);
}
} // namespace TextRenderer
} // namespace trc

#endif /* __TRC_TEXTRENDERER_HPP__ */
