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

/* FIXME: This is a pretty horrendous translation layer. While the protocol
 * stuff is mostly fine, we need a better way to deal with type properties,
 * message types, and so on.
 *
 * What we have right now "works" insofar as parsing goes, but can be subtly
 * incorrect and misinterpret e.g. login messages as look messages. Once we've
 * got a reasonably sized corpus of recordings, it would be nice to start over
 * from scratch and methodically map out all the constants used from 7.0
 * onwards. */

#include "versions.h"
#include "utils.h"

#include <stddef.h>
#include <stdlib.h>

static bool version_LoadData(struct trc_data_reader *pictureData,
                             struct trc_data_reader *spriteData,
                             struct trc_data_reader *typeData,
                             struct trc_version *version) {
    if (!pictures_Load(version, pictureData)) {
        return trc_ReportError("Could not load pictures");
    }

    if (!sprites_Load(version, spriteData)) {
        return trc_ReportError("Could not load sprites");
    }

    if (!types_Load(version, typeData)) {
        return trc_ReportError("Could not load types");
    }

    if (!fonts_Load(version)) {
        return trc_ReportError("Could not initialize fonts");
    }

    if (!icons_Load(version)) {
        return trc_ReportError("Could not initialize icons");
    }

    return true;
}

static void version_InitFeatures(struct trc_version *version) {
    if (VERSION_AT_LEAST(version, 7, 50)) {
        version->Features.IconBar = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 55)) {
        version->Features.TypeZDiv = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 53)) {
        version->Features.ModernStacking = 1;
    }

    if (VERSION_AT_LEAST(version, 9, 6)) {
        version->Features.SpriteIndexU32 = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 50)) {
        version->Features.AnimationPhases = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 57)) {
        version->Features.FrameGroups = 1;
    }
}

static void version_InitProtocol(struct trc_version *version) {
    if (VERSION_AT_LEAST(version, 7, 20)) {
        version->Protocol.BugReporting = 1;
        version->Protocol.SkullIcon = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 24)) {
        version->Protocol.ShieldIcon = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 40)) {
        version->Protocol.MoveDeniedDirection = 1;
        version->Protocol.SkillPercentages = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 50)) {
        version->Protocol.SoulPoints = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 55)) {
        version->Protocol.RawEffectIds = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 60)) {
        version->Protocol.TextEditAuthorName = 1;
        version->Protocol.LevelU16 = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 70)) {
        version->Protocol.ReportMessages = 1;
        version->Protocol.OutfitsU16 = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 80)) {
        version->Protocol.RuneChargeCount = 1;
        version->Protocol.OutfitAddons = 1;
        version->Protocol.Stamina = 1;
        version->Protocol.SpeakerLevel = 1;
        version->Protocol.IconsU16 = 1;
    }

    if (VERSION_AT_LEAST(version, 7, 90)) {
        version->Protocol.TextEditDate = 1;
        version->Protocol.OutfitNames = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 30)) {
        version->Protocol.NPCVendorWeight = 1;
        version->Protocol.CapacityU32 = 1;

        /* ??? */
        version->Protocol.TextEditObject = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 41)) {
        version->Protocol.AddObjectStackPosition = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 53)) {
        version->Protocol.PassableCreatures = 1;
        version->Protocol.WarIcon = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 70)) {
        version->Protocol.Mounts = 1;
    }

    /* HAZY: Catch-all for properties of unknown versions to get 8.55 rolling.
     *
     * These may belong to any version between 8.55 and 9.32. */
    if (VERSION_AT_LEAST(version, 9, 0)) {
        version->Protocol.CancelAttackId = 1;
        version->Protocol.HazyNewTileStuff = 1;
        version->Protocol.MaxCapacity = 1;
        version->Protocol.ExperienceU64 = 1;
        version->Protocol.PlayerSpeed = 1;
        version->Protocol.PlayerHunger = 1;
        version->Protocol.ItemAnimation = 1;
        version->Protocol.NPCVendorName = 1;
        version->Protocol.MessageEffects = 1;
        version->Protocol.ChannelParticipants = 1;

        /* ?? */
        version->Protocol.OutfitCountU16 = 1;
    }

    if (VERSION_AT_LEAST(version, 9, 32)) {
        version->Protocol.NPCVendorItemCountU16 = 1;
    }

    if (VERSION_AT_LEAST(version, 9, 54)) {
        version->Protocol.OfflineStamina = 1;
        version->Protocol.PassableCreatureUpdate = 1;
    }

    if (VERSION_AT_LEAST(version, 9, 72)) {
        version->Protocol.PlayerMoneyU64 = 1;
        version->Protocol.ExtendedDeathDialog = 1;
    }

    if (VERSION_AT_LEAST(version, 9, 83)) {
        version->Protocol.ContainerIndexU16 = 1;
    }

    if ((version->Major == 9 && version->Minor == 83) && version->Preview) {
        version->Protocol.PreviewByte = 1;
    }

    if (VERSION_AT_LEAST(version, 9, 84)) {
        version->Protocol.PreviewByte = 1;
        version->Protocol.ContainerPagination = 1;
    }

    if (VERSION_AT_LEAST(version, 9, 86) ||
        (version->Major == 9 && version->Minor == 85 && version->Preview)) {
        version->Protocol.CreatureMarks = 1;
        version->Protocol.ItemMarks = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 36)) {
        version->Protocol.NPCCategory = 1;
        version->Protocol.SinglePVPHelper = 1;
        version->Protocol.LoyaltyBonus = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 37)) {
        version->Protocol.PremiumUntil = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 53) ||
        (version->Major == 10 && version->Minor == 52 && version->Preview)) {
        version->Protocol.PVPFraming = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 54) ||
        (version->Major == 10 && version->Minor == 53 && version->Preview)) {
        version->Protocol.ExperienceBonus = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 55)) {
        version->Protocol.UnfairFightReduction = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 58)) {
        version->Protocol.ExpertMode = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 59)) {
        version->Protocol.CreatureSpeedPadding = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 65)) {
        version->Protocol.GuildChannelId = 1;
        version->Protocol.PartyChannelId = 1;
    }

    if (VERSION_AT_LEAST(version, 10, 95)) {
        version->Protocol.SkillsUnknownPadding = 1;
    }
}

