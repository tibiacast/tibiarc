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

struct trc_recording_tmv2 {
    struct trc_recording Base;

    uint32_t FirstPacketTimestamp;
    uint32_t PacketNumber;
    uint32_t PacketCount;

    struct trc_data_reader Reader;
    uint8_t *Uncompressed;
};

static bool tmv2_ProcessNextPacket(struct trc_recording_tmv2 *recording,
                                   struct trc_game_state *gamestate) {
    uint16_t outerLength, innerLength;
    uint32_t timestamp;

    if (recording->PacketNumber == recording->PacketCount) {
        recording->Base.HasReachedEnd = 1;
        return true;
    }

    if (!datareader_ReadU16(&recording->Reader, &outerLength)) {
        return trc_ReportError("Could not read outer packet length");
    }

    if (!datareader_ReadU32(&recording->Reader, &timestamp)) {
        return trc_ReportError("Could not read packet timestamp");
    }

    /* The timestamp refers to the current packet, not the next one, but I
     * figured I'd cheat since I've never seen a file other than
     * `recordings/sample.tmv2` in the wild. */
    recording->Base.NextPacketTimestamp =
            timestamp - recording->FirstPacketTimestamp;
    recording->PacketNumber++;

    if (!datareader_ReadU16(&recording->Reader, &innerLength)) {
        return trc_ReportError("Could not read inner packet length");
    }

    if (outerLength != (innerLength + 2)) {
        return trc_ReportError("Corrupt packet length");
    }

    struct trc_data_reader packetReader;
    if (!datareader_Slice(&recording->Reader, innerLength, &packetReader)) {
        return trc_ReportError("Could not read packet data");
    }

    while (datareader_Remaining(&packetReader) > 0) {
        if (!parser_ParsePacket(&packetReader, gamestate)) {
            return trc_ReportError("Failed to parse Tibia data packet");
        }
    }

    return true;
}

static bool tmv2_QueryTibiaVersion(const struct trc_data_reader *file,
                                   int *major,
                                   int *minor,
                                   int *preview) {
    struct trc_data_reader reader = *file;

    if (!datareader_Skip(&reader, 10)) {
        return trc_ReportError("Failed to skip header prologue");
    }

    uint8_t tibiaVersion[3];
    if (!datareader_ReadRaw(&reader, 3, tibiaVersion)) {
        return trc_ReportError("Failed to read Tibia version");
    }

    *major = tibiaVersion[0];
    *minor = tibiaVersion[1] * 10 + tibiaVersion[2];
    *preview = 0;

    if (!(CHECK_RANGE(*major, 7, 12) && CHECK_RANGE(*minor, 0, 99))) {
        return trc_ReportError("Invalid Tibia version");
    }

    return true;
}

static bool tmv2_DetermineLength(struct trc_recording_tmv2 *recording,
                                 uint32_t packetCount) {
    struct trc_data_reader reader = recording->Reader;
    uint32_t minTimestamp = UINT32_MAX, maxTimestamp = 0;

    while (--packetCount) {
        uint16_t packetLength;
        uint32_t timestamp;

        if (!datareader_ReadU16(&reader, &packetLength) ||
            !datareader_ReadU32(&reader, &timestamp) ||
            !datareader_Skip(&reader, packetLength)) {
            return trc_ReportError("Failed to read timestamp or skip payload");
        }

        minTimestamp = MIN(minTimestamp, timestamp);
        maxTimestamp = MAX(maxTimestamp, timestamp);

        if (minTimestamp > maxTimestamp) {
            return trc_ReportError("Invalid timestamp in recording");
        }
    }

    recording->Base.Runtime = maxTimestamp - minTimestamp;
    recording->FirstPacketTimestamp = minTimestamp;

    return true;
}

static bool tmv2_Open(struct trc_recording_tmv2 *recording,
                      const struct trc_data_reader *file,
                      struct trc_version *version) {
    struct trc_data_reader reader = *file;

    uint32_t magic, options, packetCount;
    uint16_t containerVersion;
    size_t decompressedSize;

    if (!datareader_ReadU32(&reader, &magic)) {
        return trc_ReportError("Failed to skip magic");
    }

    if (magic != 0x32564D54) {
        return trc_ReportError("Unknown file magic %x", magic);
    }

    if (!datareader_ReadU32(&reader, &options)) {
        return trc_ReportError("Failed to read options");
    }

    recording->Compressed = options & 1;

    if (!datareader_ReadU16(&reader, &containerVersion)) {
        return trc_ReportError("Failed to read container version");
    }

    if (containerVersion != 1) {
        return trc_ReportError("Unknown file version %u", containerVersion);
    }

    if (!datareader_Skip(&reader, 3)) {
        return trc_ReportError("Failed to skip Tibia version");
    }

    if (!datareader_Skip(&reader, 4)) {
        return trc_ReportError("Failed to skip record creation time");
    }

    if (!datareader_ReadU32(&reader, &packetCount)) {
        return trc_ReportError("Failed to read packet count");
    }

    if (!datareader_Skip(&reader, 4)) {
        return trc_ReportError("Failed to skip broken timestamp");
    }

    if (!datareader_ReadU32(&reader, &decompressedSize)) {
        return trc_ReportError("Failed to read decompressed size");
    }

    if (recording->Compressed) {
        /* Will be deallocated in tmv2_Free when set. */
        recording->Uncompressed = checked_allocate(1, decompressedSize);

        decompressedSize =
                tinfl_decompress_mem_to_mem(recording->Uncompressed,
                                            decompressedSize,
                                            datareader_RawData(&reader),
                                            datareader_Remaining(&reader),
                                            0);

        if (decompressedSize == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
            return trc_ReportError("Could not decompress recording");
        }

        recording->Reader.Data = recording->Uncompressed;
        recording->Reader.Length = decompressedSize;
        recording->Reader.Position = 0;
    } else {
        recording->Reader = reader;
    }

    recording->PacketCount = packetCount;

    if (!tmv2_DetermineLength(recording, packetCount)) {
        return trc_ReportError("Failed to determine length of recording");
    }

    recording->Base.Version = version;

    return true;
}

static void tmv2_Free(struct trc_recording_tmv2 *recording) {
    checked_deallocate(recording->Uncompressed);
    checked_deallocate(recording);
}

struct trc_recording *tmv2_Create() {
    struct trc_recording_tmv2 *recording = (struct trc_recording_tmv2 *)
            checked_allocate(1, sizeof(struct trc_recording_tmv2));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    tmv2_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))tmv2_Open;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))tmv2_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))tmv2_Free;

    return (struct trc_recording *)recording;
}
