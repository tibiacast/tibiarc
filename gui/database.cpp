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

#include "database.hpp"
#include "memoryfile.hpp"
#include "versions.hpp"

#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <ranges>
#include <sstream>

#ifndef DISABLE_QT_CONCURRENT
#    include <QtConcurrent>
#endif

using namespace nlohmann;

namespace nlohmann {
template <> struct adl_serializer<std::chrono::milliseconds> {
    static void from_json(const json &j, std::chrono::milliseconds &d) {
        int64_t time;

        nlohmann::from_json(j, time);

        d = std::chrono::milliseconds(time);
    }

    static void to_json(json &j, const std::chrono::milliseconds &d) {
        nlohmann::to_json(j, d.count());
    }
};
} // namespace nlohmann

namespace trc {

NLOHMANN_JSON_SERIALIZE_ENUM(Recordings::Format,
                             {{Recordings::Format::Cam, "Cam"},
                              {Recordings::Format::Rec, "Rec"},
                              {Recordings::Format::Tibiacast, "Tibiacast"},
                              {Recordings::Format::TibiaMovie1, "TibiaMovie1"},
                              {Recordings::Format::TibiaMovie2, "TibiaMovie2"},
                              {Recordings::Format::TibiaReplay, "TibiaReplay"},
                              {Recordings::Format::TibiaTimeMachine,
                               "TibiaTimeMachine"},
                              {Recordings::Format::YATC, "YATC"}});

void to_json(json &j, const VersionTriplet &version) {
    j = json{{"Major", version.Major},
             {"Minor", version.Minor},
             {"Preview", version.Preview}};
}

void from_json(const json &j, VersionTriplet &version) {
    j.at("Major").get_to(version.Major);
    j.at("Minor").get_to(version.Minor);
    j.at("Preview").get_to(version.Preview);
}

namespace GUI {

struct Signatures {
    uint32_t Dat, Pic, Spr;
};

const std::map<VersionTriplet, Signatures> CanonicalSignatures = {
        {{7, 11, 0}, {1040141098, 1034952072, 1040141035}},
        {{7, 13, 0}, {1070922641, 1070900920, 1070922591}},
        {{7, 21, 0}, {1071595718, 1071573965, 1071595658}},
        {{7, 23, 0}, {1078860820, 1078851198, 1078518254}},
        {{7, 24, 0}, {1078860820, 1078851198, 1078518254}},
        {{7, 26, 0}, {1078860820, 1078851198, 1078518254}},
        {{7, 27, 0}, {1078860820, 1078851198, 1078518254}},
        {{7, 30, 0}, {1092248115, 1078851198, 1092248185}},
        {{7, 40, 0}, {1103061404, 1100782760, 1102703238}},
        {{7, 41, 0}, {1103061404, 1100782760, 1102703238}},
        {{7, 50, 0}, {1123555699, 1122402677, 1123555657}},
        {{7, 55, 0}, {1132145551, 1122402677, 1129290974}},
        {{7, 60, 0}, {1134385715, 1122402677, 1134056126}},
        {{7, 70, 0}, {1134385715, 1146144984, 1134056126}},
        {{7, 72, 0}, {1134385715, 1146144984, 1134056126}},
        {{7, 80, 0}, {1154369347, 1154337224, 1154368006}},
        {{7, 81, 0}, {1154369347, 1154337224, 1154368006}},
        {{7, 90, 0}, {1165854030, 1164380451, 1165580232}},
        {{7, 92, 0}, {1168014195, 1164380451, 1166544872}},
        {{8, 00, 0}, {1182783462, 1164380451, 1182768756}},
        {{8, 11, 0}, {1207307831, 1195572439, 1206630834}},
        {{8, 20, 0}, {1214842282, 1213604102, 1214835913}},
        {{8, 21, 0}, {1215089195, 1213604102, 1214835913}},
        {{8, 22, 0}, {1218019489, 1213604102, 1218019493}},
        {{8, 30, 0}, {1222254518, 1213604102, 1221125906}},
        {{8, 31, 0}, {1222254518, 1213604102, 1221125906}},
        {{8, 40, 0}, {1228759162, 1226064248, 1228754556}},
        {{8, 41, 0}, {1236782105, 1226064248, 1236353258}},
        {{8, 42, 0}, {1237464009, 1226064248, 1236353258}},
        {{8, 50, 0}, {1246348779, 1244637398, 1246035278}},
        {{8, 52, 0}, {1246544092, 1254222903, 1246035278}},
        {{8, 53, 0}, {1256813714, 1256571859, 1254838832}},
        {{8, 54, 0}, {1260268714, 1256571859, 1260268679}},
        {{8, 55, 0}, {1268318035, 1256571859, 1267808369}},
        {{8, 56, 0}, {1268318035, 1256571859, 1267808369}},
        {{8, 57, 0}, {1268318035, 1256571859, 1267808369}},
        {{8, 60, 0}, {1277736737, 1256571859, 1277298068}},
        {{8, 61, 0}, {1282034876, 1256571859, 1281618245}},
        {{8, 62, 0}, {1284977744, 1281694936, 1281618245}}};

static std::optional<RecordingMetadata> ReadMetadata(
        const Version &version,
        const std::filesystem::path &path) {
    const MemoryFile file(path);
    const auto reader = file.Reader();

    const auto format = Recordings::GuessFormat(path, reader);

    try {
        auto [parsed, partial] = Recordings::Read(format, reader, version);

        if (!partial) {
            Gamestate state(version);

            for (const auto &frame : parsed->Frames) {
                for (const auto &event : frame.Events) {
                    event->Update(state);
                }
            }

            if (state.Creatures.contains(state.Player.Id)) {
                return std::make_optional<RecordingMetadata>(
                        format,
                        version.Triplet,
                        std::set<std::filesystem::path>({path.filename()}),
                        parsed->Runtime);
            }
        }
    } catch ([[maybe_unused]] const NotSupportedError &) {
        /* We don't have data files for this version, or cannot handle the
         * format under this configuration. Skip it. */
    } catch ([[maybe_unused]] const InvalidDataError &) {
        /* Most likely an error reading the container, we can't even
         * guess the version. */
    }

    return std::nullopt;
}

Database::Database(const std::filesystem::path &root, QObject *parent)
    : QAbstractListModel(parent),
      RootDirectory(root),
      DatDirectory(root / "dat"),
      PicDirectory(root / "pic"),
      SprDirectory(root / "spr"),
      VideoDirectory(root / "videos") {
    for (const auto &path :
         {DatDirectory, PicDirectory, SprDirectory, VideoDirectory}) {
        if (!std::filesystem::is_directory(path)) {
            std::filesystem::create_directories(path);
        }
    }

    Restore();
}

Database::~Database() {
}

template <typename Collection, typename Map, typename Reduce, typename Scalar>
Database::Future<Scalar> MappedReduced(const Collection &collection,
                                       Map map,
                                       Reduce reduce,
                                       Scalar accumulator) {
#ifdef DISABLE_QT_CONCURRENT
    for (const auto &value : collection) {
        reduce(accumulator, map(value));
    }

    return Database::Future(accumulator);
#else
    return QtConcurrent::mappedReduced(collection, map, reduce, accumulator);
#endif
}

std::pair<size_t, Database::Future<size_t>> Database::ImportDataFiles(
        const std::vector<std::filesystem::path> &paths) {
    return std::make_pair(
            paths.size(),
            MappedReduced(
                    paths,
                    [](const std::filesystem::path &path) {
                        return Collation::GatherDataFile(path);
                    },
                    [this](size_t &succeeded,
                           std::optional<Collation::DataFile> data) -> void {
                        if (data) {
                            const std::filesystem::path *root;
                            std::set<uint32_t> *collection;

                            switch (data->Kind) {
                            case Collation::DataFile::Type::Dat:
                                collection = &DatFiles;
                                root = &DatDirectory;
                                break;
                            case Collation::DataFile::Type::Pic:
                                collection = &PicFiles;
                                root = &PicDirectory;
                                break;
                            case Collation::DataFile::Type::Spr:
                                collection = &SprFiles;
                                root = &SprDirectory;
                                break;
                            }

                            auto [_, added] =
                                    collection->insert(data->Signature);
                            if (added) {
                                std::filesystem::copy_file(
                                        data->Path,
                                        *root / std::format("{}",
                                                            data->Signature),
                                        std::filesystem::copy_options::
                                                overwrite_existing);
                            }

                            succeeded++;
                        }
                    },
                    static_cast<size_t>(0)));
}

std::pair<size_t, Database::Future<size_t>> Database::ImportRecordingFiles(
        const std::vector<std::filesystem::path> &paths) {
    return std::make_pair(
            paths.size(),
            MappedReduced(
                    paths,
                    [this](const std::filesystem::path &path)
                            -> std::optional<std::pair<Collation::RecordingFile,
                                                       RecordingMetadata>> {
                        if (auto file = Collation::GatherRecordingFile(path)) {
                            for (auto &[triplet, version] : LoadedVersions) {
                                if (auto metadata = ReadMetadata(*version.get(),
                                                                 file->Path)) {
                                    return std::make_optional(
                                            std::make_pair(*file, *metadata));
                                }
                            }
                        }

                        return std::nullopt;
                    },
                    [this](size_t &succeeded,
                           std::optional<std::pair<Collation::RecordingFile,
                                                   RecordingMetadata>> result)
                            -> void {
                        if (result) {
                            const auto &[file, metadata] = *result;

                            auto existing = VideoFiles.find(file.Checksum);

                            if (existing != VideoFiles.end()) {
                                auto &previous = existing->second;

                                previous.Names.insert(file.Path.filename());
                                previous.Version = metadata.Version;
                            } else {
                                std::filesystem::copy_file(
                                        file.Path,
                                        VideoDirectory /
                                                std::format(
                                                        "{}",
                                                        static_cast<
                                                                std::string>(
                                                                file.Checksum)),
                                        std::filesystem::copy_options::
                                                overwrite_existing);
                                VideoFiles.emplace(file.Checksum, metadata);
                                VideoIndex.push_back(file.Checksum);
                            }

                            succeeded++;
                        }
                    },
                    static_cast<size_t>(0)));
}

std::pair<size_t, Database::Future<size_t>> Database::LoadData() {
    auto missingVersions =
            CanonicalSignatures | std::views::filter([this](const auto &pair) {
                const auto &[triplet, signatures] = pair;
                return !LoadedVersions.contains(triplet) &&
                       DatFiles.contains(signatures.Dat) &&
                       PicFiles.contains(signatures.Pic) &&
                       SprFiles.contains(signatures.Spr);
            });

    const std::vector<std::pair<VersionTriplet, Signatures>> versionsToLoad(
            missingVersions.begin(),
            missingVersions.end());

    return std::make_pair(
            versionsToLoad.size(),
            MappedReduced(
                    versionsToLoad,
                    [this](const auto &pair) {
                        const auto &[triplet, signatures] = pair;

                        const MemoryFile dat(DatDirectory /
                                             std::format("{}", signatures.Dat));
                        const MemoryFile pic(PicDirectory /
                                             std::format("{}", signatures.Pic));
                        const MemoryFile spr(SprDirectory /
                                             std::format("{}", signatures.Spr));

                        /* Note that we cannot use `std::unique_ptr` since
                         * `QtConcurrent` requires copyable values. */
                        return std::make_shared<Version>(triplet,
                                                         pic.Reader(),
                                                         spr.Reader(),
                                                         dat.Reader());
                    },
                    [this](size_t &loaded,
                           const std::shared_ptr<Version> &version) {
                        LoadedVersions.emplace(version->Triplet, version);
                        loaded++;
                    },
                    static_cast<size_t>(0)));
}

RecordingMetadata &Database::EditRecording(const QModelIndex &index) {
    auto it = VideoIndex.begin();

    std::advance(it, index.row());

    return VideoFiles.at(*it);
}

std::filesystem::path Database::RecordingPath(const QModelIndex &index) {
    const auto &checksum = VideoIndex.at(index.row());

    return VideoDirectory / static_cast<std::string>(checksum);
}

void Database::DeleteRecording(const QModelIndex &index) {
    auto it = VideoIndex.begin();

    std::advance(it, index.row());

    auto checksum = *it;
    AbortUnless(1 == VideoFiles.erase(checksum));
    VideoIndex.erase(it);

    /* The index MUST be updated before deleting, lest we leave it in an
     * inconsistent state.
     *
     * If we crash during/after writing but before the file is removed, its
     * absence from the index is wholly benign. */
    Persist();

    std::filesystem::remove(VideoDirectory /
                            static_cast<std::string>(checksum));
}

std::pair<const Version &, std::unique_ptr<Recordings::Recording>> Database::
        LoadRecording(const QModelIndex &id) {
    const auto &checksum = VideoIndex.at(id.row());
    const auto &metadata = VideoFiles.at(checksum);
    const auto &version = *LoadedVersions.at(metadata.Version);

    const MemoryFile file(VideoDirectory / static_cast<std::string>(checksum));
    const auto reader = file.Reader();

    auto &&[parsed, partial] =
            Recordings::Read(metadata.Format, reader, version);
    AbortUnless(!partial);

    return std::pair<const Version &, std::unique_ptr<Recordings::Recording>>(
            version,
            std::move(parsed));
}

void to_json(json &j, const RecordingMetadata &metadata) {
    j = json{{"Format", metadata.Format},
             {"Names", metadata.Names},
             {"Runtime", metadata.Runtime},
             {"Version", metadata.Version}};
}

void from_json(const json &j, RecordingMetadata &metadata) {
    j.at("Format").get_to(metadata.Format);
    j.at("Names").get_to(metadata.Names);
    j.at("Runtime").get_to(metadata.Runtime);
    j.at("Version").get_to(metadata.Version);
}

void Database::Persist() {
    json index{{"Version", 0},
               {"DatFiles", DatFiles},
               {"PicFiles", PicFiles},
               {"SprFiles", SprFiles},
               {"VideoFiles", VideoFiles}};

    /* Write to a temporary file first so that we don't blow the index if we
     * happen to crash in the middle of writing it. */
    const auto dstPath = RootDirectory / "index.json";
    const auto tmpPath = RootDirectory / "index.tmp";
    std::ofstream f(tmpPath);

    f << std::setw(4) << index << std::endl;
    f.flush();
    f.close();

    std::filesystem::rename(tmpPath, dstPath);
}

void Database::Restore() {
    const auto path = RootDirectory / "index.json";

    if (!std::filesystem::is_regular_file(path)) {
        return;
    }

    std::ifstream f(path);
    json index = json::parse(f);

    int version = -1;
    index.at("Version").get_to(version);

    if (version != 0) {
        throw InvalidDataError();
    }

    index.at("DatFiles").get_to(DatFiles);
    index.at("PicFiles").get_to(PicFiles);
    index.at("SprFiles").get_to(SprFiles);
    index.at("VideoFiles").get_to(VideoFiles);

    for (const auto &[id, _] : VideoFiles) {
        VideoIndex.emplace_back(id);
    }
}

int Database::rowCount([[maybe_unused]] const QModelIndex &parent) const {
    return VideoIndex.size();
}

int Database::columnCount([[maybe_unused]] const QModelIndex &parent) const {
    return 3;
}

QVariant Database::headerData(int section,
                              Qt::Orientation orientation,
                              int role) const {
    if (orientation == Qt::Orientation::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0:
            return "Name(s)";
        case 1:
            return "Version";
        case 2:
            return "Duration";
        }
    }

    return {};
}

QVariant Database::data(const QModelIndex &index, int role) const {
    if (index.isValid() && role == Qt::DisplayRole) {
        const auto &metadata = VideoFiles.at(VideoIndex.at(index.row()));

        if (index.column() == 0) {
            std::stringstream concatenated;

            const char *delimiter = "";
            for (const auto &name : metadata.Names) {
                concatenated << std::exchange(delimiter, ", ") << name.string();
            }

            return QVariant(concatenated.str().c_str());
        } else if (index.column() == 1) {
            return QVariant(static_cast<std::string>(metadata.Version).c_str());
        } else if (index.column() == 2) {
            auto runtime =
                    std::chrono::floor<std::chrono::seconds>(metadata.Runtime);
            return QVariant(std::format("{:%H:%M:%S}", runtime).c_str());
        }
    }

    return {};
}

void Database::resetModel() {
    QModelIndex parent;

    beginResetModel();
    insertRows(0, VideoIndex.size(), parent);
    endResetModel();
}

} // namespace GUI
} // namespace trc