#ifdef DUMP_ITEMS
#    include "renderer.h"
#    include "textrenderer.h"

#    define DUMP_PROPERTY(Name)                                                \
        if (type->Properties.Name) {                                           \
            textrenderer_DrawString(&version->Fonts.InterfaceFontSmall,        \
                                    &fontColor,                                \
                                    0 + (emitted % 2) * 64,                    \
                                    66 + (emitted / 2) * 14,                   \
                                    CONSTSTRLEN(#Name),                        \
                                    #Name,                                     \
                                    canvas);                                   \
            emitted++;                                                         \
        }

static void version_DumpItems(struct trc_version *version) {
    const struct trc_pixel fontColor = {.Red = 0xFF,
                                        .Green = 0xFF,
                                        .Blue = 0xFF};
    const struct trc_type_file *types = &version->Types;
    struct trc_canvas *canvas = canvas_Create(128, 128);

    for (int i = types->Items.MinId; i < types->Items.MaxId; i++) {
        struct trc_entitytype *type;
        char path[512];
        int emitted = 0;

        canvas_Wipe(canvas);

        renderer_DumpItem(version, i, canvas);
        types_GetItem(version, i, &type);

        DUMP_PROPERTY(Animated);
        DUMP_PROPERTY(AnimateIdle);
        DUMP_PROPERTY(Blocking);
        DUMP_PROPERTY(Container);
        DUMP_PROPERTY(DontHide);
        DUMP_PROPERTY(EquipmentSlotRestricted);
        DUMP_PROPERTY(ForceUse);
        DUMP_PROPERTY(Hangable);
        DUMP_PROPERTY(Horizontal);
        DUMP_PROPERTY(LiquidContainer);
        DUMP_PROPERTY(LiquidPool);
        DUMP_PROPERTY(LookThrough);
        DUMP_PROPERTY(MarketItem);
        DUMP_PROPERTY(MultiUse);
        DUMP_PROPERTY(NoMoveAnimation);
        DUMP_PROPERTY(RedrawNearbyTop);
        DUMP_PROPERTY(Rotate);
        DUMP_PROPERTY(Rune);
        DUMP_PROPERTY(Stackable);
        DUMP_PROPERTY(Takeable);
        DUMP_PROPERTY(TopEffect);
        DUMP_PROPERTY(Translucent);
        DUMP_PROPERTY(Unlookable);
        DUMP_PROPERTY(Unmovable);
        DUMP_PROPERTY(Unpathable);
        DUMP_PROPERTY(Unwrappable);
        DUMP_PROPERTY(Usable);
        DUMP_PROPERTY(Vertical);
        DUMP_PROPERTY(Walkable);
        DUMP_PROPERTY(Wrappable);
        DUMP_PROPERTY(Write);
        DUMP_PROPERTY(WriteOnce);

        sprintf(path, "items/%i.bmp", i);
        canvas_Dump(path, canvas);
    }

    canvas_Free(canvas);
}
#endif

bool version_Load(int major,
                  int minor,
                  int preview,
                  struct trc_data_reader *pictureData,
                  struct trc_data_reader *spriteData,
                  struct trc_data_reader *typeData,
                  struct trc_version **out) {
    struct trc_version *version =
            (struct trc_version *)checked_allocate(1,
                                                   sizeof(struct trc_version));

    version->Major = major;
    version->Minor = minor;
    version->Preview = preview;

    version_InitFeatures(version);
    version_InitProtocol(version);

    if (version_LoadData(pictureData, spriteData, typeData, version)) {
#ifdef DUMP_ITEMS
        version_DumpItems(version);
#endif

        (*out) = version;
        return true;
    }

    version_Free(version);
    return false;
}

void version_Free(struct trc_version *version) {
    icons_Free(version);
    fonts_Free(version);
    types_Free(version);
    pictures_Free(version);
    sprites_Free(version);

    checked_deallocate(version);
}

#define SO(Sprite) offsetof(struct trc_icons, Sprite)

void version_IconTable(const struct trc_version *version,
                       int *size,
                       const struct trc_icon_table_entry **table) {
    /* As icons never move between versions (they are only added, removed, or
     * replaced), we can simplify things by including the coordinates of all
     * known icons in a single table.
     *
     * We can safely ignore icons that are out of bounds or have changed
     * appearance as they won't appear in the recording we're converting. */
    static const struct trc_icon_table_entry until_1095[] = {
            {SO(RiskyIcon), 230, 218, 11, 11},
            {SO(IconBarWar), 251, 218, 9, 9},
            {SO(IconBarBackground), 98, 240, 108, 13},
            {SO(SecondaryStatBackground), 315, 32, 34, 21},
            {SO(InventoryBackground), 186, 64, 34, 34},
            {SO(ClientBackground), 0, 0, 96, 96},
            {SO(EmptyStatusBar), 96, 64, 90, 11},
            {SO(HealthBar), 96, 75, 90, 11},
            {SO(ManaBar), 96, 86, 90, 11},
            {SO(HealthIcon), 220, 76, 11, 11},
            {SO(ManaIcon), 220, 87, 11, 11},
            {SO(ShieldSprites[PARTY_SHIELD_YELLOW - 1]), 54, 236, 11, 11},
            {SO(ShieldSprites[PARTY_SHIELD_BLUE - 1]), 65, 236, 11, 11},
            {SO(ShieldSprites[PARTY_SHIELD_WHITEYELLOW - 1]), 76, 236, 11, 11},
            {SO(ShieldSprites[PARTY_SHIELD_WHITEBLUE - 1]), 87, 236, 11, 11},
            {SO(ShieldSprites[PARTY_SHIELD_YELLOW_SHAREDEXP - 1]),
             76,
             214,
             11,
             11},
            {SO(ShieldSprites[PARTY_SHIELD_BLUE_SHAREDEXP - 1]),
             87,
             214,
             11,
             11},
            {SO(ShieldSprites[PARTY_SHIELD_YELLOW_NOSHAREDEXP_BLINK - 1]),
             168,
             261,
             11,
             11},
            {SO(ShieldSprites[PARTY_SHIELD_YELLOW_NOSHAREDEXP - 1]),
             168,
             261,
             11,
             11},
            {SO(ShieldSprites[PARTY_SHIELD_BLUE_NOSHAREDEXP_BLINK - 1]),
             179,
             261,
             11,
             11},
            {SO(ShieldSprites[PARTY_SHIELD_BLUE_NOSHAREDEXP - 1]),
             179,
             261,
             11,
             11},
            {SO(ShieldSprites[PARTY_SHIELD_GRAY - 1]), 43, 236, 11, 11},
            {SO(SkullSprites[CHARACTER_SKULL_GREEN - 1]), 54, 225, 11, 11},
            {SO(SkullSprites[CHARACTER_SKULL_YELLOW - 1]), 65, 225, 11, 11},
            {SO(SkullSprites[CHARACTER_SKULL_WHITE - 1]), 76, 225, 11, 11},
            {SO(SkullSprites[CHARACTER_SKULL_RED - 1]), 87, 225, 11, 11},
            {SO(SkullSprites[CHARACTER_SKULL_BLACK - 1]), 98, 207, 11, 11},
            {SO(SkullSprites[CHARACTER_SKULL_ORANGE - 1]), 208, 218, 11, 11},
            {SO(WarSprites[WAR_ICON_ALLY - 1]), 287, 218, 11, 11},
            {SO(WarSprites[WAR_ICON_ENEMY - 1]), 298, 218, 11, 11},
            {SO(WarSprites[WAR_ICON_NEUTRAL - 1]), 309, 218, 11, 11},
            {SO(WarSprites[WAR_ICON_MEMBER - 1]), 219, 218, 11, 11},
            {SO(WarSprites[WAR_ICON_OTHER - 1]), 276, 218, 11, 11},
            {SO(SummonSprites[SUMMON_ICON_MINE - 1]), 220, 229, 11, 11},
            {SO(SummonSprites[SUMMON_ICON_OTHERS - 1]), 220, 240, 11, 11},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_AMULET]), 96, 0, 32, 32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_HEAD]), 128, 0, 32, 32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_BACKPACK]),
             160,
             0,
             32,
             32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_LEFTARM]), 192, 0, 32, 32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_RIGHTARM]),
             224,
             0,
             32,
             32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_CHEST]), 96, 32, 32, 32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_LEGS]), 128, 32, 32, 32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_RING]), 160, 32, 32, 32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_QUIVER]), 192, 32, 32, 32},
            {SO(EmptyInventorySprites[INVENTORY_SLOT_BOOTS]), 224, 32, 32, 32},
            {SO(IconBarStatusSprites[STATUS_ICON_POISON]), 279, 32, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_BURN]), 288, 32, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_ENERGY]), 297, 32, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_SWORDS]), 306, 32, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_DRUNK]), 279, 32, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_MANASHIELD]), 288, 41, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_HASTE]), 297, 41, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_PARALYZE]), 306, 41, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_DROWNING]), 279, 59, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_FREEZING]), 279, 68, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_DAZZLED]), 279, 77, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_CURSED]), 279, 86, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_PARTY_BUFF]), 307, 148, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_PZBLOCK]), 310, 191, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_PZ]), 310, 182, 9, 9},
            {SO(IconBarStatusSprites[STATUS_ICON_BLEEDING]), 322, 0, 9, 9},
            {SO(IconBarSkullSprites[CHARACTER_SKULL_GREEN - 1]), 279, 50, 9, 9},
            {SO(IconBarSkullSprites[CHARACTER_SKULL_YELLOW - 1]),
             288,
             50,
             9,
             9},
            {SO(IconBarSkullSprites[CHARACTER_SKULL_WHITE - 1]), 297, 50, 9, 9},
            {SO(IconBarSkullSprites[CHARACTER_SKULL_RED - 1]), 306, 50, 9, 9},
            {SO(IconBarSkullSprites[CHARACTER_SKULL_BLACK - 1]),
             342,
             200,
             9,
             9},
            {SO(IconBarSkullSprites[CHARACTER_SKULL_ORANGE - 1]),
             242,
             218,
             9,
             9}};

    *size = sizeof(until_1095) / sizeof(until_1095[0]);
    *table = until_1095;
}
#undef SO

