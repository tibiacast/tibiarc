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

#include "fonts.h"

#include "characterset.h"
#include "pictures.h"
#include "versions.h"

#include "utils.h"

static bool fonts_LoadFont(const struct trc_version *version,
                           enum TrcPictureIndex fontIndex,
                           int rectWidth,
                           int rectHeight,
                           int widthAdjust,
                           struct trc_font *font) {
    struct trc_canvas *fontCanvas;

    if (!picture_GetPictureCanvas(version, fontIndex, &fontCanvas)) {
        return trc_ReportError("Could not get the font canvas");
    }

    font->Height = rectHeight;

    for (int character = ' '; character < 256; character++) {
        int baseXCoordinate, baseYCoordinate, renderWidth, renderHeight;
        struct trc_sprite *characterSprite;

        baseXCoordinate = ((character - ' ') % 32) * rectWidth;
        baseYCoordinate = ((character - ' ') / 32) * rectHeight;

        characterSprite = &font->Characters[character].Sprite;

        renderWidth = 0;

        canvas_ExtractSprite(fontCanvas,
                             baseXCoordinate,
                             baseYCoordinate,
                             rectWidth,
                             rectHeight,
                             &renderWidth,
                             &renderHeight,
                             characterSprite);

        if (character != ' ') {
            font->Characters[character].Width = renderWidth + widthAdjust;
        } else {
            /* Spaces lack render width/height, make sure they still look
             * okay. */
            font->Characters[character].Width = 2;
        }
    }

    return true;
}

bool fonts_Load(struct trc_version *version) {
    struct trc_fonts *fonts = &version->Fonts;

    if (!fonts_LoadFont(version,
                        PICTURE_INDEX_FONT_GAME,
                        16,
                        16,
                        0,
                        &fonts->GameFont)) {
        return trc_ReportError("Failed to load game font");
    }

    fonts->GameFont.IsBordered = true;

    if (!fonts_LoadFont(version,
                        PICTURE_INDEX_FONT_INTERFACE_SMALL,
                        8,
                        8,
                        1,
                        &fonts->InterfaceFontSmall)) {
        return trc_ReportError("Failed to load small client font");
    }

    fonts->InterfaceFontSmall.IsBordered = 0;

    if (!fonts_LoadFont(version,
                        PICTURE_INDEX_FONT_INTERFACE_LARGE,
                        8,
                        16,
                        2,
                        &fonts->InterfaceFontLarge)) {
        return trc_ReportError("Failed to load large client font");
    }

    fonts->InterfaceFontLarge.IsBordered = 0;

    return true;
}

static void fonts_FreeFont(struct trc_font *font) {
    for (int character = ' '; character < 256; character++) {
        struct trc_sprite *sprite = &font->Characters[character].Sprite;
        checked_deallocate((void *)sprite->Buffer);
    }
}

void fonts_Free(struct trc_version *version) {
    struct trc_fonts *fonts = &version->Fonts;

    fonts_FreeFont(&fonts->GameFont);
    fonts_FreeFont(&fonts->InterfaceFontSmall);
    fonts_FreeFont(&fonts->InterfaceFontLarge);
}
