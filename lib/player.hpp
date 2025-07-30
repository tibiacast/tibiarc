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

#ifndef __TRC_PLAYER_HPP__
#define __TRC_PLAYER_HPP__

#include <cstdint>
#include <utility>
#include <array>

#include "icons.hpp"
#include "object.hpp"
#include "utils.hpp"

#define PLAYER_SKILL_COUNT 7

namespace trc {
class PlayerData {
    std::array<Object,
               std::to_underlying(InventorySlot::Last) -
                       std::to_underlying(InventorySlot::First) + 1>
            Inventory_;

public:
    uint32_t Id = 0;
    uint16_t BeatDuration = 0;
    bool AllowBugReports = false;

    bool IsPremium = false;
    uint32_t PremiumUntil = 0;
    uint8_t Vocation = 0;

    StatusIcon Icons = static_cast<StatusIcon>(0);

    uint16_t Blessings = 0;
    uint32_t HotkeyPreset = 0;

    struct {
        int16_t Health;
        int16_t MaxHealth;

        uint32_t Capacity;
        uint32_t MaxCapacity;

        uint64_t Experience;
        double ExperienceBonus;

        uint16_t Level;
        uint8_t LevelPercent;

        int16_t Mana;
        int16_t MaxMana;

        uint8_t MagicLevel;
        uint8_t MagicLevelBase;
        uint8_t MagicLevelPercent;

        uint8_t SoulPoints;
        uint16_t Stamina;

        int16_t Speed;

        uint16_t Fed;

        uint16_t OfflineStamina;
    } Stats = {};

    struct {
        uint16_t Effective;
        uint16_t Actual;
        uint8_t Percent;
    } Skills[PLAYER_SKILL_COUNT] = {};

    uint8_t AttackMode = 0;
    uint8_t ChaseMode = 0;
    uint8_t SecureMode = 0;
    uint8_t PvPMode = 0;

    uint8_t OpenPvPSituations = 0;

    struct {
        uint8_t ProgressDay;
        uint8_t KillsRemainingDay;
        uint8_t ProgressWeek;
        uint8_t KillsRemainingWeek;
        uint8_t ProgressMonth;
        uint8_t KillsRemainingMonth;
        uint8_t SkullDuration;
    } UnjustifiedKillsInfo = {};

    Object &Inventory(InventorySlot slot) {
        return Inventory_[std::to_underlying(slot) - 1];
    }
};
} // namespace trc

#endif /* __TRC_PLAYER_HPP__ */
