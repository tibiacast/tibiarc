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

static void translation_Initialize(struct trc_translation_table *table) {
    table->Max = -1;
    memset(table->Map, -1, sizeof(table->Map));
}

static void translation_Insert(struct trc_translation_table *table,
                               int index,
                               int8_t value) {
    ABORT_UNLESS(MAX(table->Max, index) < sizeof(table->Map));

    if (index <= table->Max) {
        memmove(&table->Map[index + 1],
                &table->Map[index],
                table->Max - index + 1);
        table->Max++;
    }

    table->Max = MAX(table->Max, index);
    table->Map[index] = value;
}

static void translation_Remove(struct trc_translation_table *table,
                               int index,
                               int8_t value) {
    ABORT_UNLESS(table->Map[index] == value);

    if (index < table->Max) {
        memmove(&table->Map[index], &table->Map[index + 1], table->Max - index);
    } else {
        ABORT_UNLESS(index == table->Max);
        table->Map[index] = -1;
    }

    table->Max--;
}

static void translation_Replace(struct trc_translation_table *table,
                                int index,
                                int8_t from,
                                int8_t to) {
    ABORT_UNLESS(table->Map[index] == from);
    ABORT_UNLESS(index <= table->Max);
    table->Map[index] = to;
}

static bool translation_Get(const struct trc_translation_table *table,
                            uint8_t index,
                            int *out) {
    ASSERT(table->Max > -1);

    if (index > table->Max) {
        return false;
    }

    *out = table->Map[index];
    return *out != -1;
}

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

