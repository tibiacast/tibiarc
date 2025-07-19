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

#include "cli.hpp"

#include "events.hpp"
#include "crypto.hpp"
#include "memoryfile.hpp"
#include "recordings.hpp"
#include "utils.hpp"
#include "versions.hpp"

#include <chrono>
#include <compare>
#include <cstdlib>
#ifndef _WIN32
/* N.B: This fails under MXE cross-compilation. */
#    include <execution>
#endif
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <system_error>
#include <vector>

using namespace trc;

constexpr size_t SHA1Size = 20;

struct Checksum : std::array<uint8_t, SHA1Size> {
    Checksum(const std::string text) {
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

    Checksum(const Checksum &other) {
        std::copy(other.begin(), other.end(), begin());
    }

    Checksum() {
    }

    operator std::string() const {
        std::stringstream result;

        for (int i = 0; i < size(); i++) {
            const char as_hex[] = "0123456789abcdef";
            uint32_t byte = this->operator[](i);

            result << as_hex[byte >> 4] << as_hex[byte & 0xF];
        }

        return result.str();
    }
};

struct File {
    std::filesystem::path Path;
    ::Checksum Checksum;

    File(const std::filesystem::path &path, const ::Checksum &checksum)
        : Path(path), Checksum(checksum) {
    }

    File() {
    }

    std::strong_ordering operator<=>(const File &other) const {
        /* We assume that all paths are unique, relative or otherwise. */
        return Path <=> other.Path;
    }
};

class Line : public std::string {};

std::istream &operator>>(std::istream &is, Line &line) {
    std::getline(is, line);
    return is;
}

void ParseDenyList(std::set<Checksum> &denyList, std::string path) {
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

                denyList.emplace(checksum);
            }
        } catch (const std::string &reason) {
            std::cerr << "error: failed to parse deny-list, line " << index
                      << ": " << reason << std::endl;
            std::exit(1);
        }
    }
}

std::list<std::unique_ptr<Version>> GetVersions(
        const std::filesystem::path &dataRoot) {
    std::list<std::unique_ptr<Version>> result;

    for (const auto &directory :
         std::filesystem::directory_iterator(dataRoot)) {
        if (!directory.is_directory()) {
            continue;
        }

        const auto picPath = directory / std::filesystem::path("Tibia.pic");
        const auto sprPath = directory / std::filesystem::path("Tibia.spr");
        const auto datPath = directory / std::filesystem::path("Tibia.dat");

        if (std::filesystem::is_regular_file(picPath) &&
            std::filesystem::is_regular_file(sprPath) &&
            std::filesystem::is_regular_file(datPath)) {
            const auto name = directory.path().filename().string();
            int major, minor;

            if (sscanf(name.c_str(), "%u.%u", &major, &minor) == 2) {
                const MemoryFile pic(picPath);
                const MemoryFile spr(sprPath);
                const MemoryFile dat(datPath);

                result.push_back(std::make_unique<Version>(major,
                                                           minor,
                                                           0,
                                                           pic.Reader(),
                                                           spr.Reader(),
                                                           dat.Reader()));
            }
        }
    }

    result.sort([](const std::unique_ptr<Version> &lhs,
                   const std::unique_ptr<Version> &rhs) {
        if (lhs->Major == rhs->Major && lhs->Minor < rhs->Minor) {
            return true;
        }

        return lhs->Major < rhs->Major;
    });

    if (result.empty()) {
        std::cerr << "error: failed to find Tibia data" << std::endl;
        std::exit(1);
    } else {
        std::cout << "Found versions:" << std::endl;
        for (const auto &version : result) {
            std::cout << " " << version->Major << "." << version->Minor;
        }
        std::cout << std::endl;
    }

    return result;
}

void GatherRecordingPaths(const std::filesystem::path &recordingsRoot,
                          std::vector<std::filesystem::path> &paths) {
    for (const auto &entity :
         std::filesystem::directory_iterator(recordingsRoot)) {
        if (is_directory(entity)) {
            GatherRecordingPaths(entity, paths);
        } else {
            const auto &path = entity.path();
            const auto extension = path.filename().extension();

            if (extension == ".cam" || extension == ".rec" ||
                extension == ".recording" || extension == ".tmv" ||
                extension == ".tmv2" || extension == ".trp" ||
                extension == ".ttm" || extension == ".yatc") {
                paths.push_back(path);
            }
        }
    }
}

