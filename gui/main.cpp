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

#include <QApplication>
#include <QLockFile>
#include <QStandardPaths>

#include "database.hpp"
#include "window.hpp"

#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif

using namespace trc;

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    application.setApplicationName("tibiarc_gui");

#ifndef EMSCRIPTEN
    const std::filesystem::path root =
            QStandardPaths::writableLocation(
                    QStandardPaths::AppLocalDataLocation)
                    .toStdString();

    if (!std::filesystem::is_directory(root)) {
        std::filesystem::create_directories(root);
    }

    QLockFile lock((root / "lock").c_str());

    if (!lock.tryLock()) {
        /* FIXME: ask user to delete lock file? */
        return 1;
    }
#else
    const std::filesystem::path root = "/tibiarc";

    /* Store all of our data in IndexedDB. */
    EM_ASM(FS.mkdir('/tibiarc');
           FS.mount(IDBFS, {autoPersist : true}, '/tibiarc'););
#endif

    GUI::Database database(root);
    GUI::Window window(database);

    window.show();

    int result = application.exec();

    if (result == 0) {
        database.Persist();
    }

    return result;
}
