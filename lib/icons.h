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

#ifndef __TRC_ICONS_H__
#define __TRC_ICONS_H__

#include "versions_decl.h"

#include "sprites.h"

enum TrcCharacterSkull {
    CHARACTER_SKULL_NONE,
    CHARACTER_SKULL_YELLOW,
    CHARACTER_SKULL_GREEN,
    CHARACTER_SKULL_WHITE,
    CHARACTER_SKULL_RED,
    CHARACTER_SKULL_BLACK,
    CHARACTER_SKULL_ORANGE,

    CHARACTER_SKULL_FIRST = CHARACTER_SKULL_NONE,
    CHARACTER_SKULL_LAST = CHARACTER_SKULL_ORANGE,
};

enum TrcCreatureType {
    CREATURE_TYPE_PLAYER,
    CREATURE_TYPE_MONSTER,
    CREATURE_TYPE_NPC,
    CREATURE_TYPE_SUMMON_OWN,
    CREATURE_TYPE_SUMMON_OTHERS,

    CREATURE_TYPE_FIRST = CREATURE_TYPE_PLAYER,
    CREATURE_TYPE_LAST = CREATURE_TYPE_SUMMON_OTHERS,
};

enum TrcNPCCategory {
    NPC_CATEGORY_NONE = 0,
    NPC_CATEGORY_NORMAL = 1,
    NPC_CATEGORY_TRADER = 2,
    NPC_CATEGORY_QUEST = 3,
    NPC_CATEGORY_TRADER_QUEST = 4,

    NPC_CATEGORY_FIRST = NPC_CATEGORY_NONE,
    NPC_CATEGORY_LAST = NPC_CATEGORY_TRADER_QUEST,
};

enum TrcInventorySlot {
    INVENTORY_SLOT_HEAD,
    INVENTORY_SLOT_AMULET,
    INVENTORY_SLOT_BACKPACK,
    INVENTORY_SLOT_CHEST,
    INVENTORY_SLOT_RIGHTARM,
    INVENTORY_SLOT_LEFTARM,
    INVENTORY_SLOT_LEGS,
    INVENTORY_SLOT_BOOTS,
    INVENTORY_SLOT_RING,
    INVENTORY_SLOT_QUIVER,
    INVENTORY_SLOT_PURSE,

    INVENTORY_SLOT_FIRST = INVENTORY_SLOT_HEAD,
    INVENTORY_SLOT_LAST = INVENTORY_SLOT_PURSE,
};

enum TrcPartyShield {
    PARTY_SHIELD_NONE,
    PARTY_SHIELD_WHITEYELLOW,
    PARTY_SHIELD_WHITEBLUE,
    PARTY_SHIELD_BLUE,
    PARTY_SHIELD_YELLOW,
    PARTY_SHIELD_BLUE_SHAREDEXP,
    PARTY_SHIELD_YELLOW_SHAREDEXP,
    PARTY_SHIELD_BLUE_NOSHAREDEXP_BLINK,
    PARTY_SHIELD_YELLOW_NOSHAREDEXP_BLINK,
    PARTY_SHIELD_BLUE_NOSHAREDEXP,
    PARTY_SHIELD_YELLOW_NOSHAREDEXP,
    PARTY_SHIELD_GRAY,

    PARTY_SHIELD_FIRST = PARTY_SHIELD_NONE,
    PARTY_SHIELD_LAST = PARTY_SHIELD_GRAY,
};

enum TrcStatusIcon {
    STATUS_ICON_POISON = 0,
    STATUS_ICON_BURN = 1,
    STATUS_ICON_ENERGY = 2,
    STATUS_ICON_DRUNK = 3,
    STATUS_ICON_MANASHIELD = 4,
    STATUS_ICON_PARALYZE = 5,
    STATUS_ICON_HASTE = 6,
    STATUS_ICON_SWORDS = 7,
    STATUS_ICON_DROWNING = 8,
    STATUS_ICON_FREEZING = 9,
    STATUS_ICON_DAZZLED = 10,
    STATUS_ICON_CURSED = 11,
    STATUS_ICON_PARTY_BUFF = 12,
    STATUS_ICON_PZBLOCK = 13,
    STATUS_ICON_PZ = 14,
    STATUS_ICON_BLEEDING = 15,

    STATUS_ICON_FIRST = STATUS_ICON_POISON,
    STATUS_ICON_LAST = STATUS_ICON_BLEEDING,
};

enum TrcSummonIcon {
    SUMMON_ICON_NONE = 0,
    SUMMON_ICON_MINE = 1,
    SUMMON_ICON_OTHERS = 2,

    SUMMON_ICON_FIRST = SUMMON_ICON_NONE,
    SUMMON_ICON_LAST = SUMMON_ICON_OTHERS,
};

enum TrcWarIcon {
    WAR_ICON_NONE = 0,
    WAR_ICON_ALLY = 1,
    WAR_ICON_ENEMY = 2,
    WAR_ICON_NEUTRAL = 3,
    WAR_ICON_MEMBER = 4,
    WAR_ICON_OTHER = 5,

    WAR_ICON_FIRST = WAR_ICON_NONE,
    WAR_ICON_LAST = WAR_ICON_OTHER,
};

struct trc_icons {
    struct trc_sprite SecondaryStatBackground;
    struct trc_sprite InventoryBackground;
    struct trc_sprite IconBarBackground;

    struct trc_sprite ClientBackground;

    struct trc_sprite EmptyStatusBar;
    struct trc_sprite HealthBar;
    struct trc_sprite ManaBar;

    struct trc_sprite HealthIcon;
    struct trc_sprite ManaIcon;

    struct trc_sprite IconBarWar;

    struct trc_sprite RiskyIcon;

    struct trc_sprite EmptyInventorySprites[10];

    struct trc_sprite IconBarStatusSprites[16];
    struct trc_sprite IconBarSkullSprites[6];

    struct trc_sprite ShieldSprites[11];
    struct trc_sprite SkullSprites[6];
    struct trc_sprite WarSprites[5];

    struct trc_sprite SummonSprites[2];
};

struct trc_icon_table_entry {
    int SpriteOffset, X, Y, Width, Height;
};

bool icons_Load(struct trc_version *version);
void icons_Free(struct trc_version *version);

const struct trc_sprite *icons_GetShield(const struct trc_version *version,
                                         enum TrcPartyShield shield);
const struct trc_sprite *icons_GetSkull(const struct trc_version *version,
                                        enum TrcCharacterSkull skull);
const struct trc_sprite *icons_GetWar(const struct trc_version *version,
                                      enum TrcWarIcon war);
const struct trc_sprite *icons_GetCreatureTypeIcon(
        const struct trc_version *version,
        enum TrcCreatureType type);
const struct trc_sprite *icons_GetIconBarSkull(
        const struct trc_version *version,
        enum TrcCharacterSkull skull);
const struct trc_sprite *icons_GetIconBarStatus(
        const struct trc_version *version,
        enum TrcStatusIcon status);
const struct trc_sprite *icons_GetEmptyInventory(
        const struct trc_version *version,
        enum TrcInventorySlot slot);

#endif /* __TRC_ICONS_H__ */
