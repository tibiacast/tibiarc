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

#ifndef DISABLE_ZLIB
extern "C" {
#    include <zlib.h>
}
#endif

#include "utils.hpp"

#include "parser.hpp"

namespace trc {
namespace Recordings {
namespace TibiaMovie2 {
bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet) {
    DataReader reader = file;

    /* Header prologue. */
    reader.Skip(10);

    uint8_t tibiaVersion[3];
    reader.Copy(3, tibiaVersion);

    triplet.Major = tibiaVersion[0];
    triplet.Minor = tibiaVersion[1] * 10 + tibiaVersion[2];
    triplet.Preview = 0;

    return triplet.Major >= 7 && triplet.Major <= 12 && triplet.Minor <= 99;
}

void ReadNextFrame(DataReader &reader, Parser &parser, Recording &recording) {
    auto outerLength = reader.ReadU16();
    auto timestamp = reader.ReadU32();
    auto innerLength = reader.ReadU16();

    if (outerLength != (innerLength + 2)) {
        throw InvalidDataError();
    }

    DataReader packetReader = reader.Slice(innerLength);
    auto &frame = recording.Frames.emplace_back();
    frame.Timestamp = std::chrono::milliseconds(timestamp);

    while (packetReader.Remaining() > 0) {
        frame.Events.splice(frame.Events.end(), parser.Parse(packetReader));
    }
}

std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                 const Version &version,
                                                 Recovery recovery) {
    DataReader reader = file;

    /* Magic */
    reader.SkipU32<0x32564D54, 0x32564D54>();

    /* Options bitfield, 1 means compressed */
    bool compressed = reader.ReadU32<0, 1>();

    /* Container version, must be 1 */
    reader.SkipU16<1, 1>();

    /* Tibia version */
    reader.Skip(3);

    /* Creation time (POSIX?) */
    reader.SkipU32();

    auto packetCount = reader.ReadU32();

    /* Broken timestamp field; useless */
    reader.SkipU32();

    auto decompressedSize = reader.ReadU32();

    auto recording = std::make_unique<Recording>();
    bool partialReturn = false;

    try {
        Parser parser(version, recovery == Recovery::Repair);

        if (compressed) {
#ifdef DISABLE_ZLIB
            throw NotSupportedError();
#else
            uLongf actualSize;
            int result;

            auto buffer = std::make_unique<uint8_t[]>(decompressedSize);

            result = uncompress((Bytef *)buffer.get(),
                                &actualSize,
                                reader.RawData(),
                                reader.Remaining());

            if (result != Z_OK) {
                throw InvalidDataError();
            }

            if (actualSize != decompressedSize) {
                throw InvalidDataError();
            }

            reader = DataReader(decompressedSize, buffer.get());

            while (packetCount--) {
                ReadNextFrame(reader, parser, *recording);
            }
#endif
        } else {
            while (packetCount--) {
                ReadNextFrame(reader, parser, *recording);
            }
        }

        if (recording->Frames.empty()) {
            throw InvalidDataError();
        }
    } catch ([[maybe_unused]] const InvalidDataError &e) {
        partialReturn = true;
    }

    recording->Runtime = recording->Frames.back().Timestamp;

    return std::make_pair(std::move(recording), partialReturn);
}

} // namespace TibiaMovie2
} // namespace Recordings
} // namespace trc
