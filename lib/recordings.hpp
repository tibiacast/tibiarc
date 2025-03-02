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

#ifndef __TRC_RECORDINGS_HPP__
#define __TRC_RECORDINGS_HPP__

#include "datareader.hpp"
#include "gamestate.hpp"

#include "versions_decl.hpp"
#include "events.hpp"

#include <filesystem>
#include <memory>
#include <list>

namespace trc {
namespace Recordings {
enum class Format {
    Cam,
    Rec,
    Tibiacast,
    TibiaMovie1,
    TibiaMovie2,
    TibiaReplay,
    TibiaTimeMachine,
    YATC,
    Unknown
};

enum class Recovery { None, Repair };

struct Recording {
    struct Frame {
        uint32_t Timestamp;
        std::list<std::unique_ptr<Events::Base>> Events;
    };

    uint32_t Runtime;
    std::list<Frame> Frames;
};

Format GuessFormat(const std::filesystem::path &path, const DataReader &file);
std::string FormatName(Format format);

bool QueryTibiaVersion(Format format,
                       const DataReader &file,
                       int &major,
                       int &minor,
                       int &preview);
std::pair<std::unique_ptr<Recording>, bool> Read(
        Format format,
        const DataReader &file,
        const Version &version,
        Recovery recovery = Recovery::None);
} // namespace Recordings
} // namespace trc

#endif /* __TRC_RECORDINGS_HPP__ */
