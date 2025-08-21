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
#include "collation.hpp"
#include "memoryfile.hpp"
#include "recordings.hpp"
#include "utils.hpp"
#include "versions.hpp"

#include <chrono>
#include <compare>
#include <cstdlib>
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

                result.push_back(std::make_unique<Version>(
                        VersionTriplet(major, minor, 0),
                        pic.Reader(),
                        spr.Reader(),
                        dat.Reader()));
            }
        }
    }

    result.sort([](const std::unique_ptr<Version> &lhs,
                   const std::unique_ptr<Version> &rhs) {
        auto order = lhs->Triplet <=> rhs->Triplet;
        return (order == std::strong_ordering::equal) ||
               (order == std::strong_ordering::less);
    });

    if (result.empty()) {
        std::cerr << "error: failed to find Tibia data" << std::endl;
        std::exit(1);
    } else {
        std::cout << "Found versions:" << std::endl;

        for (const auto &version : result) {
            std::cout << static_cast<std::string>(version->Triplet) << " ";
        }

        std::cout << std::endl;
    }

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
        if (frame.Timestamp > std::chrono::seconds(2)) {
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

std::pair<Collation::RecordingFile, std::filesystem::path> ProcessRecording(
        const Collation::RecordingFile &source,
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

                    if (state.Creatures.contains(state.Player.Id)) {
                        std::filesystem::path folder =
                                static_cast<std::string>(version->Triplet);
                        return std::make_pair(source, folder);
                    }
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
    VersionTriplet triplet;
    if (Recordings::QueryTibiaVersion(format, reader, triplet)) {
        guessedVersion = std::make_optional(static_cast<std::string>(triplet));
    } else if (!guessedVersion) {
        guessedVersion = std::make_optional("unversioned");
    }

    return std::make_pair(source, "graveyard" / *guessedVersion);
}

std::vector<std::pair<Collation::RecordingFile, std::filesystem::path>>
ProcessRecordings(const std::vector<Collation::RecordingFile> &recordings,
                  const std::list<std::unique_ptr<Version>> &versions) {
    std::vector<std::pair<Collation::RecordingFile, std::filesystem::path>>
            result(recordings.size());

    std::transform(recordings.begin(),
                   recordings.end(),
                   result.begin(),
                   [&versions](const Collation::RecordingFile &in) {
                       return ProcessRecording(in, versions);
                   });

    return result;
}

std::filesystem::path MangleDestination(const Collation::RecordingFile &file,
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
                  const Collation::RecordingFile &file,
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
    Collation::DenyList denyList;
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
                   Collation::ParseDenyList(args[0], denyList);
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

    std::vector<Collation::RecordingFile> recordings;

    Collation::GatherRecordingFiles(sourceRoot, recordings);

    if (recordings.empty()) {
        std::cerr << "error: failed to find any recordings" << std::endl;
        std::exit(1);
    }

    std::vector<Collation::RecordingFile> accepted;

    for (const auto &file : recordings) {
        if (denyList.contains(file.Checksum)) {
            accepted.push_back(file);
        }
    }

    const size_t skipped = recordings.size() - accepted.size();
    std::cout << "Found " << recordings.size() << " recordings";
    if (skipped != 0) {
        std::cout << ", skipped " << skipped << " due to deny-list";
    }
    std::cout << std::endl;

    auto versions = GetVersions(dataRoot);
    auto transfers = ProcessRecordings(accepted, versions);

    /* Perform all file operations serially to avoid races, they're plenty
     * fast compared to the hashing and version determination done above. */
    for (const auto &[source, destination] : transfers) {
        TransferFile(action, verbose, source, recordingsRoot, destination);
    }

    return 0;
}
