/*
 * Copyright 2024 "Simon Sandström"
 * Copyright 2024 "John Högberg"
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

#ifndef PLAYER_PLAYBACK_H
#define PLAYER_PLAYBACK_H

#include <stdint.h>

#include "memoryfile.h"
#include "datareader.h"
#include "versions.h"
#include "recordings.h"
#include "gamestate.h"

struct playback {
    struct trc_version *Version;
    struct trc_recording *Recording;
    struct trc_game_state *Gamestate;

    uint32_t BaseTick;
    uint32_t ScaleTick;
    float Scale;
};

/** @brief Loads the desired recording with client data into the playback
 * state.
 *
 * The data files can be freed after this call, but the recording must stay
 * alive until \c playback_Free */
bool playback_Init(struct playback *playback,
                   const char *recording_name,
                   struct trc_data_reader *recording,
                   int major,
                   int minor,
                   int preview,
                   struct trc_data_reader *pic,
                   struct trc_data_reader *spr,
                   struct trc_data_reader *dat);
void playback_Free(struct playback *playback);

uint32_t playback_GetPlaybackTick(const struct playback *playback);
void playback_ProcessPackets(struct playback *playback);
void playback_TogglePlayback(struct playback *playback);

void playback_SetSpeed(struct playback *playback, float speed);
void playback_Skip(struct playback *playback, int32_t by);

#endif /* PLAYER_PLAYBACK_H */
