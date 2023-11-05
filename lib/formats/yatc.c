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

struct trc_recording_yatc {
    struct trc_recording Base;
    struct trc_data_reader RawReader;
};

static bool yatc_HandleTibiaData(struct trc_recording_yatc *recording,
                                 struct trc_game_state *gamestate) {
    uint16_t subpacketLength;

    if (!datareader_ReadU16(&recording->RawReader, &subpacketLength)) {
        return trc_ReportError("Read failed on subpacketLength");
    }

    struct trc_data_reader packetReader;
    if (!datareader_Slice(&recording->RawReader,
                          subpacketLength,
                          &packetReader)) {
        return trc_ReportError("Bad sub-packet length");
    }

    if (datareader_Remaining(&recording->RawReader) == 0) {
        recording->Base.HasReachedEnd = true;
    } else if (!datareader_ReadU32(&recording->RawReader,
                                   &recording->Base.NextPacketTimestamp)) {
        return trc_ReportError("Failed to read next timestamp");
    }

    while (datareader_Remaining(&packetReader) > 0) {
        if (!parser_ParsePacket(&packetReader, gamestate)) {
            return trc_ReportError("Failed to parse Tibia data packet");
        }
    }

    return true;
}

static bool yatc_ProcessNextPacket(struct trc_recording_yatc *recording,
                                   struct trc_game_state *gamestate) {
    if (!recording->Base.HasReachedEnd) {
        return yatc_HandleTibiaData(recording, gamestate);
    }

    return true;
}

static bool yatc_QueryTibiaVersion(const struct trc_data_reader *file,
                                   int *major,
                                   int *minor,
                                   int *preview) {
    (void)file;
    (void)major;
    (void)minor;
    (void)preview;

    return trc_ReportError("YATC captures don't store Tibia version");
}

static int yatc_Open(struct trc_recording_yatc *recording,
                     const struct trc_data_reader *file,
                     struct trc_version *version) {
    recording->RawReader = *file;

    while (datareader_Remaining(&recording->RawReader) > 0) {
        uint16_t subpacketLength;

        if (!datareader_ReadU32(&recording->RawReader,
                                &recording->Base.Runtime)) {
            return trc_ReportError("Failed to read next timestamp");
        }

        if (!datareader_ReadU16(&recording->RawReader, &subpacketLength)) {
            return trc_ReportError("Read failed on subpacketLength");
        }

        if (!datareader_Skip(&recording->RawReader, subpacketLength)) {
            return trc_ReportError("Failed to skip data");
        }
    }

    recording->Base.HasReachedEnd = false;
    recording->Base.Version = version;
    recording->RawReader = *file;

    if (!datareader_Skip(&recording->RawReader, 4)) {
        return trc_ReportError("Failed to skip first timestamp");
    }

    return true;
}

static void yatc_Free(struct trc_recording_yatc *recording) {
    checked_deallocate(recording);
}

struct trc_recording *yatc_Create() {
    struct trc_recording_yatc *recording = (struct trc_recording_yatc *)
            checked_allocate(1, sizeof(struct trc_recording_yatc));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    yatc_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))yatc_Open;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))yatc_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))yatc_Free;

    return (struct trc_recording *)recording;
}
