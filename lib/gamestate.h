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

#ifndef __TRC_GAMESTATE_H__
#define __TRC_GAMESTATE_H__

#include <stdint.h>

#include "versions_decl.h"

#include "container.h"
#include "creature.h"
#include "map.h"
#include "message.h"
#include "missile.h"
#include "player.h"

#define MAX_MISSILES_IN_GAMESTATE 64

struct trc_game_state {
    const struct trc_version *Version;

    struct trc_player Player;

    double SpeedA;
    double SpeedB;
    double SpeedC;

    struct trc_container *ContainerList;
    struct trc_creature *CreatureList;
    struct trc_message_list MessageList;

    unsigned MissileIndex;
    struct trc_missile MissileList[MAX_MISSILES_IN_GAMESTATE];

    struct trc_map Map;

    uint32_t CurrentTick;
};

void gamestate_AddMissileEffect(struct trc_game_state *gamestate,
                                struct trc_position *origin,
                                struct trc_position *target,
                                uint8_t missileId);
void gamestate_AddTextMessage(struct trc_game_state *gamestate,
                              struct trc_position *position,
                              enum TrcMessageMode messageType,
                              uint16_t authorLength,
                              const char *author,
                              uint16_t messageLength,
                              const char *message);

struct trc_game_state *gamestate_Create(const struct trc_version *version);
void gamestate_Free(struct trc_game_state *gamestate);

#endif /* __TRC_GAMESTATE_H__ */
