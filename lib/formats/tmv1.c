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

#ifndef DISABLE_ZLIB
#    include <zlib.h>
#endif

#include "utils.h"

struct trc_recording_tmv1 {
    struct trc_recording Base;

    struct trc_packet *Packets, *Next;
};

static bool tmv1_ProcessNextPacket(struct trc_recording_tmv1 *recording,
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

static bool tmv1_QueryTibiaVersion(const struct trc_data_reader *file,
                                   int *major,
                                   int *minor,
                                   int *preview) {
#ifdef DISABLE_ZLIB
    return trc_ReportError("Failed to decompress, zlib library missing");
#else
    uint8_t buffer[4];
    uLongf bufferSize;
    int result;

    bufferSize = sizeof(buffer);
    result = uncompress((Bytef *)buffer,
                        &bufferSize,
                        datareader_RawData(file),
                        datareader_Remaining(file));

    if (result == Z_MEM_ERROR && bufferSize == 0) {
        struct trc_data_reader reader = {.Data = buffer,
                                         .Length = sizeof(buffer)};
        uint16_t containerVersion, tibiaVersion;

        if (!datareader_ReadU16(&reader, &containerVersion)) {
            return trc_ReportError("Failed to read container version");
        }

        if (containerVersion != 2) {
            return trc_ReportError("Invalid container version");
        }

        if (!datareader_ReadU16(&reader, &tibiaVersion)) {
            return trc_ReportError("Failed to read Tibia version");
        }

        *major = tibiaVersion / 100;
        *minor = tibiaVersion % 100;
        *preview = 0;

        if (!(CHECK_RANGE(*major, 7, 12) && CHECK_RANGE(*minor, 0, 99))) {
            return trc_ReportError("Invalid Tibia version");
        }

        preview = 0;

        return true;
    }

    return trc_ReportError("Failed to decompress, corrupt file?");
#endif
}

static bool tmv1_Consolidate(struct trc_recording_tmv1 *recording,
                             struct trc_data_reader *reader,
                             struct trc_demuxer *demuxer) {
    uint32_t timestamp = 0;

    if (!datareader_Skip(reader, 2)) {
        return trc_ReportError("Failed to skip container version");
    }

    if (!datareader_Skip(reader, 2)) {
        return trc_ReportError("Failed to skip Tibia version");
    }

    if (!datareader_ReadU32(reader, &recording->Base.Runtime)) {
        return trc_ReportError("Failed to read runtime");
    }

    while (datareader_Remaining(reader) > 0) {
        uint8_t frameType;

        if (!datareader_ReadU8(reader, &frameType)) {
            return trc_ReportError("Could not read frame type");
        }

        switch (frameType) {
        case 0: {
            struct trc_data_reader frameReader;
            uint32_t frameDelay, frameSize;

            if (!datareader_ReadU32(reader, &frameDelay)) {
                return trc_ReportError("Could not read frame delay");
            }

            if (!datareader_ReadU16(reader, &frameSize)) {
                return trc_ReportError("Could not read frame size");
            }

            if (!datareader_Slice(reader, frameSize, &frameReader)) {
                return trc_ReportError("Could not read frame data");
            }

            if (!demuxer_Submit(demuxer, timestamp, &frameReader)) {
                return trc_ReportError("Failed to demux frame");
            }

            timestamp += frameDelay;
            break;
        }
        case 1:
            break;
        default:
            return trc_ReportError("Invalid frame type");
        }
    }

    if (!demuxer_Finish(demuxer,
                        &recording->Base.Runtime,
                        &recording->Packets)) {
        return trc_ReportError("Failed to finish demuxing");
    }

    return true;
}

static bool tmv1_Uncompress(struct trc_recording_tmv1 *recording,
                            const struct trc_data_reader *reader,
                            size_t *length,
                            uint8_t **uncompressed) {
#ifdef DISABLE_ZLIB
    return trc_ReportError("Failed to decompress, zlib library missing");
#else
    z_stream stream;
    size_t size;
    int error;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    if (inflateInit2(&stream, 31) != Z_OK) {
        return trc_ReportError("Failed to initialize zlib");
    }

    stream.next_in = (Bytef *)datareader_RawData(reader);
    stream.avail_in = datareader_Remaining(reader);
    size = 0;

    /* The TMV1 format lacks a size marker, determine actual size by
     * decompressing until EOF. */
    do {
        do {
            Bytef outBuffer[4 << 10];

            stream.next_out = outBuffer;
            stream.avail_out = sizeof(outBuffer);

            error = inflate(&stream, Z_NO_FLUSH);

            size += sizeof(outBuffer) - stream.avail_out;
        } while (stream.avail_out == 0);
    } while (error == Z_OK);

    if (inflateEnd(&stream) != Z_OK) {
        return trc_ReportError("Internal zlib state corrupt");
    }

    if (error == Z_STREAM_END) {
        /* We successfully determined the size and can move on with
         * decompression, it should not be possible to fail from here on. */
        bool success;

        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.avail_in = 0;
        stream.next_in = Z_NULL;

        ABORT_UNLESS(inflateInit2(&stream, 31) == Z_OK);

        *length = size;
        *uncompressed = checked_allocate(1, size);

        stream.next_in = (Bytef *)datareader_RawData(reader);
        stream.avail_in = datareader_Remaining(reader);
        stream.next_out = (Bytef *)*uncompressed;
        stream.avail_out = size;

        ABORT_UNLESS(inflate(&stream, Z_FINISH) == Z_STREAM_END);
        ABORT_UNLESS(stream.avail_in == 0 && stream.avail_out == 0);
        ABORT_UNLESS(inflateEnd(&stream) == Z_OK);

        return true;
    }

    return trc_ReportError("Failed to decompress data %i", error);
#endif
}

static void tmv1_Rewind(struct trc_recording_tmv1 *recording) {
    recording->Next = recording->Packets;
    recording->Base.NextPacketTimestamp = (recording->Next)->Timestamp;
}

static bool tmv1_Open(struct trc_recording_tmv1 *recording,
                      const struct trc_data_reader *file,
                      struct trc_version *version) {
    struct trc_demuxer demuxer;
    uint8_t *uncompressed;
    size_t length;
    bool success;

    if (!tmv1_Uncompress(recording, file, &length, &uncompressed)) {
        return trc_ReportError("Failed to decompress recording");
    }

    demuxer_Initialize(&demuxer, 2);

    success = tmv1_Consolidate(
            recording,
            &(struct trc_data_reader){.Data = uncompressed, .Length = length},
            &demuxer);

    demuxer_Free(&demuxer);
    checked_deallocate(uncompressed);

    if (success) {
        recording->Base.Version = version;
        tmv1_Rewind(recording);
    }

    return success;
}

static void tmv1_Free(struct trc_recording_tmv1 *recording) {
    struct trc_packet *iterator = recording->Packets;

    while (iterator != NULL) {
        struct trc_packet *prev = iterator;
        iterator = iterator->Next;
        packet_Free(prev);
    }

    checked_deallocate(recording);
}

struct trc_recording *tmv1_Create() {
    struct trc_recording_tmv1 *recording = (struct trc_recording_tmv1 *)
            checked_allocate(1, sizeof(struct trc_recording_tmv1));

    recording->Base.QueryTibiaVersion =
            (bool (*)(const struct trc_data_reader *, int *, int *, int *))
                    tmv1_QueryTibiaVersion;
    recording->Base.Open = (bool (*)(struct trc_recording *,
                                     const struct trc_data_reader *,
                                     struct trc_version *))tmv1_Open;
    recording->Base.Rewind = (void (*)(struct trc_recording *))tmv1_Rewind;
    recording->Base.ProcessNextPacket =
            (bool (*)(struct trc_recording *,
                      struct trc_game_state *))tmv1_ProcessNextPacket;
    recording->Base.Free = (void (*)(struct trc_recording *))tmv1_Free;

    return (struct trc_recording *)recording;
}