bool version_TranslateTypeProperty(const struct trc_version *version,
                                   uint8_t index,
                                   enum TrcTypeProperty *out) {
    /* This covers a vast span of versions before 10.95, but I'm not entirely
     * sure about the lower bound. The only thing I know for certain is that
     * 8.55 crashes with this. */
    static const char until_1095[] = {/* 0 */ TYPEPROPERTY_GROUND,
                                      /* 1 */ TYPEPROPERTY_CLIP,
                                      /* 2 */ TYPEPROPERTY_BOTTOM,
                                      /* 3 */ TYPEPROPERTY_TOP,
                                      /* 4 */ TYPEPROPERTY_CONTAINER,
                                      /* 5 */ TYPEPROPERTY_STACKABLE,
                                      /* 6 */ TYPEPROPERTY_FORCE_USE,
                                      /* 7 */ TYPEPROPERTY_MULTI_USE,
                                      /* 8 */ TYPEPROPERTY_WRITE,
                                      /* 9 */ TYPEPROPERTY_WRITE_ONCE,
                                      /* 10 */ TYPEPROPERTY_LIQUID_CONTAINER,
                                      /* 11 */ TYPEPROPERTY_LIQUID_POOL,
                                      /* 12 */ TYPEPROPERTY_BLOCKING,
                                      /* 13 */ TYPEPROPERTY_UNMOVABLE,
                                      /* 14 */ TYPEPROPERTY_UNLOOKABLE,
                                      /* 15 */ TYPEPROPERTY_UNPATHABLE,
                                      /* 16 */ TYPEPROPERTY_NO_MOVE_ANIMATION,
                                      /* 17 */ TYPEPROPERTY_TAKEABLE,
                                      /* 18 */ TYPEPROPERTY_HANGABLE,
                                      /* 19 */ TYPEPROPERTY_VERTICAL,
                                      /* 20 */ TYPEPROPERTY_HORIZONTAL,
                                      /* 21 */ TYPEPROPERTY_ROTATE,
                                      /* 22 */ TYPEPROPERTY_LIGHT,
                                      /* 23 */ TYPEPROPERTY_DONT_HIDE,
                                      /* 24 */ TYPEPROPERTY_TRANSLUCENT,
                                      /* 25 */ TYPEPROPERTY_DISPLACEMENT,
                                      /* 26 */ TYPEPROPERTY_HEIGHT,
                                      /* 27 */ TYPEPROPERTY_REDRAW_NEARBY_TOP,
                                      /* 28 */ TYPEPROPERTY_ANIMATE_IDLE,
                                      /* 29 */ TYPEPROPERTY_AUTOMAP,
                                      /* 30 */ TYPEPROPERTY_LENSHELP,
                                      /* 31 */ TYPEPROPERTY_WALKABLE,
                                      /* 32 */ TYPEPROPERTY_LOOK_THROUGH,
                                      /* 33 */ TYPEPROPERTY_EQUIPMENT_SLOT,
                                      /* 34 */ TYPEPROPERTY_MARKET_ITEM,
                                      /* 35 */ TYPEPROPERTY_DEFAULT_ACTION,
                                      /* 36 */ TYPEPROPERTY_WRAPPABLE,
                                      /* 37 */ TYPEPROPERTY_UNWRAPPABLE,
                                      /* 38 */ TYPEPROPERTY_TOP_EFFECT};

    /* 7.80 - 8.55, and perhaps a few after that as well. */
    static const char until_855[] = {
            /* 0 */ TYPEPROPERTY_GROUND,
            /* 1 */ TYPEPROPERTY_CLIP,
            /* 2 */ TYPEPROPERTY_BOTTOM,
            /* 3 */ TYPEPROPERTY_TOP,
            /* 4 */ TYPEPROPERTY_CONTAINER,
            /* 5 */ TYPEPROPERTY_STACKABLE,
            /* 6 */ TYPEPROPERTY_FORCE_USE,
            /* 7 */ TYPEPROPERTY_USABLE,
            /* 8 */ TYPEPROPERTY_RUNE,
            /* 9 */ TYPEPROPERTY_WRITE,
            /* 10 */ TYPEPROPERTY_WRITE_ONCE,
            /* 11 */ TYPEPROPERTY_LIQUID_CONTAINER,
            /* 12 */ TYPEPROPERTY_LIQUID_POOL,
            /* 13 */ TYPEPROPERTY_BLOCKING,
            /* 14 */ TYPEPROPERTY_UNMOVABLE,
            /* 15 */ TYPEPROPERTY_BLOCKING,   /* Unsure */
            /* 16 */ TYPEPROPERTY_UNPATHABLE, /* Unsure */
            /* 17 */ TYPEPROPERTY_TAKEABLE,
            /* 18 */ TYPEPROPERTY_HANGABLE,
            /* 19 */ TYPEPROPERTY_VERTICAL,
            /* 20 */ TYPEPROPERTY_HORIZONTAL,
            /* 21 */ TYPEPROPERTY_ROTATE,
            /* 22 */ TYPEPROPERTY_LIGHT,
            /* 23 */ TYPEPROPERTY_DONT_HIDE,   /* Unsure */
            /* 24 */ TYPEPROPERTY_TRANSLUCENT, /* Unsure */
            /* 25 */ TYPEPROPERTY_DISPLACEMENT,
            /* 26 */ TYPEPROPERTY_HEIGHT,
            /* 27 */ TYPEPROPERTY_REDRAW_NEARBY_TOP,
            /* 28 */ TYPEPROPERTY_ANIMATE_IDLE,
            /* 29 */ TYPEPROPERTY_AUTOMAP,
            /* 30 */ TYPEPROPERTY_LENSHELP,
            /* 31 */ TYPEPROPERTY_WALKABLE,
            /* 32 */ TYPEPROPERTY_LOOK_THROUGH};

    /* 7.55 - 7.72 */
    static const char until_772[] = {
            /* 0 */ TYPEPROPERTY_GROUND,
            /* 1 */ TYPEPROPERTY_CLIP,
            /* 2 */ TYPEPROPERTY_BOTTOM,
            /* 3 */ TYPEPROPERTY_TOP,
            /* 4 */ TYPEPROPERTY_CONTAINER,
            /* 5 */ TYPEPROPERTY_STACKABLE,
            /* 6 */ TYPEPROPERTY_FORCE_USE,
            /* 7 */ TYPEPROPERTY_USABLE,
            /* 8 */ TYPEPROPERTY_WRITE,
            /* 9 */ TYPEPROPERTY_WRITE_ONCE,
            /* 10 */ TYPEPROPERTY_LIQUID_CONTAINER,
            /* 11 */ TYPEPROPERTY_LIQUID_POOL,
            /* 12 */ TYPEPROPERTY_BLOCKING,
            /* 13 */ TYPEPROPERTY_UNMOVABLE,
            /* 14 */ TYPEPROPERTY_BLOCKING,   /* Unsure */
            /* 15 */ TYPEPROPERTY_UNPATHABLE, /* Unsure */
            /* 16 */ TYPEPROPERTY_TAKEABLE,
            /* 17 */ TYPEPROPERTY_HANGABLE,
            /* 18 */ TYPEPROPERTY_VERTICAL,
            /* 19 */ TYPEPROPERTY_HORIZONTAL,
            /* 20 */ TYPEPROPERTY_ROTATE,
            /* 21 */ TYPEPROPERTY_LIGHT,
            /* 22 */ -1,
            /* 23 */ TYPEPROPERTY_DONT_HIDE, /* Unsure */
            /* 24 */ TYPEPROPERTY_DISPLACEMENT,
            /* 25 */ TYPEPROPERTY_HEIGHT,
            /* 26 */ TYPEPROPERTY_REDRAW_NEARBY_TOP,
            /* 27 */ TYPEPROPERTY_INERT,
            /* 28 */ TYPEPROPERTY_AUTOMAP,
            /* 29 */ TYPEPROPERTY_LENSHELP,
            /* 30 */ TYPEPROPERTY_WALKABLE};

    /* 7.35 - 7.50 */
    static const char until_750[] = {
            /* 0 */ TYPEPROPERTY_GROUND,
            /* 1 */ TYPEPROPERTY_CLIP,
            /* 2 */ TYPEPROPERTY_BOTTOM,
            /* 3 */ TYPEPROPERTY_CONTAINER,
            /* 4 */ TYPEPROPERTY_STACKABLE,
            /* 5 */ TYPEPROPERTY_USABLE,
            /* 6 */ TYPEPROPERTY_FORCE_USE,
            /* 7 */ TYPEPROPERTY_WRITE,
            /* 8 */ TYPEPROPERTY_WRITE_ONCE,
            /* 9 */ TYPEPROPERTY_LIQUID_CONTAINER,
            /* 10 */ TYPEPROPERTY_LIQUID_POOL,
            /* 11 */ TYPEPROPERTY_BLOCKING,
            /* 12 */ TYPEPROPERTY_UNMOVABLE,
            /* 13 */ TYPEPROPERTY_BLOCKING,   /* Unsure */
            /* 14 */ TYPEPROPERTY_UNPATHABLE, /* Unsure */
            /* 15 */ TYPEPROPERTY_TAKEABLE,
            /* 16 */ TYPEPROPERTY_LIGHT,
            /* 17 */ TYPEPROPERTY_DONT_HIDE,
            /* 18 */ TYPEPROPERTY_BLOCKING, /* Unsure */
            /* 19 */ TYPEPROPERTY_HEIGHT,
            /* 20 */ TYPEPROPERTY_DISPLACEMENT_LEGACY,
            /* 21 */ -1,
            /* 22 */ TYPEPROPERTY_AUTOMAP,
            /* 23 */ TYPEPROPERTY_ROTATE,
            /* 24 */ TYPEPROPERTY_INERT,
            /* 25 */ TYPEPROPERTY_HANGABLE,
            /* 26 */ TYPEPROPERTY_VERTICAL,
            /* 27 */ TYPEPROPERTY_HORIZONTAL,
            /* 28 */ TYPEPROPERTY_ANIMATE_IDLE,
            /* 29 */ TYPEPROPERTY_LENSHELP};

    /* 7.00 - 7.30 */
    static const char until_730[] = {
            /* 0 */ TYPEPROPERTY_GROUND,
            /* 1 */ TYPEPROPERTY_CLIP,
            /* 2 */ TYPEPROPERTY_BOTTOM,
            /* 3 */ TYPEPROPERTY_CONTAINER,
            /* 4 */ TYPEPROPERTY_STACKABLE,
            /* 5 */ TYPEPROPERTY_USABLE,
            /* 6 */ TYPEPROPERTY_FORCE_USE,
            /* 7 */ TYPEPROPERTY_WRITE,
            /* 8 */ TYPEPROPERTY_WRITE_ONCE,
            /* 9 */ TYPEPROPERTY_LIQUID_CONTAINER,
            /* 10 */ TYPEPROPERTY_LIQUID_POOL,
            /* 11 */ TYPEPROPERTY_BLOCKING,
            /* 12 */ TYPEPROPERTY_UNMOVABLE,
            /* 13 */ TYPEPROPERTY_BLOCKING,   /* Unsure */
            /* 14 */ TYPEPROPERTY_UNPATHABLE, /* Unsure */
            /* 15 */ TYPEPROPERTY_TAKEABLE,
            /* 16 */ TYPEPROPERTY_LIGHT,
            /* 17 */ TYPEPROPERTY_DONT_HIDE,
            /* 18 */ TYPEPROPERTY_BLOCKING, /* Unsure */
            /* 19 */ TYPEPROPERTY_HEIGHT,
            /* 20 */ TYPEPROPERTY_DISPLACEMENT_LEGACY,
            /* 21 */ -1,
            /* 22 */ TYPEPROPERTY_AUTOMAP,
            /* 23 */ TYPEPROPERTY_ROTATE,
            /* 24 */ TYPEPROPERTY_INERT,
            /* 25 */ TYPEPROPERTY_HANGABLE,
            /* 26 */ TYPEPROPERTY_LENSHELP, /* Wild guess */
            /* 27 */ TYPEPROPERTY_HORIZONTAL,
            /* 28 */ TYPEPROPERTY_ANIMATE_IDLE,
            /* 29 */ TYPEPROPERTY_LENSHELP};

    if (index == 255) {
        *out = TYPEPROPERTY_ENTRY_END_MARKER;
        return true;
    }

    int translated;

    if (version->Major > 8) {
        if (index == 254) {
            *out = TYPEPROPERTY_USABLE;
            return true;
        }

        if (index > sizeof(until_1095)) {
            return false;
        }

        translated = until_1095[index];
    } else if (VERSION_AT_LEAST(version, 7, 80)) {
        if (index > sizeof(until_855)) {
            return false;
        }

        translated = until_855[index];
    } else if (VERSION_AT_LEAST(version, 7, 55)) {
        if (index > sizeof(until_772)) {
            return false;
        }

        translated = until_772[index];
    } else if (VERSION_AT_LEAST(version, 7, 40)) {
        if (index > sizeof(until_750)) {
            return false;
        }

        translated = until_750[index];
    } else {
        if (index > sizeof(until_730)) {
            return false;
        }

        translated = until_730[index];
    }

    if (translated == -1) {
        return false;
    }

    *out = (enum TrcTypeProperty)translated;
    return true;
}

