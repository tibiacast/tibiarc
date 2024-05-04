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

#ifndef __TRC_RECORDINGS_H__
#define __TRC_RECORDINGS_H__

#include <stdbool.h>

#include "datareader.h"
#include "gamestate.h"

enum TrcRecordingFormat {
    RECORDING_FORMAT_CAM,
    RECORDING_FORMAT_REC,
    RECORDING_FORMAT_TIBIACAST,
    RECORDING_FORMAT_TMV2,
    RECORDING_FORMAT_TRP,
    RECORDING_FORMAT_TTM,
    RECORDING_FORMAT_YATC,

    RECORDING_FORMAT_FIRST = RECORDING_FORMAT_CAM,
    RECORDING_FORMAT_LAST = RECORDING_FORMAT_YATC,

    RECORDING_FORMAT_UNKNOWN
};

struct trc_recording {
    struct trc_version *Version;

    uint32_t NextPacketTimestamp;
    uint32_t Runtime;
    bool HasReachedEnd;

    bool (*QueryTibiaVersion)(const struct trc_data_reader *file,
                              int *major,
                              int *minor,
                              int *preview);
    bool (*Open)(struct trc_recording *recording,
                 const struct trc_data_reader *file,
                 struct trc_version *version);
    void (*Rewind)(struct trc_recording *recording);
    bool (*ProcessNextPacket)(struct trc_recording *recording,
                              struct trc_game_state *gamestate);
    void (*Free)(struct trc_recording *recording);
};

const char *recording_FormatName(enum TrcRecordingFormat format);

enum TrcRecordingFormat recording_GuessFormat(
        const char *path,
        const struct trc_data_reader *file);

struct trc_recording *recording_Create(enum TrcRecordingFormat format);

bool recording_QueryTibiaVersion(struct trc_recording *recording,
                                 const struct trc_data_reader *file,
                                 int *major,
                                 int *minor,
                                 int *preview);

bool recording_Open(struct trc_recording *recording,
                    const struct trc_data_reader *file,
                    struct trc_version *version);
void recording_Rewind(struct trc_recording *recording);
void recording_Free(struct trc_recording *recording);

bool recording_ProcessNextPacket(struct trc_recording *recording,
                                 struct trc_game_state *gamestate);

#endif /* __TRC_RECORDINGS_H__ */
