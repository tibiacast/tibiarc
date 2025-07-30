/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2025 "John Högberg"
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

#include "recordings.hpp"
#include "versions.hpp"
#include "demuxer.hpp"

#ifndef DISABLE_ZLIB
extern "C" {
#    include <zlib.h>
}
#endif

#include "utils.hpp"

#include "parser.hpp"

namespace trc {
namespace Recordings {
namespace TibiaMovie1 {
bool QueryTibiaVersion([[maybe_unused]] const DataReader &file,
                       [[maybe_unused]] VersionTriplet &triplet) {
#ifndef DISABLE_ZLIB
    uint8_t buffer[4];
    uLongf bufferSize;
    int result;

    bufferSize = sizeof(buffer);
    result = uncompress((Bytef *)buffer,
                        &bufferSize,
                        file.RawData(),
                        file.Remaining());

    if (result == Z_MEM_ERROR && bufferSize == 0) {
        DataReader reader(sizeof(buffer), buffer);

        /* Container version, must be 2. */
        reader.ReadU16<2, 2>();
        auto tibiaVersion = reader.ReadU16();

        triplet.Major = tibiaVersion / 100;
        triplet.Minor = tibiaVersion % 100;
        triplet.Preview = 0;

        return triplet.Major >= 7 && triplet.Major <= 12 && triplet.Minor <= 99;
    }
#endif

    return false;
}

#ifndef DISABLE_ZLIB
static std::unique_ptr<uint8_t[]> Uncompress(const DataReader &reader,
                                             size_t &size) {
    z_stream stream;
    int error;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    AbortUnless(inflateInit2(&stream, 31) == Z_OK);

    stream.next_in = (Bytef *)reader.RawData();
    stream.avail_in = reader.Remaining();
    size = 0;

    /* The TibiaMovie1 format lacks a size marker, determine actual size by
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
        throw InvalidDataError();
    }

    if (error != Z_STREAM_END) {
        throw InvalidDataError();
    }

    /* We successfully determined the size and can move on with
     * decompression, it should not be possible to fail from here on. */
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;

    AbortUnless(inflateInit2(&stream, 31) == Z_OK);

    auto buffer = std::make_unique<uint8_t[]>(size);

    stream.next_in = (Bytef *)reader.RawData();
    stream.avail_in = reader.Remaining();
    stream.next_out = (Bytef *)buffer.get();
    stream.avail_out = size;

    AbortUnless(inflate(&stream, Z_FINISH) == Z_STREAM_END);
    AbortUnless(stream.avail_in == 0 && stream.avail_out == 0);
    AbortUnless(inflateEnd(&stream) == Z_OK);

    return buffer;
}
#endif

std::pair<std::unique_ptr<Recording>, bool> Read(
        [[maybe_unused]] const DataReader &file,
        [[maybe_unused]] const Version &version,
        [[maybe_unused]] Recovery recovery) {
#ifdef DISABLE_ZLIB
    throw NotSupportedError();
#else
    size_t decompressedSize;
    auto buffer = Uncompress(file, decompressedSize);

    DataReader reader(decompressedSize, buffer.get());

    /* Container version. */
    reader.SkipU16();
    /* Tibia version. */
    reader.SkipU16();

    auto recording = std::make_unique<Recording>();
    bool partialReturn = false;

    recording->Runtime = std::chrono::milliseconds(reader.ReadU32());

    try {
        Parser parser(version, recovery == Recovery::Repair);
        Demuxer demuxer(2);

        std::chrono::milliseconds frameTime(0);
        while (reader.Remaining() > 0) {
            if (reader.ReadU8<0, 1>() == 0) {
                auto frameDelay = std::chrono::milliseconds(reader.ReadU32());

                DataReader frameReader = reader.Slice(reader.ReadU16());
                demuxer.Submit(frameTime,
                               frameReader,
                               [&](DataReader packetReader, auto timestamp) {
                                   recording->Frames.emplace_back(
                                           timestamp,
                                           parser.Parse(packetReader));
                               });

                frameTime += frameDelay;
            }
        }

        if (recording->Frames.empty()) {
            throw InvalidDataError();
        }
    } catch ([[maybe_unused]] const InvalidDataError &e) {
        partialReturn = true;
    }

    recording->Runtime =
            std::max(recording->Runtime, recording->Frames.back().Timestamp);

    return std::make_pair(std::move(recording), partialReturn);
#endif
}

} // namespace TibiaMovie1
} // namespace Recordings
} // namespace trc