bool version_TranslateSpeakMode(const struct trc_version *version,
                                uint8_t index,
                                enum TrcMessageMode *out) {
    /* HAZY: I'm not entirely sure when these were merged, but it was probably
     * after 9.X */
    if (VERSION_AT_LEAST(version, 9, 0)) {
        return version_TranslateMessageMode(version, index, out);
    }

    static const char until_855[] = {/* 0 */ -1,
                                     /* 1 */ MESSAGEMODE_SAY,
                                     /* 2 */ MESSAGEMODE_WHISPER,
                                     /* 3 */ MESSAGEMODE_YELL,
                                     /* 4 */ MESSAGEMODE_PLAYER_TO_NPC,
                                     /* 5 */ MESSAGEMODE_NPC_START,
                                     /* 6 */ MESSAGEMODE_PRIVATE_IN,
                                     /* 7 */ MESSAGEMODE_CHANNEL_YELLOW,
                                     /* 8 */ MESSAGEMODE_CHANNEL_WHITE,
                                     /* 9 */ MESSAGEMODE_GM_TO_PLAYER,
                                     /* 10 */ MESSAGEMODE_PLAYER_TO_GM,
                                     /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
                                     /* 12 */ MESSAGEMODE_BROADCAST,
                                     /* 13 */ MESSAGEMODE_CHANNEL_RED,
                                     /* 14 */ MESSAGEMODE_CHANNEL_ORANGE,
                                     /* 15 */ MESSAGEMODE_CHANNEL_ORANGE,
                                     /* 16 */ -1,
                                     /* 17 */ MESSAGEMODE_CHANNEL_RED,
                                     /* 18 */ -1,
                                     /* 19 */ MESSAGEMODE_MONSTER_SAY,
                                     /* 20 */ MESSAGEMODE_MONSTER_YELL};

    static const char until_811[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ MESSAGEMODE_MONSTER_SAY,
            /* 17 */ MESSAGEMODE_MONSTER_YELL,
            /* 18 */ MESSAGEMODE_WARNING, /* "Warning! The ... " */
            /* 19 */ MESSAGEMODE_GAME,    /* "You advanced in ..." */
            /* 20 */ MESSAGEMODE_LOGIN,   /* "Your last visit in Tibia" */
            /* 21 */ MESSAGEMODE_WARNING,
            /* 22 */ -1,
            /* 23 */ MESSAGEMODE_STATUS,
            /* 24 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 25 */ MESSAGEMODE_LOOK,
            /* 26 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 27 */ MESSAGEMODE_STATUS};

    /* Super buggy, */
    static const char until_760[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_PLAYER_TO_GM, /* FIXME? */
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ MESSAGEMODE_MONSTER_SAY,
            /* 17 */ MESSAGEMODE_MONSTER_YELL,
            /* 18 */ MESSAGEMODE_WARNING, /* "Warning! The ... " */
            /* 19 */ MESSAGEMODE_GAME,    /* "You advanced in ..." */
            /* 20 */ MESSAGEMODE_LOGIN,   /* "Your last visit in Tibia" */
            /* 21 */ MESSAGEMODE_WARNING,
            /* 22 */ -1,
            /* 23 */ MESSAGEMODE_STATUS,
            /* 24 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 25 */ MESSAGEMODE_LOOK,
            /* 26 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 27 */ MESSAGEMODE_STATUS};

    static const char until_727[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ MESSAGEMODE_MONSTER_SAY,
            /* 17 */ MESSAGEMODE_MONSTER_YELL,
            /* 18 */ MESSAGEMODE_GAME,  /* "You advanced in ..." */
            /* 19 */ MESSAGEMODE_LOGIN, /* "Your last visit in Tibia" */
            /* 20 */ MESSAGEMODE_WARNING,
            /* 21 */ -1,
            /* 22 */ MESSAGEMODE_STATUS,
            /* 23 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 24 */ MESSAGEMODE_LOOK,
            /* 25 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 26 */ MESSAGEMODE_STATUS};

    static const char until_721[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ MESSAGEMODE_MONSTER_SAY,
            /* 16 */ MESSAGEMODE_MONSTER_YELL,
            /* 17 */ -1,
            /* 18 */ MESSAGEMODE_GAME,  /* "You advanced in ..." */
            /* 19 */ MESSAGEMODE_LOGIN, /* "Your last visit in Tibia" */
            /* 20 */ MESSAGEMODE_WARNING,
            /* 21 */ -1,
            /* 22 */ MESSAGEMODE_STATUS,
            /* 23 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 24 */ MESSAGEMODE_LOOK,
            /* 25 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 26 */ MESSAGEMODE_STATUS};

    static const char until_711[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ MESSAGEMODE_MONSTER_SAY,
            /* 14 */ MESSAGEMODE_MONSTER_YELL,
            /* 15 */ MESSAGEMODE_WARNING, /* "Warning! The ... " */
            /* 16 */ MESSAGEMODE_GAME,    /* "You advanced in ..." */
            /* 17 */ MESSAGEMODE_LOGIN,   /* "Your last visit in Tibia" */
            /* 18 */ MESSAGEMODE_WARNING,
            /* 19 */ -1,
            /* 20 */ MESSAGEMODE_STATUS,
            /* 21 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 22 */ MESSAGEMODE_LOOK,
            /* 23 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 24 */ MESSAGEMODE_STATUS};

    int translated;

    if (VERSION_AT_LEAST(version, 8, 20)) {
        if (index > sizeof(until_855)) {
            return false;
        }

        translated = until_855[index];
    } else if (VERSION_AT_LEAST(version, 7, 70)) {
        if (index > sizeof(until_811)) {
            return false;
        }

        translated = until_811[index];
    } else if (VERSION_AT_LEAST(version, 7, 50)) {
        if (index > sizeof(until_760)) {
            return false;
        }

        translated = until_760[index];
    } else if (VERSION_AT_LEAST(version, 7, 24)) {
        if (index > sizeof(until_727)) {
            return false;
        }

        translated = until_727[index];
    } else if (VERSION_AT_LEAST(version, 7, 20)) {
        if (index > sizeof(until_721)) {
            return false;
        }

        translated = until_721[index];
    } else {
        if (index > sizeof(until_711)) {
            return false;
        }

        translated = until_711[index];
    }

    if (translated == -1) {
        return false;
    }

    *out = (enum TrcMessageMode)translated;
    return true;
}

