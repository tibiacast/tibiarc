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

#include "recordings.h"
#include "parser.h"
#include "versions.h"

#include "utils.h"

static const struct {
    const char *ShortName;
    const char *Extension;
} FormatDescriptions[] = {
        {"rec", ".rec"},
        {"tibiacast", ".recording"},
        {"tmv2", ".tmv2"},
        {"trp", ".trp"},
        {"yatc", ".yatc"},
};

const char *recording_FormatName(enum TrcRecordingFormat format) {
    if (format >= RECORDING_FORMAT_FIRST && format <= RECORDING_FORMAT_LAST) {
        return FormatDescriptions[format].ShortName;
    }

    return "unknown";
}

enum TrcRecordingFormat recording_GuessFormat(
        const char *path,
        const struct trc_data_reader *reader) {
    const char *extension;
    uint32_t magic;

    if (datareader_PeekU32(reader, &magic)) {
        switch (magic) {
        case 0x32564D54: /* 'TMV2' */
            return RECORDING_FORMAT_TMV2;
        case 0x00505254: /* 'TRP\0' */
            return RECORDING_FORMAT_TRP;
        }

        if ((magic & 0xFFFF) == 0x1337) {
            /* Old TibiaReplay format */
            return RECORDING_FORMAT_TRP;
        }
    }

    extension = strrchr(path, '.');
    if (extension != NULL) {
        for (int guess = RECORDING_FORMAT_FIRST; guess <= RECORDING_FORMAT_LAST;
             guess++) {
            if (!strcmp(extension, FormatDescriptions[guess].Extension)) {
                return (enum TrcRecordingFormat)guess;
            }
        }
    }

    return RECORDING_FORMAT_TIBIACAST;
}

struct trc_recording *recording_Create(enum TrcRecordingFormat format) {
    extern struct trc_recording *rec_Create(void);
    extern struct trc_recording *tibiacast_Create(void);
    extern struct trc_recording *tmv2_Create(void);
    extern struct trc_recording *trp_Create(void);
    extern struct trc_recording *yatc_Create(void);

    switch (format) {
    case RECORDING_FORMAT_REC:
        return rec_Create();
    case RECORDING_FORMAT_TIBIACAST:
        return tibiacast_Create();
    case RECORDING_FORMAT_TMV2:
        return tmv2_Create();
    case RECORDING_FORMAT_TRP:
        return trp_Create();
    case RECORDING_FORMAT_YATC:
        return yatc_Create();
    default:
        abort();
    }
}

bool recording_QueryTibiaVersion(struct trc_recording *recording,
                                 const struct trc_data_reader *reader,
                                 int *major,
                                 int *minor,
                                 int *preview) {
    return recording->QueryTibiaVersion(reader, major, minor, preview);
}

bool recording_Open(struct trc_recording *recording,
                    const struct trc_data_reader *reader,
                    struct trc_version *version) {
    return recording->Open(recording, reader, version);
}

void recording_Rewind(struct trc_recording *recording) {
    recording->HasReachedEnd = false;
    recording->Rewind(recording);
}

bool recording_ProcessNextPacket(struct trc_recording *recording,
                                 struct trc_game_state *gamestate) {
    return recording->ProcessNextPacket(recording, gamestate);
}

void recording_Free(struct trc_recording *recording) {
    recording->Free(recording);
}
