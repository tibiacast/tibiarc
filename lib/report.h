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

#ifndef __TRC_REPORT_H__
#define __TRC_REPORT_H__

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

enum TrcErrorReportMode {
    TRC_ERROR_REPORT_MODE_NONE,
    TRC_ERROR_REPORT_MODE_ABORT,
    TRC_ERROR_REPORT_MODE_TEXT
};

#define trc_ReportError(Format, ...)                                           \
    (_trc_ReportError("%s (line %u): " Format "\n",                            \
                      __FUNCTION__,                                            \
                      __LINE__,                                                \
                      ##__VA_ARGS__),                                          \
     false)

void _trc_ReportError(const char *format, ...);

bool trc_GetLastError(size_t size, char *result);

enum TrcErrorReportMode trc_ChangeErrorReporting(enum TrcErrorReportMode mode);

#endif /* __TRC_REPORT_H__ */
