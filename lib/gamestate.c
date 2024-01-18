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

#include "gamestate.h"

#include "utils.h"

void gamestate_AddMissileEffect(struct trc_game_state *gamestate,
                                struct trc_position *origin,
                                struct trc_position *target,
                                uint8_t missileId) {
    gamestate->MissileList[gamestate->MissileIndex].StartTick =
            gamestate->CurrentTick;
    gamestate->MissileList[gamestate->MissileIndex].Id = missileId;

    gamestate->MissileList[gamestate->MissileIndex].Origin = (*origin);
    gamestate->MissileList[gamestate->MissileIndex].Target = (*target);

    gamestate->MissileIndex =
            (gamestate->MissileIndex + 1) % MAX_MISSILES_IN_GAMESTATE;
}

void gamestate_AddTextMessage(struct trc_game_state *gamestate,
                              struct trc_position *position,
                              enum TrcMessageMode messageType,
                              uint16_t authorLength,
                              const char *author,
                              uint16_t messageLength,
                              const char *message) {
    messagelist_AddMessage(&gamestate->MessageList,
                           position,
                           gamestate->CurrentTick,
                           messageType,
                           authorLength,
                           author,
                           messageLength,
                           message);
}

void gamestate_Reset(struct trc_game_state *gamestate) {
    containerlist_Free(&gamestate->ContainerList);
    gamestate->ContainerList = NULL;

    creaturelist_Free(&gamestate->CreatureList);
    gamestate->CreatureList = NULL;

    messagelist_Free(&gamestate->MessageList);
    messagelist_Initialize(&gamestate->MessageList);
}

struct trc_game_state *gamestate_Create(const struct trc_version *version) {
    struct trc_game_state *gamestate = (struct trc_game_state *)
            checked_allocate(1, sizeof(struct trc_game_state));

    gamestate->Version = version;
    messagelist_Initialize(&gamestate->MessageList);

    return gamestate;
}

void gamestate_Free(struct trc_game_state *gamestate) {
    containerlist_Free(&gamestate->ContainerList);
    creaturelist_Free(&gamestate->CreatureList);
    messagelist_Free(&gamestate->MessageList);

    checked_deallocate(gamestate);
}
