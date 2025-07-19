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

#ifdef _WIN32
#    define NOMINMAX
#endif

extern "C" {
#include "deps/7z/Precomp.h"
#include "deps/7z/Alloc.h"
#include "deps/7z/LzmaDec.h"
}

#include "utils.hpp"

#include "demuxer.hpp"
#include "parser.hpp"

namespace trc {
namespace Recordings {
namespace Cam {
bool QueryTibiaVersion(const DataReader &file,
                       int &major,
                       int &minor,
                       int &preview) {
    trc::DataReader reader = file;
    uint8_t version[4];

    reader.Skip(32);
    reader.Copy(4, version);

    major = version[0];
    minor = version[1] * 10 + version[2];
    preview = 0;

    return CheckRange(major, 7, 12) && CheckRange(minor, 0, 99);
}

static void Decompress(uint8_t lzmaProperties[5],
                       size_t compressedSize,
                       const uint8_t *compressedData,
                       size_t decompressedSize,
                       uint8_t *decompressedData) {
    size_t decompressedPosition = 0;
    SRes result;

    do {
        size_t in_size, out_size;
        ELzmaStatus status;

        out_size = decompressedSize - decompressedPosition;
        in_size = compressedSize;

        result = LzmaDecode((Byte *)&decompressedData[decompressedPosition],
                            &out_size,
                            (const Byte *)compressedData,
                            &in_size,
                            (Byte *)lzmaProperties,
                            5,
                            LZMA_FINISH_ANY,
                            &status,
                            &g_Alloc);

        AbortUnless(out_size <= (decompressedSize - decompressedPosition));
        AbortUnless(in_size <= compressedSize);

        if (result == SZ_OK || result == SZ_ERROR_INPUT_EOF) {
            compressedData += in_size;
            compressedSize -= in_size;
            decompressedPosition += out_size;
        }
    } while (result == SZ_ERROR_INPUT_EOF);

    if (result != SZ_OK) {
        throw InvalidDataError();
    }
}

std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                 const Version &version,
                                                 Recovery recovery) {
    DataReader reader = file;

    /* Header */
    reader.Skip(32);

    /* Tibia version */
    reader.Skip(4);

    /* Metadata blob */
    auto metaLength = reader.ReadU32();
    reader.Skip(metaLength);

    auto compressedSize = reader.ReadU32();
    uint8_t lzmaProperties[5];
    reader.Copy(5, lzmaProperties);
    auto decompressedSize = reader.ReadU64();

    auto decompressedData = std::make_unique<uint8_t[]>(decompressedSize);
    Decompress(lzmaProperties,
               compressedSize,
               reader.RawData(),
               decompressedSize,
               decompressedData.get());

    reader = DataReader(decompressedSize, decompressedData.get());

    auto recording = std::make_unique<Recording>();
    bool partialReturn = false;

    /* Bogus container version. */
    reader.SkipU16();

    int32_t frameCount = reader.ReadS32<58>();
    frameCount -= 57;

    try {
        Parser parser(version, recovery == Recovery::Repair);
        Demuxer demuxer(2);

        for (int32_t i = 0; i < frameCount; i++) {
            uint16_t fragmentLength = reader.ReadU16();
            uint32_t fragmentTimestamp = reader.ReadU32();

            auto fragment = reader.Slice(fragmentLength);
            demuxer.Submit(fragmentTimestamp,
                           fragment,
                           [&](DataReader packetReader, uint32_t timestamp) {
                               recording->Frames.emplace_back(
                                       timestamp,
                                       parser.Parse(packetReader));
                           });

            /* Fragment checksum; usually not even valid. */
            reader.SkipU32();
        }

        demuxer.Finish();
    } catch ([[maybe_unused]] const InvalidDataError &e) {
        partialReturn = true;
    }

    recording->Runtime =
            std::max(recording->Runtime, recording->Frames.back().Timestamp);

    return std::make_pair(std::move(recording), partialReturn);
}

} // namespace Cam
} // namespace Recordings
} // namespace trc
