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

#include "fonts.hpp"

#include "pictures.hpp"
#include "versions.hpp"

#include "utils.hpp"

namespace trc {

Font::Font(const Canvas &canvas,
           int width,
           int height,
           int spacing,
           bool bordered)
    : Bordered(bordered), Height(height) {
    for (int i = ' '; i < 256; i++) {
        auto &character = Characters[i];
        int x = ((i - ' ') % 32) * width;
        int y = ((i - ' ') / 32) * height;

        character.Sprite =
                Sprite(canvas, x, y, width, height, Sprite::Trim::Right);

        if (i != ' ') {
            character.Width = character.Sprite.Width + spacing;
        } else {
            /* Spaces lack render width/height, make sure they still look
             * okay. */
            character.Width = 2;
        }
    }
}

Fonts::Fonts(const Version &version)
    : Game(version.Pictures.Get(PictureIndex::FontGame), 16, 16, 0, true),
      InterfaceSmall(version.Pictures.Get(PictureIndex::FontInterfaceSmall),
                     8,
                     8,
                     1,
                     false),
      InterfaceLarge(version.Pictures.Get(PictureIndex::FontInterfaceLarge),
                     8,
                     16,
                     2,
                     false) {
}

} // namespace trc
