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

#include "icons.h"

#include "canvas.h"
#include "pictures.h"
#include "player.h"
#include "versions.h"

#include "utils.h"

const struct trc_sprite *icons_GetShield(const struct trc_version *version,
                                         enum TrcPartyShield shield) {
    const struct trc_icons *icons = &version->Icons;

    ASSERT(shield != PARTY_SHIELD_NONE &&
           CHECK_RANGE(shield, PARTY_SHIELD_FIRST, PARTY_SHIELD_LAST));

    return &icons->ShieldSprites[shield - 1];
}

const struct trc_sprite *icons_GetSkull(const struct trc_version *version,
                                        enum TrcCharacterSkull skull) {
    const struct trc_icons *icons = &version->Icons;

    ASSERT(skull != CHARACTER_SKULL_NONE &&
           CHECK_RANGE(skull, CHARACTER_SKULL_FIRST, CHARACTER_SKULL_LAST));
    return &icons->SkullSprites[skull - 1];
}

const struct trc_sprite *icons_GetWar(const struct trc_version *version,
                                      enum TrcWarIcon war) {
    const struct trc_icons *icons = &version->Icons;

    ASSERT(war != WAR_ICON_NONE &&
           CHECK_RANGE(war, WAR_ICON_FIRST, WAR_ICON_LAST));
    return &icons->WarSprites[war - 1];
}

const struct trc_sprite *icons_GetCreatureTypeIcon(
        const struct trc_version *version,
        enum TrcCreatureType type) {
    const struct trc_icons *icons = &version->Icons;

    ASSERT(CHECK_RANGE(type,
                       CREATURE_TYPE_SUMMON_OWN,
                       CREATURE_TYPE_SUMMON_OTHERS));
    return &icons->SummonSprites[type - CREATURE_TYPE_SUMMON_OWN];
}

const struct trc_sprite *icons_GetIconBarSkull(
        const struct trc_version *version,
        enum TrcCharacterSkull skull) {
    const struct trc_icons *icons = &version->Icons;

    ASSERT(skull != CHARACTER_SKULL_NONE &&
           CHECK_RANGE(skull, CHARACTER_SKULL_FIRST, CHARACTER_SKULL_LAST));
    return &icons->IconBarSkullSprites[skull - 1];
}

const struct trc_sprite *icons_GetIconBarStatus(
        const struct trc_version *version,
        enum TrcStatusIcon status) {
    const struct trc_icons *icons = &version->Icons;

    ASSERT(CHECK_RANGE(status, STATUS_ICON_FIRST, STATUS_ICON_LAST));
    return &icons->IconBarStatusSprites[status];
}

const struct trc_sprite *icons_GetEmptyInventory(
        const struct trc_version *version,
        enum TrcInventorySlot slot) {
    const struct trc_icons *icons = &version->Icons;

    ASSERT(CHECK_RANGE(slot, INVENTORY_SLOT_FIRST, INVENTORY_SLOT_LAST));
    return &icons->EmptyInventorySprites[slot];
}

bool icons_Load(struct trc_version *version) {
    struct trc_icons *icons = &version->Icons;
    struct trc_canvas *canvas;

    if (!picture_GetPictureCanvas(version, PICTURE_INDEX_ICONS, &canvas)) {
        return trc_ReportError("Could not get the icon set canvas");
    }

    const struct trc_icon_table_entry *table;
    int size;

    version_IconTable(version, &size, &table);

    for (int i = 0; i < size; i++) {
        const struct trc_icon_table_entry *entry = &table[i];
        int renderWidth, renderHeight;
        struct trc_sprite *sprite;

        /* This greatly simplifies version handling, see version_IconTable for
         * details. */
        if (((entry->X + entry->Width) > canvas->Width) ||
            ((entry->Y + entry->Height) > canvas->Height)) {
            continue;
        }

        sprite = (struct trc_sprite *)(&((char *)icons)[entry->SpriteOffset]);

        canvas_ExtractSprite(canvas,
                             entry->X,
                             entry->Y,
                             entry->Width,
                             entry->Height,
                             &renderWidth,
                             &renderHeight,
                             sprite);

        (void)renderWidth;
        (void)renderHeight;
    }

    return true;
}

void icons_Free(struct trc_version *version) {
    struct trc_icons *icons = &version->Icons;
    const struct trc_icon_table_entry *table;
    int size;

    version_IconTable(version, &size, &table);

    for (int i = 0; i < size; i++) {
        struct trc_sprite *sprite =
                (struct trc_sprite *)(&((char *)icons)[table[i].SpriteOffset]);

        ABORT_UNLESS(((sprite->Width > 0 && sprite->Height > 0)) ==
                     (sprite->Buffer != NULL));
        checked_deallocate((void *)sprite->Buffer);
    }
}
