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

#include "memoryfile.h"

#include "utils.h"

#if !defined(_WIN32)
#    include <errno.h>
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#define MEMORYFILE_MAX_SIZE (1u << 30)

bool memoryfile_Open(const char *path, struct memory_file *file) {
#if defined(_WIN32)
    LARGE_INTEGER size;

    file->Handle = CreateFileA(path,
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_EXISTING,
                               FILE_FLAG_RANDOM_ACCESS,
                               NULL);

    if (file->Handle == INVALID_HANDLE_VALUE) {
        return trc_ReportError("Could not open file. WinErr: %u",
                               GetLastError());
    }

    if (GetFileSizeEx(file->Handle, &size) == FALSE) {
        DWORD error = GetLastError();
        CloseHandle(file->Handle);
        return trc_ReportError("Could not get size of file. WinErr: %u", error);
    }

    if (size.QuadPart > MEMORYFILE_MAX_SIZE) {
        CloseHandle(file->Handle);
        return trc_ReportError("File is too large");
    }

    file->Size = size.QuadPart;

    file->Mapping =
            CreateFileMapping(file->Handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (file->Mapping == NULL) {
        DWORD error = GetLastError();
        CloseHandle(file->Handle);
        return trc_ReportError("Could not open file mapping. WinErr: %u",
                               error);
    }

    file->View = (const uint8_t *)
            MapViewOfFile(file->Mapping, FILE_MAP_READ, 0, 0, 0);
    if (file->View == NULL) {
        DWORD error = GetLastError();
        CloseHandle(file->Mapping);
        CloseHandle(file->Handle);
        return trc_ReportError("Could not map a view of the file. WinErr: %u",
                               error);
    }
#else
    file->Fd = open(path, O_RDONLY);
    if (file->Fd == -1) {
        return trc_ReportError("Could not open file %s. errno: %u",
                               path,
                               errno);
    } else {
        struct stat fileStats;

        if (fstat(file->Fd, &fileStats) == -1) {
            close(file->Fd);
            return trc_ReportError("Could not determine size of the file. "
                                   "errno: %u",
                                   errno);
        }

        if (fileStats.st_size > MEMORYFILE_MAX_SIZE) {
            close(file->Fd);
            return trc_ReportError("File is too large");
        }

        file->Size = fileStats.st_size;
        file->View = (const uint8_t *)mmap(NULL,
                                           fileStats.st_size,
                                           PROT_READ,
                                           MAP_SHARED,
                                           file->Fd,
                                           0);
        if (file->View == MAP_FAILED) {
            close(file->Fd);
            return trc_ReportError(
                    "Could not map a view of the file. errno: %u",
                    errno);
        }
    }
#endif

    return true;
}

void memoryfile_Close(struct memory_file *file) {
    /* These can only fail due to some funny corruption of internal state, and
     * there's nothing sensible we can do about that besides dumping core. */
#if defined(_WIN32)
    ABORT_UNLESS(CloseHandle(file->Mapping));
    ABORT_UNLESS(CloseHandle(file->Handle));
#else
    ABORT_UNLESS(close(file->Fd) == 0);
#endif
}
