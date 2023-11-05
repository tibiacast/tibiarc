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

    bool Compressed;

    uint32_t FirstPacketTimestamp;
    uint32_t PacketNumber;
    uint32_t PacketCount;

    struct trc_data_reader RawReader;
    struct trc_data_reader DataReader;
    uint8_t DataBuffer[128 << 10];

    tinfl_decompressor Decompressor;
    size_t DecompressedWritePos;
    size_t DecompressedReadPos;
    uint8_t DecompressionBuffer[128 << 10];
};

static bool tmv2_Decompress(struct trc_recording_tmv2 *recording,
                            size_t desiredCount,
                            void *buffer) {
    if (recording->Compressed) {
        while (recording->DecompressedWritePos <
                       sizeof(recording->DecompressionBuffer) &&
               datareader_Remaining(&recording->RawReader) > 0) {
            size_t in_bytes, out_bytes;
            tinfl_status status;

            out_bytes = sizeof(recording->DecompressionBuffer) -
                        recording->DecompressedWritePos;
            in_bytes = datareader_Remaining(&recording->RawReader);

            status = tinfl_decompress(
                    &recording->Decompressor,
                    (mz_uint8 *)datareader_RawData(&recording->RawReader),
                    &in_bytes,
                    (mz_uint8 *)&recording->DecompressionBuffer[0],
                    (mz_uint8 *)&recording->DecompressionBuffer
                            [recording->DecompressedWritePos],
                    &out_bytes,
                    TINFL_FLAG_HAS_MORE_INPUT);

            ABORT_UNLESS(datareader_Remaining(&recording->RawReader) >=
                         in_bytes);
            datareader_Skip(&recording->RawReader, in_bytes);
            recording->DecompressedWritePos += out_bytes;

            if (status == TINFL_STATUS_DONE) {
                break;
            } else if (status != TINFL_STATUS_NEEDS_MORE_INPUT) {
                return trc_ReportError("Failed to decompress data");
            }
        }

        size_t availableBytes, copyBytes;

        availableBytes = recording->DecompressedWritePos -
                         recording->DecompressedReadPos;
        copyBytes = MIN(desiredCount, availableBytes);

        memcpy(buffer,
               &recording->DecompressionBuffer[recording->DecompressedReadPos],
               copyBytes);
        recording->DecompressedReadPos += copyBytes;

        if (availableBytes < desiredCount) {
            if (datareader_Remaining(&recording->RawReader) == 0) {
                return trc_ReportError("The input buffer has run dry");
            } else {
                recording->DecompressedWritePos = 0;
                recording->DecompressedReadPos = 0;

                return tmv2_Decompress(recording,
                                       desiredCount - availableBytes,
                                       &((char *)buffer)[copyBytes]);
            }
        }
    } else {
        if (!datareader_ReadRaw(&recording->RawReader, desiredCount, buffer)) {
            return trc_ReportError("The input buffer has run dry");
        }
    }

    return true;
}

static bool tmv2_DecompressPacket(struct trc_recording_tmv2 *recording) {
    uint16_t packetLength;
    uint32_t timestamp;

    if (recording->PacketNumber == recording->PacketCount) {
        recording->Base.HasReachedEnd = 1;
        return true;
    }

    if (!tmv2_Decompress(recording, 6, recording->DataBuffer)) {
        return trc_ReportError("Could not decompress packet header");
    }

    recording->DataReader.Length = 6;
    recording->DataReader.Position = 0;

    if (!datareader_ReadU16(&recording->DataReader, &packetLength)) {
        return trc_ReportError("Could not decompress packet length");
    }

    if (!datareader_ReadU32(&recording->DataReader, &timestamp)) {
        return trc_ReportError("Could not decompress packet timestamp");
    }

    /* The timestamp refers to the current packet, not the next one, but I
     * figured I'd cheat since I've never seen a file other than
     * `recordings/sample.tmv2` in the wild. */
    recording->Base.NextPacketTimestamp =
            timestamp - recording->FirstPacketTimestamp;
    recording->DataReader.Length = packetLength;
    recording->DataReader.Position = 0;
    recording->PacketNumber++;

    if (packetLength > 0) {
        if (!tmv2_Decompress(recording, packetLength, recording->DataBuffer)) {
            return trc_ReportError("Could not decompress packet data");
        }
    }

    return true;
}

