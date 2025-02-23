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

#include "types.hpp"

#include "datareader.hpp"
#include "sprites.hpp"
#include "versions.hpp"

#include "utils.hpp"

#include <utility>
#include <limits>

namespace trc {

#ifdef DUMP_ITEMS
#    define types_ReadOptionalU16(Reader, Result) Result = Reader.ReadU16()
#    define types_ReadOptionalString(Reader, Result)                           \
        Result = Reader.ReadString()
#    define types_SetOptionalProperty(Expr) Expr = 1
#else
#    define types_ReadOptionalU16(Reader, Result) Reader.SkipU16();
#    define types_ReadOptionalString(Reader, Result) Reader.SkipString()
#    define types_SetOptionalProperty(Expr)
#endif

void EntityType::ReadProperties(const Version &version, DataReader &reader) {
    Properties.StackPriority = 5;

    for (;;) {
        switch (version.TranslateTypeProperty(reader.ReadU8())) {
        case TypeProperty::Ground:
            Properties.Speed = reader.ReadU16();
            Properties.StackPriority = 0;

            break;
        case TypeProperty::Clip:
            Properties.StackPriority = 1;
            break;
        case TypeProperty::Bottom:
            Properties.StackPriority = 2;
            break;
        case TypeProperty::Top:
            Properties.StackPriority = 3;
            break;
        case TypeProperty::Stackable:
            Properties.Stackable = true;
            break;
        case TypeProperty::Rune:
            Properties.Rune = true;
            break;
        case TypeProperty::LiquidContainer:
            Properties.LiquidContainer = true;
            break;
        case TypeProperty::LiquidPool:
            Properties.LiquidPool = true;
            break;
        case TypeProperty::Unlookable:
            Properties.Unlookable = true;
            break;
        case TypeProperty::Hangable:
            Properties.Hangable = true;
            break;
        case TypeProperty::Vertical:
            Properties.Vertical = true;
            break;
        case TypeProperty::Horizontal:
            Properties.Horizontal = true;
            break;
        case TypeProperty::DontHide:
            Properties.DontHide = true;
            break;
        case TypeProperty::Displacement:
            Properties.DisplacementX = reader.ReadU16();
            Properties.DisplacementY = reader.ReadU16();

            break;
        case TypeProperty::DisplacementLegacy:
            Properties.DisplacementX = 8;
            Properties.DisplacementY = 8;

            break;
        case TypeProperty::Height:
            Properties.Height = reader.ReadU16();

            break;
        case TypeProperty::RedrawNearbyTop:
            Properties.RedrawNearbyTop = true;
            break;
        case TypeProperty::AnimateIdle:
            Properties.AnimateIdle = true;
            break;

            /* Optional properties follow: these are parsed but not used. */
        case TypeProperty::Container:
            types_SetOptionalProperty(Properties.Container);
            break;
        case TypeProperty::Automap:
            types_ReadOptionalU16(reader, Properties.AutomapColor);
            break;
        case TypeProperty::Lenshelp:
            types_ReadOptionalU16(reader, Properties.LensHelp);
            break;
        case TypeProperty::Wrappable:
            types_SetOptionalProperty(Properties.Wrappable);
            break;
        case TypeProperty::Unwrappable:
            types_SetOptionalProperty(Properties.Unwrappable);
            break;
        case TypeProperty::TopEffect:
            types_SetOptionalProperty(Properties.TopEffect);
            break;
        case TypeProperty::NoMoveAnimation:
            types_SetOptionalProperty(Properties.NoMoveAnimation);
            break;
        case TypeProperty::Usable:
            types_SetOptionalProperty(Properties.Usable);
            break;
        case TypeProperty::Corpse:
            types_SetOptionalProperty(Properties.Corpse);
            break;
        case TypeProperty::Blocking:
            types_SetOptionalProperty(Properties.Blocking);
            break;
        case TypeProperty::Unmovable:
            types_SetOptionalProperty(Properties.Unmovable);
            break;
        case TypeProperty::Unpathable:
            types_SetOptionalProperty(Properties.Unpathable);
            break;
        case TypeProperty::Takeable:
            types_SetOptionalProperty(Properties.Takeable);
            break;
        case TypeProperty::ForceUse:
            types_SetOptionalProperty(Properties.ForceUse);
            break;
        case TypeProperty::MultiUse:
            types_SetOptionalProperty(Properties.MultiUse);
            break;
        case TypeProperty::Translucent:
            types_SetOptionalProperty(Properties.Translucent);
            break;
        case TypeProperty::Walkable:
            types_SetOptionalProperty(Properties.Walkable);
            break;
        case TypeProperty::LookThrough:
            types_SetOptionalProperty(Properties.LookThrough);
            break;
        case TypeProperty::Rotate:
            types_SetOptionalProperty(Properties.Rotate);
            break;
        case TypeProperty::Write:
            types_SetOptionalProperty(Properties.Write);
            types_ReadOptionalU16(reader, Properties.MaxTextLength);
            break;
        case TypeProperty::WriteOnce:
            types_SetOptionalProperty(Properties.WriteOnce);
            types_ReadOptionalU16(reader, Properties.MaxTextLength);
            break;
        case TypeProperty::Light:
            types_ReadOptionalU16(reader, Properties.LightIntensity);
            types_ReadOptionalU16(reader, Properties.LightColor);
            break;
        case TypeProperty::EquipmentSlot:
            types_SetOptionalProperty(Properties.EquipmentSlotRestricted);
            types_ReadOptionalU16(reader, Properties.EquipmentSlot);

            break;
        case TypeProperty::MarketItem:
            types_SetOptionalProperty(Properties.MarketItem);
            types_ReadOptionalU16(reader, Properties.MarketCategory);
            types_ReadOptionalU16(reader, Properties.TradeAs);
            types_ReadOptionalU16(reader, Properties.ShowAs);
            types_ReadOptionalString(reader, Properties.Name);
            types_ReadOptionalU16(reader, Properties.VocationRestriction);
            types_ReadOptionalU16(reader, Properties.LevelRestriction);

            break;
        case TypeProperty::DefaultAction:
            types_ReadOptionalU16(reader, Properties.DefaultAction);
            break;
        case TypeProperty::UnknownU16:
            types_SetOptionalProperty(Properties.UnknownU16);
            reader.SkipU16();
            break;
        case TypeProperty::EntryEndMarker:
            return;
        }
    }
}

void EntityType::ReadFrameGroup(const Version &version,
                                DataReader &reader,
                                FrameGroupIndex groupIndex) {
    auto &group = FrameGroups[static_cast<int>(groupIndex)];

    size_t spriteCount = 1;
    group.Active = 1;

    group.SizeX = reader.ReadU8<1, 255>();
    spriteCount *= group.SizeX;
    group.SizeY = reader.ReadU8<1, 255>();
    spriteCount *= group.SizeY;

    if (spriteCount > 1) {
        group.RenderSize = reader.ReadU8();
    } else {
        /* Default to 1x1 tiles. */
        group.RenderSize = 32;
    }

    group.LayerCount = reader.ReadU8<1, 255>();
    spriteCount *= group.LayerCount;

    group.XDiv = reader.ReadU8<1, 255>();
    spriteCount *= group.XDiv;

    group.YDiv = reader.ReadU8<1, 255>();
    spriteCount *= group.YDiv;

    if (version.Features.TypeZDiv) {
        group.ZDiv = reader.ReadU8<1, 255>();
        spriteCount *= group.ZDiv;
    } else {
        group.ZDiv = 1;
    }

    group.FrameCount = reader.ReadU8<1, 255>();
    spriteCount *= group.FrameCount;

    Properties.Animated = (group.FrameCount > 1);

    if (spriteCount > std::numeric_limits<uint16_t>::max()) {
        throw InvalidDataError();
    }

    if (Properties.Animated && version.Features.AnimationPhases) {
        group.StartPhase = reader.ReadU8();
        group.LoopCount = reader.ReadU32();

        group.AnimationType = reader.ReadU8();

        for (int i = 0; i < group.FrameCount; i++) {
            auto minimum = reader.ReadU32();
            auto maximum = reader.ReadU32();

            group.Phases.emplace_back(minimum, maximum);
        }
    }

    for (unsigned i = 0; i < spriteCount; i++) {
        auto spriteId = version.Features.SpriteIndexU32 ? reader.ReadU32()
                                                        : reader.ReadU16();

        group.Sprites.emplace_back(&version.Sprites.Get(spriteId));
    }

    /* For types that have the same idle and walking frames, Cipsoft simply
     * omits the idle and uses walking.
     *
     * We'll do the same for versions before frame groups are present. */
    if (version.Features.FrameGroups) {
        if ((groupIndex == FrameGroupIndex::Walking &&
             (FrameGroups[std::to_underlying(FrameGroupIndex::Idle)].Active ==
                      0 ||
              FrameGroups[std::to_underlying(FrameGroupIndex::Idle)]
                              .FrameCount == 0))) {
            FrameGroups[std::to_underlying(FrameGroupIndex::Idle)] = group;
        }

        return;
    }

    AbortUnless(groupIndex == FrameGroupIndex::Idle);
    FrameGroups[std::to_underlying(FrameGroupIndex::Walking)] = group;
}

EntityType::EntityType(const Version &version,
                       DataReader &data,
                       bool hasFrameGroups)
    : Properties({}) {
    ReadProperties(version, data);

    Properties.Animated = 0;

    auto groupCount = hasFrameGroups ? data.ReadU8<1, 2>() : 1;

    for (int i = 0; i < groupCount; i++) {
        ReadFrameGroup(version,
                       data,
                       hasFrameGroups ? data.Read<FrameGroupIndex>()
                                      : FrameGroupIndex::Default);
    }
}

TypeFile::TypeCategory::TypeCategory(const Version &version,
                                     DataReader &data,
                                     uint16_t minId,
                                     uint16_t maxId,
                                     bool hasFrameGroups)
    : MinId(minId), MaxId(maxId) {
    for (auto idx = minId; idx <= maxId; idx++) {
        Entities.emplace(std::piecewise_construct,
                         std::forward_as_tuple(idx),
                         std::forward_as_tuple(version, data, hasFrameGroups));
    }
}

TypeFile::TypeFile(const Version &version, DataReader data)
    : Signature(data.ReadU32()),
      ItemMaxId(data.ReadU16()),
      OutfitMaxId(data.ReadU16()),
      EffectMaxId(data.ReadU16()),
      MissileMaxId(data.ReadU16()),
      Items(version, data, 100, ItemMaxId, false),
      Outfits(version, data, 1, OutfitMaxId, version.Features.FrameGroups),
      Effects(version, data, 1, EffectMaxId, false),
      Missiles(version, data, 1, MissileMaxId, false) {
}

const EntityType &TypeFile::GetItem(uint16_t id) const {
    auto it = Items.Entities.find(id);

    if (it != Items.Entities.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const EntityType &TypeFile::GetOutfit(uint16_t id) const {
    auto it = Outfits.Entities.find(id);

    if (it != Outfits.Entities.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const EntityType &TypeFile::GetEffect(uint16_t id) const {
    auto it = Effects.Entities.find(id);

    if (it != Effects.Entities.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const EntityType &TypeFile::GetMissile(uint16_t id) const {
    auto it = Missiles.Entities.find(id);

    if (it != Missiles.Entities.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

}; // namespace trc