bool version_TranslateMessageMode(const struct trc_version *version,
                                  uint8_t index,
                                  enum TrcMessageMode *out) {
    static const char until_1095[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_PRIVATE_OUT,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_SPELL,
            /* 10 */ MESSAGEMODE_NPC_START,
            /* 11 */ MESSAGEMODE_NPC_CONTINUED,
            /* 12 */ MESSAGEMODE_PLAYER_TO_NPC,
            /* 13 */ MESSAGEMODE_BROADCAST,
            /* 14 */ MESSAGEMODE_CHANNEL_RED,
            /* 15 */ MESSAGEMODE_GM_TO_PLAYER,
            /* 16 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 17 */ MESSAGEMODE_LOGIN,
            /* 18 */ MESSAGEMODE_ADMIN,
            /* 19 */ MESSAGEMODE_GAME,
            /* 20 */ -1, /* Unknown */
            /* 21 */ MESSAGEMODE_FAILURE,
            /* 22 */ MESSAGEMODE_LOOK,
            /* 23 */ MESSAGEMODE_DAMAGE_DEALT,
            /* 24 */ MESSAGEMODE_DAMAGE_RECEIVED,
            /* 25 */ MESSAGEMODE_HEALING,
            /* 26 */ MESSAGEMODE_EXPERIENCE,
            /* 27 */ MESSAGEMODE_DAMAGE_RECEIVED_OTHERS,
            /* 28 */ MESSAGEMODE_HEALING_OTHERS,
            /* 29 */ MESSAGEMODE_EXPERIENCE_OTHERS,
            /* 30 */ MESSAGEMODE_STATUS,
            /* 31 */ MESSAGEMODE_LOOT,
            /* 32 */ MESSAGEMODE_NPC_TRADE,
            /* 33 */ MESSAGEMODE_GUILD,
            /* 34 */ MESSAGEMODE_PARTY_WHITE,
            /* 35 */ MESSAGEMODE_PARTY,
            /* 36 */ MESSAGEMODE_MONSTER_SAY,
            /* 37 */ MESSAGEMODE_MONSTER_YELL,
            /* 38 */ MESSAGEMODE_REPORT,
            /* 39 */ MESSAGEMODE_HOTKEY,
            /* 40 */ MESSAGEMODE_TUTORIAL,
            /* 41 */ MESSAGEMODE_THANK_YOU,
            /* 42 */ MESSAGEMODE_MARKET,
            /* 43 */ MESSAGEMODE_MANA};

    static const char until_855[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ -1,
            /* 17 */ MESSAGEMODE_ADMIN,
            /* 18 */ MESSAGEMODE_GAME,
            /* 19 */ -1,
            /* 20 */ -1,
            /* 21 */ MESSAGEMODE_WARNING, /* "Warning! The ... " */
            /* 22 */ -1,
            /* 23 */ MESSAGEMODE_LOGIN,
            /* 24 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 25 */ MESSAGEMODE_LOOK,
            /* 26 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 27 */ MESSAGEMODE_STATUS};

    static const char until_811[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ -1,
            /* 17 */ MESSAGEMODE_ADMIN,
            /* 18 */ MESSAGEMODE_GAME,    /* ?? */
            /* 19 */ MESSAGEMODE_GAME,    /* Raid, "Goblins of the ... " */
            /* 20 */ MESSAGEMODE_LOGIN,   /* "Your last visit in Tibia" */
            /* 21 */ MESSAGEMODE_WARNING, /* "Warning! The ... ", or "you
                                                   lose X hitpoints"
                                                   in 7.55-7.70(?) */
            /* 22 */ MESSAGEMODE_HOTKEY,
            /* 23 */ MESSAGEMODE_FAILURE, /*"Sorry, not possible?" in 7.70
                                           */
            /* 24 */ MESSAGEMODE_STATUS,  /* "You deal X damage ... " */
            /* 25 */ MESSAGEMODE_LOOK,
            /* 26 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible?" */
            /* 27 */ MESSAGEMODE_STATUS};

    static const char until_727[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ MESSAGEMODE_ADMIN,
            /* 17 */ MESSAGEMODE_GAME,    /* ?? */
            /* 18 */ MESSAGEMODE_WARNING, /* Server save */
            /* 19 */ MESSAGEMODE_LOGIN,
            /* 20 */ MESSAGEMODE_STATUS, /* "You lose ... " */
            /* 21 */ MESSAGEMODE_STATUS,
            /* 22 */ MESSAGEMODE_LOOK,
            /* 23 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 24 */ -1,
            /* 25 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 26 */ MESSAGEMODE_STATUS};

    static const char until_721[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ MESSAGEMODE_ADMIN,
            /* 17 */ MESSAGEMODE_WARNING, /* "Warning! The ... " */
            /* 18 */ MESSAGEMODE_GAME,    /* Raid, "Goblins of the ... " */
            /* 19 */ MESSAGEMODE_LOGIN,
            /* 20 */ MESSAGEMODE_STATUS, /* "You lose ... " */
            /* 21 */ MESSAGEMODE_LOOK,
            /* 22 */ MESSAGEMODE_STATUS, /* "Message sent to ... " */
            /* 23 */ -1,
            /* 24 */ -1,
            /* 25 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 26 */ MESSAGEMODE_STATUS};

    static const char until_711[] = {
            /* 0 */ -1,
            /* 1 */ MESSAGEMODE_SAY,
            /* 2 */ MESSAGEMODE_WHISPER,
            /* 3 */ MESSAGEMODE_YELL,
            /* 4 */ MESSAGEMODE_PRIVATE_IN,
            /* 5 */ MESSAGEMODE_CHANNEL_YELLOW,
            /* 6 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 7 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 8 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 9 */ MESSAGEMODE_BROADCAST,
            /* 10 */ MESSAGEMODE_CHANNEL_RED,
            /* 11 */ MESSAGEMODE_PLAYER_TO_GM,
            /* 12 */ MESSAGEMODE_CHANNEL_ORANGE,
            /* 13 */ -1,
            /* 14 */ MESSAGEMODE_CHANNEL_WHITE,
            /* 15 */ -1,
            /* 16 */ MESSAGEMODE_GAME,   /* "You advanced in ..." */
            /* 17 */ MESSAGEMODE_LOGIN,  /* ?? */
            /* 18 */ MESSAGEMODE_STATUS, /* "You lose ... " */
            /* 19 */ MESSAGEMODE_LOOK,
            /* 20 */ MESSAGEMODE_WARNING, /* "Warning! The ... " */
            /* 21 */ -1,
            /* 22 */ MESSAGEMODE_STATUS,
            /* 23 */ MESSAGEMODE_STATUS, /* "You deal X damage ... " */
            /* 24 */ -1,
            /* 25 */ MESSAGEMODE_FAILURE, /* "Sorry, not possible? "*/
            /* 26 */ MESSAGEMODE_STATUS};

    int translated;

    /* HACK: get 8.55 rolling */
    if (VERSION_AT_LEAST(version, 9, 0)) {
        if (!VERSION_AT_LEAST(version, 10, 36)) {
            if (index >= 10) {
                index += 1;
            }
        }

        if (!VERSION_AT_LEAST(version, 10, 55)) {
            if (index >= 20) {
                index += 1;
            }
        }

        if (index > sizeof(until_1095)) {
            return false;
        }

        translated = until_1095[index];
    } else if (VERSION_AT_LEAST(version, 8, 20)) {
        if (index > sizeof(until_855)) {
            return false;
        }

        translated = until_855[index];
    } else if (VERSION_AT_LEAST(version, 7, 30)) {
        if (index > sizeof(until_811)) {
            return false;
        }

        translated = until_811[index];
    } else if (VERSION_AT_LEAST(version, 7, 24)) {
        if (index > sizeof(until_727)) {
            return false;
        }

        translated = until_727[index];
    } else if (VERSION_AT_LEAST(version, 7, 20)) {
        if (index > sizeof(until_721)) {
            return false;
        }

        translated = until_721[index];
    } else {
        if (index > sizeof(until_711)) {
            return false;
        }

        translated = until_711[index];
    }

    if (translated == -1) {
        return false;
    }

    *out = (enum TrcMessageMode)translated;
    return true;
}

