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
#include "versions.h"

#include "utils.h"

struct trc_recording_ttm {
    struct trc_recording Base;

    struct trc_data_reader InitialReader;
    struct trc_data_reader Reader;
};

static bool ttm_ProcessNextPacket(struct trc_recording_ttm *recording,
                                  struct trc_game_state *gamestate) {
    uint16_t packetLength;
    if (!datareader_ReadU16(&recording->Reader, &packetLength)) {
        return trc_ReportError("Could not read packet length");
    }

    struct trc_data_reader packetReader;
    if (!datareader_Slice(&recording->Reader, packetLength, &packetReader)) {
        return trc_ReportError("Bad packet length");
    }

    while (datareader_Remaining(&packetReader) > 0) {
        if (!parser_ParsePacket(&packetReader, gamestate)) {
            return trc_ReportError("Failed to parse Tibia data packet");
        }
    }

    if (datareader_Remaining(&recording->Reader) == 0) {
        recording->Base.HasReachedEnd = true;
    } else {
        uint16_t packetDelay;
        uint8_t packetType;

        if (!datareader_ReadU8(&recording->Reader, &packetType)) {
            return trc_ReportError("Could not read packet type");
        }

        switch (packetType) {
        case 0:
            if (!datareader_ReadU16(&recording->Reader, &packetDelay)) {
                return trc_ReportError("Could not read packet delay");
            }
            recording->Base.NextPacketTimestamp += packetDelay;
            break;
        case 1:
            recording->Base.NextPacketTimestamp += 1000;
            break;
        default:
            return trc_ReportError("Invalid packet type");
        }
    }

    return true;
}

static bool ttm_QueryTibiaVersion(const struct trc_data_reader *file,
                                  int *major,
                                  int *minor,
                                  int *preview) {
    struct trc_data_reader reader = *file;
    uint16_t tibiaVersion;

    if (!datareader_ReadU16(&reader, &tibiaVersion)) {
        return trc_ReportError("Failed to read Tibia version");
    }

    *major = tibiaVersion / 100;
    *minor = tibiaVersion % 100;
    *preview = 0;

    if (!(CHECK_RANGE(*major, 7, 12))) {
        return trc_ReportError("Invalid Tibia version");
    }

    return true;
}

static bool ttm_Open(struct trc_recording_ttm *recording,
                     const struct trc_data_reader *file,
                     struct trc_version *version) {
    struct trc_data_reader reader = *file;

    uint32_t runtime, frames;
    uint8_t serverLength;
    uint16_t magic;

    if (!datareader_Skip(&reader, 2)) {
        return trc_ReportError("Failed to skip Tibia version");
    }

    if (!datareader_ReadU8(&reader, &serverLength)) {
        return trc_ReportError("Failed to read server name length");
    }

    if (!datareader_Skip(&reader, serverLength)) {
        return trc_ReportError("Failed to skip server name");
    }

    if (!datareader_ReadU32(&reader, &runtime)) {
        return trc_ReportError("Failed to read runtime");
    }

    recording->Base.Version = version;
    recording->Base.Runtime = runtime;
    recording->Base.NextPacketTimestamp = 0;
    recording->Base.HasReachedEnd = false;

    recording->InitialReader = reader;
    recording->Reader = reader;

    return true;
}

static void ttm_Rewind(struct trc_recording_ttm *recording) {
    recording->Reader = recording->InitialReader;
    recording->Base.NextPacketTimestamp = 0;
}

static void ttm_Free(struct trc_recording_ttm *recording) {
    checked_deallocate(recording);
}

struct trc_recording *ttm_Create() {
    struct trc_recording_ttm *recording = (struct trc_recording_ttm *)
            checked_allocate(1, sizeof(struct trc_recording_ttm));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    ttm_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))ttm_Open;
    recording->Base.Rewind = (void (*)(struct trc_recording *))ttm_Rewind;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))ttm_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))ttm_Free;

    return (struct trc_recording *)recording;
}
