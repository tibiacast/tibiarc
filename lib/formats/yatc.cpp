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

#include "parser.hpp"

namespace trc {
namespace Recordings {
namespace YATC {
bool QueryTibiaVersion([[maybe_unused]] const DataReader &file,
                       [[maybe_unused]] VersionTriplet &triplet) {
    return false;
}

std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                 const Version &version,
                                                 Recovery recovery) {
    DataReader reader = file;

    auto recording = std::make_unique<Recording>();
    bool partialReturn = false;

    try {
        Parser parser(version, recovery == Recovery::Repair);

        while (reader.Remaining() > 0) {
            auto timestamp = reader.ReadU32();
            auto packetReader = reader.Slice(reader.ReadU16());

            recording->Frames.emplace_back(timestamp,
                                           parser.Parse(packetReader));
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

} // namespace YATC
} // namespace Recordings
} // namespace trc