bool version_TranslatePictureIndex(const struct trc_version *version,
                                   enum TrcPictureIndex index,
                                   int *out) {
    int translated;

    char until_1095[] = {
            /* PICTURE_INDEX_SPLASH_BACKGROUND */ 0,
            /* PICTURE_INDEX_SPLASH_LOGO */ 1,
            /* PICTURE_INDEX_TUTORIAL */ 2,
            /* PICTURE_INDEX_FONT_UNBORDERED */ 3,
            /* PICTURE_INDEX_ICONS */ 4,
            /* PICTURE_INDEX_FONT_GAME */ 5,
            /* PICTURE_INDEX_FONT_INTERFACE_SMALL */ 6,
            /* PICTURE_INDEX_LIGHT_FALLBACKS */ 7,
            /* PICTURE_INDEX_FONT_INTERFACE_LARGE */ 8,
    };

    char until_854[] = {
            /* PICTURE_INDEX_SPLASH_BACKGROUND */ 0,
            /* PICTURE_INDEX_SPLASH_LOGO */ -1,
            /* PICTURE_INDEX_TUTORIAL */ 1,
            /* PICTURE_INDEX_FONT_UNBORDERED */ 2,
            /* PICTURE_INDEX_ICONS */ 3,
            /* PICTURE_INDEX_FONT_GAME */ 4,
            /* PICTURE_INDEX_FONT_INTERFACE_SMALL */ 5,
            /* PICTURE_INDEX_LIGHT_FALLBACKS */ 6,
            /* PICTURE_INDEX_FONT_INTERFACE_LARGE */ 7,
    };

    if (version->Major > 8) {
        if (index > sizeof(until_1095)) {
            return false;
        }

        translated = until_1095[index];
    } else {
        if (index > sizeof(until_854) || until_854[index] == -1) {
            return false;
        }

        translated = until_854[index];
    }

    *out = translated;
    return true;
}

bool version_TranslateFluidColor(const struct trc_version *version,
                                 uint8_t color,
                                 int *out) {
    enum {
        FLUID_EMPTY = 0x00,
        FLUID_BLUE = 0x01,
        FLUID_RED = 0x02,
        FLUID_BROWN = 0x03,
        FLUID_GREEN = 0x04,
        FLUID_YELLOW = 0x05,
        FLUID_WHITE = 0x06,
        FLUID_PURPLE = 0x07
    };

    static const char until_1095[] = {
            FLUID_EMPTY,
            FLUID_BLUE,
            FLUID_PURPLE,
            FLUID_BROWN,
            FLUID_BROWN,
            FLUID_RED,
            FLUID_GREEN,
            FLUID_BROWN,
            FLUID_YELLOW,
            FLUID_WHITE,
            FLUID_PURPLE,
            FLUID_RED,
            FLUID_YELLOW,
            FLUID_BROWN,
            FLUID_YELLOW,
            FLUID_WHITE,
            FLUID_BLUE,
    };

    if (VERSION_AT_LEAST(version, 7, 80)) {
        if (color > sizeof(until_1095)) {
            return false;
        }

        *out = until_1095[color];
    } else {
        *out = color % 8;
    }

    return true;
}