static bool tmv2_HandleTibiaData(struct trc_recording_tmv2 *recording,
                                 struct trc_game_state *gamestate) {
    uint16_t subpacketLength;

    if (!datareader_ReadU16(&recording->DataReader, &subpacketLength)) {
        return trc_ReportError("Read failed on subpacketLength");
    }

    if (datareader_Remaining(&recording->DataReader) != subpacketLength) {
        return trc_ReportError("Bad sub-packet length");
    }

    while (datareader_Remaining(&recording->DataReader) > 0) {
        if (!parser_ParsePacket(&recording->DataReader, gamestate)) {
            return trc_ReportError("Failed to parse Tibia data packet");
        }
    }

    return true;
}

static bool tmv2_ProcessNextPacket(struct trc_recording_tmv2 *recording,
                                   struct trc_game_state *gamestate) {
    if (!tmv2_DecompressPacket(recording)) {
        return trc_ReportError("Could not decompress packet");
    }

    if (!recording->Base.HasReachedEnd) {
        return tmv2_HandleTibiaData(recording, gamestate);
    }

    return true;
}

static void tmv2_Rewind(struct trc_recording_tmv2 *recording,
                        const struct trc_data_reader *reader) {
    tinfl_init(&recording->Decompressor);

    recording->Base.NextPacketTimestamp = recording->FirstPacketTimestamp;
    recording->Base.HasReachedEnd = 0;

    recording->RawReader = *reader;
    recording->DecompressedWritePos = 0;
    recording->DecompressedReadPos = 0;
    recording->PacketNumber = 0;
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
    if (!(datareader_ReadU8(&reader, &tibiaVersion[0]) &&
          datareader_ReadU8(&reader, &tibiaVersion[1]) &&
          datareader_ReadU8(&reader, &tibiaVersion[2]))) {
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

static bool tmv2_Open(struct trc_recording_tmv2 *recording,
                      const struct trc_data_reader *file,
                      struct trc_version *version) {
    struct trc_data_reader reader = *file;

    uint32_t magic, options, packetCount, decompressedSize, firstTimestamp;
    uint16_t containerVersion;

    if (!datareader_ReadU32(&reader, &magic)) {
        return trc_ReportError("Failed to skip magic");
    }

    if (magic != 0x32564D54) {
        return trc_ReportError("Unknown file magic %x", magic);
    }

    if (!datareader_ReadU32(&reader, &options)) {
        return trc_ReportError("Failed to read options");
    }

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

    recording->Compressed = (options & 1);
    recording->PacketCount = packetCount;

    tmv2_Rewind(recording, &reader);

    /* Manually determine the length of the file.
     *
     * This is really ugly but the only file I could find -- sample.tmv in
     * the TibiaMovie repository -- appears to store the runtime instead of the
     * last packet timestamp as documented. Luckily, manually determining the
     * length works regardless of whether this number is correct or not. */
    if (!tmv2_DecompressPacket(recording)) {
        return trc_ReportError("Failed to determine first timestamp of file");
    }

    firstTimestamp = recording->Base.NextPacketTimestamp;

    while (--packetCount) {
        if (!tmv2_DecompressPacket(recording)) {
            return trc_ReportError("Failed to determine runtime of file");
        }
    }

    recording->Base.Runtime =
            recording->Base.NextPacketTimestamp - firstTimestamp;
    recording->FirstPacketTimestamp = firstTimestamp;
    recording->Base.Version = version;

    tmv2_Rewind(recording, &reader);

    return true;
}

static void tmv2_Free(struct trc_recording_tmv2 *recording) {
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

    recording->DataReader.Data = &recording->DataBuffer[0];

    return (struct trc_recording *)recording;
}
