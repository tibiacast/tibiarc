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

#ifndef __TRC_GUI_DATABASE_HPP__
#define __TRC_GUI_DATABASE_HPP__

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include <QAbstractListModel>

#ifndef DISABLE_QT_CONCURRENT
#    include <QFuture>
#endif

#include "collation.hpp"
#include "recordings.hpp"
#include "versions.hpp"

namespace trc {
namespace GUI {

struct RecordingMetadata {
    /* The names this recording has been seen with; per-hash duplicates are
     * quite common. */
    Recordings::Format Format;
    VersionTriplet Version;

    std::set<std::filesystem::path> Names;
    std::chrono::milliseconds Runtime;
};

class Database : public QAbstractListModel {
    Q_OBJECT

    const std::filesystem::path RootDirectory, DatDirectory, PicDirectory,
            SprDirectory, VideoDirectory;

public:
#ifndef DISABLE_QT_CONCURRENT
    template <typename T> using Future = QFuture<T>;
#else
    /* QFuture<T> doesn't work properly under the Emscripten version used by
     * Qt for Webassembly (3.0.17 as of writing), so we cheat by making
     * everything synchronous for the time being. */
    template <typename T> class Future {
        T Value;

    public:
        Future(T value) : Value(value) {
        }

        Future(const Future<T> &other) : Value(other.result()) {
        }

        bool isValid() const {
            return true;
        }

        T result() const {
            return Value;
        }

        void waitForFinished() {
        }
    };
#endif

    Database(const std::filesystem::path &root, QObject *parent = nullptr);
    virtual ~Database();

    void Persist();

    std::pair<size_t, Future<size_t>> ImportDataFiles(
            const std::vector<std::filesystem::path> &paths);
    std::pair<size_t, Future<size_t>> ImportRecordingFiles(
            const std::vector<std::filesystem::path> &paths);
    std::pair<size_t, Future<size_t>> LoadData();

    RecordingMetadata &EditRecording(const QModelIndex &index);
    std::filesystem::path RecordingPath(const QModelIndex &index);
    void DeleteRecording(const QModelIndex &index);

    std::pair<const Version &, std::unique_ptr<Recordings::Recording>>
    LoadRecording(const QModelIndex &index);

    /* List model interface */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role) const;
    QVariant data(const QModelIndex &index, int role) const;
    void resetModel();

private:
    void Restore();

    std::map<VersionTriplet, std::shared_ptr<Version>> LoadedVersions;
    std::set<uint32_t> DatFiles, SprFiles, PicFiles;
    std::map<Collation::Checksum, RecordingMetadata> VideoFiles;
    std::vector<Collation::Checksum> VideoIndex;
};

} // namespace GUI
} // namespace trc

#endif
