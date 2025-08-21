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

#include "utils.hpp"
#include <unordered_map>

namespace trc {
namespace Recordings {
static const std::unordered_map<Format, FormatNames> FormatDescriptions(
        {{Format::Cam, {"TibiacamTV", "cam", ".cam"}},
         {Format::Rec, {"TibiCAM", "rec", ".rec"}},
         {Format::Tibiacast, {"Tibiacast", "tibiacast", ".recording"}},
         {Format::TibiaMovie1, {"TibiaMovie", "tmv1", ".tmv"}},
         {Format::TibiaMovie2, {"TibiaMovie", "tmv2", ".tmv2"}},
         {Format::TibiaReplay, {"TibiaReplay", "trp", ".trp"}},
         {Format::TibiaTimeMachine, {"TibiaTimeMachine", "ttm", ".ttm"}},
         {Format::YATC, {"YATC", "yatc", ".yatc"}}});

namespace Cam {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace Cam

namespace Rec {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace Rec

namespace Tibiacast {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace Tibiacast

namespace TibiaMovie1 {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace TibiaMovie1

namespace TibiaMovie2 {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace TibiaMovie2

namespace TibiaReplay {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace TibiaReplay

namespace TibiaTimeMachine {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace TibiaTimeMachine

namespace YATC {
extern bool QueryTibiaVersion(const DataReader &file, VersionTriplet &triplet);
extern std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                        const Version &version,
                                                        Recovery recovery);
} // namespace YATC

Format GuessFormat(const std::filesystem::path &path, const DataReader &file) {
    auto magic = file.Peek<uint32_t>();

    switch (magic) {
    case 0x32564D54: /* 'TMV2' */
        return Format::TibiaMovie2;
    case 0x00505254: /* 'TRP\0' */
        return Format::TibiaReplay;
    }

    if ((magic & 0xFFFF) == 0x1337) {
        /* Old TibiaReplay format */
        return Format::TibiaReplay;
    }

    for (const auto &[format, names] : FormatDescriptions) {
        if (path.extension() == names.Extension) {
            return format;
        }
    }

    return Format::Unknown;
}

bool QueryTibiaVersion(Format format,
                       const DataReader &file,
                       VersionTriplet &triplet) {
    switch (format) {
    case Format::Cam:
        return Cam::QueryTibiaVersion(file, triplet);
    case Format::Rec:
        return Rec::QueryTibiaVersion(file, triplet);
    case Format::Tibiacast:
        return Tibiacast::QueryTibiaVersion(file, triplet);
    case Format::TibiaMovie1:
        return TibiaMovie1::QueryTibiaVersion(file, triplet);
    case Format::TibiaMovie2:
        return TibiaMovie2::QueryTibiaVersion(file, triplet);
    case Format::TibiaReplay:
        return TibiaReplay::QueryTibiaVersion(file, triplet);
    case Format::TibiaTimeMachine:
        return TibiaTimeMachine::QueryTibiaVersion(file, triplet);
    case Format::YATC:
        return YATC::QueryTibiaVersion(file, triplet);
    default:
        abort();
    }
}

std::pair<std::unique_ptr<Recording>, bool> Read(Format format,
                                                 const DataReader &file,
                                                 const Version &version,
                                                 Recovery recovery) {
    switch (format) {
    case Format::Cam:
        return Cam::Read(file, version, recovery);
    case Format::Rec:
        return Rec::Read(file, version, recovery);
    case Format::Tibiacast:
        return Tibiacast::Read(file, version, recovery);
    case Format::TibiaMovie1:
        return TibiaMovie1::Read(file, version, recovery);
    case Format::TibiaMovie2:
        return TibiaMovie2::Read(file, version, recovery);
    case Format::TibiaReplay:
        return TibiaReplay::Read(file, version, recovery);
    case Format::TibiaTimeMachine:
        return TibiaTimeMachine::Read(file, version, recovery);
    case Format::YATC:
        return YATC::Read(file, version, recovery);
    default:
        abort();
    }
}

const FormatNames &FormatNames::Get(Format format) {
    static const FormatNames Unknown{"unknown", "unknown", ".unknown"};

    auto it = FormatDescriptions.find(format);

    if (it != FormatDescriptions.end()) {
        return it->second;
    }

    return Unknown;
}

} // namespace Recordings
} // namespace trc
