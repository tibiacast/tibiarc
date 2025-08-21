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

#include "gamestate.hpp"

#include "utils.hpp"

namespace trc {

void Gamestate::AddMissileEffect(const trc::Position &origin,
                                 const trc::Position &target,
                                 uint8_t missileId) {
    auto &missile = MissileList[MissileIndex];

    MissileIndex = (MissileIndex + 1) % MaxMissiles;

    missile.StartTick = CurrentTick;
    missile.Id = missileId;
    missile.Origin = origin;
    missile.Target = target;
}

void Gamestate::AddTextMessage(MessageMode type,
                               const std::string &message,
                               const std::string &author,
                               const Position &position) {
    Messages.AddMessage(type, position, author, message, CurrentTick);
}

void Gamestate::Reset() {
    memset((void *)&MissileList, 0, sizeof(MissileList));

    Containers.clear();
    Creatures.clear();
    Messages.Clear();
    Map.Clear();
}

Gamestate::Gamestate(const trc::Version &version) : Version(version) {
}

Creature *Gamestate::FindCreature(uint32_t id) {
    auto it = Creatures.find(id);

    if (it != Creatures.end()) {
        return &it->second;
    }

    return nullptr;
}

const Creature *Gamestate::FindCreature(uint32_t id) const {
    auto it = Creatures.find(id);

    if (it != Creatures.end()) {
        return &it->second;
    }

    return nullptr;
}

Creature &Gamestate::GetCreature(uint32_t id) {
    auto it = Creatures.find(id);

    if (it != Creatures.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

const Creature &Gamestate::GetCreature(uint32_t id) const {
    auto it = Creatures.find(id);

    if (it != Creatures.end()) {
        return it->second;
    }

    throw InvalidDataError();
}

} // namespace trc
