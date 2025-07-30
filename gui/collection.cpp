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

#include "collection.hpp"

#include <QFileDialog>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>

#include <format>

namespace trc {
namespace GUI {

RecordingsModel::RecordingsModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
}

RecordingsModel::~RecordingsModel() {
}

void RecordingsModel::SetSearchPattern(const QString &pattern) {
    Pattern = QStringMatcher(pattern, Qt::CaseSensitivity::CaseInsensitive);
}

bool RecordingsModel::filterAcceptsColumn(
        [[maybe_unused]] int sourceColumn,
        [[maybe_unused]] const QModelIndex &sourceParent) const {
    return true;
}

bool RecordingsModel::filterAcceptsRow(int sourceRow,
                                       const QModelIndex &sourceParent) const {
    auto nameIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    auto name = sourceModel()->data(nameIndex).toString();

    return Pattern.indexIn(name) >= 0;
}

Collection::Collection(GUI::Database &database, QWidget *parent)
    : QWidget(parent),
      Database(database),
      Layout(this),
      SearchBar(this),
      Recordings(this),
      ActionExportRecording(this),
      ActionDeleteRecording(this),
      ActionPlayRecording(this),
      ActionRenameRecording(this) {

    Layout.addWidget(&SearchBar);

    Recordings.setContextMenuPolicy(Qt::ContextMenuPolicy::ActionsContextMenu);
    Recordings.setProperty("showDropIndicator", QVariant(false));
    Recordings.setSelectionMode(
            QAbstractItemView::SelectionMode::SingleSelection);
    Recordings.setSelectionBehavior(
            QAbstractItemView::SelectionBehavior::SelectRows);
    Recordings.setTextElideMode(Qt::TextElideMode::ElideNone);
    Recordings.setShowGrid(false);
    Recordings.setSortingEnabled(true);
    Recordings.setWordWrap(true);
    Recordings.setCornerButtonEnabled(false);
    Recordings.horizontalHeader()->setCascadingSectionResizes(false);
    Recordings.horizontalHeader()->setHighlightSections(false);
    Recordings.horizontalHeader()->setStretchLastSection(true);
    Recordings.verticalHeader()->setVisible(false);

    Layout.addWidget(&Recordings);

    Recordings.setModel(&Model);

    Recordings.horizontalHeader()->setSectionResizeMode(
            QHeaderView::ResizeToContents);

    ActionExportRecording.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::DocumentSaveAs));
    ActionExportRecording.setText("Save as");
    ActionDeleteRecording.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::EditDelete));
    ActionDeleteRecording.setText("Delete");
    ActionPlayRecording.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStart));
    ActionPlayRecording.setText("Play");
    ActionRenameRecording.setIcon(
            QIcon::fromTheme(QIcon::ThemeIcon::InputKeyboard));
    ActionRenameRecording.setText("Rename");

    Recordings.addAction(&ActionPlayRecording);
    Recordings.addAction(&ActionExportRecording);
    Recordings.addAction(&ActionRenameRecording);
    Recordings.addAction(&ActionDeleteRecording);

    SearchBar.setPlaceholderText("Search ...");
    connect(&SearchBar, &QLineEdit::textChanged, [this](const QString &string) {
        Model.SetSearchPattern(string);
        Model.invalidate();
    });

    connect(&Recordings, &QTableView::doubleClicked, [this](QModelIndex index) {
        emit play(Model.mapToSource(index));
    });

    connect(&ActionPlayRecording, &QAction::triggered, [this]() {
        emit play(Model.mapToSource(Recordings.currentIndex()));
    });

    Model.setSourceModel(&Database);

    connect(&ActionRenameRecording, &QAction::triggered, [this]() {
        auto index =
                Model.mapToSource(Recordings.currentIndex().siblingAtColumn(0));
        auto &metadata = Database.EditRecording(index);
        bool rename;

        QString name = QInputDialog::getText(
                this,
                "Rename recording",
                "Please enter a new name for this recording",
                QLineEdit::Normal,
                Database.data(index, Qt::DisplayRole).toString(),
                &rename);

        if (rename) {
            metadata.Names.clear();
            metadata.Names.insert(name.toStdString());

            Database.resetModel();
        }
    });

    connect(&ActionExportRecording, &QAction::triggered, [this]() {
        auto index = Model.mapToSource(Recordings.currentIndex());
        const auto &metadata = Database.EditRecording(index);

        const auto formatName =
                trc::Recordings::FormatNames::Get(metadata.Format);
        QString destination = QFileDialog::getSaveFileName(
                this,
                "Save recording as ...",
                std::format("recording{}", formatName.Extension.string())
                        .c_str(),
                std::format("{} recording (*{})",
                            formatName.Long,
                            formatName.Extension.string())
                        .c_str());

        if (!destination.isEmpty()) {
            std::filesystem::copy_file(
                    Database.RecordingPath(index),
                    destination.toStdString(),
                    std::filesystem::copy_options::overwrite_existing);
        }
    });

    connect(&ActionDeleteRecording, &QAction::triggered, [this]() {
        auto index =
                Model.mapToSource(Recordings.currentIndex().siblingAtColumn(0));

        auto reply = QMessageBox::question(
                this,
                "Delete recording?",
                "Are you sure you want to delete this recording?\n\n" +
                        Database.data(index, Qt::DisplayRole).toString(),
                QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            Database.DeleteRecording(index);
            Database.resetModel();
        }
    });
}

Collection::~Collection() {
}

} // namespace GUI
} // namespace trc