static void version_InitTypeProperties(struct trc_version *version) {
    struct trc_translation_table *table = &version->TypeProperties;

    translation_Initialize(table);

    /* 7.00 - 7.30, serving as a baseline. */
    ABORT_UNLESS(VERSION_AT_LEAST(version, 7, 00));
    translation_Insert(table, 0, TYPEPROPERTY_GROUND);
    translation_Insert(table, 1, TYPEPROPERTY_CLIP);
    translation_Insert(table, 2, TYPEPROPERTY_BOTTOM);
    translation_Insert(table, 3, TYPEPROPERTY_CONTAINER);
    translation_Insert(table, 4, TYPEPROPERTY_STACKABLE);
    translation_Insert(table, 5, TYPEPROPERTY_USABLE);
    translation_Insert(table, 6, TYPEPROPERTY_FORCE_USE);
    translation_Insert(table, 7, TYPEPROPERTY_WRITE);
    translation_Insert(table, 8, TYPEPROPERTY_WRITE_ONCE);
    translation_Insert(table, 9, TYPEPROPERTY_LIQUID_CONTAINER);
    translation_Insert(table, 10, TYPEPROPERTY_LIQUID_POOL);
    translation_Insert(table, 11, TYPEPROPERTY_BLOCKING);
    translation_Insert(table, 12, TYPEPROPERTY_UNMOVABLE);
    translation_Insert(table, 13, TYPEPROPERTY_BLOCKING);
    translation_Insert(table, 14, TYPEPROPERTY_UNPATHABLE);
    translation_Insert(table, 15, TYPEPROPERTY_TAKEABLE);
    translation_Insert(table, 16, TYPEPROPERTY_LIGHT);
    translation_Insert(table, 17, TYPEPROPERTY_DONT_HIDE);
    translation_Insert(table, 18, TYPEPROPERTY_BLOCKING);
    translation_Insert(table, 19, TYPEPROPERTY_HEIGHT);
    translation_Insert(table, 20, TYPEPROPERTY_DISPLACEMENT_LEGACY);
    translation_Insert(table, 21, -1);
    translation_Insert(table, 22, TYPEPROPERTY_AUTOMAP);
    translation_Insert(table, 23, TYPEPROPERTY_ROTATE);
    translation_Insert(table, 24, TYPEPROPERTY_CORPSE);
    translation_Insert(table, 25, TYPEPROPERTY_HANGABLE);
    translation_Insert(table, 26, TYPEPROPERTY_UNKNOWN_U16);
    translation_Insert(table, 27, TYPEPROPERTY_HORIZONTAL);
    translation_Insert(table, 28, TYPEPROPERTY_ANIMATE_IDLE);
    translation_Insert(table, 29, TYPEPROPERTY_LENSHELP);

    if (VERSION_AT_LEAST(version, 7, 40)) {
        translation_Replace(table,
                            26,
                            TYPEPROPERTY_UNKNOWN_U16,
                            TYPEPROPERTY_VERTICAL);
    }

    if (VERSION_AT_LEAST(version, 7, 55)) {
        translation_Insert(table, 3, TYPEPROPERTY_TOP);

        /* FORCE_USE and USABLE have changed places. */
        translation_Replace(table,
                            6,
                            TYPEPROPERTY_USABLE,
                            TYPEPROPERTY_FORCE_USE);
        translation_Replace(table,
                            7,
                            TYPEPROPERTY_FORCE_USE,
                            TYPEPROPERTY_USABLE);

        /* Lots of fields were reordered, seemingly for the heck of it. Did
         * they sort an enum or something? */
        translation_Replace(table,
                            17,
                            TYPEPROPERTY_LIGHT,
                            TYPEPROPERTY_HANGABLE);
        translation_Replace(table,
                            18,
                            TYPEPROPERTY_DONT_HIDE,
                            TYPEPROPERTY_VERTICAL);
        translation_Replace(table,
                            19,
                            TYPEPROPERTY_BLOCKING,
                            TYPEPROPERTY_HORIZONTAL);
        translation_Replace(table,
                            20,
                            TYPEPROPERTY_HEIGHT,
                            TYPEPROPERTY_ROTATE);
        translation_Replace(table,
                            21,
                            TYPEPROPERTY_DISPLACEMENT_LEGACY,
                            TYPEPROPERTY_LIGHT);
        translation_Replace(table, 22, -1, TYPEPROPERTY_DONT_HIDE);
        translation_Replace(table,
                            23,
                            TYPEPROPERTY_AUTOMAP,
                            TYPEPROPERTY_TRANSLUCENT);
        translation_Replace(table,
                            24,
                            TYPEPROPERTY_ROTATE,
                            TYPEPROPERTY_DISPLACEMENT);
        translation_Replace(table,
                            25,
                            TYPEPROPERTY_CORPSE,
                            TYPEPROPERTY_HEIGHT);
        translation_Replace(table,
                            26,
                            TYPEPROPERTY_HANGABLE,
                            TYPEPROPERTY_REDRAW_NEARBY_TOP);
        translation_Replace(table,
                            27,
                            TYPEPROPERTY_VERTICAL,
                            TYPEPROPERTY_ANIMATE_IDLE);
        translation_Replace(table,
                            28,
                            TYPEPROPERTY_HORIZONTAL,
                            TYPEPROPERTY_AUTOMAP);
        translation_Replace(table,
                            29,
                            TYPEPROPERTY_ANIMATE_IDLE,
                            TYPEPROPERTY_LENSHELP);
        translation_Insert(table, 30, TYPEPROPERTY_WALKABLE);
    }

    if (VERSION_AT_LEAST(version, 7, 80)) {
        translation_Insert(table, 8, TYPEPROPERTY_RUNE);
        translation_Insert(table, 32, TYPEPROPERTY_LOOK_THROUGH);
    }

    if (VERSION_AT_LEAST(version, 8, 60)) {
        translation_Remove(table, 8, TYPEPROPERTY_RUNE);
    }

    if (VERSION_AT_LEAST(version, 10, 10)) {
        translation_Insert(table, 16, TYPEPROPERTY_NO_MOVE_ANIMATION);
    }

    /* TODO: Figure out which versions these belong to:
     *
     * 33:  TYPEPROPERTY_EQUIPMENT_SLOT,
     * 34:  TYPEPROPERTY_MARKET_ITEM,
     * 35:  TYPEPROPERTY_DEFAULT_ACTION,
     * 36:  TYPEPROPERTY_WRAPPABLE,
     * 37:  TYPEPROPERTY_UNWRAPPABLE,
     * 38:  TYPEPROPERTY_TOP_EFFECT */
}

static void version_InitMessageTypes(struct trc_version *version) {
    struct trc_translation_table *table = &version->MessageModes;

    translation_Initialize(table);

    /* 7.11, serving as a baseline. */
    ABORT_UNLESS(VERSION_AT_LEAST(version, 7, 11));
    translation_Insert(table, 16, MESSAGEMODE_GAME);
    translation_Insert(table, 17, MESSAGEMODE_LOGIN);
    translation_Insert(table, 18, MESSAGEMODE_STATUS);
    translation_Insert(table, 19, MESSAGEMODE_LOOK);
    translation_Insert(table, 20, MESSAGEMODE_FAILURE);

    if (VERSION_AT_LEAST(version, 7, 20)) {
        /* Dummy entry, no idea where this should be. */
        translation_Insert(table, 0, -1);

        translation_Insert(table, 17, MESSAGEMODE_WARNING);
    }

    if (VERSION_AT_LEAST(version, 7, 24)) {
        /* Dummy entry, no idea where this should be. */
        translation_Insert(table, 0, -1);
    }

    if (VERSION_AT_LEAST(version, 8, 20)) {
        /* Red message to console */
        translation_Insert(table, 19, -1);
        /* Event-related status message */
        translation_Insert(table, 20, MESSAGEMODE_STATUS);
    }

    if (VERSION_AT_LEAST(version, 8, 40)) {
        /* Unhandled orange console message */
        translation_Insert(table, 20, -1);
    }
}

