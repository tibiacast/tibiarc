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

#include "memoryfile.hpp"

#include "utils.hpp"

#if !defined(_WIN32)
#    include <errno.h>
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#define MEMORYFILE_MAX_SIZE (1u << 30)

namespace trc {
MemoryFile::MemoryFile(const std::string &path) {
#if defined(_WIN32)
    LARGE_INTEGER size;

    Handle = CreateFileA(path.c_str(),
                         GENERIC_READ,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_FLAG_RANDOM_ACCESS,
                         NULL);

    if (Handle == INVALID_HANDLE_VALUE) {
        throw IOError();
    }

    if (GetFileSizeEx(Handle, &size) == FALSE) {
        CloseHandle(Handle);
        throw IOError();
    }

    if (size.QuadPart > MEMORYFILE_MAX_SIZE) {
        CloseHandle(Handle);
        throw InvalidDataError();
    }

    Size = size.QuadPart;

    Mapping = CreateFileMapping(Handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (Mapping == NULL) {
        CloseHandle(Handle);
        throw IOError();
    }

    file->View = (const uint8_t *)
            MapViewOfFile(file->Mapping, FILE_MAP_READ, 0, 0, 0);
    if (file->View == NULL) {
        CloseHandle(Mapping);
        CloseHandle(Handle);
        throw IOError();
    }
#else
    Fd = open(path.c_str(), O_RDONLY);
    if (Fd == -1) {
        throw IOError();
    } else {
        struct stat fileStats;

        if (fstat(Fd, &fileStats) == -1) {
            close(Fd);
            throw IOError();
        }

        if (fileStats.st_size > MEMORYFILE_MAX_SIZE) {
            close(Fd);
            throw InvalidDataError();
        }

        Size = fileStats.st_size;
        View = (const uint8_t *)
                mmap(NULL, fileStats.st_size, PROT_READ, MAP_SHARED, Fd, 0);
        if (View == MAP_FAILED) {
            close(Fd);
            throw IOError();
        }
    }
#endif
}

MemoryFile::~MemoryFile() {
    /* These can only fail due to some funny corruption of internal state, and
     * there's nothing sensible we can do about that besides dumping core. */
#if defined(_WIN32)
    AbortUnless(CloseHandle(Mapping));
    AbortUnless(CloseHandle(Handle));
#else
    AbortUnless(close(Fd) == 0);
#endif
}
} // namespace trc
