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

#ifndef __TRC_PLAYER_H__
#define __TRC_PLAYER_H__

#include <stdbool.h>
#include <stdint.h>

#include "icons.h"
#include "object.h"

#include "utils.h"

#define PLAYER_SKILL_COUNT 7

struct trc_player {
    uint32_t Id;
    uint16_t BeatDuration;
    bool AllowBugReports;

    bool IsPremium;
    uint32_t PremiumUntil;
    uint8_t Vocation;

    enum TrcStatusIcon Icons;

    uint16_t Blessings;
    uint32_t HotkeyPreset;

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
    } Stats;

    struct {
        uint16_t Effective;
        uint16_t Actual;
        uint8_t Percent;
    } Skills[PLAYER_SKILL_COUNT];

    uint8_t AttackMode;
    uint8_t ChaseMode;
    uint8_t SecureMode;
    uint8_t PvPMode;

    uint8_t OpenPvPSituations;

    struct {
        uint8_t ProgressDay;
        uint8_t KillsRemainingDay;
        uint8_t ProgressWeek;
        uint8_t KillsRemainingWeek;
        uint8_t ProgressMonth;
        uint8_t KillsRemainingMonth;
        uint8_t SkullDuration;
    } UnjustifiedKillsInfo;

    struct trc_object Inventory[INVENTORY_SLOT_LAST - INVENTORY_SLOT_FIRST + 1];
};

TRC_UNUSED static struct trc_object *player_GetInventoryObject(
        struct trc_player *player,
        enum TrcInventorySlot slot) {
    ASSERT(CHECK_RANGE(slot, INVENTORY_SLOT_FIRST, INVENTORY_SLOT_LAST));
    return &player->Inventory[slot];
}

TRC_UNUSED static void player_SetInventoryObject(struct trc_player *player,
                                                 enum TrcInventorySlot slot,
                                                 struct trc_object *object) {
    ASSERT(CHECK_RANGE(slot, INVENTORY_SLOT_FIRST, INVENTORY_SLOT_LAST));
    player->Inventory[slot] = (*object);
}

#endif /* __TRC_PLAYER_H__ */
