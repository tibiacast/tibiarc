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

#include "window.hpp"

#include "characterset.hpp"
#include "renderer.hpp"
#include "collation.hpp"

#ifndef DISABLE_QT_CONCURRENT
#    include <QFutureWatcher>
#endif

#include <QGraphicsPixmapItem>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <format>

namespace trc {
namespace GUI {

void Window::ImportFilesOrDirectories(
        std::function<void(const QStringList &)> callback,
        QFileDialog::FileMode mode,
        const QString &caption,
        const QString &filter) {
#ifndef EMSCRIPTEN
    QFileDialog dialog(this, caption);

    dialog.setNameFilter(filter);
    dialog.setFileMode(mode);

    if (dialog.exec()) {
        callback(dialog.selectedFiles());
    }
#else
    Q_UNUSED(mode);
    Q_UNUSED(caption);

    QFileDialog::getOpenFileContent(
            filter,
            [this, callback](const QString &name, const QByteArray &contents) {
                if (!name.isEmpty()) {
                    std::filesystem::path localName =
                            "XXXXXX" + name.toStdString();
                    QTemporaryFile tmp(localName.filename());

                    if (tmp.open()) {
                        auto filename = tmp.fileName();

                        tmp.write(contents);
                        tmp.flush();
                        tmp.close();

                        callback(QStringList({filename}));
                    }
                }
            });
#endif
}

void Window::ShowFutureProgress(size_t size,
                                QString caption,
                                Database::Future<size_t> future) {
#ifndef DISABLE_QT_CONCURRENT
    /* As the dialog is modal, `setValue(...)` will handle application events
     * for us, and allow the user to cancel the operation despite us not
     * returning to the main event loop. */
    QProgressDialog progress(caption, "Abort", 0, size, this);
    QFutureWatcher<size_t> watcher;

    progress.setWindowModality(Qt::WindowModal);
    watcher.setFuture(future);

    QObject::connect(&watcher, SIGNAL(finished()), &progress, SLOT(reset()));
    QObject::connect(&progress, SIGNAL(canceled()), &watcher, SLOT(cancel()));
    QObject::connect(&watcher,
                     SIGNAL(progressRangeChanged(int, int)),
                     &progress,
                     SLOT(setRange(int, int)));
    QObject::connect(&watcher,
                     SIGNAL(progressValueChanged(int)),
                     &progress,
                     SLOT(setValue(int)));

    (void)progress.exec();
    watcher.waitForFinished();
#else
    Q_UNUSED(size);
    Q_UNUSED(caption);
    Q_UNUSED(future);
#endif
}

void Window::ImportRecordings(const QStringList &roots) {
    std::vector<std::filesystem::path> paths;

    for (const auto &root : roots) {
        Collation::GatherRecordingPaths(root.toStdString(), paths);
    }

    auto [size, imported] = Database.ImportRecordingFiles(paths);

    ShowFutureProgress(size, "Importing recordings ...", imported);
    Database.resetModel();

    if (imported.isValid() && size != imported.result()) {
        QMessageBox::warning(
                this,
                "Failed to import recordings",
                std::format("Failed to import {} out of {} recordings, "
                            "they're either corrupt or you're lacking "
                            "appropriate data files.",
                            size - imported.result(),
                            size)
                        .c_str());
    }

#ifdef EMSCRIPTEN
    Database.Persist();
#endif
}

void Window::ImportData(const QStringList &roots) {
    std::vector<std::filesystem::path> paths;

    for (const auto &root : roots) {
        Collation::GatherDataPaths(root.toStdString(), paths);
    }

    auto [_, imported] = Database.ImportDataFiles(paths);
    imported.waitForFinished();

    auto [size, loaded] = Database.LoadData();

    ShowFutureProgress(imported.result(),
                       "Importing Tibia data files ...",
                       loaded);

    if (loaded.isValid() && size != loaded.result()) {
        QMessageBox::warning(
                this,
                "Failed to import data",
                std::format("Failed to import {} out of {} versions, the data "
                            "files were either corrupt or for unsupported "
                            "versions.",
                            size - loaded.result(),
                            size)
                        .c_str());
    }

#ifdef EMSCRIPTEN
    Database.Persist();
#endif
}

Window::Window(GUI::Database &database, QMainWindow *parent)
    : QMainWindow(parent),
      Database(database),
      Pages(this),

      Player(&Pages),
      Collection(database, &Pages),