std::vector<File> GatherRecordings(const std::filesystem::path &recordingsRoot,
                                   const std::set<Checksum> &denyList) {
    std::vector<std::filesystem::path> paths;

    GatherRecordingPaths(recordingsRoot, paths);

    std::vector<std::optional<File>> unfiltered(paths.size());
    std::transform(
#ifndef _WIN32
            std::execution::par_unseq,
#endif
            paths.begin(),
            paths.end(),
            unfiltered.begin(),
            [&denyList](
                    const std::filesystem::path &path) -> std::optional<File> {
                const MemoryFile file(path);
                const auto &reader = file.Reader();
                auto context = Crypto::SHA1::Create();
                unsigned int size;
                Checksum checksum;

                context->Hash(reader.Data, reader.Length, checksum.data());

                if (denyList.contains(checksum)) {
                    return std::nullopt;
                }

                return std::make_optional<File>(path, checksum);
            });

    if (unfiltered.empty()) {
        std::cerr << "error: failed to find any recordings" << std::endl;
        std::exit(1);
    }

    std::vector<File> result;
    for (const auto &entity : unfiltered) {
        if (entity) {
            result.push_back(*entity);
        }
    }
    const size_t skipped = unfiltered.size() - result.size();
    std::cout << "Found " << result.size() << " recordings";
    if (skipped != 0) {
        std::cout << ", skipped " << skipped << " due to deny-list";
    }
    std::cout << std::endl;

    return result;
}

static constexpr auto YMD(int year, int month, int day) {
    return std::chrono::year_month_day{std::chrono::year(year),
                                       std::chrono::month(month),
                                       std::chrono::day(day)};
}

std::optional<std::string> GuessVersion(
        const std::list<Recordings::Recording::Frame> &frames) {
    /* Versions with significant data or protocol changes that we've been able
     * to find data files for. This is just intended as a rough guide for later
     * analysis. */
    static const std::vector<
            std::pair<std::chrono::year_month_day, std::string>>
            versions{{YMD(2002, 8, 28), "7.00"},  {YMD(2003, 7, 27), "7.11"},
                     {YMD(2004, 1, 21), "7.21"},  {YMD(2004, 3, 9), "7.23"},
                     {YMD(2004, 3, 14), "7.24"},  {YMD(2004, 5, 4), "7.26"},
                     {YMD(2004, 7, 22), "7.27"},  {YMD(2004, 8, 11), "7.30"},
                     {YMD(2004, 12, 10), "7.40"}, {YMD(2005, 7, 7), "7.41"},
                     {YMD(2005, 8, 9), "7.50"},   {YMD(2005, 11, 16), "7.55"},
                     {YMD(2005, 12, 12), "7.60"}, {YMD(2006, 5, 17), "7.70"},
                     {YMD(2006, 6, 8), "7.72"},   {YMD(2006, 8, 1), "7.80"},
                     {YMD(2006, 8, 29), "7.81"},  {YMD(2006, 12, 12), "7.90"},
                     {YMD(2007, 1, 8), "7.92"},   {YMD(2007, 6, 26), "8.0"},
                     {YMD(2008, 4, 8), "8.11"},   {YMD(2008, 7, 2), "8.20"},
                     {YMD(2008, 7, 24), "8.21"},  {YMD(2008, 8, 12), "8.22"},
                     {YMD(2008, 9, 30), "8.30"},  {YMD(2008, 10, 1), "8.31"},
                     {YMD(2008, 12, 10), "8.40"}, {YMD(2009, 3, 18), "8.41"},
                     {YMD(2009, 4, 22), "8.42"},  {YMD(2009, 7, 1), "8.50"},
                     {YMD(2009, 10, 1), "8.52"},  {YMD(2009, 11, 5), "8.53"},
                     {YMD(2009, 12, 9), "8.54"},  {YMD(2010, 3, 17), "8.55"},
                     {YMD(2010, 5, 5), "8.56"},   {YMD(2010, 5, 6), "8.57"},
                     {YMD(2010, 6, 30), "8.60"},  {YMD(2010, 8, 23), "8.61"},
                     {YMD(2010, 9, 22), "8.62"}};

    static const std::array<std::string, 12> months{"Jan",
                                                    "Feb",
                                                    "Mar",
                                                    "Apr",
                                                    "May",
                                                    "Jun",
                                                    "Jul",
                                                    "Aug",
                                                    "Sep",
                                                    "Oct",
                                                    "Nov",
                                                    "Dec"};

    for (const auto &frame : frames) {
        /* Don't bother looking too for into a recording; we'll either see the
         * login message almost immediately or not at all. */
        if (frame.Timestamp > 2000) {
            break;
        }

        for (const auto &event : frame.Events) {
            if (event->Kind() != Events::Type::StatusMessageReceived) {
                continue;
            }

            const auto &message =
                    static_cast<Events::StatusMessageReceived &>(*event)
                            .Message;
            int day, year;
            char month[4];

            if (sscanf(message.c_str(),
                       "Your last visit in Tibia: %i. %3s %i",
                       &day,
                       month,
                       &year) < 3 ||
                (day < 1 || day > 31) || (year < 1900 || year > 2100)) {
                continue;
            }

            auto it = std::find(months.begin(), months.end(), month);
            if (it == months.end()) {
                continue;
            }

            std::chrono::year_month_day date(
                    std::chrono::year(year),
                    std::chrono::month{static_cast<unsigned int>(
                            std::distance(months.begin(), it) + 1)},
                    std::chrono::day(day));

            auto candidate = std::find_if(
                    versions.rbegin(),
                    versions.rend(),
                    [&](const auto &other) { return other.first < date; });

            if (candidate != versions.rend()) {
                return std::optional(candidate->second);
            }
        }
    }

    return std::nullopt;
}

