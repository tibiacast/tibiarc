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

#include "playback.h"

#include <SDL.h>

bool playback_Init(struct playback *playback,
                   const char *recording_name,
                   struct trc_data_reader *recording,
                   int major,
                   int minor,
                   int preview,
                   struct trc_data_reader *pic,
                   struct trc_data_reader *spr,
                   struct trc_data_reader *dat) {

    enum TrcRecordingFormat format =
            recording_GuessFormat(recording_name, recording);
    playback->Recording = recording_Create(format);

    if ((major | minor | preview) > 0 ||
        recording_QueryTibiaVersion(playback->Recording,
                                    recording,
                                    &major,
                                    &minor,
                                    &preview)) {
        if (version_Load(major,
                         minor,
                         preview,
                         pic,
                         spr,
                         dat,
                         &playback->Version)) {
            if (recording_Open(playback->Recording,
                               recording,
                               playback->Version)) {
                playback->Gamestate = gamestate_Create(playback->Version);
                playback->ScaleTick = SDL_GetTicks();
                playback->Scale = 1.0;
                playback->BaseTick = 0;

                /* Initialize the playback by processing the first packet
                 * (assuming that first packet has time = 0) */
                playback->Gamestate->CurrentTick = 0;
                if (recording_ProcessNextPacket(playback->Recording,
                                                playback->Gamestate)) {
                    return true;
                } else {
                    fprintf(stderr, "Could not process packet\n");
                }

                gamestate_Free(playback->Gamestate);
            } else {
                fprintf(stderr, "Could not load recording\n");
            }

            version_Free(playback->Version);
        } else {
            fprintf(stderr, "Could not load files\n");
        }
    } else {
        fprintf(stderr, "Failed to query version\n");
    }

    recording_Free(playback->Recording);
    return false;
}

void playback_Free(struct playback *playback) {
    recording_Free(playback->Recording);
    playback->Recording = NULL;

    version_Free(playback->Version);
    playback->Version = NULL;

    gamestate_Free(playback->Gamestate);
    playback->Gamestate = NULL;
}

uint32_t playback_GetPlaybackTick(const struct playback *playback) {
    return MIN(playback->BaseTick +
                       (SDL_GetTicks() - playback->ScaleTick) * playback->Scale,
               (playback->Recording)->Runtime);
}

void playback_ProcessPackets(struct playback *playback) {
    /* Process packets until we have caught up */
    uint32_t playback_tick = playback_GetPlaybackTick(playback);

    while ((playback->Recording)->NextPacketTimestamp <= playback_tick) {
        if ((playback->Recording)->HasReachedEnd) {
            break;
        }

        if (!recording_ProcessNextPacket(playback->Recording,
                                         playback->Gamestate)) {
            fprintf(stderr, "Could not process packet\n");
            abort();
        }
    }

    /* Advance the gamestate */
    playback->Gamestate->CurrentTick = playback_tick;
}

void playback_TogglePlayback(struct playback *playback) {
    if (playback->Scale > 0.0) {
        playback_SetSpeed(playback, 0.0);
        puts("Playback paused");
    } else {
        playback_SetSpeed(playback, 1.0);
        puts("Playback resumed");
    }
}

void playback_SetSpeed(struct playback *playback, float speed) {
    playback->BaseTick = playback_GetPlaybackTick(playback);
    playback->ScaleTick = SDL_GetTicks();
    playback->Scale = speed;

    printf("Speed changed to %f\n", speed);
}

void playback_Skip(struct playback *playback, int32_t by) {
    playback->BaseTick = playback_GetPlaybackTick(playback);
    playback->ScaleTick = SDL_GetTicks();

    if (by < 0) {
        recording_Rewind(playback->Recording);
        gamestate_Reset(playback->Gamestate);

        /* Initialize the playback by processing the first packet (assuming
         * that first packet has time = 0) */
        (playback->Gamestate)->CurrentTick = 0;
        if (!recording_ProcessNextPacket(playback->Recording,
                                         playback->Gamestate)) {
            fprintf(stderr, "Could not process packet\n");
            abort();
        }

        if (playback->BaseTick < -by) {
            playback->BaseTick = 0;
        } else {
            playback->BaseTick += by;
        }

        playback_ProcessPackets(playback);
    } else {
        playback->BaseTick =
                MIN(playback->BaseTick + by, (playback->Recording)->Runtime);
    }
}
