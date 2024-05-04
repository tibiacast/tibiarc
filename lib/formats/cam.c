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
#include "demuxer.h"

#include "deps/7z/Precomp.h"
#include "deps/7z/Alloc.h"
#include "deps/7z/LzmaDec.h"

#include "utils.h"

struct trc_recording_cam {
    struct trc_recording Base;
    struct trc_packet *Packets, *Next;
};

static bool cam_ProcessNextPacket(struct trc_recording_cam *recording,
                                  struct trc_game_state *gamestate) {
    struct trc_packet *packet = recording->Next;
    struct trc_data_reader reader = {.Length = packet->Length,
                                     .Data = packet->Data};

    while (datareader_Remaining(&reader) > 0) {
        if (!parser_ParsePacket(&reader, gamestate)) {
            return trc_ReportError("Failed to parse Tibia data packet");
        }
    }

    if (packet->Next != NULL) {
        packet = packet->Next;

        recording->Base.NextPacketTimestamp = packet->Timestamp;
        recording->Next = packet;
        return true;
    }

    recording->Base.HasReachedEnd = true;
    return true;
}

static bool cam_QueryTibiaVersion(const struct trc_data_reader *file,
                                  int *major,
                                  int *minor,
                                  int *preview) {
    struct trc_data_reader reader = *file;
    uint8_t version[4];

    if (!datareader_Skip(&reader, 32)) {
        return trc_ReportError("Could not skip header");
    }

    if (!datareader_ReadRaw(&reader, 4, version)) {
        return trc_ReportError("Could not read Tibia version");
    }

    *major = version[0];
    *minor = version[1] * 10 + version[2];
    *preview = 0;

    if (!(CHECK_RANGE(*major, 7, 12))) {
        return trc_ReportError("Invalid Tibia version");
    }

    return true;
}

static bool cam_Consolidate(struct trc_recording_cam *recording,
                            struct trc_demuxer *demuxer,
                            uint8_t lzmaProperties[5],
                            size_t compressedSize,
                            const uint8_t *compressedData,
                            size_t decompressedSize,
                            uint8_t *decompressedData) {
    struct trc_data_reader reader = {.Data = decompressedData};
    SRes result;

    do {
        size_t in_size, out_size;
        ELzmaStatus status;

        out_size = decompressedSize - reader.Length;
        in_size = compressedSize;

        result = LzmaDecode((Byte *)&decompressedData[reader.Length],
                            &out_size,
                            (const Byte *)compressedData,
                            &in_size,
                            (Byte *)lzmaProperties,
                            5,
                            LZMA_FINISH_ANY,
                            &status,
                            &g_Alloc);

        ABORT_UNLESS(out_size <= (decompressedSize - reader.Length));
        ABORT_UNLESS(in_size <= compressedSize);

        if (result == SZ_OK || result == SZ_ERROR_INPUT_EOF) {
            compressedData += in_size;
            compressedSize -= in_size;
            reader.Length += out_size;
        }
    } while (result == SZ_ERROR_INPUT_EOF);

    if (result) {
        return trc_ReportError("Failed to decompress data");
    }

    if (!datareader_Skip(&reader, 2)) {
        return trc_ReportError("Failed to skip bogus container version");
    }

    int32_t frameCount;

    if (!datareader_ReadI32(&reader, &frameCount)) {
        return trc_ReportError("Failed to read frame count");
    }

    if (frameCount <= 57) {
        return trc_ReportError("Invalid frame count");
    }

    frameCount -= 57;

    for (int32_t i = 0; i < frameCount; i++) {
        struct trc_data_reader frameReader;
        uint32_t frameTimestamp;
        uint16_t frameLength;

        if (!datareader_ReadU16(&reader, &frameLength)) {
            return trc_ReportError("Failed to read frame length");
        }

        if (!datareader_ReadU32(&reader, &frameTimestamp)) {
            return trc_ReportError("Failed to read frame timestamp");
        }

        if (!datareader_Slice(&reader, frameLength, &frameReader)) {
            return trc_ReportError("Failed to slice frame data");
        }

        if (!demuxer_Submit(demuxer, frameTimestamp, &frameReader)) {
            return trc_ReportError("Failed to demux frame");
        }

        if (!datareader_Skip(&reader, 4)) {
            return trc_ReportError("Failed to skip frame checksum");
        }
    }

    if (!demuxer_Finish(demuxer,
                        &recording->Base.Runtime,
                        &recording->Packets)) {
        return trc_ReportError("Failed to finish demuxing");
    }

    return true;
}

static void cam_Rewind(struct trc_recording_cam *recording) {
    recording->Next = recording->Packets;
    recording->Base.NextPacketTimestamp = (recording->Next)->Timestamp;
}

static bool cam_Open(struct trc_recording_cam *recording,
                     const struct trc_data_reader *file,
                     struct trc_version *version) {
    struct trc_data_reader reader = *file;

    uint8_t lzmaProperties[5];
    size_t decompressedSize;
    size_t compressedSize;
    uint32_t metaLength;

    if (!datareader_Skip(&reader, 32)) {
        return trc_ReportError("Could not skip header");
    }

    if (!datareader_Skip(&reader, 4)) {
        return trc_ReportError("Could not skip Tibia version");
    }

    if (!datareader_ReadU32(&reader, &metaLength)) {
        return trc_ReportError("Could not read metadata length");
    }

    if (!datareader_Skip(&reader, metaLength)) {
        return trc_ReportError("Could not skip metadata");
    }

    if (!datareader_ReadU32(&reader, &compressedSize)) {
        return trc_ReportError("Could not read compressed size");
    }

    if (!datareader_ReadRaw(&reader, 5, lzmaProperties)) {
        return trc_ReportError("Could not read LZMA properties");
    }

    if (!datareader_ReadU64(&reader, &decompressedSize)) {
        return trc_ReportError("Could not read decompressed size");
    }

    struct trc_demuxer demuxer;
    uint8_t *decompressedData;
    bool success;

    demuxer_Initialize(&demuxer, 2);
    decompressedData = checked_allocate(1, decompressedSize);
    success = cam_Consolidate(recording,
                              &demuxer,
                              lzmaProperties,
                              compressedSize,
                              &reader.Data[reader.Position],
                              decompressedSize,
                              decompressedData);
    checked_deallocate(decompressedData);
    demuxer_Free(&demuxer);

    if (!success) {
        return trc_ReportError("Failed to consolidate recording");
    }

    recording->Base.Version = version;
    recording->Base.HasReachedEnd = 0;
    cam_Rewind(recording);

    return true;
}

static void cam_Free(struct trc_recording_cam *recording) {
    struct trc_packet *iterator = recording->Packets;

    while (iterator != NULL) {
        struct trc_packet *prev = iterator;
        iterator = iterator->Next;
        packet_Free(prev);
    }

    checked_deallocate(recording);
}

struct trc_recording *cam_Create() {
    struct trc_recording_cam *recording = (struct trc_recording_cam *)
            checked_allocate(1, sizeof(struct trc_recording_cam));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    cam_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))cam_Open;
    recording->Base.Rewind = (void (*)(struct trc_recording *))cam_Rewind;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))cam_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))cam_Free;

    return (struct trc_recording *)recording;
}