std::pair<File, std::filesystem::path> ProcessRecording(
        const File &source,
        const std::list<std::unique_ptr<Version>> &versions) {
    std::optional<std::filesystem::path> guessedVersion;
    const MemoryFile file(source.Path);
    const auto reader = file.Reader();

    const auto format = Recordings::GuessFormat(source.Path, reader);

    for (const auto &version : versions) {
        try {
            auto [recording, partial] =
                    Recordings::Read(format, reader, *version);

            if (!partial) {
                Gamestate state(*version);

                try {
                    for (const auto &frame : recording->Frames) {
                        for (const auto &event : frame.Events) {
                            event->Update(state);
                        }
                    }

                    std::filesystem::path folder =
                            std::to_string(version->Major) + "." +
                            std::to_string(version->Minor);
                    return std::make_pair(source, folder);
                } catch ([[maybe_unused]] const InvalidDataError &e) {
                    /* There's something wrong with the underlying data, but
                     * we may still be able to guess the version. */
                }
            }

            if (!guessedVersion) {
                guessedVersion = GuessVersion(recording->Frames);
            }
        } catch ([[maybe_unused]] const InvalidDataError &e) {
            /* Most likely an error reading the container, we can't even
             * guess the version. */
        }
    }

    /* If the container provides a version, trust it over our best-effort
     * guess.
     *
     * Note that we do not use this to speed up processing; we're placing
     * recordings in the earliest version for which they finish gracefully,
     * regardless of advertised version, and using the container-provided
     * version would interfere with that. */
    int major, minor, preview;
    if (Recordings::QueryTibiaVersion(format, reader, major, minor, preview)) {
        guessedVersion = std::make_optional(std::to_string(major) + "." +
                                            std::to_string(minor));
    } else if (!guessedVersion) {
        guessedVersion = std::make_optional("unversioned");
    }

    return std::make_pair(source, "graveyard" / *guessedVersion);
}

std::vector<std::pair<File, std::filesystem::path>> ProcessRecordings(
        const std::vector<File> &recordings,
        const std::list<std::unique_ptr<Version>> &versions) {
    std::vector<std::pair<File, std::filesystem::path>> result(
            recordings.size());

    std::transform(
#ifndef _WIN32
            std::execution::par_unseq,
#endif
            recordings.begin(),
            recordings.end(),
            result.begin(),
            [&versions](const File &in) {
                return ProcessRecording(in, versions);
            });

    return result;
}

std::filesystem::path MangleDestination(const File &file,
                                        const std::filesystem::path &root,
                                        const std::filesystem::path folder) {
    std::basic_string<std::filesystem::path::value_type> filename =
            file.Path.filename();

    /* Replace characters that are incompatible with CMake so that we can test
     * the recording collection with CTest */
    std::replace(filename.begin(), filename.end(), '[', '(');
    std::replace(filename.begin(), filename.end(), ']', ')');
    std::replace(filename.begin(), filename.end(), ';', '_');

    return root / folder / filename;
}