static void version_InitSpeakTypes(struct trc_version *version) {
    struct trc_translation_table *table = &version->SpeakModes;

    translation_Initialize(table);

    /* 7.11, serving as a baseline. */
    ABORT_UNLESS(VERSION_AT_LEAST(version, 7, 11));
    translation_Insert(table, 1, MESSAGEMODE_SAY);
    translation_Insert(table, 2, MESSAGEMODE_WHISPER);
    translation_Insert(table, 3, MESSAGEMODE_YELL);
    translation_Insert(table, 4, MESSAGEMODE_PRIVATE_IN);
    translation_Insert(table, 5, MESSAGEMODE_CHANNEL_YELLOW);
    translation_Insert(table, 9, MESSAGEMODE_BROADCAST);
    translation_Insert(table, 10, MESSAGEMODE_CHANNEL_RED);
    translation_Insert(table, 13, MESSAGEMODE_MONSTER_SAY);
    translation_Insert(table, 14, MESSAGEMODE_MONSTER_YELL);

    if (VERSION_AT_LEAST(version, 7, 20)) {
        translation_Insert(table, 12, MESSAGEMODE_CHANNEL_ORANGE);

        /* TODO: Figure out what was added before MESSAGEMODE_MONSTER_SAY */
        translation_Insert(table, 13, -1);
    }

    if (VERSION_AT_LEAST(version, 7, 23)) {
        /* TODO: Figure out what was added before MESSAGEMODE_MONSTER_SAY */
        translation_Insert(table, 13, -1);
    }

    if (VERSION_AT_LEAST(version, 8, 20)) {
        translation_Insert(table, 4, MESSAGEMODE_PLAYER_TO_NPC);
        translation_Insert(table, 5, MESSAGEMODE_NPC_START);
    }

    if (VERSION_AT_LEAST(version, 8, 40)) {
        translation_Insert(table, 8, MESSAGEMODE_CHANNEL_WHITE);
    }
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
    }

    if (VERSION_AT_LEAST(version, 8, 40)) {
        version->Protocol.TextEditObject = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 41)) {
        version->Protocol.AddObjectStackPosition = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 53)) {
        version->Protocol.PassableCreatures = 1;
        version->Protocol.WarIcon = 1;
    }

    if (VERSION_AT_LEAST(version, 8, 60)) {
        version->Protocol.CancelAttackId = 1;
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
        DUMP_PROPERTY(Corpse);
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
        DUMP_PROPERTY(UnknownU16);
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

    version_InitTypeProperties(version);
    version_InitMessageTypes(version);
    version_InitSpeakTypes(version);
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
            {SO(IconBarStatusSprites[STATUS_ICON_DRUNK]), 279, 41, 9, 9},
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
    if (index == 255) {
        *out = TYPEPROPERTY_ENTRY_END_MARKER;
        return true;
    }

    int translated;

    if (translation_Get(&version->TypeProperties, index, &translated)) {
        *out = (enum TrcTypeProperty)translated;
        return true;
    }

    return false;
}

bool version_TranslateSpeakMode(const struct trc_version *version,
                                uint8_t index,
                                enum TrcMessageMode *out) {
    int translated;

    if (translation_Get(&version->SpeakModes, index, &translated)) {
        *out = (enum TrcMessageMode)translated;
        return true;
    }

    return false;
}

bool version_TranslateMessageMode(const struct trc_version *version,
                                  uint8_t index,
                                  enum TrcMessageMode *out) {
    int translated;

    if (translation_Get(&version->MessageModes, index, &translated)) {
        *out = (enum TrcMessageMode)translated;
        return true;
    }

    return false;
}

bool version_TranslatePictureIndex(const struct trc_version *version,
                                   enum TrcPictureIndex index,
                                   int *out) {
    int translated;

    /* Reject PICTURE_INDEX_SPLASH_BACKGROUND/LOGO to simplify the stuff
     * below. */
    ABORT_UNLESS(CHECK_RANGE((int)index,
                             (int)PICTURE_INDEX_TUTORIAL,
                             (int)PICTURE_INDEX_LAST));
    /* As the enum, but PICTURE_INDEX_SPLASH_LOGO is missing. We need to figure
     * out when that was added. */
    translated = (int)index - 1;

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
