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

#include "collation.hpp"

#include "crypto.hpp"
#include "memoryfile.hpp"
#include "utils.hpp"

#include <chrono>
#include <compare>
#include <cstdlib>
#ifndef _WIN32
/* N.B: This fails under MXE cross-compilation. */
#    include <execution>
#endif
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <sstream>
#include <system_error>
#include <vector>

namespace trc {
namespace Collation {
Checksum::Checksum(const std::string text) {
    const auto to_nibble = [](uint8_t character) {
        if (character >= '0' && character <= '9') {
            return character - '0';
        } else if (character >= 'a' && character <= 'f') {
            return 10 + (character - 'a');
        } else if (character >= 'A' && character <= 'F') {
            return 10 + (character - 'A');
        }

        throw InvalidDataError();
    };

    try {
        if (text.size() != (SHA1Size * 2)) {
            throw InvalidDataError();
        }

        for (int i = 0; i < text.size(); i += 2) {
            uint8_t high = to_nibble(text[i]), low = to_nibble(text[i + 1]);
            this->operator[](i / 2) = high << 4 | low;
        }
    } catch ([[maybe_unused]] const InvalidDataError &ignored) {
        throw text + " is not a valid SHA1 checksum";
    }
}

Checksum::Checksum(const Checksum &other) {
    std::copy(other.begin(), other.end(), begin());
}

Checksum::Checksum() {
}

Checksum::operator std::string() const {
    std::stringstream result;

    for (int i = 0; i < size(); i++) {
        const char as_hex[] = "0123456789abcdef";
        uint32_t byte = this->operator[](i);

        result << as_hex[byte >> 4] << as_hex[byte & 0xF];
    }

    return result.str();
}

RecordingFile::RecordingFile(const std::filesystem::path &path,
                             const Collation::Checksum &checksum)
    : Path(path), Checksum(checksum) {
}

RecordingFile::RecordingFile() {
}

std::strong_ordering RecordingFile::operator<=>(
        const RecordingFile &other) const {
    /* We assume that all paths are unique, relative or otherwise. */
    return Path <=> other.Path;
}

class Line : public std::string {};

std::istream &operator>>(std::istream &is, Line &line) {
    std::getline(is, line);
    return is;
}

void ParseDenyList(const std::filesystem::path &path, DenyList &result) {
    std::ifstream file(path);
    std::istream_iterator<Line> begin(file), end;
    int index = 0;

    for (const auto &line : std::ranges::subrange(begin, end)) {
        index++;

        try {
            auto checksum = line.substr(
                    0,
                    std::min(line.size(),
                             line.find_first_not_of("0123456789abcdef")));

            if (checksum.size() > 0) {
                result.emplace(checksum);
            }
        } catch (const std::string &reason) {
            throw std::format("error: failed to parse deny-list, line {}: {}",
                              index,
                              reason);
        }
    }
}

static void GatherPaths(const std::filesystem::path &path,
                        const std::set<std::filesystem::path> &extensions,
                        std::vector<std::filesystem::path> &paths) {
    if (std::filesystem::is_regular_file(path)) {
        if (extensions.contains(path.filename().extension())) {
            paths.push_back(path);
        }
    } else if (std::filesystem::is_directory(path)) {
        for (const auto &entry : std::filesystem::directory_iterator(path)) {
            GatherPaths(entry.path(), extensions, paths);
        }
    }
}

void GatherRecordingPaths(const std::filesystem::path &root,
                          std::vector<std::filesystem::path> &result) {
    GatherPaths(root,
                {".cam",
                 ".rec",
                 ".recording",
                 ".tmv",
                 ".tmv2",
                 ".trp",
                 ".ttm",
                 ".yatc"},
                result);
}

std::optional<RecordingFile> GatherRecordingFile(
        const std::filesystem::path &path) {
    const MemoryFile file(path);
    const auto &reader = file.Reader();
    auto context = Crypto::SHA1::Create();
    unsigned int size;
    Checksum checksum;

    context->Hash(reader.Data, reader.Length, checksum.data());

    return std::make_optional<RecordingFile>(path, checksum);
}

void GatherRecordingFiles(const std::filesystem::path &root,
                          std::vector<RecordingFile> &result) {
    std::vector<std::filesystem::path> paths;

    GatherRecordingPaths(root, paths);

    std::vector<std::optional<RecordingFile>> unfiltered(paths.size());
    std::transform(
#ifndef _WIN32
            std::execution::par_unseq,
#endif
            paths.begin(),
            paths.end(),
            unfiltered.begin(),
            [](const std::filesystem::path &path) {
                return GatherRecordingFile(path);
            });

    for (const auto &file : unfiltered) {
        if (file) {
            result.push_back(*file);
        }
    }
}

DataFile::DataFile(const std::filesystem::path &path,
                   uint32_t signature,
                   Type kind)
    : Path(path), Signature(signature), Kind(kind) {
}

DataFile::DataFile() {
}

void GatherDataPaths(const std::filesystem::path &root,
                     std::vector<std::filesystem::path> &result) {
    GatherPaths(root, {".dat", ".pic", ".spr"}, result);
}

std::optional<DataFile> GatherDataFile(const std::filesystem::path &path) {
    const auto extension = path.filename().extension();

    try {
        MemoryFile data(path);
        DataFile::Type kind;
        uint32_t signature;

        signature = data.Reader().ReadU32();

        if (extension == ".dat") {
            kind = DataFile::Type::Dat;
        } else if (extension == ".pic") {
            kind = DataFile::Type::Pic;
        } else {
            AbortUnless(extension == ".spr");
            kind = DataFile::Type::Spr;
        }

        return std::make_optional<DataFile>(path, signature, kind);
    } catch (const InvalidDataError &) {
        /* */
    } catch (const IOError &) {
        /* */
    }

    return std::nullopt;
}

void GatherDataFiles(const std::filesystem::path &root,
                     std::vector<DataFile> &result) {
    std::vector<std::filesystem::path> paths;

    GatherDataPaths(root, paths);

    for (const auto &path : paths) {
        auto file = GatherDataFile(path);

        if (file) {
            result.push_back(*file);
        }
    }
}

} // namespace Collation
} // namespace trc