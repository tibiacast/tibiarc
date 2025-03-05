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
namespace TibiaTimeMachine {
bool QueryTibiaVersion(const DataReader &file,
                       int &major,
                       int &minor,
                       int &preview) {
    auto tibiaVersion = file.Peek<uint16_t>();

    major = tibiaVersion / 100;
    minor = tibiaVersion % 100;
    preview = 0;

    if (major < 7 || major > 12) {
        throw InvalidDataError();
    }

    return true;
}

std::pair<std::unique_ptr<Recording>, bool> Read(const DataReader &file,
                                                 const Version &version,
                                                 Recovery recovery) {
    DataReader reader = file;

    /* Tibia version */
    reader.SkipU16();

    /* Byte-prefixed server name, non-zero if OT */
    auto serverLength = reader.ReadU8();
    reader.Skip(serverLength);
    if (serverLength > 0) {
        /* Server port */
        reader.SkipU16();
    }

    auto recording = std::make_unique<Recording>();
    bool partialReturn = false;

    recording->Runtime = reader.ReadU32();

    try {
        Parser parser(version, recovery == Recovery::Repair);

        uint32_t timestamp = 0;

        for (;;) {
            auto packetReader = reader.Slice(reader.ReadU16());

            recording->Frames.emplace_back(timestamp,
                                           parser.Parse(packetReader));

            if (reader.Remaining() == 0) {
                break;
            }

            if (reader.ReadU8<0, 1>() == 0) {
                /* Packet delay. */
                timestamp += reader.ReadU16();
            } else {
                /* Fixed delay of 1s. */
                timestamp += 1000;
            }
        }
    } catch ([[maybe_unused]] const InvalidDataError &e) {
        partialReturn = true;
    }

    return std::make_pair(std::move(recording), partialReturn);
}

} // namespace TibiaTimeMachine
} // namespace Recordings
} // namespace trc
