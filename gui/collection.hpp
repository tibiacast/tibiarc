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

#ifndef __TRC_GUI_COLLECTION_HPP__
#define __TRC_GUI_COLLECTION_HPP__

#include "database.hpp"

#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStringMatcher>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

namespace trc {
namespace GUI {

class RecordingsModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit RecordingsModel(QObject *parent = nullptr);
    virtual ~RecordingsModel();

    void SetSearchPattern(const QString &pattern);

protected:
    bool filterAcceptsColumn(int sourceColumn,
                             const QModelIndex &sourceParent) const;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

private:
    QStringMatcher Pattern;
};

class Collection : public QWidget {
    Q_OBJECT

    GUI::Database &Database;

    QVBoxLayout Layout;
    QLineEdit SearchBar;
    QTableView Recordings;

    QAction ActionExportRecording;
    QAction ActionDeleteRecording;
    QAction ActionPlayRecording;
    QAction ActionRenameRecording;

    RecordingsModel Model;

public:
    explicit Collection(GUI::Database &database, QWidget *parent = nullptr);
    virtual ~Collection();

signals:
    void play(QModelIndex index);
};

} // namespace GUI
} // namespace trc

#endif
