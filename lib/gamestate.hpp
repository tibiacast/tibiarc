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

#ifndef __TRC_GAMESTATE_HPP__
#define __TRC_GAMESTATE_HPP__

#include <cstdint>

#include "versions_decl.hpp"

#include "container.hpp"
#include "creature.hpp"
#include "map.hpp"
#include "message.hpp"
#include "missile.hpp"
#include "player.hpp"

#include <unordered_map>

namespace trc {
struct Gamestate {
    static constexpr int MaxMissiles = 64;

    const trc::Version &Version;

    PlayerData Player;

    double SpeedA;
    double SpeedB;
    double SpeedC;

    std::unordered_map<uint32_t, Container> Containers;
    std::unordered_map<uint32_t, Creature> Creatures;
    MessageList Messages;

    /* FIXME: C++ migration. */
    unsigned MissileIndex = 0;
    std::array<Missile, MaxMissiles> MissileList = {};

    trc::Map Map;

    uint32_t CurrentTick = 0;

    Gamestate(const trc::Version &version);

    Creature &GetCreature(uint32_t id);
    const Creature &GetCreature(uint32_t id) const;

    void AddMissileEffect(const Position &origin,
                          const Position &target,
                          uint8_t missileId);
    void AddTextMessage(MessageMode messageType,
                        const std::string &message,
                        const std::string &author = std::string(),
                        const Position &position = Position());

    void Reset();
};
} // namespace trc

#endif /* __TRC_GAMESTATE_HPP__ */