enum class TransferAction { None, CopyFile, MoveFile };

void TransferFile(TransferAction action,
                  bool verbose,
                  const File &file,
                  const std::filesystem::path &root,
                  const std::filesystem::path &folder) {
    auto destination = MangleDestination(file, root, folder);

    if (std::filesystem::exists(destination)) {
        const MemoryFile lhs(file.Path), rhs(destination);
        const auto &lhs_reader = lhs.Reader();
        const auto &rhs_reader = rhs.Reader();

        if (lhs_reader.Length == rhs_reader.Length &&
            !std::memcmp(lhs_reader.Data, rhs_reader.Data, lhs_reader.Length)) {
            if (verbose) {
                std::cout << file.Path << " is already present in collection"
                          << std::endl;
            }

            return;
        }

        std::cerr << "warning: Conflict between " << file.Path << " and "
                  << destination << std::endl;
        destination.replace_extension((std::string)file.Checksum +
                                      destination.extension().string());
    }

    std::error_code error;
    switch (action) {
    case TransferAction::CopyFile:
        if (verbose) {
            std::cout << "verbose: copying " << file.Path << " to "
                      << destination << std::endl;
        }

        (void)std::filesystem::create_directories(root / folder);
        std::filesystem::copy_file(file.Path, destination, error);
        break;
    case TransferAction::MoveFile:
        if (verbose) {
            std::cout << "verbose: moving " << file.Path << " to "
                      << destination << std::endl;
        }

        (void)std::filesystem::create_directories(root / folder);
        std::filesystem::rename(file.Path, destination, error);
        break;
    case TransferAction::None:
        std::cout << "dry-run: transferring " << file.Path << " to "
                  << destination << std::endl;
        break;
    }

    if (error.value() != 0) {
        std::cerr << "warning: Failed to transfer " << file.Path << " to "
                  << destination << std::endl;
    }
}

int main(int argc, char **argv) {
    TransferAction action = TransferAction::CopyFile;
    std::set<Checksum> denyList;
    bool verbose = false;

    auto paths = CLI::Process(
            argc,
            argv,
            "tibiarc-collator -- a program for validating and versioning "
            "Tibia recordings",
            "tibiarc-collator 0.3",
            {"collection_root", "source"},
            {{"move",
              {"move files instead of copying them",
               {},
               [&]([[maybe_unused]] const CLI::Range &args) {
                   action = TransferAction::MoveFile;
               }}},
             {"deny-list",
              {"skip files whose hashes are listed in the given file",
               {"file"},
               [&]([[maybe_unused]] const CLI::Range &args) {
                   ParseDenyList(denyList, args[0]);
               }}},
             {"verbose",
              {"print the action taken for every recording",
               {},
               [&]([[maybe_unused]] const CLI::Range &args) {
                   verbose = true;
               }}},

             {"dry-run",
              {"don't do anything. This is only intended for testing",
               {},
               [&]([[maybe_unused]] const CLI::Range &args) {
                   action = TransferAction::None;
               }}}});

    const std::filesystem::path collectionRoot = paths[0];
    const std::filesystem::path sourceRoot = paths[1];

    const std::filesystem::path dataRoot = collectionRoot / "data";
    const std::filesystem::path recordingsRoot = collectionRoot / "videos";

    if (!std::filesystem::is_directory(dataRoot) ||
        !std::filesystem::is_directory(recordingsRoot)) {
        std::cerr << "error: collection root must be a directory containing "
                     "'data' and 'recordings' folders"
                  << std::endl;
        std::exit(1);
    }

    if (!std::filesystem::is_directory(sourceRoot)) {
        std::cerr << "error: source must be a directory" << std::endl;
        std::exit(1);
    }

    auto versions = GetVersions(dataRoot);
    auto recordings = GatherRecordings(sourceRoot, denyList);
    auto transfers = ProcessRecordings(recordings, versions);

    /* Perform all file operations serially to avoid races, they're plenty
     * fast compared to the hashing and version determination done above. */
    for (const auto &[source, destination] : transfers) {
        TransferFile(action, verbose, source, recordingsRoot, destination);
    }

    return 0;
}
