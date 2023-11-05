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

#ifndef __TRC_CREATURE_H__
#define __TRC_CREATURE_H__

#include <stdint.h>

#include "deps/uthash.h"

#include "characterset.h"
#include "icons.h"
#include "object.h"
#include "position.h"

enum TrcCreatureDirection {
    CREATURE_DIRECTION_NORTH = 0,
    CREATURE_DIRECTION_EAST = 1,
    CREATURE_DIRECTION_SOUTH = 2,
    CREATURE_DIRECTION_WEST = 3,

    CREATURE_DIRECTION_FIRST = CREATURE_DIRECTION_NORTH,
    CREATURE_DIRECTION_LAST = CREATURE_DIRECTION_WEST,
};

struct trc_outfit {
    uint16_t Id;
    uint16_t MountId;

    union {
        struct {
            uint8_t HeadColor;
            uint8_t PrimaryColor;
            uint8_t SecondaryColor;
            uint8_t DetailColor;
            uint8_t Addons;
        };
        struct trc_object Item;
    };
};

struct trc_creature {
    UT_hash_handle hh;

    struct {
        uint32_t LastUpdateTick;

        uint32_t WalkStartTick;
        uint32_t WalkEndTick;

        int WalkOffsetX;
        int WalkOffsetY;

        struct trc_position Origin;
        struct trc_position Target;
    } MovementInformation;

    uint32_t Id;

    enum TrcCreatureType Type;
    enum TrcNPCCategory NPCCategory;

    uint16_t GuildMembersOnline;
    uint8_t MarkIsPermanent;
    uint8_t Mark;

    uint8_t Health;
    enum TrcCreatureDirection Direction;

    uint8_t LightIntensity;
    uint8_t LightColor;

    int16_t Speed;

    enum TrcCharacterSkull Skull;
    enum TrcPartyShield Shield;
    enum TrcWarIcon WarIcon;

    uint8_t Impassable;

    struct trc_outfit Outfit;
    uint16_t NameLength;
    char Name[64];
};

void creaturelist_ReplaceCreature(struct trc_creature **creatureList,
                                  uint32_t newId,
                                  uint32_t oldId,
                                  struct trc_creature **result);
bool creaturelist_GetCreature(struct trc_creature **creatureList,
                              uint32_t id,
                              struct trc_creature **result);

void creaturelist_Free(struct trc_creature **creatureList);

#endif /* __TRC_CREATURE_H__ */