      MenuBar(this),
      MenuFile(&MenuBar),
      MenuImportRecordings(&MenuFile),
      MenuImportDataFiles(&MenuFile),

      ActionExit(this),
      ActionImportRecordingFiles(this),
      ActionImportRecordingDirectories(this),
      ActionImportDataFiles(this),
      ActionImportDataDirectories(this) {
    resize(1680, 1050);

    {
        QSizePolicy policy(QSizePolicy::Policy::Preferred,
                           QSizePolicy::Policy::Preferred);
        policy.setHorizontalStretch(0);
        policy.setVerticalStretch(0);
        policy.setHeightForWidth(sizePolicy().hasHeightForWidth());

        setSizePolicy(policy);
        setMinimumSize(QSize(240, 240));
        setUnifiedTitleAndToolBarOnMac(false);
        setWindowTitle("tibiarc GUI");
    }

    ActionExit.setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ApplicationExit));
    ActionExit.setText("Exit");

    ActionImportRecordingFiles.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));
    ActionImportRecordingFiles.setText("From files");
    ActionImportDataFiles.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen));
    ActionImportDataFiles.setText("From files");

    MenuImportRecordings.addAction(&ActionImportRecordingFiles);
    MenuImportDataFiles.addAction(&ActionImportDataFiles);

#ifndef EMSCRIPTEN
    ActionImportRecordingDirectories.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::FolderOpen));
    ActionImportRecordingDirectories.setText("From directories");
    ActionImportDataDirectories.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::FolderOpen));
    ActionImportDataDirectories.setText("From directories");

    MenuImportRecordings.addAction(&ActionImportRecordingDirectories);
    MenuImportDataFiles.addAction(&ActionImportDataDirectories);
#endif

    MenuImportRecordings.setTitle("Import recordings ...");
    MenuImportDataFiles.setTitle("Import Tibia data ...");
    MenuFile.setTitle("File");

    MenuBar.addAction(MenuFile.menuAction());
    MenuFile.addAction(MenuImportRecordings.menuAction());
    MenuFile.addAction(MenuImportDataFiles.menuAction());
    MenuFile.addSeparator();
    MenuFile.addAction(&ActionExit);

    Pages.addWidget(&Collection);
    Pages.addWidget(&Player);

    setCentralWidget(&Pages);
    setMenuBar(&MenuBar);

    connect(&ActionExit, &QAction::triggered, [this]() { close(); });

    connect(&ActionImportRecordingFiles, &QAction::triggered, [this]() {
        ImportFilesOrDirectories(
                [this](auto roots) { ImportRecordings(roots); },
                QFileDialog::FileMode::ExistingFiles,
                "Open Tibia recording files",
                "All recordings (*.cam *.rec *.recording "
                "*.tmv *.tmv2 *.trp *.ttm *.yatc);;"
                "TibiaCAM (*.cam);;"
                "Tibiacast (*.recording);;"
                "TibiaMovie (*.tmv *.tmv2);;"
                "TibiaReplay (*.trp);;"
                "TibiaTimeMachine (*.ttm);;"
                "TibiCAM (*.rec);;"
                "YATC (*.yatc)");
    });

    connect(&ActionImportDataFiles, &QAction::triggered, [this]() {
        ImportFilesOrDirectories([this](auto roots) { ImportData(roots); },
                                 QFileDialog::FileMode::ExistingFiles,
                                 "Open Tibia data files",
                                 "Tibia data (*.dat *.pic *.spr)");
    });

#ifndef EMSCRIPTEN
    connect(&ActionImportRecordingDirectories, &QAction::triggered, [this]() {
        ImportFilesOrDirectories(
                [this](auto roots) { ImportRecordings(roots); },
                QFileDialog::FileMode::Directory,
                "Open Tibia recording directories");
    });

    connect(&ActionImportDataDirectories, &QAction::triggered, [this]() {
        ImportFilesOrDirectories([this](auto roots) { ImportData(roots); },
                                 QFileDialog::FileMode::Directory,
                                 "Open Tibia data directories");
    });
#endif

    connect(&Player, &Player::stop, [this]() { Pages.setCurrentIndex(0); });

    connect(&Collection, &Collection::play, [this](QModelIndex index) {
        auto [version, recording] = Database.LoadRecording(index);
        Player.Open(version, std::move(recording));

        Pages.setCurrentIndex(1);
    });

    auto [size, loaded] = Database.LoadData();
    ShowFutureProgress(size, "Loading Tibia data", loaded);
}

Window::~Window() {
}

} // namespace GUI
} // namespace trc
