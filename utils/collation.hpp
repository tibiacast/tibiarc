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

#ifndef __TRC_COLLATION_HPP__
#define __TRC_COLLATION_HPP__

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "utils.hpp"

namespace trc {
namespace Collation {
static constexpr size_t SHA1Size = 20;

struct Checksum : std::array<uint8_t, SHA1Size> {
    Checksum(const std::string text);
    Checksum(const Checksum &other);
    Checksum();

    operator std::string() const;
};

struct RecordingFile {
    std::filesystem::path Path;
    Collation::Checksum Checksum;

    RecordingFile(const std::filesystem::path &path,
                  const Collation::Checksum &checksum);
    RecordingFile();

    std::strong_ordering operator<=>(const RecordingFile &other) const;
};

struct DataFile {
    std::filesystem::path Path;
    uint32_t Signature;
    enum class Type { Dat, Pic, Spr } Kind;

    DataFile(const std::filesystem::path &path, uint32_t signature, Type kind);
    DataFile();
};

using DenyList = std::set<Checksum>;
void ParseDenyList(const std::filesystem::path &path, DenyList &result);

void GatherRecordingPaths(const std::filesystem::path &root,
                          std::vector<std::filesystem::path> &result);
void GatherDataPaths(const std::filesystem::path &root,
                     std::vector<std::filesystem::path> &result);

std::optional<RecordingFile> GatherRecordingFile(
        const std::filesystem::path &path);
void GatherRecordingFiles(const std::filesystem::path &root,
                          std::vector<RecordingFile> &result);

std::optional<DataFile> GatherDataFile(const std::filesystem::path &path);
void GatherDataFiles(const std::filesystem::path &root,
                     std::vector<DataFile> &result);

}; // namespace Collation
} // namespace trc

#endif
