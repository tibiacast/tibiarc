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

#include "recordings.hpp"
#include "versions.hpp"

#include "utils.hpp"

#include "parser.hpp"

namespace trc {
namespace Recordings {
namespace TibiaReplay {
bool QueryTibiaVersion(const DataReader &file,
                       int &major,
                       int &minor,
                       int &preview) {
    DataReader reader = file;

    auto magic = reader.ReadU16();

    if (magic != 0x1337) {
        reader.SkipU16();
    }

    auto tibiaVersion = reader.ReadU16();

    major = tibiaVersion / 100;
    minor = tibiaVersion % 100;
    preview = 0;

    if (major < 7 || major > 12) {
        throw InvalidDataError();
    }

    return true;
}

std::unique_ptr<Recording> Read(const DataReader &file,
                                const Version &version,
                                Recovery recovery) {
    DataReader reader = file;

    auto magic = reader.ReadU16();
    if (magic != 0x1337) {
        reader.SkipU16();
    }

    /* Tibia version */
    reader.SkipU16();

    auto recording = std::make_unique<Recording>();

    recording->Runtime = reader.ReadU32();
    auto frames = reader.ReadU32();

    try {
        Parser parser(version, recovery == Recovery::Repair);

        while (frames--) {
            auto timestamp = reader.ReadU32();
            auto packetReader = reader.Slice(reader.ReadU16());

            recording->Frames.emplace_back(timestamp,
                                           parser.Parse(packetReader));
        }
    } catch ([[maybe_unused]] const InvalidDataError &e) {
        if (recovery != Recovery::PartialReturn) {
            throw;
        }
    }

    return recording;
}

} // namespace TibiaReplay
} // namespace Recordings
} // namespace trc
