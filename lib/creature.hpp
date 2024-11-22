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

#ifndef __TRC_CREATURE_HPP__
#define __TRC_CREATURE_HPP__

#include <cstdint>
#include <utility>
#include <string>

#include "object.hpp"
#include "position.hpp"

namespace trc {

enum class CharacterSkull : uint8_t {
    None = 0,

    Yellow,
    Green,
    White,
    Red,
    Black,
    Orange,

    First = None,
    Last = Orange,
};

enum class CreatureType : uint8_t {
    Player = 0,
    Monster,
    NPC,
    SummonOwn,
    SummonOthers,

    First = Player,
    Last = SummonOthers,
};

enum class NPCCategory : uint8_t {
    None = 0,

    Normal,
    Trader,
    Quest,
    TraderQuest,

    First = None,
    Last = TraderQuest,
};

enum class InventorySlot : uint8_t {
    Head = 1,
    Amulet,
    Backpack,
    Chest,
    RightArm,
    LeftArm,
    Legs,
    Boots,
    Ring,
    Quiver,
    Purse,

    First = Head,
    Last = Purse,
};

enum class PartyShield : uint8_t {
    None = 0,

    WhiteYellow,
    WhiteBlue,
    Blue,
    Yellow,
    BlueSharedExp,
    YellowSharedExp,
    BlueNoSharedExpBlink,
    YellowNoSharedExpBlink,
    BlueNoSharedExp,
    YellowNoSharedExp,
    Gray,

    First = None,
    Last = Gray,
};

enum class WarIcon : uint8_t {
    None = 0,
    Ally,
    Enemy,
    Neutral,
    Member,
    Other,

    First = None,
    Last = Other,
};

/* This is ugly: `enum class` is not particularly suitable for bit fields like
 * this but plain `enum` leaks the members into the outer scope. */
enum class StatusIcon : uint16_t {
    Poison = (1 << 0),
    Burn = (1 << 1),
    Energy = (1 << 2),
    Drunk = (1 << 3),
    ManaShield = (1 << 4),
    Paralyze = (1 << 5),
    Haste = (1 << 6),
    Swords = (1 << 7),
    Drowning = (1 << 8),
    Freezing = (1 << 9),
    Dazzled = (1 << 10),
    Cursed = (1 << 11),
    PartyBuff = (1 << 12),
    PZBlock = (1 << 13),
    PZ = (1 << 14),
    Bleeding = (1 << 15),
};

static constexpr std::initializer_list<StatusIcon> StatusIcons = {
        StatusIcon::Poison,
        StatusIcon::Burn,
        StatusIcon::Energy,
        StatusIcon::Drunk,
        StatusIcon::ManaShield,
        StatusIcon::Paralyze,
        StatusIcon::Haste,
        StatusIcon::Swords,
        StatusIcon::Drowning,
        StatusIcon::Freezing,
        StatusIcon::Dazzled,
        StatusIcon::Cursed,
        StatusIcon::PartyBuff,
        StatusIcon::PZBlock,
        StatusIcon::PZ,
        StatusIcon::Bleeding,
};

inline StatusIcon operator&(StatusIcon lhs, StatusIcon rhs) {
    return static_cast<StatusIcon>(std::to_underlying(lhs) &
                                   std::to_underlying(rhs));
}

struct Appearance {
    uint16_t Id;
    uint16_t MountId;

    uint8_t HeadColor;
    uint8_t PrimaryColor;
    uint8_t SecondaryColor;
    uint8_t DetailColor;
    uint8_t Addons;

    Object Item;
};

struct Creature {
    struct {
        uint32_t WalkStartTick;
        uint32_t WalkEndTick;

        Position Origin;
        Position Target;

        /* FIXME: C++ migration, handle this entirely in the renderer. */
        uint32_t LastUpdateTick;
        int WalkOffsetX;
        int WalkOffsetY;
    } MovementInformation;

    uint32_t Id;

    CreatureType Type;
    trc::NPCCategory NPCCategory;

    uint16_t GuildMembersOnline;
    uint8_t MarkIsPermanent;
    uint8_t Mark;

    uint8_t Health;

    enum class Direction : uint8_t {
        North = 0,
        East = 1,
        South = 2,
        West = 3,

        First = North,
        Last = West,
    } Heading;

    uint8_t LightIntensity;
    uint8_t LightColor;

    int16_t Speed;

    CharacterSkull Skull;
    PartyShield Shield;
    WarIcon War;

    uint8_t Impassable;

    Appearance Outfit;
    std::string Name;
};

}; // namespace trc

#endif /* __TRC_CREATURE_HPP__ */
