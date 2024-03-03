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

struct trc_recording_trp {
    struct trc_recording Base;

    uint32_t PacketNumber;
    uint32_t PacketCount;

    struct trc_data_reader InitialReader;
    struct trc_data_reader Reader;
};

static bool trp_ProcessNextPacket(struct trc_recording_trp *recording,
                                  struct trc_game_state *gamestate) {
    recording->PacketNumber++;
    recording->Base.HasReachedEnd =
            (recording->PacketNumber == recording->PacketCount);

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

    if (!recording->Base.HasReachedEnd) {
        uint32_t timestamp;
        if (!datareader_ReadU32(&recording->Reader, &timestamp)) {
            return trc_ReportError("Could not read packet timestamp");
        }

        if (timestamp < recording->Base.NextPacketTimestamp) {
            return trc_ReportError("Invalid packet timestamp");
        }

        recording->Base.NextPacketTimestamp = timestamp;
        return true;
    }

    return datareader_Remaining(&recording->Reader) == 0;
}

static bool trp_QueryTibiaVersion(const struct trc_data_reader *file,
                                  int *major,
                                  int *minor,
                                  int *preview) {
    struct trc_data_reader reader = *file;
    uint16_t magic;

    if (!datareader_ReadU16(&reader, &magic)) {
        return trc_ReportError("Failed to read magic (part 1)");
    }

    if (magic != 0x1337 && !datareader_Skip(&reader, 2)) {
        return trc_ReportError("Failed to skip magic (part 2)");
    }

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

static bool trp_Open(struct trc_recording_trp *recording,
                     const struct trc_data_reader *file,
                     struct trc_version *version) {
    struct trc_data_reader reader = *file;

    uint32_t runtime, frames;
    uint16_t magic;

    if (!datareader_ReadU16(&reader, &magic)) {
        return trc_ReportError("Failed to read magic (part 1)");
    }

    if (magic != 0x1337 && !datareader_Skip(&reader, 2)) {
        return trc_ReportError("Failed to skip magic (part 2)");
    }

    if (!datareader_Skip(&reader, 2)) {
        return trc_ReportError("Failed to skip Tibia version");
    }

    if (!datareader_ReadU32(&reader, &runtime)) {
        return trc_ReportError("Failed to read runtime");
    }

    if (!datareader_ReadU32(&reader, &frames)) {
        return trc_ReportError("Failed to read frame count");
    }

    if (!datareader_Skip(&reader, 4)) {
        return trc_ReportError("Could not skip first packet timestamp");
    }

    recording->Base.Version = version;
    recording->Base.Runtime = runtime;
    recording->Base.NextPacketTimestamp = 0;
    recording->Base.HasReachedEnd = 0;

    recording->Reader = reader;
    recording->PacketCount = frames;

    recording->InitialReader = reader;
    recording->PacketNumber = 0;

    return true;
}

static void trp_Rewind(struct trc_recording_trp *recording) {
    recording->Reader = recording->InitialReader;
    recording->PacketNumber = 0;

    recording->Base.NextPacketTimestamp = 0;
}

static void trp_Free(struct trc_recording_trp *recording) {
    checked_deallocate(recording);
}

struct trc_recording *trp_Create() {
    struct trc_recording_trp *recording = (struct trc_recording_trp *)
            checked_allocate(1, sizeof(struct trc_recording_trp));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    trp_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))trp_Open;
    recording->Base.Rewind = (void (*)(struct trc_recording *))trp_Rewind;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))trp_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))trp_Free;

    return (struct trc_recording *)recording;
}
