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

#ifndef __TRC_TYPES_H__
#define __TRC_TYPES_H__

#include <stdbool.h>
#include <stdint.h>

#include "versions_decl.h"

#include "sprites.h"
#include "datareader.h"

enum TrcTypeProperty {
    TYPEPROPERTY_ANIMATE_IDLE,
    TYPEPROPERTY_AUTOMAP,
    TYPEPROPERTY_BLOCKING,
    TYPEPROPERTY_BOTTOM,
    TYPEPROPERTY_CLIP,
    TYPEPROPERTY_CONTAINER,
    TYPEPROPERTY_DEFAULT_ACTION,
    TYPEPROPERTY_DISPLACEMENT_LEGACY,
    TYPEPROPERTY_DISPLACEMENT,
    TYPEPROPERTY_DONT_HIDE,
    TYPEPROPERTY_EQUIPMENT_SLOT,
    TYPEPROPERTY_FORCE_USE,
    TYPEPROPERTY_GROUND,
    TYPEPROPERTY_HANGABLE,
    TYPEPROPERTY_HEIGHT,
    TYPEPROPERTY_HORIZONTAL,
    TYPEPROPERTY_LENSHELP,
    TYPEPROPERTY_LIGHT,
    TYPEPROPERTY_LIQUID_CONTAINER,
    TYPEPROPERTY_LIQUID_POOL,
    TYPEPROPERTY_LOOK_THROUGH,
    TYPEPROPERTY_MARKET_ITEM,
    TYPEPROPERTY_MULTI_USE,
    TYPEPROPERTY_NO_MOVE_ANIMATION,
    TYPEPROPERTY_REDRAW_NEARBY_TOP,
    TYPEPROPERTY_ROTATE,
    TYPEPROPERTY_RUNE,
    TYPEPROPERTY_STACKABLE,
    TYPEPROPERTY_TAKEABLE,
    TYPEPROPERTY_TOP_EFFECT,
    TYPEPROPERTY_TOP,
    TYPEPROPERTY_TRANSLUCENT,
    TYPEPROPERTY_UNLOOKABLE,
    TYPEPROPERTY_UNMOVABLE,
    TYPEPROPERTY_UNPATHABLE,
    TYPEPROPERTY_UNWRAPPABLE,
    TYPEPROPERTY_USABLE,
    TYPEPROPERTY_VERTICAL,
    TYPEPROPERTY_WALKABLE,
    TYPEPROPERTY_WRAPPABLE,
    TYPEPROPERTY_WRITE_ONCE,
    TYPEPROPERTY_WRITE,

    TYPEPROPERTY_INERT,
    TYPEPROPERTY_ENTRY_END_MARKER
};

struct trc_spritephase {
    uint32_t Minimum;
    uint32_t Maximum;
};

enum TrcFrameGroupIndex {
    FRAME_GROUP_DEFAULT = 0,
    FRAME_GROUP_IDLE = 0,
    FRAME_GROUP_WALKING = 1,

    FRAME_GROUP_FIRST = FRAME_GROUP_DEFAULT,
    FRAME_GROUP_LAST = FRAME_GROUP_WALKING
};

struct trc_framegroup {
    bool Active;

    uint8_t SizeX;
    uint8_t SizeY;
    uint8_t RenderSize;
    uint8_t LayerCount;
    uint8_t XDiv;
    uint8_t YDiv;
    uint8_t ZDiv;
    uint8_t FrameCount;

    uint8_t AnimationType;
    uint8_t StartPhase;
    uint32_t LoopCount;

    struct trc_spritephase *Phases;

    uint32_t SpriteCount;
    uint32_t *SpriteIds;
};

struct trc_entitytype {
    struct {
        /* Markers and values used by the parser and renderer. These are placed
         * low and packed together according to usage in an attempt to improve
         * code density. */
        uint8_t StackPriority : 3;
        bool LiquidContainer : 1;
        bool LiquidPool : 1;
        bool Stackable : 1;
        bool Rune : 1;
        bool Animated : 1;
        bool AnimateIdle : 1;
        bool RedrawNearbyTop : 1;
        bool Hangable : 1;
        bool Vertical : 1;
        bool Horizontal : 1;
        bool DontHide : 1;
        bool Unlookable : 1;

        uint16_t DisplacementX;
        uint16_t DisplacementY;
        uint16_t Speed;
        uint16_t Height;

        /* To reduce memory usage, we only store these properties when
         * needed. */
#ifdef DUMP_ITEMS
        bool LookThrough : 1;
        bool NoMoveAnimation : 1;
        bool Translucent : 1;
        bool Blocking : 1;
        bool Container : 1;
        bool EquipmentSlotRestricted : 1;
        bool ForceUse : 1;
        bool MarketItem : 1;
        bool MultiUse : 1;
        bool Rotate : 1;
        bool Takeable : 1;
        bool TopEffect : 1;
        bool Unmovable : 1;
        bool Unpathable : 1;
        bool Unwrappable : 1;
        bool Usable : 1;
        bool Walkable : 1;
        bool Wrappable : 1;
        bool Write : 1;
        bool WriteOnce : 1;

        uint16_t LightIntensity;
        uint16_t MaxTextLength;
        uint16_t LightColor;
        uint16_t EquipmentSlot;
        uint16_t AutomapColor;
        uint16_t LensHelp;
        uint16_t DefaultAction;
        uint16_t MarketCategory;
        uint16_t TradeAs;
        uint16_t ShowAs;
        uint16_t VocationRestriction;
        uint16_t LevelRestriction;

        uint16_t NameLength;
        char Name[64];
#endif
    } Properties;

    struct trc_framegroup FrameGroups[FRAME_GROUP_LAST + 1];
};

struct trc_type_category {
    uint16_t MinId;
    uint16_t MaxId;
    struct trc_entitytype *Types;
};

struct trc_type_file {
    uint32_t Signature;

    struct trc_type_category Items;
    struct trc_type_category Outfits;
    struct trc_type_category Effects;
    struct trc_type_category Missiles;
};

bool types_Load(struct trc_version *version, struct trc_data_reader *data);
void types_Free(struct trc_version *version);

bool types_GetItem(const struct trc_version *version,
                   uint16_t id,
                   struct trc_entitytype **result);
bool types_GetOutfit(const struct trc_version *version,
                     uint16_t id,
                     struct trc_entitytype **result);
bool types_GetEffect(const struct trc_version *version,
                     uint16_t id,
                     struct trc_entitytype **result);
bool types_GetMissile(const struct trc_version *version,
                      uint16_t id,
                      struct trc_entitytype **result);

#endif /* __TRC_TYPES_H__ */
