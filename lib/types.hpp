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

#ifndef __TRC_TYPES_HPP__
#define __TRC_TYPES_HPP__

#include <cstdint>
#include <string>

#include "versions_decl.hpp"

#include "sprites.hpp"
#include "datareader.hpp"

#include <vector>

namespace trc {
enum class TypeProperty {
    AnimateIdle,
    Automap,
    Blocking,
    Bottom,
    Clip,
    Container,
    Corpse,
    DefaultAction,
    DisplacementLegacy,
    Displacement,
    DontHide,
    EquipmentSlot,
    ForceUse,
    Ground,
    Hangable,
    Height,
    Horizontal,
    Lenshelp,
    Light,
    LiquidContainer,
    LiquidPool,
    LookThrough,
    MarketItem,
    MultiUse,
    NoMoveAnimation,
    RedrawNearbyTop,
    Rotate,
    Rune,
    Stackable,
    Takeable,
    TopEffect,
    Top,
    Translucent,
    UnknownU16,
    Unlookable,
    Unmovable,
    Unpathable,
    Unwrappable,
    Usable,
    Vertical,
    Walkable,
    Wrappable,
    WriteOnce,
    Write,

    EntryEndMarker
};

enum class FrameGroupIndex {
    Default = 0,
    Idle = 0,
    Walking = 1,

    First = Default,
    Last = Walking
};

class EntityType {
    void ReadProperties(const Version &version, DataReader &reader);
    void ReadFrameGroup(const Version &version,
                        DataReader &reader,
                        FrameGroupIndex groupIndex);

public:
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
        bool Blocking : 1;
        bool Container : 1;
        bool Corpse : 1;
        bool EquipmentSlotRestricted : 1;
        bool ForceUse : 1;
        bool LookThrough : 1;
        bool MarketItem : 1;
        bool MultiUse : 1;
        bool NoMoveAnimation : 1;
        bool Rotate : 1;
        bool Takeable : 1;
        bool TopEffect : 1;
        bool Translucent : 1;
        bool UnknownU16 : 1;
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

        std::string Name;
#endif
    } Properties;

    struct FrameGroup {
        bool Active = false;

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

        struct SpritePhase {
            uint32_t Minimum;
            uint32_t Maximum;
        };

        std::vector<SpritePhase> Phases;
        std::vector<const Sprite *> Sprites;
    } FrameGroups[static_cast<int>(FrameGroupIndex::Last) + 1];

    EntityType(const Version &version, DataReader &data, bool hasFrameGroups);

    EntityType(const EntityType &other) = delete;
};

class TypeFile {
    uint32_t Signature;

    uint16_t ItemMaxId;
    uint16_t OutfitMaxId;
    uint16_t EffectMaxId;
    uint16_t MissileMaxId;

    struct TypeCategory {
        std::unordered_map<uint32_t, EntityType> Entities;

        uint16_t MinId;
        uint16_t MaxId;

        TypeCategory(const Version &version,
                     DataReader &data,
                     uint16_t minId,
                     uint16_t maxId,
                     bool hasFrameGroups);
    };

    struct TypeCategory Items;
    struct TypeCategory Outfits;
    struct TypeCategory Effects;
    struct TypeCategory Missiles;

public:
    const EntityType &GetItem(uint16_t id) const;
    const EntityType &GetOutfit(uint16_t id) const;
    const EntityType &GetEffect(uint16_t id) const;
    const EntityType &GetMissile(uint16_t id) const;

    /* FIXME: Currently needs sprites initialized, this is pretty bad coupling
     * there's not much we can do about it. */
    TypeFile(const Version &version, DataReader data);

    TypeFile(const TypeFile &other) = delete;
};
} // namespace trc

#endif /* __TRC_TYPES_HPP__ */
