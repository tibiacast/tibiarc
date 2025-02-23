/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2024 "John Högberg"
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

#ifndef __TRC_MEMORYFILE_HPP__
#define __TRC_MEMORYFILE_HPP__

#include "datareader.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
extern "C" {
#    include <windows.h>
}
#endif

namespace trc {
class MemoryFile {
#if defined(_WIN32)
    HANDLE Mapping;
    HANDLE Handle;
#else
    int Fd;
#endif
    size_t Size;
    const uint8_t *View;

public:
    DataReader Reader() const {
        return DataReader(Size, View);
    }

    MemoryFile(const std::string &path);
    ~MemoryFile();
};
} // namespace trc

struct memory_file {
#if defined(_WIN32)
    HANDLE Mapping;
    HANDLE Handle;
#else
    int Fd;
#endif

    size_t Size;
    const uint8_t *View;
};

bool memoryfile_Open(const char *path, struct memory_file *file);
void memoryfile_Close(struct memory_file *file);

#endif
