/*
 * Copyright 2025 "John Högberg"
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

#ifndef __TRC_SERIALIZER_HPP__
#define __TRC_SERIALIZER_HPP__

#include "recordings.hpp"
#include "events.hpp"
#include "versions.hpp"

#include <filesystem>
#include <iostream>
#include <unordered_set>

namespace trc {
namespace Serializer {
struct Settings {

    Recordings::Format InputFormat;
    Recordings::Recovery InputRecovery;

    std::unordered_set<Events::Type> SkippedEvents;

    int StartTime;
    int EndTime;

    VersionTriplet DesiredTibiaVersion;
    bool DryRun;
};

void Serialize(const Settings &settings,
               const std::filesystem::path &dataFolder,
               const std::filesystem::path &inputPath,
               std::ostream &output);

} // namespace Serializer
} // namespace trc

#endif