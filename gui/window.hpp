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

#ifndef __TRC_GUI_MAINWINDOW_HPP__
#define __TRC_GUI_MAINWINDOW_HPP__

#include <QAction>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsScene>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTextEdit>

#include <chrono>
#include <functional>
#include <map>
#include <string>

#include "database.hpp"
#include "collection.hpp"
#include "player.hpp"

namespace trc {
namespace GUI {

class Window : public QMainWindow {
    Q_OBJECT

    GUI::Database &Database;
    QStackedWidget Pages;

    GUI::Player Player;
    GUI::Collection Collection;

    QMenuBar MenuBar;
    QMenu MenuFile;
    QMenu MenuImportRecordings;
    QMenu MenuImportDataFiles;

    QAction ActionExit;
    QAction ActionImportRecordingFiles;
    QAction ActionImportRecordingDirectories;
    QAction ActionImportDataFiles;
    QAction ActionImportDataDirectories;

    void ImportFilesOrDirectories(
            std::function<void(const QStringList &)> callback,
            QFileDialog::FileMode mode,
            const QString &caption,
            const QString &filter = QString());

    void ShowFutureProgress(size_t size,
                            QString caption,
                            Database::Future<size_t> future);
    void ImportRecordings(const QStringList &roots);
    void ImportData(const QStringList &roots);

public:
    explicit Window(GUI::Database &database, QMainWindow *parent = 0);
    ~Window();
};

} // namespace GUI
} // namespace trc

#endif