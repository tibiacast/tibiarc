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

#include "icons.hpp"

#include "canvas.hpp"
#include "pictures.hpp"
#include "player.hpp"
#include "versions.hpp"

#include "utils.hpp"

#include <type_traits>
#include <bit>

namespace trc {

const Sprite &Icons::GetCharacterSkull(CharacterSkull skull) const {
    auto it = CharacterSkullSprites.find(skull);

    if (it != CharacterSkullSprites.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const Sprite &Icons::GetCreatureType(CreatureType type) const {
    auto it = CreatureTypeSprites.find(type);

    if (it != CreatureTypeSprites.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const Sprite &Icons::GetIconBarSkull(CharacterSkull skull) const {
    auto it = CharacterSkullSprites.find(skull);

    if (it != CharacterSkullSprites.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const Sprite &Icons::GetStatusIcon(StatusIcon status) const {
    auto it = StatusIconSprites.find(status);

    if (it != StatusIconSprites.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const Sprite &Icons::GetInventorySlot(InventorySlot slot) const {
    auto it = InventorySlotSprites.find(slot);

    if (it != InventorySlotSprites.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const Sprite &Icons::GetPartyShield(PartyShield shield) const {
    auto it = PartyShieldSprites.find(shield);

    if (it != PartyShieldSprites.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const Sprite &Icons::GetWarIcon(WarIcon war) const {
    auto it = WarIconSprites.find(war);

    if (it != WarIconSprites.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

/* As icons never move between versions (they are only added, removed, or
 * replaced) we can simplify things by including the coordinates of all
 * known icons in a single unversioned table.
 *
 * We can safely ignore icons that are out of bounds or have changed
 * appearance as they won't appear in the recording we're converting, and the
 * Sprite constructor will, by design, silently ignore sprites that are outside
 * the icon Canvas. */
Icons::Icons(const Version &version)
    : Canvas(version.Pictures.Get(PictureIndex::Icons)),
      ClientBackground(Canvas, 0, 0, 96, 96),
      EmptyStatusBar(Canvas, 96, 64, 90, 11),
      HealthBar(Canvas, 96, 75, 90, 11),
      HealthIcon(Canvas, 220, 76, 11, 11),
      IconBarBackground(Canvas, 98, 240, 108, 13),
      IconBarWar(Canvas, 251, 218, 9, 9),
      InventoryBackground(Canvas, 186, 64, 34, 34),
      ManaBar(Canvas, 96, 86, 90, 11),
      ManaIcon(Canvas, 220, 87, 11, 11),
      RiskyIcon(Canvas, 230, 218, 11, 11),
      SecondaryStatBackground(Canvas, 315, 32, 34, 21) {
    /* This is a bit ugly, but the initializer-list version of the
     * unordered_map constructor requires values to be copyable, which Sprite
     * is not. */
    {
        auto &s = CharacterSkullSprites;
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Green),
                  std::forward_as_tuple(Canvas, 54, 225, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Yellow),
                  std::forward_as_tuple(Canvas, 65, 225, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::White),
                  std::forward_as_tuple(Canvas, 76, 225, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Red),
                  std::forward_as_tuple(Canvas, 87, 225, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Black),
                  std::forward_as_tuple(Canvas, 98, 207, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Orange),
                  std::forward_as_tuple(Canvas, 208, 218, 11, 11));
    }
    {
        auto &s = IconBarSkullSprites;
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Green),
                  std::forward_as_tuple(Canvas, 279, 50, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Yellow),
                  std::forward_as_tuple(Canvas, 288, 50, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::White),
                  std::forward_as_tuple(Canvas, 297, 50, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Red),
                  std::forward_as_tuple(Canvas, 306, 50, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Black),
                  std::forward_as_tuple(Canvas, 342, 200, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CharacterSkull::Orange),
                  std::forward_as_tuple(Canvas, 242, 218, 9, 9));
    }
    {
        auto &s = CreatureTypeSprites;
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CreatureType::SummonOwn),
                  std::forward_as_tuple(Canvas, 220, 229, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(CreatureType::SummonOthers),
                  std::forward_as_tuple(Canvas, 220, 240, 11, 11));
    }
    {
        auto &s = InventorySlotSprites;
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Amulet),
                  std::forward_as_tuple(Canvas, 96, 0, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Head),
                  std::forward_as_tuple(Canvas, 128, 0, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Backpack),
                  std::forward_as_tuple(Canvas, 160, 0, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::LeftArm),
                  std::forward_as_tuple(Canvas, 192, 0, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::RightArm),
                  std::forward_as_tuple(Canvas, 224, 0, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Chest),
                  std::forward_as_tuple(Canvas, 96, 32, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Legs),
                  std::forward_as_tuple(Canvas, 128, 32, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Ring),
                  std::forward_as_tuple(Canvas, 160, 32, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Quiver),
                  std::forward_as_tuple(Canvas, 192, 32, 32, 32));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(InventorySlot::Boots),
                  std::forward_as_tuple(Canvas, 224, 32, 32, 32));
    }
    {
        auto &s = PartyShieldSprites;
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::Yellow),
                  std::forward_as_tuple(Canvas, 54, 236, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::Blue),
                  std::forward_as_tuple(Canvas, 65, 236, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::WhiteYellow),
                  std::forward_as_tuple(Canvas, 76, 236, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::WhiteBlue),
                  std::forward_as_tuple(Canvas, 87, 236, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::YellowSharedExp),
                  std::forward_as_tuple(Canvas, 76, 214, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::BlueSharedExp),
                  std::forward_as_tuple(Canvas, 87, 214, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::YellowNoSharedExpBlink),
                  std::forward_as_tuple(Canvas, 168, 261, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::YellowNoSharedExp),
                  std::forward_as_tuple(Canvas, 168, 261, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::BlueNoSharedExpBlink),
                  std::forward_as_tuple(Canvas, 179, 261, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::BlueNoSharedExp),
                  std::forward_as_tuple(Canvas, 179, 261, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(PartyShield::Gray),
                  std::forward_as_tuple(Canvas, 43, 236, 11, 11));
    }
    {
        auto &s = StatusIconSprites;
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Poison),
                  std::forward_as_tuple(Canvas, 279, 32, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Burn),
                  std::forward_as_tuple(Canvas, 288, 32, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Energy),
                  std::forward_as_tuple(Canvas, 297, 32, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Swords),
                  std::forward_as_tuple(Canvas, 306, 32, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Drunk),
                  std::forward_as_tuple(Canvas, 279, 41, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::ManaShield),
                  std::forward_as_tuple(Canvas, 288, 41, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Haste),
                  std::forward_as_tuple(Canvas, 297, 41, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Paralyze),
                  std::forward_as_tuple(Canvas, 306, 41, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Drowning),
                  std::forward_as_tuple(Canvas, 279, 59, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Freezing),
                  std::forward_as_tuple(Canvas, 279, 68, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Dazzled),
                  std::forward_as_tuple(Canvas, 279, 77, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Cursed),
                  std::forward_as_tuple(Canvas, 279, 86, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::PartyBuff),
                  std::forward_as_tuple(Canvas, 307, 148, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::PZBlock),
                  std::forward_as_tuple(Canvas, 310, 191, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::PZ),
                  std::forward_as_tuple(Canvas, 310, 182, 9, 9));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(StatusIcon::Bleeding),
                  std::forward_as_tuple(Canvas, 322, 0, 9, 9));
    }
    {
        auto &s = WarIconSprites;
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(WarIcon::Ally),
                  std::forward_as_tuple(Canvas, 287, 218, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(WarIcon::Enemy),
                  std::forward_as_tuple(Canvas, 298, 218, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(WarIcon::Neutral),
                  std::forward_as_tuple(Canvas, 309, 218, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(WarIcon::Member),
                  std::forward_as_tuple(Canvas, 219, 218, 11, 11));
        s.emplace(std::piecewise_construct,
                  std::forward_as_tuple(WarIcon::Other),
                  std::forward_as_tuple(Canvas, 276, 218, 11, 11));
    }
}

} // namespace trc
